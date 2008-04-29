#ifndef FITFIREWALLBREAKERCOMMON_H_
#define FITFIREWALLBREAKERCOMMON_H_

#ifdef WIN32
	#include <Winsock2.h>
	#include <windows.h>
	#define close closesocket
	#define snprintf _snprintf_s
	#define send(s, buf, len, flags) send(s, (const char *)(buf), len, flags)
	typedef unsigned int uint32_t;
	typedef unsigned short uint16_t;
	typedef int socklen_t;
#else
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
#endif

#define MAGIC (htonl(0x12345678))
#define HTTP_SERVER "wcgbg.net9.org"
#define QUERY_PHP "/ffb/query.php"
#define SUBMIT_PHP "/ffb/submit.php"

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
