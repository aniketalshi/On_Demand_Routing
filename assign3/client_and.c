#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <ctype.h>
#include <unp.h>
#include "utils.h"
#include <setjmp.h>

#define __SERV_PORT 5500
#define VMNAME_LEN 10
#define IP_LEN     50
#define PAYLOAD_SIZE 1440

static sigjmp_buf jmp;

/* Handle the SIGALRM. */
void
signal_handler (int signal) {
        siglongjmp(jmp, 1);
}

int main (int argc, const char* argv[]) {
    
    int fp, cli_sockfd, cli_port;
    struct sockaddr_un cliaddr;
    char serv_vm[VMNAME_LEN], canon_ip[IP_LEN], cli_ip[IP_LEN], msg[MAXLINE];
    
    msg_params_t mparams;
    memset(&mparams, 0, sizeof(msg_params_t));
    
    /* temporary sun_path name for binding socket */
    char tempfile[] = "/tmp/file_and_XXXXXX";
    if ((fp = mkstemp(tempfile)) < 0) {
        perror("mkstemp error");
        return 0;
    }
    unlink (tempfile);

    signal(SIGALRM, signal_handler);

    /* create a UNIX Domain socket */
    if ((cli_sockfd = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "\n error creating unix domain socket\n");
        return 0;
    }
    
    /* populate cli addr struct */
    memset (&cliaddr, 0, sizeof(cliaddr));
    cliaddr.sun_family = AF_LOCAL;
    strcpy (cliaddr.sun_path, tempfile);

    /* Bind the UNIX Domain socket */
    Bind (cli_sockfd, (SA *)&cliaddr, SUN_LEN(&cliaddr));
    printf("\nUnix Domain socket %d, sun_path %s\n", cli_sockfd, tempfile); 
    
    while (1) {
        /* prompt user for server it want to connect to */
        printf("\nType \"exit\" to break\nEnter the Server VM to connect: ");
        scanf("%s", serv_vm);
        
        /* if user wants to exit */
        if(strcmp(serv_vm, "exit") == 0) {
            break;
        }
        
        /* Retreive destination canonical IP */
        if ((get_canonical_ip (serv_vm, canon_ip)) < 0) {
            fprintf(stderr, "\n server vm not found. Try again");
            continue;
        }
       
        alarm (5);

        DEBUG(printf("\n Request sent to node %s with IP: %s", serv_vm, canon_ip));
        msg_send(cli_sockfd, canon_ip, __SERV_PORT, "hello world", 0, tempfile);
        
        /* Handle the timer signal, and retransmit. */
        if (sigsetjmp(jmp, 1) != 0) {

            printf ("Sending the packet again.\n");

            /* Send the packet again with route_disc flag set. */                                                                                             
            msg_send(cli_sockfd, canon_ip, __SERV_PORT, "hello world", 1, tempfile);
        }
        
        memset (&msg, 0, MAXLINE);
        /* sit on message recv */
        msg_recv (cli_sockfd, msg, cli_ip, &cli_port, &mparams);
        alarm (0);
        printf ("\nServer at %s replied with time : %s\n", serv_vm, mparams.msg);
        break;
    }

    unlink(tempfile);
    return 0;
}
