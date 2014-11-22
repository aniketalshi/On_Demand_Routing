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

#define USID_PROTO 0xA11D
#define VMNAME_LEN 10
#define IP_LEN     50
#define PAYLOAD_SIZE 1440
#define HWADDR      6

/* Type of frame */
#define __RREQ       1
#define __RREP       2
#define __DATA       3
#define __RREQ_ASENT 4

/****************** map of port to sunpath table ************/

/* struct to save port to sun_path mapping */
typedef struct map_port_sunpath map_port_sp_t;

struct map_port_sunpath {
    struct timeval ts;        // last time stamp updated
    int   portno;             // port num
    char  sun_path[100];      // sun path associated
    map_port_sp_t  *next;     // link to next entry
};

map_port_sp_t *port_spath_head; /* head of port_sunpath map */

/*************************************************************/

/* struct to store sending params */
typedef struct send_params {
    char destip[MAXLINE];
    char msg[PAYLOAD_SIZE];
    int  destport;
    int  route_disc_flag;
}send_params_t;

/************** Routing table structure ***********************/
/* routing table entry struct */
typedef struct r_entry {
    char  destip[INET_ADDRSTRLEN];    // destination canonical ip
    char  next_hop[HWADDR+1];         // Next hop ethernet addr
    int   if_no;                      // interface no to send packet
    int   no_hops;                    // Num of hops to dest
    int   broadcast_id;               // Broadcast id
    struct timeval timestamp;         // last updated time stamp
    struct r_entry *next, *prev;      // Next entry in list
}r_entry_t;

/* head of odr routing table */
r_entry_t *routing_table_head;

/**************** ODR Frame struct ***************************/

typedef struct odr_frame_struct {
   int  frame_type;                  // type of the frame
   int  broadcast_id;                // broadcast id for rrep
   int  hop_count;                   // hop count to dest
   int  payload_len;                 // length of data payload
   int  route_disc_flag;             // Route discovery flag
   int  src_port;                    // source port no
   int  dst_port;                    // Dest port no
   char dest_ip [INET_ADDRSTRLEN];   // Dest canonical ip
   char src_ip  [INET_ADDRSTRLEN];   // source canonical ip
   char payload [PAYLOAD_SIZE];      // data payload
} odr_frame_t;

/*************** Queue of pending msgs ***********************/


/* Note that everything stored in pending queue will be in
 * host byte order to be able to do lookup */
/* list of all pending messages */
typedef struct pending_msgs {
    char destip [INET_ADDRSTRLEN];
    odr_frame_t *odrframe;
    struct pending_msgs *next, *prev;
}pending_msgs_t;

/* head of pending message queue */
pending_msgs_t *pending_queue_head;
/*************************************************************/

/* staleness parameter */
extern int stale_param;

int insert_map_port_sp (int , char *);        /* insert entry in map */
send_params_t * get_send_params(char *);      /* populate send params entries */
map_port_sp_t *fetch_entry_path (char *);     /* fetch port to sunpath mapping entry using path */
map_port_sp_t *fetch_entry_port (int);        /* fetch port to sunpath mapping entry using port*/

int send_raw_frame (int, char *, char *, int, odr_frame_t*);  /* Send raw ethernet frame */




int get_r_entry (char *, r_entry_t **, int);     /* get the entry in routing table */
int insert_r_entry (odr_frame_t *, r_entry_t **, 
                              int, unsigned char*);       /* insert entry in routing table */

int check_r_entry (odr_frame_t *, r_entry_t *, int , unsigned char *);



/* broadcast the rreq packets */
int send_req_broadcast (int, int, int, int, int, int, int, char *, char *, char *);     

odr_frame_t * lookup_pending_queue (char *);/* lookup the frame in pending_queue */

int insert_pending_queue (odr_frame_t *, char *);        /* insert frame in pending queue */

int process_recvd_frame (odr_frame_t **,void *); /* process recvd frame */

/* construct and ODR frame */
odr_frame_t * construct_odr (int , int , int , int , int , int ,  
                              int , char *, char *, char *);

int send_rrep_packet (int, odr_frame_t *, r_entry_t *, int, int);    /* send the reply packet */

/* send the application payload message */
int send_data_message (int , int , send_params_t *, r_entry_t *); 

/* send msg to peer process at requested port no */
int send_to_peer_process (int, char *, char *, int, char *);

/* process the received payload message */
int process_data_message (int , int , odr_frame_t* );

/* routine to print routing table */
void print_routing_table();
#endif /*__ODR_H*/
