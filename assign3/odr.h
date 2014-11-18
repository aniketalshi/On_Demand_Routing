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
#define __SERV_PORT 5500

#define USID_PROTO 0xA17D
#define VMNAME_LEN 10
#define IP_LEN     50

/********* map of port to sunpath table ************/

/* struct to save port to sun_path mapping */
typedef struct map_port_sunpath map_port_sp_t;

struct map_port_sunpath {
    int  portno;                // port num
    char sun_path[100];         // sun path associated
    struct timeval ts;          // last time stamp updated
    map_port_sp_t *next;        // link to next entry
};

map_port_sp_t *port_spath_head; /* head of port_sunpath map */

/*******************************************************/

/* struct to store sending params */
typedef struct send_params {
    char destip[MAXLINE];
    char msg[MAXLINE];
    int destport;
    int route_disc_flag;
}send_params_t;


/************** Routing table structure ***********************/
/* routing table entry struct */
typedef struct r_entry {
    char    *destip;
    char    *next_hop;
    int     intf_no;
    int     no_hops;
    int     ts;
    int     bid;
    void    *pending_msgs;
}r_entry_t;

/* struct to hold all routing table entries */
typedef struct r_table {
    r_entry_t *r_ent[10];
}r_table_t;

/*******************************************************/

/* staleness parameter */
extern int stale_param;

int insert_map_port_sp (int , char *);           /* insert entry in map */
send_params_t * get_send_params(char *);         /* populate send params entries */
map_port_sp_t *fetch_entry (char *);             /* fetch port to sunpath mapping entry */
int send_raw_frame (int , char *, char *, int);  /* Send raw ethernet frame */
int send_req_broadcast (int , int);              /* broadcast the rreq packets */

#endif /*__ODR_H*/
