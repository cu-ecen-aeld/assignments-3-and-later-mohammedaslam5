#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <arpa/inet.h>

int shutdown_flag = 0;

static void sig_handler(int signo)
{
	switch(signo)
	{
		case SIGINT:
		case SIGTERM:
			syslog(LOG_INFO, "Caught signal, exiting");
			shutdown_flag = 1;
			break;
		default:
			break;
	}

}

int main(int argc, char **argv)
{
	pid_t pid = 0;
	int ret = -1;
	int socketfd = -1;
	int inc_sockfd = -1;

	struct addrinfo hints = {0};
	struct addrinfo *addr_info = NULL;

	struct sockaddr inc_sockaddr = {0};
	unsigned int inc_sockaddr_len = sizeof(inc_sockaddr);
	const int reuse_sock = 1;
	char *data_buffer = NULL;
	unsigned int data_buffer_size = 25 *1024;
	unsigned int recv_len;
	int flags = 0;
	FILE *fp;
	const char *filepath = "/var/tmp/aesdsocketdata";
	unsigned int write_len;
	unsigned int file_size;
	unsigned int read_len;
	unsigned int send_len;
	char addr[INET_ADDRSTRLEN];

	struct sigaction sig_action = {0};

	openlog(NULL, 0, LOG_USER);
	syslog(LOG_INFO, "Started aesdsocket program");

	data_buffer = malloc(data_buffer_size);
	if(data_buffer == NULL)
	{
		return -1;
	}
	
	fp = fopen(filepath, "w");
	if(fp != NULL)
	{
		fclose(fp);
	}

	sig_action.sa_handler = sig_handler;

	ret = sigaction(SIGINT, &sig_action, NULL);
	if(ret != 0)
	{
		syslog(LOG_ERR, "sigactiom failed for SIGINT");
                return -1;
	}

	ret = sigaction(SIGTERM, &sig_action, NULL);
        if(ret != 0)
        {
                syslog(LOG_ERR, "sigactiom failed for SIGTEM");
                return -1;
        }

	hints.ai_flags = AI_PASSIVE;

	syslog(LOG_INFO, "creating socket");
	socketfd = socket(AF_INET, SOCK_STREAM, 0);
	if(socketfd < 0)
	{
		syslog(LOG_ERR, "create socket failed");
		return -1;
	}

	ret = setsockopt(socketfd,
			SOL_SOCKET,
			SO_REUSEADDR,
			&reuse_sock,
			sizeof(reuse_sock));
	if(ret != 0)
	{
		syslog(LOG_ERR, "setsockopt() failed, ret = %d", ret);
                ret = -1;
		goto error1;
	}

	syslog(LOG_INFO, "get address to bind to socket");
	ret = getaddrinfo(NULL, "9000", &hints, &addr_info);
	if(ret != 0)
	{
		syslog(LOG_ERR, "getaddrinfo() failed, ret = %d", ret);
		ret = -1;
		goto error1;
	}

	syslog(LOG_INFO, "binding IP to socket");
	ret = bind(socketfd, addr_info->ai_addr, addr_info->ai_addrlen);
	if(ret != 0)
	{
		syslog(LOG_ERR, "bind() failed, ret = %d", ret);
		ret = -1;
		goto error2;
	}


	//syslog(LOG_INFO, "argc = %d, argv[0] = %s, argv[1] = %s", argc, argv[0], argv[1]);
	if(argc >= 2 &&  argv[1] != NULL && strcmp(argv[1], "-d") == 0)
	{
		syslog(LOG_INFO, "running app as daemon");
		pid = fork();
		if(pid < 0)
		{
			syslog(LOG_ERR, "fork() failed");
	                ret = -1;
        	        goto error2;
		}

		if (pid > 0)
		{
			syslog(LOG_INFO, "fork() success, exiting parent");
	        	return 0;
		}

		if (setsid() < 0)
		{
		        ret = -1;
			goto error2;
		}
	}

	syslog(LOG_INFO, "listening to socket");
	ret = listen(socketfd, 1);
	if(ret != 0)
	{
		syslog(LOG_ERR, "listen() failed, ret = %d", ret);
		ret = -1;
		goto error2;
	}

	while(!shutdown_flag)
	{
		syslog(LOG_INFO, "accept new connection in the socket");
		inc_sockfd = accept(socketfd, &inc_sockaddr, &inc_sockaddr_len);
		if(inc_sockfd < 0)
		{
			syslog(LOG_ERR, "accept() failed");
			ret = -1;
			goto error2;
		}
		
		if(NULL == inet_ntop(AF_INET, inc_sockaddr.sa_data, addr, INET_ADDRSTRLEN))
		{
			syslog(LOG_ERR, "inet_ntop() failed");
                        ret = -1;
                        goto error2;
		}

		syslog(LOG_INFO, "new connection in the socket: %s", addr);
		memset(data_buffer, 0, data_buffer_size);
		recv_len = 0;

		while(recv_len < sizeof(data_buffer))
		{
			ret = recv(inc_sockfd,
				&data_buffer[recv_len],
				(data_buffer_size - recv_len), flags);
			if(ret < 0)
			{
				syslog(LOG_ERR, "recv() failed, ret = %d", ret);
				break;
			}
	
			if(ret > 0)
			{
				recv_len += ret;
				if(strstr(data_buffer, "\n") != NULL)
				{
					syslog(LOG_INFO, "data_buffer_len: %d", recv_len);
					//syslog(LOG_INFO, "data_buffer: %s", data_buffer);
					break;
				}
			}
		}

		if(recv_len > 0 && strstr(data_buffer, "\n") != NULL)
		{
			syslog(LOG_INFO, "writing received data to file");
			write_len = 0;
			fp = fopen(filepath, "a");
			if(fp == NULL)
			{
				syslog(LOG_ERR, "open file %s failed", filepath);
		                ret = -1;
        		        goto error3;
			}
		
			while(write_len < recv_len)
			{
				ret = fwrite(&data_buffer[write_len],
						sizeof(char),
						recv_len - write_len,
						fp);
				if(ret < 0)
				{
					syslog(LOG_ERR, "file write failed");
					ret = -1;
					fclose(fp);
					goto error3;
				}
				write_len += ret;
			}
			fclose(fp);
		}

		fp = fopen(filepath, "r");
		if(fp == NULL)
		{
			 syslog(LOG_ERR, "open file %s failed", filepath);
                         ret = -1;
			 shutdown_flag = 1;
                         goto error3;
		}
		
		fseek(fp, 0L, SEEK_END);
		file_size = ftell(fp);
		rewind(fp);
		syslog(LOG_INFO, "file_size = %d", file_size);
		
		while(file_size > 0)
		{
			syslog(LOG_INFO, "reading file comtents");
			read_len = 0;
			memset(data_buffer, 0, data_buffer_size);
			ret = fread(data_buffer,
					sizeof(char),
					(data_buffer_size < file_size)? data_buffer_size:file_size,
					fp);
			if(ret <= 0)
			{
				syslog(LOG_ERR, "fread() failed, ret = %d", ret);
				break;
			}
			read_len = ret;
			file_size -= read_len;

			send_len = 0;
			while(send_len < read_len)
			{
				syslog(LOG_INFO, "sending file contents");
				ret = send(inc_sockfd,
						&data_buffer[send_len],
						read_len - send_len,
				       		flags);
				if(ret <= 0)
				{
					syslog(LOG_ERR, "send() failed, ret = %d", ret);
					break;
				}
				send_len += ret;
			}

			if(send_len != read_len)
			{
				syslog(LOG_ERR, "send_len != read_len");
				break;
			}
		}

		fclose(fp);

		close(inc_sockfd);
	}

error3:
	close(inc_sockfd);
error2:
	freeaddrinfo(addr_info);
error1:
	close(socketfd);

	if(data_buffer != NULL)
	{
		free(data_buffer);
	}

	syslog(LOG_INFO, "Exiting program");
	return ret;

}

