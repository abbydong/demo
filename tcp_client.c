#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lib.h"

char* getoutput(const char* host,int choose)
{
	static char tmp[MAXLINE];
	if(choose == 1)
		sprintf(tmp, "GET /%s HTTP/1.1\r\nHOST: \r\n\r\n", host);
	else if(choose == 2)
		sprintf(tmp, "POST / HTTP/1.1\r\nHOST: %s\r\n\r\n", host);
	else if(choose == 3)
		sprintf(tmp, "monitor");
	return tmp;
}


void str_cli(FILE *fp, int sockfd)
{
	char  sendline[MAXLINE], recvline[MAXLINE];
	char* req;
	int n,choose;
	printf("choose the request : \n");
	printf("1.get\n2.post\n3.monitor\n");
	scanf("%d",&choose);
	getc(fp);
	printf("input the request host\n");
	fflush(fp);

	if(fgets(sendline, MAXLINE, fp) != NULL)
	{
		req = getoutput(sendline,choose);
		writen(sockfd, req, strlen(req));
		puts("-------\n");
again:
		while((n = read(sockfd, recvline, MAXLINE)) > 0)
		{
			puts(recvline);
		}

		if (n < 0 && errno == EINTR)
			goto again;
		else if (n < 0)
		{
			perror("str_cli: read error");
			exit(6);
		}
		puts("-------\n");
		fflush(stdin);
		fflush(stdout);
	}
	printf("request done\n");
}



int main(int argc, char* argv[])
{
	int sockfd;
	struct sockaddr_in	servaddr;
	if (argc != 2)
	{
		printf("usage: %s port\n", argv[0]);
		exit(1);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		printf("create socket failed\n");
		exit(1);
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(argv[1]));
	inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);

	if (connect(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0)
	{
		perror("connect error:");
		exit(2);
	}

	str_cli(stdin, sockfd);
	return 0;
}
