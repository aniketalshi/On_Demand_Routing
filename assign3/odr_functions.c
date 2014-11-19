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

/* broadcast the rreq packet*/
int 
send_req_broadcast (int sockfd, int recvd_int_index) {
    
    struct hwa_info *curr = Get_hw_struct_head(); 
    char if_name[MAXLINE];
    
    /* Dest mac with all ones */
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
            
            if(send_raw_frame (sockfd, curr->if_haddr, dest_mac, curr->if_index) < 0)
                return -1;
        }
    }

    return 0;
}


/* send raw ethernet frame */
int 
send_raw_frame (int sockfd, char *src_macaddr, 
                    char *dest_macaddr, int int_index) {

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
    memcpy((void*)buffer, (void*)dest_mac, ETH_ALEN);
    memcpy((void*)(buffer+ETH_ALEN), (void*)src_mac, ETH_ALEN);
    eh->h_proto = htons(USID_PROTO);
    
    /*fill the frame with some data*/
    for (j = 0; j < 1500; j++) {
        data[j] = (unsigned char)((int) (255.0*rand()/(RAND_MAX+1.0)));
    }

    /*send the packet*/
    if ((send_result = sendto(sockfd, buffer, ETH_FRAME_LEN, 0, 
            (struct sockaddr*)&socket_address, sizeof(socket_address))) < 0) {
        perror("\nError in Sending frame");     
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

/* Get the canonical IP Address of the current node. */
int
get_self_ip (char *can_ip) {

    struct hwa_info *curr = Get_hw_struct_head(); 
    char if_name[MAXLINE];
    
    /* loop through all interfaces */
    for (; curr != NULL; curr = curr->hwa_next) {
       
       memset(if_name, 0, MAXLINE); 
       /* if there are aliases in if name with colons, split them */
       sscanf(curr->if_name, "%[^:]", if_name); 
        
        if (strcmp(if_name, "eth0") != 0) { 
            strcpy(can_ip, inet_ntoa(((struct sockaddr_in *)curr->ip_addr)->sin_addr));
            return 1;
        }
    }
    return -1;
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

