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
    char dstip[MAXLINE], dstport[IP_LEN], msg[MAXLINE], flag[IP_LEN];
    
    send_params_t *sp = (send_params_t *)malloc(sizeof(send_params_t));
    sscanf(str, "%[^','],%[^','],%[^','],%s", dstip, dstport, msg, flag); 
    
    strncpy(sp->destip, dstip, strlen(dstip));
    strncpy(sp->msg, msg, strlen(msg));
    sp->destport        = atoi(dstport);
    sp->route_disc_flag = atoi(flag);
    return sp;
}


/* return next portno */
int get_next_portno() {
    static int portno = 909;
    return portno++;
}

/* Fetch entry from port to sunpath table */
map_port_sp_t *
fetch_entry (char *sun_path) {
    
    assert(sun_path);
    map_port_sp_t *curr, *entry;

    if (!port_spath_head) 
        return NULL;

    for (curr = port_spath_head; curr->next != NULL; curr = curr->next) {
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
    curr->next = entry;
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

/* handle request from peer process received by ODR */
int
handle_peer_msg (int sockfd, struct sockaddr_un *proc_addr, 
                                        send_params_t *sparams) {
    assert(proc_addr);
    assert(sparams);

    int             entry_present = 0;
    map_port_sp_t   *map_entry;
    r_entry_t       *route;

    printf("\n Received data from proc sunpath %s", proc_addr->sun_path);
    DEBUG(printf("\n%d\n%d\n%s\n%s\n", sparams->route_disc_flag, 
                    sparams->destport, sparams->msg, sparams->destip));
    
    /* check if entry exist and if not insert new entry in port to sunpath table */
    if((map_entry = fetch_entry (proc_addr->sun_path)) == NULL) {
        fprintf(stderr, "unable to create entry in port sunpath table");
        return -1;
    }
   
    /* 1. Check if entry for this dest is there in routing table
     * 2. If not, insert this msg in pending_queue
     *    2.1 Flood RREQ
     * 3. If entry is present, check validity
     * 4. If all is well, send the ethernet frame on int in routing table
     */

    
    //TODO: Send canonical IP of source
 
    /* Check if the entry is already present */
    entry_present = get_r_entry (sparams->destip, &route, 
                                    sparams->route_disc_flag); 
      
    /* If the routing table is empty or if the entry is not present */
    if (entry_present <= 0) {
        //send RREQ
        //insert msg in pending queue
        return 1;
    }
    
    /* If the entry is present in the routing table, send the packet */
    if (entry_present > 0) {
        //send packet
        return 1;
    }

    /* message to send payload message */
    send_req_broadcast (sockfd, -1, get_broadcast_id(), 0, 0, "1.2.3.4", "1.2.3.4");
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
    char *self_ip, sun_path[MAXLINE];
    char *next_hop;
    r_entry_t *r_entry;
    int dest_entry_present = 0, src_entry_present = 0, intf_n = 0;
    
    /* get IP Address of the current node. */
    if ((self_ip = get_self_ip ()) == NULL) {
        return -1;
    }

    /* get the interface number of the packet. */
    intf_n = odr_addr->sll_ifindex;

    /* get the interface number of the packet. */
    next_hop = (char*) odr_addr->sll_addr;
    
    if (process_recvd_frame (&recvd_odr_frame, recv_buf) < 0) {
        perror("\nError in processing recvd frame");
        return 0;
    }
    
    if(recvd_odr_frame == NULL) {
        perror("\nNull frame recvd");
        return 0;
    }
    
    /* Check if the destip is already present in routing table. */
    dest_entry_present = get_r_entry (recvd_odr_frame->dest_ip, &r_entry, 0); 
    
    /* Check if the srcip is already present in routing table. */
    src_entry_present = get_r_entry (recvd_odr_frame->src_ip, &r_entry, 0); 
    
    DEBUG(printf("\nReceived the packet %d", recvd_odr_frame->frame_type));
    switch (recvd_odr_frame->frame_type) {
        case __RREQ: {
            printf("\n Request packet\n");
            
            /* Check if the current node is the destination node. */
            if (strcmp (recvd_odr_frame->dest_ip, self_ip) == 0){
               
                if (src_entry_present > 0) {
                    check_r_entry (recvd_odr_frame, *r_entry, intf_n, 
                                        next_hop);
                    // send RREP
                    break;
                }
                //send RREQ
                return 1;
            }

            /* Check if an entry already exists in the routing table. */
            if (dest_entry_present > 0) {
                check_r_entry (recvd_odr_frame, *r_entry, intf_n,
                                        next_hop);
                
                //send RREP
                break;
            }

            if (send_req_broadcast (odr_sockfd, odr_addr->sll_ifindex,
                        recvd_odr_frame->broadcast_id,
                        recvd_odr_frame->hop_count,
                        0, recvd_odr_frame->src_ip,
                        recvd_odr_frame->dest_ip) < 0) {
                printf ("Broadcast error!\n");
                return -1;
            }
               
            break;
        }
        
        case __RREP: {
            printf("\n Reply packet\n");
            
            /* Check if the current node is the dest node. */
            if (strcmp (recvd_odr_frame->src_ip, self_ip) == 0) {
               
                /* If the destination entry is not present. */
                if (!dest_entry_present) {
                    insert_r_entry (recvd_odr_frame, r_entry, intf_n, 
                                        next_hop);
                    // send data
                    break;
                }
                else if (dest_entry_present > 0) {
                    check_r_entry (recvd_odr_frame, r_entry, intf_n, 
                                        next_hop);
                    // send data
                    break;
                }

                //send RREQ
                return 1;
            }
            
            /* Get the next hop from the routing table. */
            if (dest_entry_present > 0) {
                //send RREP
                break;
            }

            /* If there is no entry in the routing table. */
            // broadcast RREQ
            break;
        }

        case __DATA: {
            printf("\n Data Packet");
            
            /* Check if the current node is the destination node. */
            if (strcmp (recvd_odr_frame->dest_ip, self_ip) == 0){
                //write to server/client
                
                /* Check if the routing table entry can be updated. */
                /* Write to the proper client file */
                
                if (get_file_name (recvd_odr_frame->dst_port, sun_path) < 0) {
                    printf ("Port not found!\n");
                    return -1;     
                }
    
                break;
            }

            break;
        }

        case __RREQ_ASENT: {
            printf("\n Request packet with Asent flag set");
            break;
        }

        default: {
            printf("\n Error in packet type");
            return -1;
        }
    }
    
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

    /* check if staleness parameter supplied */
    if (argc < 2) {
        fprintf(stderr, "Required args <staleness parameter>");
        return 0;
    }
    /* staleness parameter */ 
    stale_param = atoi((char *)argv[1]);
    if(stale_param == 0) {
        fprintf(stderr, "\n Invalid Staleness value");
        return 0;
    }
    
   /* create new UNIX Domain socket */
    if((proc_sockfd = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "\nerror creating unix domain socket\n");
        return 0;
    }
    
    unlink(__UNIX_PROC_PATH);
    memset(&serv_addr, 0, sizeof(serv_addr)); 
    serv_addr.sun_family = AF_LOCAL;
    strcpy(serv_addr.sun_path, __UNIX_PROC_PATH);
    
    /* Bind the UNIX Domain socket */
    Bind(proc_sockfd, (SA *)&serv_addr, SUN_LEN(&serv_addr));
    printf("\nUnix Domain socket %d, sun_path file name %s\n",
                                    proc_sockfd, __UNIX_PROC_PATH); 
    
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
            if (handle_peer_msg(odr_sockfd, &proc_addr, sparams) < 0)
                return 0;
        }
        
        /* receiving on ethernet interface */
        if (FD_ISSET(odr_sockfd, &currset)) {
            memset(recv_buf, 0, ETH_FRAME_LEN); 
            memset(&odr_addr, 0, sizeof(odr_addr));
            
            if ((len = recvfrom(odr_sockfd, recv_buf, ETH_FRAME_LEN, 0, 
                                    (struct sockaddr *)&odr_addr, &odrsize)) < 0) {
                perror("\nError in recvfrom");
                return 0;
            }
            
            //handle_eth_msg ((odr_frame_t *)recv_buf, &odr_addr, odr_sockfd);

            if(handle_ethernet_msg (odr_sockfd, proc_sockfd, &odr_addr, recv_buf) < 0)
                return 0;

            /* ODR process to send message to */
            /*
            odr_proc_addr.sun_family = AF_LOCAL;
            strcpy(odr_proc_addr.sun_path, UNIX_PROC_PATH);

            if (sendto (resp_sockfd, msg, strlen(msg), 0, 
                        (struct sockaddr *) &resp_addr, sizeof(struct sockaddr_un)) <= 0) {
                perror("\n Error in sendto");
                return -1;
            }
            */
        }
    }


    return 0;
}

