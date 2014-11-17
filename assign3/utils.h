#ifndef __UTILS_H
#define __UTILS_H
        
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <ctype.h>
#include <unp.h>
#include <netinet/in.h>
#include <arpa/inet.h>        
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <linux/if_packet.h>
      
#define UNIX_PROC_PATH "procpathfile"
#define __DEBUG 1

#ifdef __DEBUG
#  define DEBUG(x) x
#else
#  define DEBUG(x) 
#endif

/* Function to return canonical IP of given serv name */
int get_canonical_ip (char *serv_vm, char *canon_ip);

/* MSG_SEND API */
int msg_send (int sockfd, char* destip, 
              int destport, char *msg, int route_disc_flag);
    
#endif /* __UTILS_H */
