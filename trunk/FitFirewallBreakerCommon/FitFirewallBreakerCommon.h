#ifndef FITFIREWALLBREAKERCOMMON_H_
#define FITFIREWALLBREAKERCOMMON_H_

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

char *ip_ntoa(uint32_t ip);

void exchange(int fd1, int fd2);

#endif /*FITFIREWALLBREAKERCOMMON_H_*/
