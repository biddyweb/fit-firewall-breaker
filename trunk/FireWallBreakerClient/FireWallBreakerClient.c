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

#include "../FireWallBreakerServer/FireWallBreaker.h"

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
	assert(r==strlen(send_data));
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
	
	while(1)
	{
		// tcp_local_listen accept
		printf("waiting for local connection\n");
		addr_len = sizeof(local_addr);
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
		
		// tcp_data accept
		printf("waiting for establishing data connection.\n"); 
		addr_len = sizeof(proxy_addr);
		int new_connection = accept(tcp_data, (struct sockaddr *)&proxy_addr, &addr_len);
		assert(new_connection!=-1);
		printf("data connection established.\n");
		
		exchange(tcp_incoming, new_connection);
		
		close(tcp_incoming);
		close(new_connection);
		printf("data connection lost.\n");
	}
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
