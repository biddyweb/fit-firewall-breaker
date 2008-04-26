#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>

int main()
{
	struct sockaddr_in local_addr, remote_addr;
	local_addr.sin_family = PF_INET;
	local_addr.sin_port = htons(5300);
	local_addr.sin_addr.s_addr = INADDR_ANY;
	int udp = socket(PF_INET, SOCK_DGRAM, 0);
	assert(udp!=-1);
	int r;
	r = bind(udp, (struct sockaddr *)&local_addr, sizeof(local_addr));
	assert(r==0);
	socklen_t remote_addr_len = sizeof(remote_addr);
	char buffer[1000];	
	r = recvfrom(udp, buffer, sizeof(buffer), 0, (struct sockaddr *)&remote_addr, &remote_addr_len);
	assert(r!=-1);
	buffer[r] = 0;
	printf("%s\n", buffer);
	close(udp);
	return 0;	
}
