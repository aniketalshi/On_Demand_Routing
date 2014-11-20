#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "utils.h"

/* globals */
char *self_ip_addr;

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
    return 0;
}

/* function to return new broadcast id */
int
get_broadcast_id() {

    static int broadcast_id = 10;
    return ++broadcast_id;
}

/* API for receiving message */
int
msg_recv (int sockfd, char* msg_buf, char *destip, int *destport, msg_params_t *tt) {
    
    assert(destip);
    assert(destport);
    assert(msg_buf);

    fd_set currset; 
    char buff[MAXLINE], msg[MAXLINE], ip[IP_LEN], port[10];
    struct sockaddr_un proc_addr;
    int socklen;
    char flag[2];
    
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
        memset(msg, 0, MAXLINE); 
        memset(&proc_addr, 0, sizeof(proc_addr));

        /* block on recvfrom. collect info in 
         * proc_addr and data in buff */
        if (recvfrom(sockfd, buff, MAXLINE, 
                    0, (struct sockaddr *) &proc_addr, &socklen) < 0) {
            perror("Error in recvfrom");
            return 0;
        }
        
        if (buff == NULL) {
            printf("Null message received\n");
            return -1;
        }
        
        sscanf(buff, "%[^','],%[^','],%[^','],%s", ip, port, msg, flag); 
        
        *destport = atoi(port);
        strncpy(msg_buf, msg, MAXLINE); 
        strncpy(destip, ip, MAXLINE);
        
        strcpy(tt->msg, msg);
        strcpy(tt->cli_ip, ip);
        tt->port = atoi(port);
    } 
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
/***********************************************************************
 * function get_hw_addrs() has been copied from Implementation given 
 *  in a directory on minix
 ***********************************************************************/

struct hwa_info *
get_hw_addrs()
{
	struct hwa_info	*hwa, *hwahead, **hwapnext;
	int		sockfd, len, lastlen, alias, nInterfaces, i;
	char		*ptr, *buf, lastname[IF_NAME], *cptr;
	struct ifconf	ifc;
	struct ifreq	*ifr, *item, ifrcopy;
	struct sockaddr	*sinptr;

	sockfd = Socket(AF_INET, SOCK_DGRAM, 0);

	lastlen = 0;
	len = 100 * sizeof(struct ifreq);	/* initial buffer size guess */
	for ( ; ; ) {
		buf = (char*) Malloc(len);
		ifc.ifc_len = len;
		ifc.ifc_buf = buf;
		if (ioctl(sockfd, SIOCGIFCONF, &ifc) < 0) {
			if (errno != EINVAL || lastlen != 0)
				err_sys("ioctl error");
		} else {
			if (ifc.ifc_len == lastlen)
				break;		/* success, len has not changed */
			lastlen = ifc.ifc_len;
		}
		len += 10 * sizeof(struct ifreq);	/* increment */
		free(buf);
	}

	hwahead = NULL;
	hwapnext = &hwahead;
	lastname[0] = 0;
    
	ifr = ifc.ifc_req;
	nInterfaces = ifc.ifc_len / sizeof(struct ifreq);
	for(i = 0; i < nInterfaces; i++)  {
		item = &ifr[i];
 		alias = 0; 
		hwa = (struct hwa_info *) Calloc(1, sizeof(struct hwa_info));
		memcpy(hwa->if_name, item->ifr_name, IF_NAME);		/* interface name */
		hwa->if_name[IF_NAME-1] = '\0';
				/* start to check if alias address */
		if ( (cptr = (char *) strchr(item->ifr_name, ':')) != NULL)
			*cptr = 0;		/* replace colon will null */
		if (strncmp(lastname, item->ifr_name, IF_NAME) == 0) {
			alias = IP_ALIAS;
		}
		memcpy(lastname, item->ifr_name, IF_NAME);
		ifrcopy = *item;
		*hwapnext = hwa;		/* prev points to this new one */
		hwapnext = &hwa->hwa_next;	/* pointer to next one goes here */

		hwa->ip_alias = alias;		/* alias IP address flag: 0 if no; 1 if yes */
                sinptr = &item->ifr_addr;
		hwa->ip_addr = (struct sockaddr *) Calloc(1, sizeof(struct sockaddr));
	        memcpy(hwa->ip_addr, sinptr, sizeof(struct sockaddr));	/* IP address */
		if (ioctl(sockfd, SIOCGIFHWADDR, &ifrcopy) < 0)
                          perror("SIOCGIFHWADDR");	/* get hw address */
		memcpy(hwa->if_haddr, ifrcopy.ifr_hwaddr.sa_data, IF_HADDR);
		if (ioctl(sockfd, SIOCGIFINDEX, &ifrcopy) < 0)
                          perror("SIOCGIFINDEX");	/* get interface index */
		memcpy(&hwa->if_index, &ifrcopy.ifr_ifindex, sizeof(int));
	}
	free(buf);
	return(hwahead);	/* pointer to first structure in linked list */
}

void
free_hwa_info(struct hwa_info *hwahead)
{
	struct hwa_info	*hwa, *hwanext;

	for (hwa = hwahead; hwa != NULL; hwa = hwanext) {
		free(hwa->ip_addr);
		hwanext = hwa->hwa_next;	/* can't fetch hwa_next after free() */
		free(hwa);			/* the hwa_info{} itself */
	}
}
/* end free_hwa_info */

/* get hwa struct head*/
struct hwa_info *
Get_hw_struct_head()
{
        if (hwa_struct_head == NULL) {
	    if ((hwa_struct_head = get_hw_addrs()) == NULL)
	        	err_quit("get_hw_addrs error");
        }
	
        return hwa_struct_head;
}


/* convert char sequence into mac addr */
char *
convert_to_mac (char *src_macaddr) {
    assert(src_macaddr);
    
    char *mac = (char *)malloc(HW_ADDR);
    int i;
    
    for(i = 0; i < HW_ADDR; ++i) {
        mac[i] = *src_macaddr++ & 0xff;
        DEBUG(printf("%.2x%s", mac[i], (i == HW_ADDR-1)?" ":":"));
    }
    return mac;
}


/* get hw address from ip */
char *
get_hwaddr_from_int (int interface_no) {
    
    struct hwa_info *curr = Get_hw_struct_head(); 
    for (; curr != NULL; curr = curr->hwa_next) {
        
        if (curr->if_index == interface_no) {
            return curr->if_haddr;
        }
    }

    return NULL;
}

/* Get the canonical IP Address of the current node. */
char *
get_self_ip () {
    
    if (self_ip_addr != NULL) 
        return self_ip_addr;
    
    self_ip_addr = (char *)malloc(INET_ADDRSTRLEN);

    struct hwa_info *curr = Get_hw_struct_head();
    char if_name[MAXLINE];
    
    /* loop through all interfaces */
    for (; curr != NULL; curr = curr->hwa_next) {
        memset(if_name, 0, MAXLINE);
        
        /* if there are aliases in if name with colons, split them */
        sscanf(curr->if_name, "%[^:]", if_name);
        
        if (strcmp(if_name, "eth0") == 0) {
            inet_ntop(AF_INET, &( ((struct sockaddr_in *)curr->ip_addr)->sin_addr), 
                                                      self_ip_addr, INET_ADDRSTRLEN);
            assert(self_ip_addr); 
            return self_ip_addr;
        }
     }
    
     /* if no canonical ip found */
     DEBUG(printf("\nNo self canonical ip found"));
     self_ip_addr = NULL;
     return NULL;
}




