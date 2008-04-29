#include <assert.h>
#include <stdio.h>
#include "FitFirewallBreakerCommon.h"

char *ip_ntoa(uint32_t ip)
{
	struct in_addr ia;
	ia.s_addr = ip;
	return inet_ntoa(ia);
}

void exchange(int fd1, int fd2)
{
	const int buf_size = 2048;
	char buf[2048];
	int fd[2] = {fd1, fd2};
	//char arraw[2][3] = {"=>", "<="};
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
				if(r<=0)
					return;
				int data_size = r;
				r = send(fd[1-i], buf, data_size, 0);
				if(r!=data_size)
					return;
				//printf("%s %d bytes.\n", arraw[i], data_size);
			}
		}
	}
}
