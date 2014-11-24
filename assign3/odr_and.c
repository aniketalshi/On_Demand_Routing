#include "odr.h"
#include "utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* globals */
int stale_param = 0;

/* Populate entries from string to send params */
send_params_t*
get_send_params (char *str) {
    assert(str);
    char dstip[MAXLINE], dstport[IP_LEN], 
        msg[MAXLINE], flag[IP_LEN], filename[MAXLINE];
    
    send_params_t *sp = (send_params_t *)malloc(sizeof(send_params_t));
    sscanf(str, "%[^','],%[^','],%[^','],%[^','],%s", 
                            dstip, dstport, msg, filename, flag); 
    
    strncpy(sp->destip, dstip, strlen(dstip));
    strncpy(sp->msg, msg, strlen(msg));
    strncpy(sp->filename, filename, strlen(filename));
    sp->destport        = atoi(dstport);
    sp->route_disc_flag = atoi(flag);
    return sp;
}


/* return next portno */
int get_next_portno() {
    static int portno = 909;
    return portno++;
}

/* fetch port to sunpath mapping entry using port*/
map_port_sp_t *
fetch_entry_port (int portno) {
    map_port_sp_t *curr, *entry;

    if (!port_spath_head) 
        return NULL;

    for (curr = port_spath_head; curr != NULL; curr = curr->next) {
        if (curr->portno == portno) {
            return curr;
        }
    }
    return NULL;
}


/* Fetch entry from port to sunpath table */
map_port_sp_t *
fetch_entry_path (char *sun_path) {
    
    assert(sun_path);
    map_port_sp_t *curr, *entry;

    if (!port_spath_head) 
        return NULL;

    for (curr = port_spath_head; curr != NULL; curr = curr->next) {
        if (strcmp(curr->sun_path, sun_path) == 0) {
            return curr;
        }
    }

    entry = (map_port_sp_t *) calloc(1, sizeof(map_port_sp_t));

    /* get next port no for this client */
    entry->portno = get_next_portno();
    strncpy(entry->sun_path, sun_path, strlen(sun_path));
    gettimeofday(&entry->ts, NULL);
    
    /* insert this entry in table */
    entry->next = port_spath_head;
    port_spath_head = entry;
    return entry;
}


/* given the port no, get file name from port to sunpath table */
int
get_file_name (int portno, char *path) {
    map_port_sp_t *curr;
    
    if (!port_spath_head) {
        fprintf (stderr, "ERROR: Table empty!\n");
        return -1;
    }

    /* traverse till end of list */
    for(curr = port_spath_head; curr->next != NULL; curr = curr->next) {
        if (curr->portno == portno) {
            strcpy (path, curr->sun_path);
            return 1;
        }
    }
    
    /* If the entry does not exist in the table. */
    fprintf (stderr, "ERROR: Port no does not exist in the table.\n");
    return -1;
}

/********************************************************************* 
 * 1. Check if entry for this dest is there in routing table
 * 2. If not, insert this msg in pending_queue
 *    2.1 Flood RREQ
 * 3. If entry is present, check validity
 * 4. If all is well, send the ethernet frame on int in routing table
 ********************************************************************/
/* handle request from peer process received by ODR */
int
handle_peer_msg (int sockfd, int proc_sockfd, struct sockaddr_un *proc_addr, 
                                        send_params_t *sparams) {
    assert(proc_addr);
    assert(sparams);

    int entry_present = 0, broad_id = 0;
    map_port_sp_t   *map_entry, *peer_entry;
    r_entry_t       *route;
    char *self_ip = get_self_ip();
    odr_frame_t *odrframe;
    char destvm[INET_ADDRSTRLEN], src_vmname[INET_ADDRSTRLEN], dst_vmname[INET_ADDRSTRLEN];
    
    if (self_ip == NULL) 
        return -1;
    
    DEBUG(printf("\nRecvd data from peer process with sun_path: \"%s\"\n", sparams->filename));
    
    /* check if entry exist and if not insert new entry in port to sunpath table */
    if((map_entry = fetch_entry_path (sparams->filename)) == NULL) {
        fprintf(stderr, "unable to create entry in port sunpath table");
        return -1;
    }
    
    strcpy(src_vmname, get_name_ip(self_ip));
    strcpy(dst_vmname, get_name_ip(sparams->destip));
    assert(src_vmname);
    assert(dst_vmname);

    if (sparams->destport == __SERV_PORT) {
        printf("Client at %s sending request to server at %s\n", src_vmname, dst_vmname);
    } else {
        printf("Server at %s sending reply to client at %s\n", src_vmname, dst_vmname);
    }

    if (sparams->route_disc_flag) {
        printf("Received packet with route discovery flag set.\n");
    }
    
    if (strcmp (self_ip, sparams->destip) == 0) {
        peer_entry = fetch_entry_port (sparams->destport);
        assert (peer_entry);

        if (send_to_peer_process (proc_sockfd, peer_entry->sun_path, 
                            self_ip, map_entry->portno, sparams->msg) < 0) {
            
            fprintf(stderr, "unable to send entry in port sunpath table");
            return -1;
        }
        return 1;
    }

    /************************ NOTE ******************************
     * If the route disc flag is set, we check if broadcast id is
     * greater than existing entry, only then we remove the entry 
     * If flag is not set, we check if entry is stale
     ***********************************************************/
    
    /* Check if the entry is already present */
    entry_present = get_r_entry(sparams->destip, &route, sparams->route_disc_flag); 
      
    /* If the routing table is empty or if the entry is not present */
    if (entry_present <= 0) {
        DEBUG(printf ("\n Entry is not present in table. Will be sending RREQ \n"));
        
        /* send the RREQ packet*/
        if (send_req_broadcast (sockfd, -1, get_broadcast_id(),
                                  map_entry->portno, sparams->destport, 
                                    0, sparams->route_disc_flag, sparams->destip, 
                                         self_ip, sparams->msg, 0) < 0) {
            fprintf(stderr, "Error in sending broadcast\n");
            return -1;
        }
        
        /* construct odr frame for this payload msg */ 
        if ((odrframe = construct_odr (__DATA, 0, get_broadcast_id(), PAYLOAD_SIZE, 0, 
                                                    map_entry->portno, sparams->destport, 
                                                    sparams->destip, self_ip, sparams->msg)) == NULL) {
            fprintf(stderr, "Error in constructing frame\n");
            return -1;
        }
        
        /* convert this frame from network to host order before storing */
        convert_net_host_order(odrframe);
        
        //insert msg in pending queue
        if (insert_pending_queue (odrframe, sparams->destip) < 0) {
            fprintf(stderr, "Error inserting frame in pending queue\n");
            return -1;
        }
        
        strcpy(destvm, get_name_ip(sparams->destip));
        assert(destvm);
        
        //DEBUG(printf("\nInserted in pending Queue Data packet for %s\n", destvm)); 
        
        return 1;
    } else { /* If the entry is present in the routing table, send the packet */
        
        assert(route);
        strcpy(destvm, get_name_ip(sparams->destip));
        assert(destvm);
        
        DEBUG(printf("\nEntry already presnet in routing table for %s\n", destvm)); 
        
        /* send the data packet */
        if (send_data_message(sockfd, map_entry->portno, sparams, route) < 0) {
            fprintf(stderr, "\nError sending data message by ODR\n");
            return -1;
        }
        return 1;
    }

    return 0;
}

/*Handle ether messages.
 * + Check if the destination is self.
 *      + If so, write to the corresponding server/client
 * + If dest not self, see if this is RREQ or RREP
 *      + If RREQ:
 *          + See if the entry for the dest exists in routing table
 *          + If yes, prepare RREP.
 *          + If no, broadcast RREQ over all other interfaces.
 *      + If RREP:
 *          + Forward the packet to the correct node after 
 *                  looking up from routing table.
 */
/* handle message received over ethernet interface */
int
handle_ethernet_msg (int odr_sockfd, int proc_sockfd, 
                        struct sockaddr_ll *odr_addr, void *recv_buf) {

    odr_frame_t *recvd_odr_frame;
    char *self_ip, sun_path[MAXLINE], next_hop[HWADDR+1];
    char src_vmname[INET_ADDRSTRLEN], dst_vmname[INET_ADDRSTRLEN], *src_mac;
    r_entry_t *dest_r_entry, *src_r_entry, *r_entry;
    odr_frame_t *frame;
    
    int  dest_entry_present = 0, src_entry_present = 0, broadcast_id,
         intf_n = 0, asent_flag = 0;
    
    /* get IP Address of the current node. */
    if ((self_ip = get_self_ip ()) == NULL) {
        return -1;
    }

    /* get the interface number of the packet. */
    intf_n = odr_addr->sll_ifindex;
    
    /*populate the recvd odr frame, convert network to host order*/ 
    if (process_recvd_frame (&recvd_odr_frame, recv_buf) < 0) {
        perror("\nError in processing recvd frame");
        return 0;
    }
    
    if(recvd_odr_frame == NULL) {
        perror("\nNull frame recvd");
        return 0;
    }
    
    /* if this is same RREQ sent by me */
    if (recvd_odr_frame->src_ip == self_ip)
        return 1;

    /* if route disc flag is present and broadcast id is greater, 
     * remove entries in routing table */
    if (recvd_odr_frame->route_disc_flag == 1) {
        
        DEBUG (printf ("\nRecvd packet with route discovery flag set.\n"));
        /* this is duplicate RREQ packet, ignore it */
        if (is_broadid_greater(recvd_odr_frame->src_ip, 
                         recvd_odr_frame->broadcast_id) < 0) {
           return 1; 
        }
    }
        
    /* Check if the destip is already present in routing table. */
    dest_entry_present = get_r_entry (recvd_odr_frame->dest_ip, &dest_r_entry, 
                                                        recvd_odr_frame->route_disc_flag); 
    
    /* Check if the srcip is already present in routing table. */
    src_entry_present = get_r_entry (recvd_odr_frame->src_ip, &src_r_entry, 0); 
    
    /* Get name of vm we recvd packet from */
    strcpy(src_vmname, get_name_ip(recvd_odr_frame->src_ip));
    strcpy(dst_vmname, get_name_ip(recvd_odr_frame->dest_ip));
    assert(src_vmname);
    assert(dst_vmname);

    
    switch (recvd_odr_frame->frame_type) {
        case __RREQ: {
            
            printf("\nReceived RREQ from %s, intended for %s, broadcast id %d\n", 
                                src_vmname, dst_vmname, recvd_odr_frame->broadcast_id);
                
            if (src_entry_present > 0) {
                /* If entry for src ip is present, check if we can update the entry */ 
                asent_flag = check_r_entry (recvd_odr_frame, src_r_entry, intf_n, odr_addr->sll_addr); 
            } else {
                /* Insert the new information about src IP in r_table. */
                insert_r_entry (recvd_odr_frame, &src_r_entry, intf_n, odr_addr->sll_addr);
                
                /* entry is newly inserted, set asent_flag */
                asent_flag = 1;
            }

            /* Check if the current node is the destination node. */
            if (strcmp (recvd_odr_frame->dest_ip, self_ip) == 0){
                /* Send RREP. */ 
                if (send_rrep_packet (odr_sockfd, recvd_odr_frame, src_r_entry, 0, 1) < 0) {
                    fprintf(stderr, "Error sending RREP packet");
                    return -1;
                }
                break;
            }
            
            /****** You are an intermediate node receiving RREQ *********/

            /* Check if an entry already exists in the routing table for given destination */
            if (dest_entry_present > 0) {
                
                //printf ("\nEntry exists in table for node : %s", dst_vmname);
                
                /* send the rrep */
                if (send_rrep_packet (odr_sockfd, recvd_odr_frame, 
                                     src_r_entry, dest_r_entry->no_hops, 1) < 0) {
                    fprintf(stderr, "Error sending RREP packet");
                    return -1;
                }
                
                /* if asent flag is set, broadcast information with asent flag set */
                if (asent_flag == 1) { 
                    if (send_req_broadcast (odr_sockfd, intf_n, recvd_odr_frame->broadcast_id, 
                                            recvd_odr_frame->src_port, recvd_odr_frame->dst_port,
                                            recvd_odr_frame->hop_count + 1, recvd_odr_frame->route_disc_flag, 
                                            recvd_odr_frame->dest_ip, recvd_odr_frame->src_ip, 
                                            recvd_odr_frame->payload, 1) < 0) {
                        
                        fprintf(stderr, "Error Flooding Packet with ASENT flag");
                        return -1;
                    }
                }
                
                break;
            
            } else {
                
                /* Info about src entry is updated, only then we need to flood this RREQ
                 * else we have already flooded this RREQ, we can ignore this 
                 */
                if (asent_flag == 0) { 
                    DEBUG(printf("\nRREQ already flooded for this request with id %d",
                                                            recvd_odr_frame->broadcast_id));
                    break;
                }
                
                assert(recvd_odr_frame);
                
                /* if dest entry is not present, flood rreq */
                if (send_req_broadcast (odr_sockfd, intf_n, recvd_odr_frame->broadcast_id, 
                                        recvd_odr_frame->src_port, recvd_odr_frame->dst_port,
                                        recvd_odr_frame->hop_count + 1, recvd_odr_frame->route_disc_flag, 
                                        recvd_odr_frame->dest_ip, recvd_odr_frame->src_ip, 
                                        recvd_odr_frame->payload, 0) < 0) {
                    
                    fprintf(stderr, "Error Flooding RREP");
                    return -1;
                }
                break;
            }

            break;
        }
        
        case __RREP: {
            
            printf("\nRREP received Src Node: %s, Destination Node: %s\n", 
                                                        src_vmname, dst_vmname);
            
            if (src_entry_present > 0) {
                /* If entry for src ip is present, check if we can update the entry */ 
                check_r_entry (recvd_odr_frame, src_r_entry, intf_n, odr_addr->sll_addr); 
            } else {
                /* Insert the new information about src IP in r_table. */
                insert_r_entry (recvd_odr_frame, &src_r_entry, intf_n, odr_addr->sll_addr);
            }
            
            
            /* check if this RREP is intended for me */
            if (strcmp (recvd_odr_frame->dest_ip, self_ip) == 0) {
                
               /* Look for msg in msg pending queue for this destination node and port */
               if ((frame = lookup_pending_queue (recvd_odr_frame->src_ip)) != NULL) {
                    
                    /* this parked frame is an RREP frame */
                    if (frame->frame_type == __RREP) {

                        //DEBUG(printf("\nRREP Packet exists in pending queue\n"));
                        /* Forward the rrep packet */
                        if (send_rrep_packet (odr_sockfd, frame, 
                                              src_r_entry, frame->hop_count, 0) < 0) {
                            fprintf(stderr, "Error sending RREP packet");
                            return -1;
                        }

                    } else if (frame->frame_type == __DATA) {
                        //DEBUG(printf("\nDATA packet exists in pending queue\n"));
                        
                        /* convert from host to network order */
                        convert_host_net_order(frame);
                
                        /* get mac address from interface num */
                        if ((src_mac = get_hwaddr_from_int (src_r_entry->if_no)) == NULL) {
                            fprintf(stderr, "error retreiving mac address\n");
                            return -1;
                        }

                        /* send raw frame on wire */
                        if (send_raw_frame (odr_sockfd, src_mac, src_r_entry->next_hop, 
                                                            src_r_entry->if_no, frame) < 0) {
                            fprintf(stderr, "error sending raw frame on wire\n");
                            return -1;
                        }

                        assert(get_name_ip (src_r_entry->destip));
                        DEBUG(printf("\nData packet sent to node %s, on interface %d\n",
                                        get_name_ip (src_r_entry->destip), src_r_entry->if_no));
                    }
               }
               break;
            }

            /*************I am an intermediate node. Foward this RREP *******************/
            
            /* Check if an entry already exists in the routing table for given destination */
            if (dest_entry_present > 0) {

                DEBUG(printf ("\nEntry exists in table for node : %s\n", dst_vmname));
                assert(src_r_entry);    
                assert(dest_r_entry);    
                
                /* send the rrep */
                if (send_rrep_packet (odr_sockfd, recvd_odr_frame, 
                                     dest_r_entry, src_r_entry->no_hops, 0) < 0) {
                    fprintf(stderr, "Error sending RREP packet");
                    return -1;
                }
                break;
            
            }  else {
                /* If an entry for this destination is not presnt, flood out RREQ */
                assert(recvd_odr_frame);
               
               DEBUG(printf ("\nNo Entry in table for node : %s. Flooding RREQ.\n", dst_vmname));
                
                /* this RREQ will have new broadcast id */
                broadcast_id = get_broadcast_id();

                /* Insert the RREP packet in msg queue */
                if (insert_pending_queue (recvd_odr_frame, recvd_odr_frame->dest_ip) < 0) {
                    fprintf(stderr, "Error inserting packet in pending queue");
                    return -1;
                }
                
                /* flood rreq on all interfaces except one on which req arrived */
                if (send_req_broadcast (odr_sockfd, intf_n, broadcast_id, 
                                        -1, recvd_odr_frame->dst_port,
                                        0, 0, recvd_odr_frame->dest_ip, self_ip, 
                                        recvd_odr_frame->payload, 0) < 0) {
                    
                    fprintf(stderr, "Error Flooding RREP");
                    return -1;
                }
                break;
            }

            break;
        }

        case __DATA: {
            printf("Data Packet Received. Src Node: %s, Destination Node: %s\n", src_vmname, dst_vmname);
           
            /* if we can update information about source ip */
            if (src_entry_present > 0) {
                /* If entry for src ip is present, check if we can update the entry */ 
                check_r_entry (recvd_odr_frame, src_r_entry, intf_n, odr_addr->sll_addr); 
            } else {
                /* Insert the new information about src IP in r_table. */
                insert_r_entry (recvd_odr_frame, &src_r_entry, intf_n, odr_addr->sll_addr);
            }

            /* if I am the destination node */
            if (strcmp (recvd_odr_frame->dest_ip, self_ip) == 0) {

                /* check data msg and send to peer process if it is for me */
                if(process_data_message (proc_sockfd, odr_sockfd, recvd_odr_frame) < 0) {
                    fprintf (stderr, "unable to process data\n");
                    break;
                }

                break;
            }

            /**** This data packet is not for me, I am an intermediate node ********/

            /* Check if an entry already exists in the routing table for given destination */
            if (dest_entry_present > 0) {
                assert(dest_r_entry); 
                
                /* assign num of hops */
                recvd_odr_frame->hop_count = src_r_entry->no_hops;

                DEBUG(printf("\nEntry exists in routing table for node %s\n", dst_vmname));
                /* convert host to network order for sending packet */
                convert_host_net_order(recvd_odr_frame);
                
                /* get mac address from interface num */
                if ((src_mac = get_hwaddr_from_int (dest_r_entry->if_no)) == NULL) {
                    fprintf(stderr, "error retreiving mac address\n");
                    return -1;
                }
                
                /* send raw frame on wire */
                if (send_raw_frame (odr_sockfd, src_mac, dest_r_entry->next_hop, 
                                        dest_r_entry->if_no, recvd_odr_frame) < 0) {
                    fprintf(stderr, "error sending raw frame on wire\n");
                    return -1;
                }

                assert(get_name_ip (dest_r_entry->destip));
                DEBUG(printf("\nData packet sent to node %s, on interface %d\n",
                                get_name_ip (dest_r_entry->destip), dest_r_entry->if_no));

                break;
            } else {

                assert(recvd_odr_frame);
                DEBUG(printf ("\nNo Entry in table for node : %s. Flooding RREQ.\n", dst_vmname));
                
                /* this RREQ will have new broadcast id */
                broadcast_id = get_broadcast_id();

                /* Insert the RREP packet in msg queue */
                if (insert_pending_queue (recvd_odr_frame, recvd_odr_frame->dest_ip) < 0) {
                    fprintf(stderr, "Error inserting packet in pending queue");
                    return -1;
                }
                
                /* flood rreq on all interfaces except one on which req arrived */
                if (send_req_broadcast (odr_sockfd, intf_n, broadcast_id, 
                                        -1, recvd_odr_frame->dst_port,
                                        0, 0, recvd_odr_frame->dest_ip, self_ip, 
                                        recvd_odr_frame->payload, 0) < 0) {
                    
                    fprintf(stderr, "Error Flooding RREP");
                    return -1;
                }
                break;
            }

            break;
        }

        case __ASENT: {
            printf("\nReceived packet with Asent flag set");
            
            if (src_entry_present > 0) {
                /* If entry for src ip is present, check if we can update the entry */ 
                asent_flag = check_r_entry (recvd_odr_frame, src_r_entry, intf_n, odr_addr->sll_addr); 
            } else {
                /* Insert the new information about src IP in r_table. */
                insert_r_entry (recvd_odr_frame, &src_r_entry, intf_n, odr_addr->sll_addr);
                asent_flag = 1;
            }
            
            /* if asent flag is set, broadcast this to my neighbors */
            if (asent_flag == 1) {
                if (send_req_broadcast (odr_sockfd, intf_n, recvd_odr_frame->broadcast_id, 
                        recvd_odr_frame->src_port, recvd_odr_frame->dst_port,
                        recvd_odr_frame->hop_count + 1, recvd_odr_frame->route_disc_flag, 
                        recvd_odr_frame->dest_ip, recvd_odr_frame->src_ip, 
                        recvd_odr_frame->payload, 1) < 0) {
    
                         fprintf(stderr, "Error Flooding Packet with ASENT flag");
                        return -1;
                }
            }
            break;
        }

        default: {
            printf("\nError in packet type %d", recvd_odr_frame->frame_type);
            return -1;
        }
    }
    
    print_routing_table();
    return 0;
}

int main (int argc, const char *argv[]) {
    int len, proc_sockfd, odr_sockfd, resp_sockfd, socklen;
    fd_set set, currset;
    char buff[MAXLINE];
    struct sockaddr_un serv_addr, proc_addr, resp_addr;
    struct sockaddr_ll odr_addr;
    
    /* initializations */
    socklen        = sizeof(struct sockaddr_un);
    void *recv_buf = malloc(ETH_FRAME_LEN); 
    socklen_t odrsize = sizeof(struct sockaddr_ll);
    
    /* insert serv port to server sunpath mapping in table */
    if (insert_map_port_sp (__SERV_PORT, __UNIX_SERV_PATH) < 0) {
        fprintf(stderr, "Error inserting in port to sunpath map");
        return 0;
    }
    
    if(fetch_entry_port (__SERV_PORT) == NULL) {
        fprintf(stderr, "Error inserting in port to sunpath map");
        return 0;
    }
    /* check if staleness parameter supplied */
    if (argc < 2) {
        stale_param = 5;
    } else {
        /* staleness parameter */ 
        stale_param = atoi((char *)argv[1]);
    }
    
    if(stale_param < 0) {
        fprintf(stderr, "\n Invalid Staleness value");
        return 0;
    }
    
   /* create new UNIX Domain socket */
    if((proc_sockfd = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "\nerror creating unix domain socket\n");
        return 0;
    }
    

    printf("\n====================== ODR INITIALIZING ============================\n");

    unlink(__UNIX_PROC_PATH);
    memset(&serv_addr, 0, sizeof(serv_addr)); 
    serv_addr.sun_family = AF_LOCAL;
    strcpy(serv_addr.sun_path, __UNIX_PROC_PATH);
    
    /* Bind the UNIX Domain socket */
    Bind(proc_sockfd, (SA *)&serv_addr, SUN_LEN(&serv_addr));
    DEBUG(printf("\nUnix Domain socket %d, sun_path file name %s\n",
                                    proc_sockfd, __UNIX_PROC_PATH)); 
    
    /* Get the new PF Packet socket */
    if((odr_sockfd = socket(PF_PACKET, SOCK_RAW, htons(USID_PROTO))) < 0) {
       perror("error"); 
        fprintf(stderr, "error creating PF Packet socket\n");
        return 0;
    }
    
    FD_ZERO(&set);
    FD_SET(proc_sockfd, &set);
    FD_SET(odr_sockfd, &set);
    
    while(1) {
        currset = set;
        if (select(max(proc_sockfd, odr_sockfd)+1, 
                            &currset, NULL, NULL, NULL) < 0) {
            if(errno == EINTR) {
                continue;
            }
            perror("Select error");
        }
        
        /* receiving from peer process */
        if (FD_ISSET(proc_sockfd, &currset)) {
            memset(buff, 0, MAXLINE); 
            memset(&proc_addr, 0, sizeof(proc_addr));
           
            DEBUG(printf ("\n====================================PROC_MESSAGE_RECEIVED====================================\n"));
            /* block on recvfrom. collect info in 
             * proc_addr and data in buff */
            if (recvfrom(proc_sockfd, buff, MAXLINE, 
                                0, (struct sockaddr *) &proc_addr, &socklen) < 0) {
                perror("Error in recvfrom");
                return 0;
            }
            
            /* populate the send params from char sequence received from process */
            send_params_t* sparams = get_send_params (buff);
            
            /* process this msg received from peer proc */
            if (handle_peer_msg(odr_sockfd, proc_sockfd, &proc_addr, sparams) < 0)
                return 0;
            
        }
        
        /* receiving on ethernet interface */
        if (FD_ISSET(odr_sockfd, &currset)) {
            memset(recv_buf, 0, ETH_FRAME_LEN); 
            memset(&odr_addr, 0, sizeof(odr_addr));
            
            DEBUG(printf ("\n====================================ETHERNET_MESSAGE_RECEIVED====================================\n"));
            if ((len = recvfrom(odr_sockfd, recv_buf, ETH_FRAME_LEN, 0, 
                                    (struct sockaddr *)&odr_addr, &odrsize)) < 0) {
                perror("\nError in recvfrom");
                return 0;
            }
            
            //handle_eth_msg ((odr_frame_t *)recv_buf, &odr_addr, odr_sockfd);
            if(handle_ethernet_msg (odr_sockfd, proc_sockfd, &odr_addr, recv_buf) < 0)
                return 0;

        }
    }

    return 0;
}

