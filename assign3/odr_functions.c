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
int
insert_r_entry (r_table_t *r_tab, r_entry_t *r_ent, char *dest_ip, 
                char *n_hop, int intf_n, int hops, int b_id) {
    time_t      ticks;
    int         i;
    r_entry_t   *temp_r_entry;

    r_ent = calloc(0, sizeof(r_entry_t));
    ticks = time(NULL);

    r_ent->destip   = dest_ip;
    r_ent->next_hop = n_hop;
    r_ent->intf_no  = intf_n;
    r_ent->no_hops  = hops;
    r_ent->bid      = b_id;
    r_ent->ts       = *ctime (&ticks);
   
    /* check if the entry already exists in the routing table */
    if ((i = get_r_entry (r_tab, dest_ip, temp_r_entry)) > 0){
        
        /* if an old entry exists in the table */
        temp_r_entry = r_ent;
        
        memcpy (r_tab->r_ent[i], r_ent, sizeof(r_entry_t));
        free (r_ent);
        
        return 1;
    }

    /* find the first possible to slot to insert r_entry */
    for (i = 0; i < 10; i++) {
        if (!r_tab->r_ent[i]) {
            r_tab->r_ent[i] = r_ent;
            return 1;
        }
        else if (strcmp (r_tab->r_ent[i]->destip, dest_ip)) {
            /* Replace the entry if an old entry exists. */
            
            free(r_tab->r_ent[i]);
            r_tab->r_ent[i] = r_ent;
        }
    }

    /* if table full */
    printf ("ERROR: Cannot insert in routing table; table full.\n");

    free (r_ent);
}

/* search ip address in routing table */
int
get_r_entry (r_table_t *r_tab, char *dest_ip, r_entry_t *r_entry) {

    time_t  ticks;
    int     i = 0, cur_ts = 0;
    
    ticks   = time(NULL);
    cur_ts  = *ctime(&ticks);
    
    for (i = 0; i < 10; i++) {    
        if (r_tab->r_ent[i] && 
                strcmp (r_tab->r_ent[i]->destip, dest_ip)) {
            
            if ((r_tab->r_ent[i]->ts + stale_param) < cur_ts) {
                r_entry = r_tab->r_ent[i];
                return i;
            }
        }
    }
    
    /* if routing table is empty or entry not found */
    return -1;
}
