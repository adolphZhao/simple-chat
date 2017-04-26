#include <event.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>
#include <string.h>
#include <errno.h>

#define SERVER_PORT 9005

struct event_base * base;
evutil_socket_t socketlisten;
struct event * accept_event;

int all_client = 0;

struct client {
	int fd;
	struct bufferevent *buf_ev;
    int index;
};

struct client* client_list[100];

void buf_read_callback(struct bufferevent *incoming, void *arg)
{
	struct evbuffer *evreturn;
    int i=0;
	char *req = evbuffer_readline(incoming->input);

    for(i=0;i<all_client;i++){
        if(client_list[i]==NULL){
            continue;
        }

        evreturn = evbuffer_new();
        if(client_list[i]->buf_ev != incoming){
            evbuffer_add_printf(evreturn,"%s\r\n",req);
        }
        evbuffer_add_printf(evreturn,"- - - - - - - - - - - - - - - - - - - - -\r\n");

        bufferevent_write_buffer(client_list[i]->buf_ev,evreturn);
        evbuffer_free(evreturn);
    }
}

void buf_write_callback(struct bufferevent *bev,void *arg)
{

}

void buf_error_callback(struct bufferevent *bev,short what,void *arg)
{
	struct client *client = (struct client *)arg;
	bufferevent_free(client->buf_ev);
	close(client->fd);
    client_list[client->index] = NULL;
	free(client);
}

void accept_callback(int fd,short ev,void *arg)
{
	int client_fd;
	struct sockaddr_in client_addr;
	struct client *client;
	socklen_t client_len = sizeof(client_addr);
    int i=0;
	client_fd = accept(fd,(struct sockaddr *)&client_addr,&client_len);
    printf("client id : %d\r\n",client_fd);

	if (client_fd < 0){
		warn("Client: accept() failed");
		return;
	}
    evutil_make_socket_nonblocking(client_fd);

	client = calloc(1, sizeof(*client));
	if (client == NULL){
		err(1, "malloc failed");
    }
	client->fd = client_fd;
	client->buf_ev = bufferevent_new(client_fd,
			buf_read_callback,
			buf_write_callback,
			buf_error_callback,
			client);
    client->index = all_client;

    client_list[all_client]=client;
    all_client ++;
	bufferevent_enable(client->buf_ev, EV_READ);
}

int main(int argc,char **argv)
{
	struct sockaddr_in addresslisten;

	base = event_init();
	event_base_priority_init(base,0);
	socketlisten = socket(AF_INET, SOCK_STREAM, 0);

	if (socketlisten < 0){
		fprintf(stderr,"Failed to create listen socket");
		return 1;
	}

    evutil_make_listen_socket_reuseable(socketlisten);
    evutil_make_socket_nonblocking(socketlisten);

	memset(&addresslisten, 0, sizeof(addresslisten));

	addresslisten.sin_family = AF_INET;
	addresslisten.sin_addr.s_addr = INADDR_ANY;
	addresslisten.sin_port = htons(SERVER_PORT);

	if (bind(socketlisten,(struct sockaddr*)&addresslisten,sizeof(addresslisten)) < 0){
		fprintf(stderr,"Failed to bind");
		return 1;
	}

	if (listen(socketlisten, 15) < 0){
		fprintf(stderr,"Failed to listen to socket");
		return 1;
	}

	accept_event = event_new(base, socketlisten, EV_READ|EV_PERSIST, accept_callback, NULL);
	event_add(accept_event,	NULL);
	event_base_dispatch(base);
	close(socketlisten);
	return 0;
}
