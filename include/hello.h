
#ifndef HELLO_H
#define HELLO_H

#include "olsr.h"
#include "packet.h"

//This function is to send hello message
void send_hello(int sockfd, const char *src_ip);

//This function is to handle the hello message(upon receiving a hello message)
void handle_hello(struct olsr_header *hdr, const char *src_ip);

#endif
