#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <stdexcept>
#include <map>

#ifdef WIN32
	#include <Winsock2.h>
	#include <windows.h>
	#pragma comment(lib, "Ws2_32.lib")
#else
	#include <netdb.h>
	#include <netinet/in.h>
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <arpa/inet.h>
#endif

#include "FitFirewallBreakerCommon.h"

using namespace std;

typedef void (*on_receive)(void *p_map, int fd, void *argument);
typedef map <int, pair <on_receive, void *> > mymap;

void forward(mymap *p_map, int fd1, int *p_fd2)
{
	const int buf_size = 2048;
	char buf[2048];
	int r;
	r = recv(fd1, buf, buf_size, 0);
	if(r>0)
	{
		int data_size = r;
		r = send(*p_fd2, buf, data_size, 0);
		if(r>0)
		{
			if(r!=data_size)
				throw runtime_error("send()");
			return;
		}
		else
		{
			printf("error on send()\n");
		}
	}
	else if(r==0)
	{
		printf("closed by peer.\n");
	}
	else
	{
		printf("errno = %s\n", strerror(errno));
	}
	// close and clear
	close(fd1);
	close(*p_fd2);
	int *p_fd1 = (int *)(*p_map)[*p_fd2].second;
	assert(*p_fd1==fd1);
	p_map->erase(fd1);
	p_map->erase(*p_fd2);
	delete p_fd1;
	delete p_fd2;
	printf("data connection lost.\n");
}

void accept_tcp_data(mymap *p_map, int tcp_data, int *p_tcp_incoming)
{
	p_map->erase(tcp_data);
	int tcp_incoming = *p_tcp_incoming;
	delete p_tcp_incoming;
	
	// tcp_data accept
	struct sockaddr_in proxy_addr;
	socklen_t addr_len = sizeof(proxy_addr);
	int new_connection = accept(tcp_data, (struct sockaddr *)&proxy_addr, &addr_len);
	if(new_connection==-1)
	{
		printf("could not establish the data connection.\n");
		return;
	}
	printf("data connection established.\n");
	
	(*p_map)[new_connection] = make_pair((on_receive)forward, new int(tcp_incoming));
	(*p_map)[tcp_incoming] = make_pair((on_receive)forward, new int(new_connection));
}

void accept_local_listen(mymap *p_map, int tcp_local_listen, pair<int, int> *p_pair)
{
	int tcp_control = p_pair->first;
	int tcp_data = p_pair->second;
	
	// tcp_local_listen accept
	struct sockaddr_in local_addr;
	socklen_t addr_len = sizeof(local_addr);
	int r;
	r = accept(tcp_local_listen, (struct sockaddr *)&local_addr, &addr_len);
	if(r==-1)
	{
		printf("could not establish the incoming connection.\n");
		return;
	}
	int tcp_incoming = r;
	printf("accept from %s:%d\n", inet_ntoa(local_addr.sin_addr), ntohs(local_addr.sin_port));
	
	// tcp_control send
	struct tcp_request_packet trp;
	trp.magic = MAGIC;
	printf("tcp_control send.\n");
	r = send(tcp_control, &trp, sizeof(trp), 0);
	if(r!=sizeof(trp))
		throw runtime_error("send()");
	(*p_map)[tcp_data] = make_pair((on_receive)accept_tcp_data, new int(tcp_incoming));
}

void go(uint16_t local_port, uint32_t to_ip, uint16_t to_port)
{
	struct sockaddr_in proxy_addr, local_addr, http_addr;
	int r;

	// bind local listen port
	int tcp_local_listen = socket(PF_INET, SOCK_STREAM, 0);
	local_addr.sin_family = PF_INET;
	local_addr.sin_port = local_port;
	local_addr.sin_addr.s_addr = INADDR_ANY;
	r = bind(tcp_local_listen, (struct sockaddr *)&local_addr, sizeof(local_addr));
	if(r==-1)
		throw runtime_error("bind()");

	// prepare tcp_control
	socklen_t addr_len = sizeof(local_addr);
	int tcp_control = socket(PF_INET, SOCK_STREAM, 0);
	if(tcp_control==-1)
		throw runtime_error("socket()");
#ifdef WIN32
	local_addr.sin_family = PF_INET;
	local_addr.sin_port = 0;
	local_addr.sin_addr.s_addr = INADDR_ANY;
	r = bind(tcp_control, (struct sockaddr *)&local_addr, sizeof(local_addr));
	if(r==-1)
		throw runtime_error("bind()");
#endif
	r = listen(tcp_control, 1);
	if(r==-1)
		throw runtime_error("listen()");
	r = getsockname(tcp_control, (struct sockaddr *)&local_addr, &addr_len);
	if(r==-1)
		throw runtime_error("getsockname()");
	uint16_t src_control_port = local_addr.sin_port;
	
	// prepare tcp_data
	int tcp_data = socket(PF_INET, SOCK_STREAM, 0);
	if(tcp_data==-1)
		throw runtime_error("socket()");
#ifdef WIN32
	local_addr.sin_family = PF_INET;
	local_addr.sin_port = 0;
	local_addr.sin_addr.s_addr = INADDR_ANY;
	r = bind(tcp_data, (struct sockaddr *)&local_addr, sizeof(local_addr));
	if(r==-1)
		throw runtime_error("bind()");
#endif
	r = listen(tcp_data, 1);
	if(r==-1)
		throw runtime_error("listen()");
	addr_len = sizeof(local_addr);
	r = getsockname(tcp_data, (struct sockaddr *)&local_addr, &addr_len);
	if(r==-1)
		throw runtime_error("getsockname()");
	uint16_t src_data_port = local_addr.sin_port;

	// send http request
	printf("send http request: src_control_port is %d, to %s:%d.\n", ntohs(src_control_port), ip_ntoa(to_ip), ntohs(to_port));
	int http = socket(PF_INET, SOCK_STREAM, 0);
	if(http==-1)
		throw runtime_error("socket()");
	http_addr.sin_family = PF_INET;
	http_addr.sin_port = htons(80);
	http_addr.sin_addr = *((struct in_addr *)gethostbyname(HTTP_SERVER)->h_addr_list[0]);
	r = connect(http, (struct sockaddr *)&http_addr, sizeof(http_addr));
	if(r==-1)
		throw runtime_error("connect()");
	char send_data[1024];
	snprintf(send_data, 1024, 
		"GET %s?SourceControlPort=%d&SourceDataPort=%d&ToIp=%s&ToPort=%d HTTP/1.0\r\n"
		"Host: %s\r\n"
		"\r\n",
		SUBMIT_PHP, src_control_port, src_data_port, ip_ntoa(to_ip), to_port, HTTP_SERVER);
	r = send(http, send_data, strlen(send_data), 0);
	if(r!=(int)strlen(send_data))
		throw runtime_error("send()");
	char recv_data[1024];
	r = recv(http, recv_data, 1024, 0);
	if(r==-1)
		throw runtime_error("recv()");
	close(http);
	recv_data[r] = 0;
	printf("%s\n", recv_data); 
	
	// tcp_control accept
	printf("waiting for establishing control connection...\n");
	int i;
	for(i=0; ; i++)
	{
		printf("%d sec...\n", i);
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(tcp_control, &fds);
		struct timeval tv;
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		int r = select(tcp_control+1, &fds, NULL, NULL, &tv);
		if(r==1)
			break;
		if(r==-1)
			throw runtime_error("select()");
	}
	addr_len = sizeof(proxy_addr);
	r = accept(tcp_control, (struct sockaddr *)&proxy_addr, &addr_len);
	close(tcp_control);
	if(r==-1)
		throw runtime_error("accept()");
	tcp_control = r;
	printf("control connection established.\n");
	
	// tcp_local_listen listen
	r = listen(tcp_local_listen, 5);
	if(r==-1)
		throw runtime_error("listen()");
	
	mymap recv_map;
	recv_map[tcp_local_listen] = make_pair((on_receive)accept_local_listen, new pair<int, int>(tcp_control, tcp_data));
	while(!recv_map.empty())
	{
		fd_set fds;
		FD_ZERO(&fds);
		mymap::iterator i;
		int max_fd = 0;
		for(i=recv_map.begin(); i!=recv_map.end(); i++)
		{
			int fd = i->first;
			FD_SET(fd, &fds);
			if(fd > max_fd)
				max_fd = fd;
		}
		int r = select(max_fd+1, &fds, NULL, NULL, NULL);
		if(r==-1 && errno!=EINTR)
			throw runtime_error("select()");
		for(i=recv_map.begin(); i!=recv_map.end(); i++)
		{
			int fd = i->first;
			if(FD_ISSET(fd, &fds))
			{
				on_receive orcv = i->second.first;
				void *argument = i->second.second;
				orcv(&recv_map, fd, argument);
				break;
			}
		}
	}
	printf("no event to wait.\n");
}

int main(int argc, char *argv[])
{
#ifdef WIN32
	WSADATA wsd;
	WSAStartup(0x0202, &wsd);
#endif
	if(argc != 4)
	{
		printf("Usage: %s local_port host_address host_port\n\n", argv[0]);
		printf(
			"It works by allocating a socket to listen to local_port on the local side. "
			"Whenever a connection is made to this port, "
			"the connection is forwarded over a channel, "
			"and a connection is made to host port host_port. "
			"\n\n"
			);
		return 0;
	}
	uint16_t local_port = htons(atoi(argv[1]));
	struct hostent *hostinfo = gethostbyname(argv[2]);
	uint32_t to_ip = ((struct in_addr *)hostinfo->h_addr_list[0])->s_addr;
	uint16_t to_port = htons(atoi(argv[3]));
	printf("local port is %d, connect to %s:%d.\n", ntohs(local_port), ip_ntoa(to_ip), ntohs(to_port));
	go(local_port, to_ip, to_port);
#ifdef WIN32
	WSACleanup();
#endif
	return 0;
}
