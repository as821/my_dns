#include <stdio.h>              // necessary C libraries
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>          // socket struct headers
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>          // internet headers
#include <netinet/in.h>
#include <unistd.h>

#define MAX_BUF_SIZE 64     // 512 bits

int main() {
    // general set up
    int udpfd;
    struct sockaddr_in recvaddr, udpaddr;   //  udpaddr for local socket, recvaddr for addr of sender (recvfrom)
    unsigned char send_buf[MAX_BUF_SIZE], recv_buf[MAX_BUF_SIZE];
    bzero(send_buf, MAX_BUF_SIZE);
    bzero(recv_buf, MAX_BUF_SIZE);


    // UDP socket set up
    bzero(&udpaddr, sizeof(udpaddr));
    bzero(&recvaddr, sizeof(recvaddr));
    if((udpfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("UDP socket error");
        exit(-1);
    }
    udpaddr.sin_family = AF_INET;
    udpaddr.sin_port = htons(53);                   // port 53 for DNS
    inet_pton(AF_INET, "8.8.8.8", &udpaddr.sin_addr);  // make destination Google DNS name server

    printf("UDP socket created...\n");


    // build DNS query for given website (www.google.com)
    // Fields: header, question, answer, and addtl space

    // Header formating (bytes 0 to 11)
    // ID creation, 16 bit field (use ID 1337, 0x0539)
    send_buf[0] = 0x05;
    send_buf[1] = 0x39;
    // QR, OPCODE, AA, TC, and RD.  1 byte together (0, 0000, 0, 0, 1 respectively)
    send_buf[2] = 0x01;
    // RA, Z, RCODE.  1 byte (0, 000, 0000)
    send_buf[3] = 0x00;
    // QCOUNT.  2 bytes (0x0001)
    send_buf[5] = 0x01;   // send_buf[4] has already been zeroed
    // ANCOUNT, NSCOUNT, ARCOUNT are all 0. Previously zeroed.  2 bytes each, 6 bytes total


    // Question formatting
    /*
     * "A domain name... label consists of length octet followed by that number of octets.
     * ...terminates with zero length octet for null label..."
     */
    // QNAME for "www.google.com"
    // www
    send_buf[12] = 0x03;
    send_buf[13] = 0x77;
    send_buf[14] = 0x77;
    send_buf[15] = 0x77;
    // google
    send_buf[16] = 0x06;
    send_buf[17] = 0x67;
    send_buf[18] = 0x6F;
    send_buf[19] = 0x6F;
    send_buf[20] = 0x67;
    send_buf[21] = 0x6C;
    send_buf[22] = 0x65;
    //com
    send_buf[23] = 0x03;
    send_buf[24] = 0x63;
    send_buf[25] = 0x6F;
    send_buf[26] = 0x6D;
    // END name
    send_buf[27] = 0x00;
    // QTYPE    A type record, 0x0001
    send_buf[29] = 0x01;
    //QCLASS    Internet address, 0x0001
    send_buf[31] = 0x01;

    int query_question_length = 31 - 12;
    int qname_len = 27 - 12;


    // Answer formatting    All 0s for a query

    // Addtl space formatting   Empty, all 0s
    printf("Query formatting completed...\n");


    // send query to Google DNS server "8.8.8.8"
    size_t msg_len = 32;    // a result of message length (determined above through formatting)
    socklen_t addr_len = sizeof(udpaddr);
    if( sendto(udpfd, send_buf, msg_len , 0, (struct sockaddr*) &udpaddr, addr_len) == -1) {
        perror("sendto error\n");
        exit(-1);
    }
    printf("Query sent...\n");


    // recieve response from name server
    addr_len = sizeof(recvaddr);
    int recv_size = 0;
    if( (recv_size = recvfrom(udpfd, recv_buf, MAX_BUF_SIZE, 0, (struct sockaddr*) &recvaddr, &addr_len)) == -1) {
        perror("recvfrom error\n");
        exit(-1);
    }
    printf("Answer received...\n");

    for(int i = 0; i < recv_size; i++) {
        printf("%x ", recv_buf[i]);
    }
    printf("\n");


    // parse and print response
    uint8_t helper = 0, mask = 0;       // switch to uint8_t here since networking system calls require char buffers
    // check response is an answer (QR == 1) in header
    helper = recv_buf[2];
    if( helper >> 7 == 0) {
        perror("ERROR: received a query, not a response.\n");
        exit(-1);
    }
    else {
        printf("Received a response to query.\n");
    }
    // check AA in header == 1
    helper = recv_buf[2];
    if( (uint8_t) (helper << 5) >> 7 == 0) {
        printf("Unauthoritative response.\n");
    }
    else {
        printf("Response is authoritative.\n");
    }
    // check RA in header == 1, if == 0, exit with error (does not support recursion)
    helper = recv_buf[2];
    mask = 0x01;
    if( (helper &= mask) == 0 ) {
        perror("ERROR: Server does not support recursion\n");
        exit(-1);
    }
    else {
        printf("Recursion supported.\n");
    }
    // check for data compression (first two digits of Answer field will be 1s when compression is used)
    int answer_index = query_question_length + 13;  // header length: 12. +13 so is first index of Answer field
    uint32_t rdata = 0;
    uint16_t compression_offset = 0;    // 2 byte integer
    helper = recv_buf[answer_index];
    mask = 0xC0;
    if( (helper & mask) == 0xC0) {   // if first two bits are both 1s
        uint8_t temp = helper << 2;     // remove first two digits
        compression_offset = (temp >> 2);   // TODO sign extension issues when next (3rd) bit is 1??
        compression_offset << 8;    // shift 8 toi make room for rest of offset field
        compression_offset |= recv_buf[answer_index + 1];   // next byte is part of offset field as well


        // get TYPE
        helper = recv_buf[answer_index + 3]; // want second octet of TYPE field (OFFSET is 2 bytes)
        if(helper == 1) {
            printf("A record response.\n");
        }
        else if( helper == 5) {
            printf("CNAME response.\n");
        }
        else {
            perror("ERROR: invalid response type.\n");
            exit(-1);
        }

        // CLASS is 2 octets, TTL is 4 octets
        // get RLENGTH
        uint16_t rlength = recv_buf[answer_index + 10];
        rlength = rlength << 8;
        rlength |= recv_buf[answer_index + 11];
        printf("RLENGTH (with compression): %d\n", rlength);

        // read RDATA
        for(uint16_t i = 0; i < rlength; i++) {
            rdata |= recv_buf[answer_index + compression_offset + i];
            if( i != (rlength-1) ) {
                rdata = rdata << 8;     // if not last byte, left shift to make room for next
            }
        }
        printf("RDATA (with compression): %x\n", rdata);
    }
    else {  // no compression applied.  Account for question length to read other fields
        // check the TYPE field to know format of RDATA
        helper = recv_buf[answer_index + qname_len + 2]; // want second octet of TYPE field
        if(helper == 1) {
            printf("A record response.\n");
        }
        else if( helper == 5) {
            printf("CNAME response.\n");
        }
        else {
            perror("ERROR: invalid response type.\n");
            exit(-1);
        }


        // record RLENGTH to know the length of the RDATA section
        helper = recv_buf[answer_index + qname_len + 9];    // TTL field is 4 octets long
        unsigned int length = helper << 8;
        length |= recv_buf[answer_index + compression_offset + 10];   // get second octet of RLENGTH field
        printf("RLENGTH: %d\n", length);


        uint32_t rdata = 0;
        for(int i = 0; i < length; i++) {
            rdata |= recv_buf[answer_index+qname_len+ 11 + i];  // 11 is the offset from the end of NAME to RDATA
            if( i+1 != length) {
                rdata = rdata << 8;
            }
        }
        printf("RDATA (without compression): %d\n", rdata);
    }

    char presentation[INET_ADDRSTRLEN];   // IPv4 is 32 bits
    rdata = ntohl(rdata);
    inet_ntop(AF_INET, &rdata, presentation, sizeof(presentation));

    printf("IPv4 address is: ");
    fwrite(presentation, 1, sizeof(presentation), stdout);   // write INET_ADDRSTRLEN elements, 1 byte each
    printf("\n");
    close(udpfd);
    return 0;
}   // END main
