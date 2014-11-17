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
    
    int fp, cli_sockfd;
    struct sockaddr_un cliaddr;
    char serv_vm[VMNAME_LEN], canon_ip[IP_LEN];
    
    /* temporary sun_path name for binding socket */
    char tempfile[] = "/tmp/file_and_XXXXXX";
    if ((fp = mkstemp(tempfile)) < 0) {
        perror("mkstemp error");
        return 0;
    }
    unlink (tempfile);
    
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
    printf("\n Unix Domain socket %d, sun_ath %s\n", cli_sockfd, tempfile); 
    
    while (1) {
        /* prompt user for server it want to connect to */
        printf("\n Enter the Server VM: ");
        scanf("%s", serv_vm);
        
        /* Retreive destination canonical IP */
        if ((get_canonical_ip (serv_vm, canon_ip)) < 0) {
            fprintf(stderr, "\n server vm not found. Try again");
            continue;
        }
        
        msg_send(cli_sockfd, canon_ip, __SERV_PORT, "hello world", 0);
        
        //TODO:Set timer and block on receive.
    }

    unlink(tempfile);
    return 0;
}
