#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <ctype.h>
#include <unp.h>
#include "utils.h"
#include "odr.h"

#define VMNAME_LEN 10
#define IP_LEN     50

int main (int argc, const char* argv[]) {
    
    int     fp, serv_sockfd, cli_port;
    struct  sockaddr_un servaddr;
    char    serv_vm[VMNAME_LEN], msg[MAXLINE], cli_ip[IP_LEN];
    time_t  ticks;
    
    msg_params_t mparams;
    memset(&mparams, 0, sizeof(msg_params_t));
    unlink (__UNIX_SERV_PATH);
    
    /* create a UNIX Domain socket */
    if ((serv_sockfd = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "\n error creating unix domain socket\n");
        return 0;
    }
    
    /* populate serv addr struct */
    memset (&servaddr, 0, sizeof(servaddr));
    servaddr.sun_family = AF_LOCAL;
    strcpy (servaddr.sun_path, __UNIX_SERV_PATH);

    /* Bind the UNIX Domain socket */
    Bind (serv_sockfd, (SA *)&servaddr, SUN_LEN(&servaddr));
    printf("\n Unix Domain socket %d\n", serv_sockfd); 
     
    while (1) {
        
        msg_recv (serv_sockfd, msg, cli_ip, &cli_port, &mparams);
        DEBUG(printf("\nMessage received %s\n", mparams.msg));
        ticks = time(NULL);
        snprintf(msg, sizeof(msg), "%.24s\r\n", ctime(&ticks));
        
        /* send the current timestamp to client through ODR API */
        msg_send (serv_sockfd, mparams.cli_ip, cli_port, msg, 0, __UNIX_SERV_PATH); 
    }

    unlink(__UNIX_SERV_PATH);
    return 0;
}

