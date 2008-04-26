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
	struct sockaddr_in server_addr, local_addr;
	local_addr.sin_family = PF_INET;
	local_addr.sin_port = 0;
	local_addr.sin_addr.s_addr = INADDR_ANY;
	int udp = socket(PF_INET, SOCK_DGRAM, 0);
	assert(udp!=-1);
	int r;
	r = bind(udp, (struct sockaddr *)&local_addr, sizeof(local_addr));
	assert(r==0);
	server_addr.sin_family = PF_INET;
	server_addr.sin_port = htons(5300);
	server_addr.sin_addr.s_addr = inet_addr("59.66.130.153");	
	r = sendto(udp, "hello", 5, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
	printf("%d\n",r);
	close(udp);	
	return 0;
}
