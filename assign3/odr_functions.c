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


int 
send_raw_frame (int sockfd, char *src_macaddr, char *dest_macaddr, int int_index) {

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

    int j, send_result = 0;
    
    /*our MAC address*/
    unsigned char src_mac[6] = {0x00, 0x0c, 0x29, 0xd9, 0x08, 0xf6};

    /*other host MAC address*/
    unsigned char dest_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    /*prepare sockaddr_ll*/

    /*RAW communication*/
    socket_address.sll_family   = PF_PACKET;   
    socket_address.sll_protocol = htons(ETH_P_IP); 

    /*index of the network device */
    socket_address.sll_ifindex  = 3;

    /*ARP hardware identifier is ethernet*/
    socket_address.sll_hatype   = ARPHRD_ETHER;

    /*target is another host*/
    socket_address.sll_pkttype  = PACKET_OTHERHOST;

    /*address length*/
    socket_address.sll_halen    = ETH_ALEN;     
    /*MAC - begin*/
    socket_address.sll_addr[0]  = 0xff;     
    socket_address.sll_addr[1]  = 0xff;     
    socket_address.sll_addr[2]  = 0xff;
    socket_address.sll_addr[3]  = 0xff;
    socket_address.sll_addr[4]  = 0xff;
    socket_address.sll_addr[5]  = 0xff;
    /*MAC - end*/
    socket_address.sll_addr[6]  = 0x00;/*not used*/
    socket_address.sll_addr[7]  = 0x00;/*not used*/


    /*set the ethernet frame header
     * |Dest Mac | Src Mac | type id |
     * */
    memcpy((void*)buffer, (void*)dest_mac, ETH_ALEN);
    memcpy((void*)(buffer+ETH_ALEN), (void*)src_mac, ETH_ALEN);
    eh->h_proto = htons(USID_PROTO);
    
    /*fill the frame with some data*/
    for (j = 0; j < 1500; j++) {
        data[j] = (unsigned char)((int) (255.0*rand()/(RAND_MAX+1.0)));
    }

    /*send the packet*/
    send_result = sendto(sockfd, buffer, ETH_FRAME_LEN, 0, 
            (struct sockaddr*)&socket_address, sizeof(socket_address));
    
    if (send_result == -1) {
        perror("\nError in Sending frame");     
    }
    
    DEBUG(printf("\n Eth frame sent"));
    return send_result;
}
