#include <stdint.h>             // necessary C libraries
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>          // socket struct headers
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>          // internet headers
#include <netinet/in.h>
#include <unistd.h>

#define BUF_SIZE 513

void print_machine_ip();


int main() {
    int servfd, udpfd;
    struct sockaddr_in servaddr, udpaddr;       // serv is for client connection.  udp is for DNS connection to DNS name server
    char buf[BUF_SIZE];        // max UDP buffer length (512 +1 for null-termination)

    // UDP socket set up
    if((servfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket error UDP");
        exit(-1);
    }
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(53);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    // bind UDP server to socket
    socklen_t serv_len = sizeof(servaddr);
    if(bind(servfd, (struct sockaddr*) &servaddr, serv_len) == -1) {
        perror("bind error");
        exit(-1);
    }

    print_machine_ip();
    printf("UDP bound.  Waiting for DNS queries...\n");


    // wait for UDP client message
    while(1) {

        // read off client UDP
        short num_read;
        serv_len = sizeof servaddr;
        if( (num_read = recvfrom(servfd, buf, BUF_SIZE, 0, (struct sockaddr*) &servaddr, &serv_len)) == -1) {
            perror("recvfrom error");
            exit(-1);
        }
        buf[512] = '\0';                                    // null terminate message
        printf("UDP Client Buffer: ", buf);



        // DNS-facing UDP socket set up
        if((udpfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
            perror("socket error UDP-2");
            exit(-1);
        }
        bzero(&udpaddr, sizeof(udpaddr));
        udpaddr.sin_family = AF_INET;
        udpaddr.sin_port = htons(30000);                                        // random high-value port number
        inet_pton(AF_INET, "8.8.8.8", &udpaddr.sin_addr);                           // set to communicate with Google DNS

        // finish setting up DNS-facing UDP client
        socklen_t udp_len = sizeof udpaddr;
        if(connect(udpfd, (struct sockaddr*) &udpaddr, udp_len) == -1) {
            perror("bind error UDP-2");
            exit(-1);
        }

        // manipulate to network byte order
        num_read = htons(num_read);

        // send length to Google DNS server
        if( send(udpfd, &num_read, 2, 0) == -1) {          // I think this is right.  Might be wrong
            perror("len write error");
            exit(-1);
        }

        // send message to Google DNS server
        if( send(udpfd, buf, BUF_SIZE, 0) == -1) {
            perror("buf write error");
            exit(-1);
        }

        // wait for length back from Google DNS server
        char recv_len[2];
        udp_len = sizeof udpaddr;
        if( recvfrom(udpfd, recv_len, 2, 0, (struct sockaddr*) &udpaddr, &udp_len) == -1) {
            perror("recvfrom error (UDP-2, len)");
            exit(-1);
        }

        short length = recv_len[0] | recv_len[1] << 8;          // manipulate chars into short
        length = ntohs(length);                                 // return to host byte order

        // wait for message back from Google DNS
        udp_len = sizeof udpaddr;
        if( recvfrom(udpfd, buf, length, 0, (struct sockaddr*) &udpaddr, &udp_len) == -1) {
            perror("recvfrom error (UDP-2, buf)");
            exit(-1);
        }
        buf[512] = '\0';
        printf("Google DNS len: %d\nGoogle Message: ", length, buf);


        // pass Google DNS message back to UDP client
        serv_len = sizeof servaddr;
        if( sendto(servfd, buf, length, 0, (struct sockaddr*) &servaddr, serv_len) == -1) {
            perror("buf write error");
            exit(-1);
        }

        // close TCP socket and continue to next UDP connection
        close(udpfd);
    }   // end while(1)
    close(servfd);
    return 0;
}   // END main



// print_machine_ip()
void print_machine_ip() {
    char hostbuffer[256];
    char *IPbuffer;
    struct hostent *host_entry;
    int hostname;

    // retrieve hostname
    if( (hostname = gethostname(hostbuffer, sizeof(hostbuffer))) < 0 )
        perror("gethostbyname error");

    // retrieve host information
    if( (host_entry = gethostbyname(hostbuffer)) == NULL )
        perror("gethostbyname error");

    // convert an Internet network address into ASCII string
    if( (IPbuffer = inet_ntoa( *((struct in_addr*) host_entry->h_addr_list[0]))) == NULL )
        perror("gethostbyname error");

    printf("Host IP: ");
    puts(IPbuffer);
}   // END print_machine_ip()
