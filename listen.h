/*
 * listen.h
 */

#ifndef __LISTEN_H__
#define __LISTEN_H__

#include <netinet/in.h>

#include "module.h"

struct listen_data
{
	struct sockaddr_in addr;	/* ip:port to listen on */
	struct carousel *carousel;	/* carousel we are downloading */
};

int parse_addr(char *, struct in_addr *, in_port_t *);

void start_listener(struct listen_data *);

#endif
