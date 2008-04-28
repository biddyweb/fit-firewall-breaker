#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <map>

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
	if(r!=0)
	{
		assert(r!=-1);
		int data_size = r;
		r = send(*p_fd2, buf, data_size, 0);
		if(r!=0)
		{
			assert(r==data_size);
			return;
		}
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
	assert(new_connection!=-1);
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
	assert(r!=-1);
	int tcp_incoming = r;
	printf("accept from %s:%d\n", inet_ntoa(local_addr.sin_addr), ntohs(local_addr.sin_port));
	
	// tcp_control send
	struct tcp_request_packet trp;
	trp.magic = MAGIC;
	printf("tcp_control send.\n");
	r = send(tcp_control, &trp, sizeof(trp), 0);
	assert(r==sizeof(trp));
	
	(*p_map)[tcp_data] = make_pair((on_receive)accept_tcp_data, new int(tcp_incoming));
}

void go(uint16_t local_port, uint32_t to_ip, uint16_t to_port)
{
	// prepare tcp_control
	struct sockaddr_in proxy_addr, local_addr, http_addr;
	socklen_t addr_len = sizeof(local_addr);
	int tcp_control = socket(PF_INET, SOCK_STREAM, 0);
	assert(tcp_control!=-1);
	int r;
	r = listen(tcp_control, 1);
	assert(r!=-1);
	r = getsockname(tcp_control, (struct sockaddr *)&local_addr, &addr_len);
	assert(r!=-1);
	uint16_t src_control_port = local_addr.sin_port;
	
	// prepare tcp_data
	int tcp_data = socket(PF_INET, SOCK_STREAM, 0);
	assert(tcp_data!=-1);
	r = listen(tcp_data, 1);
	assert(r!=-1);
	addr_len = sizeof(local_addr);
	r = getsockname(tcp_data, (struct sockaddr *)&local_addr, &addr_len);
	assert(r!=-1);
	uint16_t src_data_port = local_addr.sin_port;

	// send http request
	printf("send http request: src_control_port is %d, to %s:%d.\n", ntohs(src_control_port), ip_ntoa(to_ip), ntohs(to_port));
	int http = socket(PF_INET, SOCK_STREAM, 0);
	assert(http!=-1);
	http_addr.sin_family = PF_INET;
	http_addr.sin_port = htons(80);
	http_addr.sin_addr = *((struct in_addr *)gethostbyname(HTTP_SERVER)->h_addr_list[0]);
	r = connect(http, (struct sockaddr *)&http_addr, sizeof(http_addr));
	assert(r!=-1);
	char send_data[1024];
	snprintf(send_data, 1024, 
		"GET %s?SourceControlPort=%d&SourceDataPort=%d&ToIp=%s&ToPort=%d HTTP/1.0\r\n"
		"Host: %s\r\n"
		"\r\n",
		SUBMIT_PHP, src_control_port, src_data_port, ip_ntoa(to_ip), to_port, HTTP_SERVER);
	r = send(http, send_data, strlen(send_data), 0);
	assert(r==(int)strlen(send_data));
	char recv_data[1024];
	r = recv(http, recv_data, 1024, 0);
	assert(r>=0);
	close(http);
	recv_data[r] = 0;
	printf("%s\n", recv_data); 
	
	// tcp_control accept
	printf("waiting for establishing control connection.\n");
	addr_len = sizeof(proxy_addr);
	r = accept(tcp_control, (struct sockaddr *)&proxy_addr, &addr_len);
	assert(r!=-1);
	close(tcp_control);
	tcp_control = r;
	printf("control connection established.\n");
	
	// tcp_local_listen listen
	int tcp_local_listen = socket(PF_INET, SOCK_STREAM, 0);
	local_addr.sin_family = PF_INET;
	local_addr.sin_port = local_port;
	local_addr.sin_addr.s_addr = INADDR_ANY;
	r = bind(tcp_local_listen, (struct sockaddr *)&local_addr, sizeof(local_addr));
	assert(r!=-1);
	r = listen(tcp_local_listen, 5);
	assert(r!=-1);
	
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
		assert(r!=-1);
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
	assert(0);
}

int main(int argc, char *argv[])
{
	assert(argc == 4);
	uint16_t local_port = htons(atoi(argv[1]));
	struct hostent *hostinfo = gethostbyname(argv[2]);
	assert(hostinfo);
	uint32_t to_ip = ((struct in_addr *)hostinfo->h_addr_list[0])->s_addr;
	uint16_t to_port = htons(atoi(argv[3]));
	printf("local port is %d, connect to %s:%d.\n", ntohs(local_port), ip_ntoa(to_ip), ntohs(to_port));
	go(local_port, to_ip, to_port);
	return 0;
}
