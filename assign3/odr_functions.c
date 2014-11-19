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

    map_port_sp_t *entry, *curr;

    entry = (map_port_sp_t *) calloc(1, sizeof(map_port_sp_t));
    entry->portno        = portno;
    strncpy(entry->sun_path, path, strlen(path));
    gettimeofday(&entry->ts, NULL);

    /* if no entry in table */
    if (!port_spath_head) {
        port_spath_head = entry;
        return 1;
    }

    /* traverse till end of list */
    for(curr = port_spath_head; curr->next != NULL; curr = curr->next); 

    curr->next = entry;
    return 1;
}

/* send the application payload message */
int
send_data_message (int sockfd, int src_port, 
                    send_params_t *sparams, r_entry_t *entry) {

    assert(sparams);
    assert(entry);
    
    char *src_ip, *src_mac;
    odr_frame_t *odrframe;
    
    
    if ((src_ip = get_self_ip()) == NULL) {
        return -1;
    }
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
send_rrep_packet (int sockfd, odr_frame_t *frame) {

    assert(frame);
    r_entry_t *entry;
    char src_mac[HW_ADDR];

    /* check if routing entry exists */
    if (get_r_entry(frame->src_ip, &entry, 0) > 0) {
        assert(entry);
        
        memset (src_mac, 0, HW_ADDR);
        /* send the reply on int in routing table */  
        memcpy(src_mac, get_hwaddr_from_int(entry->if_no), HW_ADDR);
        
        DEBUG(printf("\nHW ADDR for sending: %s",
                    convert_to_mac(get_hwaddr_from_int(entry->if_no))));

        printf ("\nRREP Sent. Interface index : %d\n", entry->if_no);
        
        /* send odr frame */
        if(send_raw_frame (sockfd, src_mac, entry->next_hop, entry->if_no, frame) < 0)
            return -1;
    }
    return 0;
}


/* broadcast the rreq packet received from this proc*/
int 
send_req_broadcast (int sockfd, int recvd_int_index, int broad_id, int hopcount, 
                            int rdisc_flag, char *dstip, char *srcip) {
   
    assert(dstip);
    assert(srcip);

    struct hwa_info *curr = Get_hw_struct_head(); 
    char if_name[MAXLINE];
    odr_frame_t *odrframe;
    
    /* dest mac with all ones */
    unsigned char dest_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    /* loop through all interfaces */
    for (; curr != NULL; curr = curr->hwa_next) {
       
       memset(if_name, 0, MAXLINE); 
       /* if there are aliases in if name with colons, split them */
       sscanf(curr->if_name, "%[^:]", if_name); 
        
       /*send packet on all except loopback
        * eth0 and interface on which packet is received */
       if ((strcmp(if_name, "lo") != 0) && (strcmp(if_name, "eth0") != 0) 
                                        && curr->if_index != recvd_int_index) {
            
            printf ("\nRREQ Sent. Interface: %s, Broadcast id %d\n", if_name, broad_id);
            
            /* construct the odr frame */
            odrframe = construct_odr (__RREP, broad_id, hopcount, 0, 
                                            rdisc_flag, 0, 0, dstip, srcip, NULL);
            
            if (odrframe == NULL) {
                fprintf(stderr, "\nError creating odr frame");
                return -1;
            }
            
            if(send_raw_frame (sockfd, curr->if_haddr, dest_mac, curr->if_index, odrframe) < 0)
                return -1;
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
    
    /* send the packet on wire */
    if ((send_result = sendto(sockfd, buffer, ETH_FRAME_LEN, 0, 
            (struct sockaddr*)&socket_address, sizeof(socket_address))) < 0) {
        
        perror("\nError in Sending frame\n");     
    }
    
    DEBUG(printf("\nEth frame sent\n"));
    
    return send_result;
}


/* fill the routing table entry*/
r_entry_t *
insert_r_entry (char *dest_ip, char *n_hop, 
                           int intf_n, int hops, int b_id) { 
    assert(dest_ip);
    assert(n_hop);
    
    r_entry_t *temp_r_entry;

    /* check if the entry already exists in the routing table */
    if (get_r_entry (dest_ip, &temp_r_entry, 0) >= 0) {
        
        /* check if entry is not null */
        assert(temp_r_entry);
        
        /* update the timestamp of this entry */
        gettimeofday(&(temp_r_entry->timestamp), 0);
        
        return temp_r_entry;
    }
   
    /* entry doesnot exist, so insert new node */
    r_entry_t *r_ent = (r_entry_t *)calloc(1, sizeof(r_entry_t));

    strcpy(r_ent->destip, dest_ip);
    strcpy(r_ent->next_hop, n_hop);
    gettimeofday(&(r_ent->timestamp), 0);
    r_ent->if_no         = intf_n;
    r_ent->no_hops       = hops;
    r_ent->broadcast_id  = b_id;
    r_ent->next          = NULL;
   
    /* insert it as top of routing table
     * if table is empty this will be first entry in table*/
    r_ent->next = routing_table_head;
    routing_table_head = r_ent;
    
    return r_ent;
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
            
            /* check if entry is stale */
            if (((currtime.tv_sec - curr->timestamp.tv_sec) < stale_param) &&
                        (!route_disc_flag)) {
                *r_entry = curr;
                return 1;
            
            } else {
                /* delete this entry */
                if(curr->prev) {
                    curr->prev->next = curr->next;
                }
                if(curr->next) {
                    curr->next->prev = curr->prev;
                }
                free(curr);
                break;
            }
        }
    }
    
    /* if routing table is empty or entry not found */
    return 0;
}

/* lookup the frame in pending message queue based on broadcast id */
odr_frame_t *
lookup_pending_queue (int broadcast_id) {
    if (!pending_queue_head)
       return NULL;
    
    pending_msgs_t *curr = pending_queue_head;
    
    for(; curr != NULL; curr = curr->next) {
        assert(curr->odrframe);
        
        /* if we have found our node */
        if (curr->odrframe->broadcast_id == broadcast_id) {
            
            /* if this is the head of queue */
            if (curr == pending_queue_head) {
                pending_queue_head = pending_queue_head->next;
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
insert_pending_queue (odr_frame_t *odrframe) {
    assert(odrframe);
    
    /* if entry is already present in queue */
    if (lookup_pending_queue (odrframe->broadcast_id) != NULL)
        return 0;
   
    pending_msgs_t *entry = calloc(1, sizeof(pending_msgs_t));
    
    /* insert at the front of the queue */
    entry->odrframe = odrframe;
    
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

    DEBUG(printf("\n Type of frame : %d", recvd_frame->frame_type));
    
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

    DEBUG(printf("Message Processed\n"));
    return 0;
}


/* construct odr frame */
odr_frame_t *
construct_odr (int type, int bid, int hopcount, int len, int rdisc_flag, int srcport, 
                                int dstport, char *dstip, char *srcip, char *payload) {
    assert(dstip);
    assert(srcip);
    
    /* if this is data frame and payload is null */
    if(type == __DATA && payload == NULL) {
        fprintf (stderr, "Invalid Payload"); 
        return NULL;
    }
    
    odr_frame_t *odr_frame     = (odr_frame_t *)calloc(1, sizeof(odr_frame_t));
    odr_frame->frame_type      = htonl(type);
    odr_frame->hop_count       = htonl(hopcount);
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

