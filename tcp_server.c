#include <time.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lib.h"
#define MAXLINE 1024
#define CHECKTIME 5 //sec

#define LOCKFILE "daemon.pid"
#define LOCKMODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)

static char filebuf[4*1024*1024];//1M

int count = 0 ; 
time_t time1;

int lockfile(int fd)
{
	struct flock fl;

	fl.l_type = F_WRLCK;
	fl.l_start = 0;
	fl.l_whence = SEEK_SET;
	fl.l_len = 0;
	return (fcntl(fd, F_SETLK, &fl));

}

int already_running(const char* cmd)
{
	int fd;
	char buf[16];

	fd = open(cmd, O_RDWR|O_CREAT, LOCKMODE);
	if (fd < 0) {
	//	syslog(LOG_ERR, "can't open %s: %s", LOCKFILE, strerror(errno));
		exit(1);
	}

	if (lockfile(fd) < 0) {
		if (errno == EACCES || errno == EAGAIN) {
			close(fd);
			return 1;
		}
		//syslog(LOG_ERR, "can't lock %s: %s", LOCKFILE, strerror(errno));
		exit(1);
	}
	ftruncate(fd, 0);
	sprintf(buf, "%ld", (long)getpid());
	write(fd, buf, strlen(buf)+1);
	return 0;
}
void daemonize(const char *cmd)
{
	int		i,fd0,fd1,fd2;
	pid_t	pid;
	struct rlimit	rl;
	struct sigaction	sa;

	umask(0);
	if (getrlimit(RLIMIT_NOFILE, &rl) < 0)
	{
		printf("%s: can't get file limit", cmd);
		exit(1);
	}

	if ((pid = fork()) < 0)
	{
		printf("%s: can't fork", cmd);
		exit(1);
	}
	else if (pid != 0)
		exit(0);
	setsid();

	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGHUP, &sa, NULL) < 0)
	{
		printf("%s: can't ignore SIGHUP",cmd);
		exit(1);
	}
	if ((pid = fork()) < 0)
	{
		printf("%s: can't fork", cmd);
		exit(1);
	}

	else if (pid != 0)
		exit(0);

	/*  
	 *		if (chdir("/") < 0)
	 *				err_quit("%s: can't change directory to /");
	 *					*/

	if (rl.rlim_max == RLIM_INFINITY)
		rl.rlim_max = 1024;
	for(i = 0;i < (int)rl.rlim_max; ++i)
		close(i);

	fd0 = open("/dev/null", O_RDWR);
	fd1 = dup(fd0);
	fd2 = dup(fd0);
}



struct Config{

	int port;
	char ip[50];
	int	daemon;
	int proxy;
	int upnum;
	char  upstreamip[50][50];
	int upstreamport[50];
};

void str_echo(int sockfd , struct Config config , struct sockaddr_in cliaddr);
struct Config read_config()
{
	char str[50],buf1[50] , buf2[50];
	struct Config config;
	int ret;
	config.upnum = 0;
	FILE* file = fopen("httpd.conf", "r");
   	if (file == NULL)
	{
		printf("not exists!!!\n");
		exit(0);
	}

	while(fgets(str , sizeof(str) , file))
	{
		ret = sscanf( str , "%s %s" , buf1 , buf2);
		if (ret == EOF)
			continue;
		if(strcmp(buf1,"ip") == 0)
			strcpy(config.ip , buf2);
		else if(strcmp(buf1,"port") == 0)
			config.port = atoi(buf2);
		else if(strcmp(buf1,"daemon") == 0)
			config.daemon = atoi(buf2);
		else if(strcmp(buf1,"proxy") == 0)
			config.proxy = atoi(buf2);
		if(config.proxy == 1)
		{
			if(strcmp(buf1,"upstream") == 0)
			{
				char * pos = strstr(buf2, ":");
				if(pos == NULL)
				{
					printf("there is no upstream!!\n");
				}
				else
				{
					config.upstreamport[config.upnum] = atoi(pos+1);
					strncpy(config.upstreamip[config.upnum] , buf2 , pos - buf2);
					printf("upstream %d: %s:%d\n", config.upnum, config.upstreamip[config.upnum], config.upstreamport[config.upnum]);
					config.upnum++;
				}
			}
		}
	
	}	
	//try to read file once, support html file which size < 1M
	return config;
}

void* checkserver(void* arg)
{
	struct Config *config = (struct Config*)arg;
	int csockfd;
	struct sockaddr_in cserveraddr;
	int choice = 0;
	
	while(1)
	{
		csockfd = socket(AF_INET , SOCK_STREAM , 0);
		if(csockfd < 0)
		{
			printf("create socket failed\n");
			exit(1);
		}

		memset(&cserveraddr , 0 , sizeof(cserveraddr));
		cserveraddr.sin_family = AF_INET;
		cserveraddr.sin_port = htons(config->upstreamport[choice]);
		inet_pton(AF_INET , config->upstreamip[choice] , &cserveraddr.sin_addr);

		if(connect(csockfd , (struct sockaddr*)&cserveraddr , sizeof(cserveraddr) ) < 0)
		{
			printf("connect error\n");
			perror("connect error\n");                                                                                                                            
		}
		else 
			printf("server %d\n",choice);
		choice = (choice + 1)%config->upnum;
		sleep(CHECKTIME);
	}
	return NULL;
}
int main(int argc, char *argv[])
{
	int	listenfd, connfd;
	pid_t childpid;
	socklen_t clilen;
	struct sockaddr_in  cliaddr, servaddr;
	int ret;
	struct Config config;
	//if (argc != 2)
	//{
	//	printf("usage: %s port\n", argv[0]);
	//}
//	time_t time1;
	time1 = time(NULL);

	config = read_config();
	char cmd[50];
	strcpy(cmd, argv[0]);
	strcat(cmd, ".pid");
	pthread_t  thread;
	if(config.proxy == 1)
		pthread_create(&thread , NULL , checkserver, &config);

	if(config.daemon == 1)
		daemonize(cmd);
	if( already_running(cmd))
	{	
		exit(1);
	}

	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenfd < 0)
	{
		printf("create socket failed\n");
		exit(1);
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	//servaddr.sin_port = htons(atoi(argv[1]));
	servaddr.sin_port = htons(config.port);
	//servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	inet_pton(AF_INET,config.ip,&servaddr.sin_addr);
	ret = bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr));
	if (ret < 0)
	{
		perror("bind error\n");
		exit(2);
	}

	ret = listen(listenfd, 1024);
	if (ret < 0)
	{
		perror("listen error\n");
		exit(3);
	}

	for(;;)
	{
		connfd = accept(listenfd, (struct sockaddr*) &cliaddr, &clilen);
		if (connfd < 0)
		{
			if (errno == EINTR)
				continue;
			perror("accept error");
			exit(4);
		}
	
		if ((childpid = fork()) == 0){
			close(listenfd);
			count++;
			str_echo(connfd , config , cliaddr);
			exit(0);
		}
		close(connfd);
	}
}

//>0: read size
//-1: not exists
int readhtml(const char *host)
{
	char fp[50]="webapps/";
	strcat(fp, host);
//	printf("%s\n",fp);
	
	FILE* file = fopen(fp, "r");
   	if (file == NULL)
		return -1;//not exists
	
	//try to read file once, support html file which size < 1M
	size_t size = fread(filebuf, sizeof(char), 4*1024*1024, file);	
	fclose(fp);
	return size;
}


int writelog()
{
	char logname[50];
	struct tm * ptm;
	time_t now;
	now = time(NULL);
	ptm = localtime(&now);

	sprintf(logname , "httpd-%d-%d-%d.log" , ptm->tm_year+1900 , ptm->tm_mon+1 , ptm->tm_mday);
//	printf("this is a test2\n");
	char fp[50] = "log/";
	strcat(fp,logname);
//	printf("fp:%s\n",fp);
	FILE* file = fopen(fp , "a+");
	if(file == NULL)
		return -1;
	fprintf(file , "%d:%d:%d\r\n" , ptm->tm_hour , ptm->tm_min , ptm->tm_sec);
	fclose(file);
	return 1;	
}

void process(int sockfd, const char *buf, ssize_t n)
{
	char host[MAXLINE];
	static char resp[4*1024*1024];
	char req1[MAXLINE];
	char req2[MAXLINE];
	char req3[MAXLINE];
	int length;
	char str[50],buf1[50],buf2[50],buf3[50];
	struct utsname name;
	time_t time2;
	time2 = time(NULL);
	char contenttype[50];
	char * p1 ;
	char * p2 ;
	int writefile = 0;
	int ret;

	uname(&name);	
	printf("buf:%s\r\n",buf);
	
	p1 = strstr(buf , "\r\n");
	strncpy(req1 , buf , p1-buf);
	p2 = strstr(p1+1,"\r\n");
	strncpy(req2 , p1+1 , p2-p1+1);
	strcpy(req3 , req1);
	strcat(req3 , req2);

	if(strcmp(buf , "monitor") == 0)
	{
		sprintf(resp,"OS:ABCDEFG\n");
		sprintf(resp,"CR:%d\n",count);
		sprintf(resp,"OS:ABCDEFG\r\nCR:%d\r\nACR:%lf\n", count ,(double) count/(time2-time1));
	}
	else if(sscanf(req1, "GET /%s HTTP/1.1\r\n", host) < 1)
	{
		printf("error request: %s\n", buf);
		sprintf(resp, "HTTP/1.1 501 NOT IMPLEMENTED\r\nServer: Apache/1.3.3.7 %s\r\nContent-Length: 0 \r\nContent-Type:  \r\n\r\n",name.sysname);
	}else{
		length = readhtml(host);
		printf("length: %d\n", length);
/*		if(strstr(host , ".jpg"))
			strcpy(contenttype , "image/jpg");
		else if(strstr(host , ".css"))
			strcpy(contenttype , "text/css");
		else if(strstr(host , ".js"))
			strcpy(contenttype , "application/x-javascript");
		else 
			strcpy(contenttype , "text/html");
*/
		FILE * file  = fopen("httpd.conf","r");
		if(file == NULL)
		{
			printf("not exists\n");
			exit(0);
		}
		while(fgets(str , sizeof(str) , file))
		{
			ret = sscanf(str ,"%s %s %s",buf1 , buf2 , buf3);
			if(ret == EOF)
				continue;
			else if(strcmp(buf1 , "AddType") == 0)
			{
				if(strstr(host , buf3))
					strcpy(contenttype , buf2);
			}
		}
		if (length < 0)
		{
			sprintf(resp, "HTTP/1.1 404 NOT FOUND\r\nServer: Apache/1.3.3.7 %s\r\nContent-Length: 0 \r\nContent-Type: %s \r\n\r\n",name.sysname , contenttype);
		}else{
			sprintf(resp, "HTTP/1.1 200 OK\r\nServer: Apache/1.3.3.7 %s\r\nContent-Length: %d \r\nContent-Type: %s \r\n\r\n",name.sysname, length,contenttype);
			writefile = 1;
		}
	}
	writelog();
	writen(sockfd, resp, strlen(resp));
	if (writefile)
		writen(sockfd, filebuf, length);

}

void processproxy(int sockfd, const char * buf , ssize_t n , struct Config config , struct sockaddr_in cliaddr)
{
	int psockfd;
	int choice;
	struct sockaddr_in pserveraddr;
	char recv[MAXLINE];
	psockfd = socket(AF_INET , SOCK_STREAM , 0);
	if(psockfd < 0)
	{
		printf("create socket failed!\n");
		exit(1);
	}
	
	choice = cliaddr.sin_addr.s_addr % config.upnum;
	
	memset(&pserveraddr , 0 , sizeof(pserveraddr));
	pserveraddr.sin_family = AF_INET;
	pserveraddr.sin_port = htons(config.upstreamport[choice]);
	inet_pton(AF_INET , config.upstreamip[choice] , &pserveraddr.sin_addr);

	if(connect(psockfd , (struct sockaddr*)&pserveraddr , sizeof(pserveraddr) ) < 0)
	{
		printf("connect error\n");
		perror("connect error\n");
		exit(2);
	}

	printf("forwording\n");
	writen(psockfd , buf , strlen(buf));
	while(read(psockfd,recv , MAXLINE)>0)
	{
		close(psockfd);
	}
	writen(sockfd,recv ,strlen(recv));
}

void str_echo(int sockfd , struct Config config , struct sockaddr_in cliaddr)
{
	ssize_t	n;
	char  buf[MAXLINE];

again:
	if((n = read(sockfd, buf, MAXLINE)) > 0)
	{
		if(config.proxy == 0)
		{
			process(sockfd, buf, n);
			printf("process once\n");
		}
		else
			processproxy(sockfd,buf,n ,config , cliaddr);
	}

	if (n < 0 && errno == EINTR)
		goto again;
	else if (n < 0)
	{
		perror("str_echo: read error");
		exit(6);
	}
}
