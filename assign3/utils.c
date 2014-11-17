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
    DEBUG(printf("\n VM name : %s canonical IP: %s",
                        he->h_name, inet_ntoa(*(struct in_addr *)addr_list[0])));
    return 0;
}


/* API for receiving message */
int
msg_recv (int sockfd, char *msg, char *destip, int destport) {
    fd_set        currset; 
    char buff[MAXLINE];
    struct sockaddr_un proc_addr;
    int socklen;

    socklen = sizeof (struct sockaddr_un);
    FD_ZERO(&currset);
    FD_SET (sockfd, &currset);    

    if (select (sockfd+1, 
                &currset, NULL, NULL, NULL) < 0) {
        if(errno == EINTR) {
            return -1;
        }
        perror("Select error");
    }
        
    /* receiving from odr process */
    if (FD_ISSET(sockfd, &currset)) {
        memset(buff, 0, MAXLINE); 
        memset(&proc_addr, 0, sizeof(proc_addr));

        /* block on recvfrom. collect info in 
         * proc_addr and data in buff */
        if (recvfrom(sockfd, buff, MAXLINE, 
                    0, (struct sockaddr *) &proc_addr, &socklen) < 0) {
            perror("Error in recvfrom");
            return 0;
        }
        printf("\n Received msg from ODR: %s", buff);
    } 
    
}

/* API for sending message */
int 
msg_send (int sockfd, char* destip, int destport, 
                      char *msg, int route_disc_flag) {
    assert(destip);
    assert(msg);
    
    struct sockaddr_un odr_proc_addr;
    memset (&odr_proc_addr, 0, sizeof(struct sockaddr_un));
    
    /* ODR process to send message to */
    odr_proc_addr.sun_family = AF_LOCAL;
    strcpy(odr_proc_addr.sun_path, UNIX_PROC_PATH);

    if (sendto (sockfd, msg, strlen(msg), 0, 
                    (struct sockaddr *) &odr_proc_addr, sizeof(struct sockaddr_un)) <= 0) {
        perror("\n Error in sendto");
        return -1;
    }
    
    return 0;
}
