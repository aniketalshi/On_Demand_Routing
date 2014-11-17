#ifndef __ODR_H
#define __ODR_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <ctype.h>
#include <unp.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <linux/if_packet.h>

/* constants */
#define __UNIX_PROC_PATH "procpathfile"
#define __UNIX_SERV_PATH "servpathfile"
#define __SERV_PORT 80
#define USID_PROTO 0xA17D

/* struct to save port to sun_path mapping */
typedef struct map_port_sunpath map_port_sp_t;

struct map_port_sunpath {
    int  portno;                // port num
    char sun_path[100];         // sun path associated
    struct timeval ts;          // last time stamp updated
    map_port_sp_t *next;        // link to next entry
};

/* head of port_sunpath map */
map_port_sp_t *port_spath_head;

/* insert entry in map */
int insert_map_port_sp (int portno, char *path);

#endif /*__ODR_H*/
