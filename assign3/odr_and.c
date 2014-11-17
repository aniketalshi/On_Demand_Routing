#include "odr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define VMNAME_LEN 10
#define IP_LEN     50
#define __SERV_PORT 5500

/* Populate entries from string to send params */
send_params_t*
get_send_params (char *str) {
    assert(str);
    char dstip[IP_LEN], dstport[IP_LEN], msg[IP_LEN], flag[IP_LEN];
    
    send_params_t *sp = (send_params_t *)malloc(sizeof(send_params_t));
    sscanf(str, "%[^','],%[^','],%[^','],%s", dstip, dstport, msg, flag); 
    
    sp->destip          = dstip;
    sp->msg             = msg;
    sp->destport        = atoi(dstport);
    sp->route_disc_flag = atoi(flag);
    return sp;
}


/* insert entry in port to sunpath table */
int
insert_map_port_sp (int portno, char *path) {
    assert(path);
    
    map_port_sp_t *entry, *curr;
    
    entry = (map_port_sp_t *) calloc(1, sizeof(map_port_sp_t));
    entry->portno        = portno;
    strncpy(entry->sun_path, path, sizeof(path));
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

/* given the port no, get file name from port to sunpath table */
int
get_file_name (int portno, char *path) {
    
    map_port_sp_t *curr;
    
    if (!port_spath_head) {
        printf ("ERROR: Table empty!\n");
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
    printf ("ERROR: Port no does not exist in the table.\n");
    return -1;
}

int main (int argc, const char *argv[]) {
    int stale_param, proc_sockfd, odr_sockfd, resp_sockfd, socklen;
    fd_set set, currset;
    struct sockaddr_un serv_addr, proc_addr, resp_addr;
    char buff[MAXLINE];
    
    /* initializations */
    socklen = sizeof(struct sockaddr_un);
    stale_param = 0;
    
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
    printf("\nUnix Domain socket %d, sun_ath %s\n", proc_sockfd, __UNIX_PROC_PATH); 
    
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
            
            send_params_t* sparams = get_send_params (buff);
            
            printf("\n Received data from proc sunpath %s", proc_addr.sun_path);
            //printf("\n%d\n%d\n%s\n%s\n", sparams->route_disc_flag, sparams->destport, sparams->msg, sparams->destip);
        }
        
        /* receiving on ethernet interface */
        if (FD_ISSET(odr_sockfd, &currset)) {
            
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
