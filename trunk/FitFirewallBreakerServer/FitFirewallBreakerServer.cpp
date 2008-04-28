#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "FitFirewallBreakerCommon.h"

int fetch_request(struct http_request_packet *hrp)
{
	int http = socket(PF_INET, SOCK_STREAM, 0);
	assert(http!=-1);
	struct sockaddr_in remote_addr;
	remote_addr.sin_family = PF_INET;
	remote_addr.sin_port = htons(80);
	remote_addr.sin_addr = *((struct in_addr *)gethostbyname(HTTP_SERVER)->h_addr_list[0]);
	int r;
	r = connect(http, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
	assert(r!=-1);
	char send_data[] = 
		"GET " QUERY_PHP " HTTP/1.0\r\n"
		"Host: " HTTP_SERVER "\r\n"
		"\r\n";
	r = send(http, send_data, strlen(send_data), 0);
	assert(r==(int)strlen(send_data));
	char recv_data[1024];
	r = recv(http, recv_data, 1024, 0);
	assert(r>=0);
	close(http);
	recv_data[r] = 0;
	char *content = strstr(recv_data, "\r\n\r\n");
	assert(content);
	content += 4; // for "\r\n\r\n"
	if(content[0] == 'E')
		return -1;
	int i;
	for(i=0; i<5; i++)
	{
		char *p = strchr(content, ' ');
		assert(p);
		*p = 0;
		if(i==0)
			hrp->src_ip = inet_addr(content);
		else if(i==1)
			hrp->src_control_port = atoi(content);
		else if(i==2)
			hrp->src_data_port = atoi(content);
		else if(i==3)
			hrp->to_ip = inet_addr(content);
		else if(i==4)
			hrp->to_port = atoi(content);
		content = p+1;
	}
	return 0;
}

void port_forward(uint32_t ip1, uint16_t port1, uint32_t ip2, uint16_t port2)
{
	printf("port_forward %s:%d, %s:%d\n", ip_ntoa(ip1), ntohs(port1), ip_ntoa(ip2), ntohs(port2));
	struct sockaddr_in addr1, addr2;
	int fd1 = socket(PF_INET, SOCK_STREAM, 0);
	int fd2 = socket(PF_INET, SOCK_STREAM, 0);
	assert(fd1!=-1);
	assert(fd2!=-1);
	addr1.sin_family = PF_INET;
	addr2.sin_family = PF_INET;
	addr1.sin_port = port1;
	addr2.sin_port = port2;
	addr1.sin_addr.s_addr = ip1;
	addr2.sin_addr.s_addr = ip2;
	int r;
	r = connect(fd1, (struct sockaddr *)&addr1, sizeof(addr1));
	assert(r!=-1);
	r = connect(fd2, (struct sockaddr *)&addr2, sizeof(addr2));
	assert(r!=-1);
	exchange(fd1, fd2);
	close(fd1);
	close(fd2);
}


void control(struct http_request_packet hrp)
{
	printf("request feched. tcp connect to %s:%d.\n", ip_ntoa(hrp.src_ip), hrp.src_control_port);
	int tcp = socket(PF_INET, SOCK_STREAM, 0);
	assert(tcp!=-1);
	struct sockaddr_in remote_addr;
	remote_addr.sin_family = PF_INET;
	remote_addr.sin_port = hrp.src_control_port;
	remote_addr.sin_addr.s_addr = hrp.src_ip;
	int r;
	r = connect(tcp, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
	assert(r!=-1);
	while(1)
	{
		printf("waiting for new connection\n");
		struct tcp_request_packet trp;
		r = recv(tcp, &trp, sizeof(trp), 0);
		if(r==0)
			break;
		assert(r==sizeof(trp));
		assert(trp.magic == MAGIC);
		if(fork()==0)
		{
			close(tcp);
			port_forward(hrp.src_ip, hrp.src_data_port, hrp.to_ip, hrp.to_port);
			return;
		}
	}
	close(tcp);
}

int main()
{
	while(1)
	{
		printf("fetch_request\n");
		struct http_request_packet hrp;
		int r = fetch_request(&hrp);
		if(r==0)
		{
			if(fork()==0)
			{
				control(hrp);
				return 0;
			}
		}
		else
			sleep(1);
	}
	return 0;	
}
