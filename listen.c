/*
 * listen.c
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "command.h"
#include "utils.h"

/* listen() backlog, 5 is max for BSD apparently */
#define BACKLOG		5

/* internal functions */
static int get_host_addr(char *, struct in_addr *);

static bool handle_connection(struct listen_data *, int, struct sockaddr_in *);
static void dead_child(int);

/*
 * extract the IP addr and port number from a string in one of these forms:
 * host:port
 * ip-addr:port
 * host
 * ip-addr
 * port
 * if the string is NULL, or the host or the port are not defined in the string,
 * the corresponding param passed to this routine is not changed
 * ip and port are both returned in network byte order
 * returns -1 on error (can't resolve host name)
 */

int
parse_addr(char *str, struct in_addr *ip, in_port_t *port)
{
	char *p;
	int ishost;

	/* easy case */
	if(str == NULL)
		return 0;

	if((p = strchr(str, ':')) != NULL)
	{
		/* its either host:port or ip:port */
		*(p++) = '\0';
		if(get_host_addr(str, ip) < 0)
			return -1;
		*port = htons(atoi(p));
		/* reconstruct the string */
		*(--p) = ':';
	}
	else
	{
		/* its either host, ip, or port */
		/* all digits => a port number */
		ishost = 0;
		for(p=str; *p && !ishost; p++)
			ishost = !isdigit(*p);
		if(ishost)
		{
			if(get_host_addr(str, ip) < 0)
				return -1;
		}
		else
		{
			*port = htons(atoi(str));
		}
	}

	return 0;
}

/*
 * puts the IP address associated with the given host into output buffer
 * host can be a.b.c.d or a host name
 * returns 0 if successful, -1 on error
 */

static int
get_host_addr(char *host, struct in_addr *output)
{
	struct hostent *he;
	int error = 0;

	if(((he = gethostbyname(host)) != NULL) && (he->h_addrtype == AF_INET))
		memcpy(output, he->h_addr, sizeof(struct in_addr));
	else
		error = -1;

	return error;
}

/*
 * start a process to listen on the given interface for commands from a remote rb-browser
 */

void
start_listener(struct listen_data *listen_data)
{
	struct sigaction action;
	pid_t child;
	int sockopt;
	int listen_sock;
	int accept_sock;
	fd_set read_fds;
	socklen_t addr_len;
	struct sockaddr_in client_addr;
	bool quit;

	/*
	 * fork:
	 * the parent listens for commands,
	 * the child returns and downloads the carousel
	 */

	/* don't let our children become zombies */
	action.sa_handler = dead_child;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	if(sigaction(SIGCHLD, &action, NULL) < 0)
		fatal("signal: SIGCHLD: %s", strerror(errno));

	/* if we can't fork it's probably best to kill ourselves*/
	if((child = fork()) < 0)
		fatal("fork: %s", strerror(errno));
	/* child returns */
	else if(child == 0)
		return;
	/* parent continues */

	/* listen on the given ip:port */
	printf("Listening on %s:%u\n", inet_ntoa(listen_data->addr.sin_addr), ntohs(listen_data->addr.sin_port));

	if((listen_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		fatal("socket: %s", strerror(errno));

	/* in case someones already using it */
	sockopt = 1;
	if(setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt)) < 0)
		fatal("setsockopt: SO_REUSEADDR: %s", strerror(errno));

	if(bind(listen_sock, (struct sockaddr *) &listen_data->addr, sizeof(struct sockaddr_in)) < 0)
		fatal("bind: %s", strerror(errno));

	if(listen(listen_sock, BACKLOG) < 0)
		fatal("listen: %s", strerror(errno));

	/* listen for connections */
	while(true)
	{
		FD_ZERO(&read_fds);
		FD_SET(listen_sock, &read_fds);
		if(select(listen_sock + 1, &read_fds, NULL, NULL, NULL) < 0)
		{
			/* could have been interupted by SIGCHLD */
			if(errno != EINTR)
				error("select: %s", strerror(errno));
			continue;
		}
		/* check select didnt fuck up */
		if(!FD_ISSET(listen_sock, &read_fds))
			continue;
		addr_len = sizeof(client_addr);
		if((accept_sock = accept(listen_sock, (struct sockaddr *) &client_addr, &addr_len)) < 0)
		{
			/* we get ECONNABORTED in Linux if we're being SYN scanned */
			error("accept: %s", strerror(errno));
			continue;
		}
		/* fork off a child to handle it */
		if((child = fork()) < 0)
		{
			/* if we can't fork it's probably best to kill ourselves*/
			fatal("fork: %s", strerror(errno));
		}
		else if(child == 0)
		{
			/* child */
			close(listen_sock);
			quit = handle_connection(listen_data, accept_sock, &client_addr);
			close(accept_sock);
/* TODO */
if(quit) printf("QUIT!!!\n");
			exit(EXIT_SUCCESS);
		}
		else
		{
			/* parent */
			close(accept_sock);
		}
	}

	/* we never get here */
	close(listen_sock);

	return;
}

/*
 * handle a connection from a remote rb-browser
 */

static bool
handle_connection(struct listen_data *listen_data, int client_sock, struct sockaddr_in *client_addr)
{
	char cmd[1024];
	ssize_t nread;
	bool quit = false;

	printf("Connection from %s:%d\n", inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));

	/* read the command from the client */
	if((nread = read(client_sock, cmd, sizeof(cmd)-1)) > 0)
	{
		/* \0 terminate the buffer */
		cmd[nread] = '\0';
		/* strip off an trailing \n (only needed when testing with telnet etc) */
		nread --;
		while(nread > 0 && (cmd[nread] == '\n' || cmd[nread] == '\r'))
			cmd[nread--] = '\0';
		/* process the command */
		quit = process_command(listen_data, client_sock, cmd);
	}

	printf("Connection from %s:%d closed\n", inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));

	return quit;
}

static void
dead_child(int signo)
{
	if(signo == SIGCHLD)
		wait(NULL);

	return;
}
