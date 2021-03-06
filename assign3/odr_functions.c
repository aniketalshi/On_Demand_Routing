#include "odr.h"
#include "utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* insert entry in port to sunpath table */
int
insert_map_port_sp (int portno, char *path) {
    assert(path);
    
    map_port_sp_t *entry;

    entry = (map_port_sp_t *) calloc(1, sizeof(map_port_sp_t));
    entry->portno = portno;
    strncpy(entry->sun_path, path, strlen(path));
    gettimeofday(&entry->ts, NULL);

    /* if no entry in table */
    if (!port_spath_head) {
        port_spath_head = entry;
        DEBUG(printf("\nInserted in Table port no %d : path %s\n", portno, path));
        return 1;
    }

    /* insert at start of table */
    entry->next = port_spath_head;
    port_spath_head = entry;
    
    DEBUG(printf("\nInserted in Table port no %d : path %s\n", portno, path));
    return 1;
}

/* send msg to peer process at requested port no */
int 
send_to_peer_process (int sockfd, char *sun_path, 
                        char *src_ip, int src_port, char *payload) {
    assert(sun_path);
    assert(src_ip);
    
    struct sockaddr_un proc_addr;
    
    /* construct string to send to process */
    void* str_seq = (void*)malloc(ETH_FRAME_LEN);
    
    /* TODO:add route disc flag here */
    sprintf(str_seq, "%s,%d,%s,%d\n", src_ip, src_port, payload, 0);
    
    //DEBUG(printf("\nPayload send to peer proc : %s\n", payload));
    if (src_port = __SERV_PORT) {
        printf("ODR sending sending data to client process");
    } else {
        printf("ODR sending sending data to server process\n");
    }

    memset(&proc_addr, 0, sizeof(struct sockaddr_un));
    
    /* process to send message to */
    proc_addr.sun_family = AF_LOCAL;
    strcpy(proc_addr.sun_path, sun_path);

    
    /* write the data on socket */
    if (sendto (sockfd, str_seq, strlen(str_seq), 0, 
                    (struct sockaddr *) &proc_addr, SUN_LEN(&proc_addr)) <= 0) {
        perror("\n Error in sendto");
        return -1;
    }
    
    return 0;
}


/* process the received payload message */
int 
process_data_message (int proc_sockfd, int odr_sockfd, odr_frame_t* odrframe) {
    assert(odrframe);
    
    map_port_sp_t *port_sp_entry; 
     
    /* retreive self canonical ip addr */
    char *self_ip = get_self_ip();
    if (self_ip == NULL) 
        return -1;

    /* if this data frame is for me */
    if (strcmp(odrframe->dest_ip, self_ip) == 0) {
        
        /* retereive entry from sunpath to port table */
        if ((port_sp_entry = fetch_entry_port (odrframe->dst_port)) == NULL) {
            fprintf (stderr, "no sunpath for this port %d\n", odrframe->dst_port);
            return -1;
        }

        //printf("Sun Path entry for application %s\n", port_sp_entry->sun_path);
        
        /* send msg to peer process at requested port no 
         * send source nodes ip address, port num and payload*/
        if (send_to_peer_process (proc_sockfd, port_sp_entry->sun_path, 
                                    odrframe->src_ip, odrframe->src_port, 
                                                       odrframe->payload) < 0) {
            fprintf (stderr, "Error sending data to application process");
            return -1;
        }
    }
    return 0;
}


/* send the application payload message */
int
send_data_message (int sockfd, int src_port, 
                    send_params_t *sparams, r_entry_t *entry) {

    assert(sparams);
    assert(entry);
    
    char *src_ip, *src_mac;
    odr_frame_t *odrframe;
    char src_vmname[INET_ADDRSTRLEN], dst_vmname[INET_ADDRSTRLEN];
    
    if ((src_ip = get_self_ip()) == NULL) {
        return -1;
    }
   
    /* Get name of vm we recvd packet from */
    strcpy(src_vmname, get_name_ip(src_ip));
    strcpy(dst_vmname, get_name_ip(sparams->destip));
    assert(src_vmname);
    assert(dst_vmname);
    
    if (sparams->destport == __SERV_PORT)
        printf("Client at %s sending data packet, requesting time from server at %s\n", src_vmname, dst_vmname);
    else
        printf("Server at %s sending data packet to client at %s\n", src_vmname, dst_vmname);

    /* construct ODR Frame */
    if ((odrframe = construct_odr (__DATA, entry->broadcast_id, 0, PAYLOAD_SIZE, 
                            sparams->route_disc_flag, src_port, sparams->destport, 
                                        sparams->destip, src_ip , sparams->msg)) == NULL) {
        fprintf(stderr, "error constructing odr frame\n");
        return -1;
    }
    
    /* get mac address from interface num */
    if ((src_mac = get_hwaddr_from_int (entry->if_no)) == NULL) {
        fprintf(stderr, "error retreiving mac address\n");
        return -1;
    }
    
    printf("\nSending Data packet. Src Node : %s, Destination Node %s\n",
                                                       src_vmname, dst_vmname);

    /* send raw frame on wire */
    if (send_raw_frame (sockfd, src_mac, 
                         entry->next_hop, entry->if_no, odrframe) < 0) {
        fprintf(stderr, "error sending raw frame on wire\n");
        return -1;
    }

    return 0;
}

/* send the reply packet */
int
send_rrep_packet (int sockfd, odr_frame_t *frame, r_entry_t *entry, int hop_count, int swap) { 

    assert(frame);
    assert(entry);
    char *src_mac, *self_ip, vmname[INET_ADDRSTRLEN];
    
    int srcport = frame->src_port;
    int dstport = frame->dst_port;
    int broadid = frame->broadcast_id;
    
    char srcip[INET_ADDRSTRLEN], dstip[INET_ADDRSTRLEN]; 

    strncpy(srcip, frame->src_ip, INET_ADDRSTRLEN);
    strncpy(dstip, frame->dest_ip, INET_ADDRSTRLEN);
    
    /* if we are the final destination node, swap srcip - dstip, srcport - dstport */
    if (swap) {
        srcport = frame->dst_port;
        dstport = frame->src_port;
        strncpy(srcip, frame->dest_ip, INET_ADDRSTRLEN);
        strncpy(dstip, frame->src_ip, INET_ADDRSTRLEN);
        broadid = get_broadcast_id();
    } 
    
    if ((frame = construct_odr (__RREP, broadid, hop_count, frame->payload_len, 0, srcport, 
                                  dstport, dstip, srcip, frame->payload)) == NULL) {
        fprintf(stderr, "Error creating rrep");
        return -1;
    }
    
    /* get own mac from interface num */
    src_mac = get_hwaddr_from_int (entry->if_no);

    /* get own ip addr */
    self_ip = get_self_ip();
    assert(self_ip);
    
    /* get name corresponding to this ip */
    strcpy(vmname, get_name_ip(self_ip));
    assert(vmname);
    
    /* send raw frame on wire */     
    if(send_raw_frame (sockfd, src_mac, entry->next_hop, entry->if_no, frame) < 0)
        return -1;

    printf("\nRREP Sent from node : %s. Outgoing interface %d\n",
                                                         vmname, entry->if_no);
    return 0;
}


/* broadcast the rreq packet received from this proc*/
int 
send_req_broadcast (int sockfd, int recvd_int_index, int broad_id, 
                                int src_port, int dstport, int hopcount, 
                                int rdisc_flag, char *dstip, char *srcip, 
                                char *payload, int asent_flag) {
    assert(dstip);
    assert(srcip);
    assert(payload);

    struct hwa_info *curr = Get_hw_struct_head(); 
    char if_name[MAXLINE];
    odr_frame_t *odrframe;

    char src_vmname[INET_ADDRSTRLEN], dst_vmname[INET_ADDRSTRLEN];

    strcpy(src_vmname, get_name_ip(srcip));
    strcpy(dst_vmname, get_name_ip(dstip));
    assert(src_vmname);
    assert(dst_vmname);

    int type = __RREQ;

    /* if asent flag is sent, this frame is of type asent */
    if (asent_flag == 1)
        type = __ASENT;
    
    /* dest mac with all ones */
    unsigned char dest_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    /* loop through all interfaces */
    for (; curr != NULL; curr = curr->hwa_next) {
       
       memset(if_name, 0, MAXLINE); 
       
       /* if there are aliases in if name with colons, split them */
       sscanf(curr->if_name, "%[^:]", if_name); 
        
       /*send packet on all except loopback
        * eth0 and interface on which packet is received 
        */
       if ((strcmp(if_name, "lo") != 0) && (strcmp(if_name, "eth0") != 0) 
                                        && curr->if_index != recvd_int_index) {
            
            DEBUG(printf ("\nRREQ Sent. Interface: %s, Broadcast id %d\n", if_name, broad_id));
            printf ("RREQ sent. Source node %s, Destination Node %s on"
                                "interface %s\n", src_vmname, dst_vmname, if_name);

            if (asent_flag == 1) {
                printf("ASENT Flag set. No reply expected\n");
            }
            
            /* construct the odr frame */
            odrframe = construct_odr (type, broad_id, hopcount, 0, rdisc_flag, 
                                                    src_port, dstport, dstip, srcip, payload);

            if (odrframe == NULL) {
                fprintf(stderr, "\nError creating odr frame");
                return -1;
            }
            
            if(send_raw_frame (sockfd, curr->if_haddr, 
                                dest_mac, curr->if_index, odrframe) < 0) {
                return -1;
            }
        }
    }
    return 0;
}

/* send raw ethernet frame */
int 
send_raw_frame (int sockfd, char *src_macaddr, 
                    char *dest_macaddr, int int_index, odr_frame_t *odrframe) {

    /*target address*/
    struct sockaddr_ll socket_address;

    /*buffer for ethernet frame*/
    void* buffer = (void*)malloc(ETH_FRAME_LEN);

    /*pointer to ethenet header*/
    unsigned char* etherhead = buffer;

    /*userdata in ethernet frame*/
    unsigned char* data = buffer + 14;

    /*another pointer to ethernet header*/
    struct ethhdr *eh = (struct ethhdr *)etherhead;

    int i, j, send_result = 0;
    
    /*our MAC address*/
    char *src_mac  = convert_to_mac(src_macaddr);

    /* dest Mac address */
    char *dest_mac = convert_to_mac(dest_macaddr);
    
    /*prepare sockaddr_ll*/

    /*RAW communication*/
    socket_address.sll_family   = PF_PACKET;   
    socket_address.sll_protocol = htons(ETH_P_IP); 

    /*index of the network device */
    socket_address.sll_ifindex  = int_index;

    /*ARP hardware identifier is ethernet*/
    socket_address.sll_hatype   = ARPHRD_ETHER;

    /*target is another host*/
    socket_address.sll_pkttype  = PACKET_OTHERHOST;

    /*address length*/
    socket_address.sll_halen    = ETH_ALEN;     
    
    /*MAC - begin*/
    for(i = 0; i < HW_ADDR; ++i) {
        socket_address.sll_addr[i]  = dest_mac[i];     
    }
    socket_address.sll_addr[6]  = 0x00;/*not used*/
    socket_address.sll_addr[7]  = 0x00;/*not used*/


    /* construct the ethernet frame header
     * -------------------------------
     * |Dest Mac | Src Mac | type id |
     * -------------------------------
     * */
    memcpy((void *)buffer, (void *)dest_mac, ETH_ALEN);
    memcpy((void *)(buffer+ETH_ALEN), (void *)src_mac, ETH_ALEN);
    eh->h_proto = htons(USID_PROTO);
    
    /* copy the odr frame data into ethernet frame */
    memcpy((void *)data, (void *)odrframe, sizeof(odr_frame_t));
   

    char src_vmname[INET_ADDRSTRLEN], *src_ip;
    if ((src_ip = get_self_ip()) == NULL) {
        return -1;
    }
   
    /* Get name of vm we recvd packet from */
    strcpy(src_vmname, get_name_ip(src_ip));
    assert(src_vmname);

    printf("ODR on node %s sending ETH_FRAME. SRC MAC_ADDR :", src_vmname);
    print_mac (src_macaddr);
    printf("DEST MAC_ADDR :");
    print_mac (dest_macaddr);
    printf("\n");

    /* send the packet on wire */
    if ((send_result = sendto(sockfd, buffer, ETH_FRAME_LEN, 0, 
            (struct sockaddr*)&socket_address, sizeof(socket_address))) < 0) {
        
        perror("\nError in Sending frame\n");     
    }
    
    return send_result;
}

/* Update the routing table entry */
int
update_r_entry (odr_frame_t *recv_buf, r_entry_t *r_entry, 
                    int intf_n, unsigned char *next_hop) {
    
    char src_vmname[INET_ADDRSTRLEN];

    if (!recv_buf || !r_entry || !next_hop)
        return -1;

    strcpy(r_entry->destip, recv_buf->src_ip);
    memcpy(r_entry->next_hop, next_hop, HWADDR);
    
    gettimeofday(&(r_entry->timestamp), 0);
    r_entry->if_no         = intf_n;
    r_entry->no_hops       = recv_buf->hop_count + 1;
    r_entry->broadcast_id  = recv_buf->broadcast_id;

    strcpy(src_vmname, get_name_ip(r_entry->destip));
    printf("Updating entry in routing table for ip %s\n", src_vmname);
    return 1;
}

/* file the routing table entry */
int
insert_r_entry (odr_frame_t *recvd_odr_frame, r_entry_t **r_entry,
                                int intf_n, unsigned char *next_hop) {
    assert(recvd_odr_frame);
    assert(next_hop);
    
    
    /* create new entry */
    *r_entry = (r_entry_t *)calloc(1, sizeof(r_entry_t));
    
    /* insert all the fields in the routing table. */
    if (update_r_entry (recvd_odr_frame, *r_entry, intf_n, next_hop) < 0)
        return -1;
    
    DEBUG(printf("\nInserting in routing table for dest ip %s\n", (*r_entry)->destip));
    
    /* if head is null */
    if (!routing_table_head ) {
        routing_table_head = *r_entry;
        return 1;
    }
    
    /* insert it as top of routing table */
    routing_table_head->prev = (*r_entry);
    (*r_entry)->next = routing_table_head;
    routing_table_head = *r_entry;

    return 1;
}

/* Check if the routing table entry needs to be updated, if so update it.*/
int
check_r_entry (odr_frame_t *recvd_odr_frame, 
                      r_entry_t *r_entry, int intf_n, unsigned char *next_hop) {
    
    assert (recvd_odr_frame);
    assert (r_entry);
    assert (next_hop);
   
    
    /* if this is new broadcast id */
    if (recvd_odr_frame->broadcast_id > r_entry->broadcast_id) {

        /* Update the routing table entry */
        update_r_entry (recvd_odr_frame, r_entry, intf_n, next_hop); 
        return 1;
    }

    /* if this is existing broadcast id */
    if (recvd_odr_frame->broadcast_id == r_entry->broadcast_id &&
         ((recvd_odr_frame->hop_count + 1) <= r_entry->no_hops)) {

            /* Update the routing table entry */
            update_r_entry (recvd_odr_frame, r_entry, intf_n, next_hop); 
            return 1;
     }
    /* Entry not updated. */
    return 0;
}

/* search ip address in routing table */
int
get_r_entry (char *dest_ip, r_entry_t **r_entry, int route_disc_flag) {
    
    assert(dest_ip);

    /* if routing table is empty */
    if (!routing_table_head) 
        return 0;
    
    r_entry_t *curr = routing_table_head;
    int i = 0;
    
    struct timeval currtime;
    gettimeofday(&currtime, 0);
    

    /* iterate over all entries in routing table */
    for (; curr != NULL; curr = curr->next) {
        
        /* if entry with given destination exists */
        if (strcmp (curr->destip, dest_ip) == 0) {
           
            // DEBUG (printf ("\nStale parameter: %d, currtime: %d, recvd time: %d\n", stale_param,
            //             currtime.tv_sec, curr->timestamp.tv_sec));
            /* check if entry is stale or
             * if route disc flag is set and broadcast id is greater than existing entry
             */
            if (((currtime.tv_sec - curr->timestamp.tv_sec) >= stale_param) || route_disc_flag == 1) {

                DEBUG(printf("\nDeleting entry in routing table for %s\n", curr->destip));

                /* if this is routing table head */
                if (routing_table_head == curr) {
                    routing_table_head = curr->next;
                }

                /* delete this entry */
                if(curr->prev) {
                    curr->prev->next = curr->next;
                }
                if(curr->next) {
                    curr->next->prev = curr->prev;
                }
                
                //free(curr);
                break;
            } else {
                *r_entry = curr;
                return 1;
            } 
        }
    }
    
    /* if routing table is empty or entry not found */
    return -1;
}

/* lookup in pending queue based on dest ip */
odr_frame_t *
lookup_pending_queue (char *destip) {
    assert(destip);

    if (!pending_queue_head)
       return NULL;

    //DEBUG(printf("\nLooking in pending queue for dest %s\n", destip));
    pending_msgs_t *curr = pending_queue_head;
    
    for(; curr != NULL; curr = curr->next) {
        assert(curr->odrframe);
        /* if we have found our node */
        if (strcmp(curr->destip, destip) == 0) {
            
            /* if this is the head of queue */
            if (curr == pending_queue_head) {
                pending_queue_head = pending_queue_head->next;
                curr->next = NULL;
                return curr->odrframe;
            }
            
            /* adjust the previous and next pointers */
            if(curr->prev)
                curr->prev->next = curr->next;

            if(curr->next)
                curr->next->prev = curr->prev;

            return curr->odrframe;
        }
    }
    
    /* if node is not found */
    return NULL;
}

/* insert message in pending queue */
int
insert_pending_queue (odr_frame_t *odrframe, char* ip) {
    assert(odrframe);
    assert(ip);
    
    pending_msgs_t *entry = calloc(1, sizeof(pending_msgs_t));
    
    /* insert at the front of the queue */
    entry->odrframe     = odrframe;
    strncpy(entry->destip, ip, INET_ADDRSTRLEN);
    
    /* if head is not present */
    if (!pending_queue_head) {
        pending_queue_head = entry;
        return 0;
    }
    
    /* make this entry head */
    entry->next              = pending_queue_head;
    pending_queue_head->prev = entry;
    pending_queue_head       = entry;
    
    return 0;
}

/* convert the uint32 attribs from network to host order */
int 
convert_net_host_order(odr_frame_t* recvd_frame) {
    assert(recvd_frame); 

    recvd_frame->frame_type      = ntohl(recvd_frame->frame_type);
    recvd_frame->broadcast_id    = ntohl(recvd_frame->broadcast_id);
    recvd_frame->hop_count       = ntohl(recvd_frame->hop_count);
    recvd_frame->payload_len     = ntohl(recvd_frame->payload_len);
    recvd_frame->route_disc_flag = ntohl(recvd_frame->route_disc_flag);
    recvd_frame->src_port        = ntohl(recvd_frame->src_port);
    recvd_frame->dst_port        = ntohl(recvd_frame->dst_port);

    return 0;
}


/* convert the uint32 attribs from host to network order */
int 
convert_host_net_order(odr_frame_t* recvd_frame) {
    assert(recvd_frame); 

    recvd_frame->frame_type      = htonl(recvd_frame->frame_type);
    recvd_frame->broadcast_id    = htonl(recvd_frame->broadcast_id);
    recvd_frame->hop_count       = htonl(recvd_frame->hop_count);
    recvd_frame->payload_len     = htonl(recvd_frame->payload_len);
    recvd_frame->route_disc_flag = htonl(recvd_frame->route_disc_flag);
    recvd_frame->src_port        = htonl(recvd_frame->src_port);
    recvd_frame->dst_port        = htonl(recvd_frame->dst_port);

    return 0;
}


/* populate the recv_buf and populate the recvd_odr_frame */
int
process_recvd_frame (odr_frame_t **recvd_odr_frame, void *recv_buf) {
    assert(recv_buf);
    
    /* buffer for ethernet frame */
    void* buffer = (void*)malloc(ETH_FRAME_LEN);
   
    /* copy data to buffer */
    memcpy(buffer, recv_buf, ETH_FRAME_LEN); 

    /* userdata in ethernet frame */
    unsigned char* data = buffer + 14;
    
    *recvd_odr_frame = (odr_frame_t *) data;
    assert(*recvd_odr_frame); 
    
    /* convert from network to host order */
    convert_net_host_order(*recvd_odr_frame); 

    return 0;
}


/* construct odr frame */
odr_frame_t *
construct_odr (int type, int bid, int hopcount, int len, int rdisc_flag, int srcport, 
                                int dstport, char *dstip, char *srcip, char *payload) {
    assert(dstip);
    assert(srcip);
    assert(payload);
    
    /* if this is data frame and payload is null */
    if(type == __DATA && payload == NULL) {
        fprintf (stderr, "Invalid Payload"); 
        return NULL;
    }
    
    odr_frame_t *odr_frame     = (odr_frame_t *)calloc(1, sizeof(odr_frame_t));
    odr_frame->frame_type      = htonl(type);
    odr_frame->hop_count       = htonl(hopcount);
    odr_frame->broadcast_id    = htonl(bid);
    odr_frame->route_disc_flag = htonl(rdisc_flag);
    odr_frame->src_port        = htonl(srcport);
    odr_frame->dst_port        = htonl(dstport);
    odr_frame->payload_len     = htonl(len);
    
    strcpy(odr_frame->dest_ip, dstip);
    strcpy(odr_frame->src_ip, srcip);
    
    if(type == __DATA)
        strcpy(odr_frame->payload, payload);
   
   return odr_frame; 
}

/* Print the routing table */
void 
print_routing_table() {
    if (!routing_table_head) {
        printf("\nRouting table Empty\n");
        return;
    }
    
    r_entry_t *curr;
    time_t nowtime;
    struct tm *nowtm;
    char tmbuf[64], buf[64];

    printf("\n================================ ODR Routing Table "
            "===================================================\n");
    printf("-----------------------------------------------"
            "----------------------------------------------------\n");
    printf("| %15s | %20s | %5s | %5s | %5s | %30s |\n", "destination ip", "next hop",
                                                            "ifno", "hops", 
                                                            "b_id",
                                                            "timestamp");

    printf("-----------------------------------------------"
            "----------------------------------------------------\n");
    
    /* iterate over all entries in routing table */
    for (curr = routing_table_head; curr != NULL; curr = curr->next) {
        
        /* convert time to representation format */
        nowtime = curr->timestamp.tv_sec;
        nowtm   = localtime(&nowtime);
        
        strftime(tmbuf, sizeof(tmbuf), "%Y-%m-%d %H:%M:%S", nowtm);
        snprintf(buf, sizeof(buf), "%s.%06d", tmbuf, curr->timestamp.tv_usec);

        printf("| %15s |    ", curr->destip); 
        print_mac(curr->next_hop);
        
        printf("| %5d | %5d | %5d | %30s |\n", curr->if_no, 
                                curr->no_hops, curr->broadcast_id, buf);
    }
    printf("-----------------------------------------------"
            "----------------------------------------------------\n");
    return;
}

int is_broadid_greater(char *srcip, int recvd_broad_id) {

    r_entry_t *curr = routing_table_head;
    
    if (!routing_table_head)
        return 1;

    /* iterate over all entries in routing table */
    for (; curr != NULL; curr = curr->next) {
        /* if entry matches and broadcast id is less or equal to 
         * existing broad id */
        if (strcmp(curr->destip, srcip) == 0 &&
            curr->broadcast_id >= recvd_broad_id) {
            return -1;
        }
    }
    
    return 1;
}

void 
remove_r_entry (char *ip) {

    assert(ip);
    r_entry_t *curr = routing_table_head;
    
    if (!routing_table_head)
        return;

    /* iterate over all entries in routing table */
    for (; curr != NULL; curr = curr->next) {
        
        /* remove the entry */
        if (strcmp(curr->destip, ip)) {
            DEBUG(printf("\nRemoving entry in routing table for ip %s\n", ip));
            /* if this is routing table head */
            if (routing_table_head == curr) {
                routing_table_head = curr->next;
            }
            
            /* delete this entry */
            if(curr->prev) {
                curr->prev->next = curr->next;
            }
            if(curr->next) {
                curr->next->prev = curr->prev;
            }
            return;
        }
    }
    
    return ;
}
