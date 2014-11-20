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
#include <errno.h>		/* error numbers */
#include <sys/ioctl.h>          /* ioctls */

      
#define UNIX_PROC_PATH "procpathfile"
#define	IF_NAME		16	/* same as IFNAMSIZ    in <net/if.h> */
#define	HW_ADDR	         6	/* same as IFHWADDRLEN in <net/if.h> */
#define	IF_HADDR	 6	/* same as IFHWADDRLEN in <net/if.h> */

#define	IP_ALIAS  	 1	/* hwa_addr is an alias */

#define __DEBUG 1

#ifdef __DEBUG
#  define DEBUG(x) x
#else
#  define DEBUG(x) 
#endif

/************* store info about hw address, int index, ip **************/
struct hwa_info {
  char    if_name[IF_NAME];	/* interface name, null terminated */
  char    if_haddr[IF_HADDR];	/* hardware address */
  int     if_index;		/* interface index */
  short   ip_alias;		/* 1 if hwa_addr is an alias IP address */
  struct  sockaddr  *ip_addr;	/* IP address */
  struct  hwa_info  *hwa_next;	/* next of these structures */
};

/* head of struct hwa_info */
struct hwa_info *hwa_struct_head;

/* self canonical ip */
extern char * self_ip_addr;
/*********************************************************************/


/* functions for interface indexes */
struct hwa_info	*get_hw_addrs();
struct hwa_info	*Get_hw_addrs();
struct hwa_info * Get_hw_struct_head();
void   free_hwa_info(struct hwa_info *);


int get_canonical_ip (char *, char *);
int msg_send (int, char*, int, char*, int );
char *convert_to_mac(char *);
int get_broadcast_id();
char *get_hwaddr_from_int(int);
char * get_self_ip ();

#endif /* __UTILS_H */
