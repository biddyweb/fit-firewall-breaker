#ifndef FIREWALLBREAKER_H_
#define FIREWALLBREAKER_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//#define PROXY_SERVER (inet_addr("166.111.142.69"))
//#define PROXY_UDP_PORT (htons(5300))
#define MAGIC (htonl(0x12345678))
#define HTTP_SERVER "59.66.130.153"
#define QUERY_PHP "/ffb/query.php"
#define SUBMIT_PHP "/ffb/test.php"

struct http_request_packet {
	uint32_t src_ip;
	uint16_t src_control_port;
	uint16_t src_data_port;
	uint32_t to_ip;
	uint16_t to_port;
};

struct tcp_request_packet {
	uint32_t magic;
};

inline char *ip_ntoa(uint32_t ip)
{
	struct in_addr ia;
	ia.s_addr = ip;
	return inet_ntoa(ia);
}

inline void exchange(int fd1, int fd2)
{
	const int buf_size = 2048;
	char buf[2048];
	int fd[2] = {fd1, fd2};
	char *arraw[2] = {"=>", "<="};
	while(1)
	{
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(fd[0], &fds);
		FD_SET(fd[1], &fds);
		int nfds = ((fd[0]>fd[1])?fd[0]:fd[1])+1;
		int r = select(nfds, &fds, NULL, NULL, NULL);
		assert(r!=-1);
		int i;
		for(i=0; i<2; i++)
		{
			if(FD_ISSET(fd[i], &fds))
			{
				r = recv(fd[i], buf, buf_size, 0);
				if(r==0)
					return; // half-close problem
				assert(r!=-1);
				int data_size = r;
				r = send(fd[1-i], buf, data_size, 0);
				if(r==0)
					return; // half-close problem
				assert(r==data_size);
				printf("%s %d bytes.\n", arraw[i], data_size);
			}
		}
	}
}

#endif /*FIREWALLBREAKER_H_*/
