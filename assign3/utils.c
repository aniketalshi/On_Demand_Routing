#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "utils.h"

/* function returns canonical ip */
int 
get_canonical_ip (char *serv_vm, char *canon_ip) {
    
    assert(serv_vm);
    assert(canon_ip);
    
    struct hostent *he;
    struct inaddr **addr_list;
    
    if ((he = gethostbyname(serv_vm)) == NULL) {
        return -1;
    }
    
    addr_list = (struct in_addr **)he->h_addr_list;
    assert(addr_list);
    assert(addr_list[0]);
    
    strcpy(canon_ip, inet_ntoa(*(struct in_addr *)addr_list[0]));
    //DEBUG(printf("\n VM name : %s canonical IP: %s",
    //                    he->h_name, inet_ntoa(*(struct in_addr *)addr_list[0])));
    return 0;
}


/* API for sending message */
int 
msg_send (int sockfd, char* destip, int destport, 
                      char *msg, int route_disc_flag) {
    assert(destip);
    assert(msg);
    
    struct sockaddr_un odr_proc_addr;
    char str_seq[MAXLINE];
    memset (&odr_proc_addr, 0, sizeof(struct sockaddr_un));
    
    /* ODR process to send message to */
    odr_proc_addr.sun_family = AF_LOCAL;
    strcpy(odr_proc_addr.sun_path, UNIX_PROC_PATH);
    
    /* construct the char sequence to write on UNIX Domain socket */
    sprintf(str_seq, "%s,%d,%s,%d\n", destip, destport, msg, route_disc_flag);
    
    if (sendto (sockfd, str_seq, strlen(str_seq), 0, 
                    (struct sockaddr *) &odr_proc_addr, sizeof(struct sockaddr_un)) <= 0) {
        perror("\n Error in sendto");
        return -1;
    }
    
    return 0;
}
