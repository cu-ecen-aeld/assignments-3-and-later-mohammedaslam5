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
#include <pthread.h>
#include "sys/queue.h"
#include <sys/time.h>
#include <errno.h>
#include <sys/mman.h>

#define MILLION (unsigned long int) 	1000000

void *handle_client(void *arg);

struct node
{
    int inc_sock;
    pthread_t thread_id;
    TAILQ_ENTRY(node) nodes;
};

int socketfd = -1;

pthread_mutex_t f_lock = PTHREAD_MUTEX_INITIALIZER; 

int shutdown_flag = 0;

void timer_thread(union sigval arg)
{
	int ret = -1;
	FILE *fp;
	const char *filepath = "/var/tmp/aesdsocketdata";

	struct tm* local; 
	time_t t = time(NULL); 
	char formatted_time[200] = {0};

	pthread_mutex_lock(&f_lock);
	local = localtime(&t); 

	strftime(formatted_time, sizeof(formatted_time), "timestamp:%a, %d %b %Y %H:%M:%S %z\n", local);

	syslog(LOG_INFO, "writing time to file: %s", formatted_time);
	fp = fopen(filepath, "a+");
	if(fp == NULL)
	{
		syslog(LOG_ERR, "open file %s failed", filepath);
				ret = -1;
				pthread_mutex_unlock(&f_lock);
				return;
	}

	ret = fwrite(formatted_time,
			sizeof(char),
			strlen(formatted_time),
			fp);
	if(ret < 0)
	{
		syslog(LOG_ERR, "file write failed");
		ret = -1;
		fclose(fp);
		pthread_mutex_unlock(&f_lock);
		return;
	}

	fclose(fp);
	pthread_mutex_unlock(&f_lock);

	// syslog(LOG_INFO, "timer expired");
}


int create_timer(unsigned long int msecs, timer_t *timerid)
{
	timer_t timer_id;
	int status;
	struct itimerspec ts = {0};
	struct sigevent se = {0};
	unsigned long int nanosecs = msecs * MILLION;

	/*
	* Set the sigevent structure to cause the signal to be
	* delivered by creating a new thread.
	*/
	se.sigev_notify = SIGEV_THREAD;
	se.sigev_value.sival_ptr = &timer_id;
	se.sigev_notify_function = timer_thread;
	//se.sigev_notify_attributes = &attr;

	ts.it_value.tv_sec = nanosecs / (unsigned long int)1000000000;
	ts.it_value.tv_nsec = nanosecs % (unsigned long int)1000000000;
	ts.it_interval.tv_sec = ts.it_value.tv_sec;
	ts.it_interval.tv_nsec = ts.it_value.tv_nsec;
	
	status = timer_create(CLOCK_REALTIME, &se, &timer_id);
	if (status == -1)
	{
		syslog(LOG_ERR, "Create timer failed");
		return -1;
	}
	else
	{
		syslog(LOG_INFO, "timer created %p", timer_id);
		status = timer_settime(timer_id, 0, &ts, NULL);
		if (status == -1)
		{
			timer_delete(timer_id);
			syslog(LOG_ERR, "Set timer failed");
			return -1;
		}

	}

	*timerid = timer_id;
	return 0;
}

static void sig_handler(int signo)
{
	switch(signo)
	{
		case SIGINT:
			case SIGTERM:
			syslog(LOG_INFO, "Caught signal, exiting");
			shutdown_flag = 1;
			// close(socketfd);
			// syslog(LOG_INFO, "socket close complete");
			break;
		default:
			break;
	}

}

int main(int argc, char **argv)
{
	pid_t pid = 0;
	int ret = -1;
	int inc_sockfd = -1;

	timer_t timer_id;

	struct addrinfo hints = {0};
	struct addrinfo *addr_info = NULL;

	struct sockaddr inc_sockaddr = {0};
	unsigned int inc_sockaddr_len = sizeof(inc_sockaddr);
	const int reuse_sock = 1;
	FILE *fp;
	const char *filepath = "/var/tmp/aesdsocketdata";
	char addr[INET_ADDRSTRLEN];
	struct sigaction sig_action = {0};

	pthread_attr_t cli_thread_attr;
	TAILQ_HEAD(head_s, node) head;
	TAILQ_INIT(&head);

	struct node *th = NULL;

	openlog(NULL, 0, LOG_USER);
	syslog(LOG_INFO, "Started aesdsocket program");

	if(pthread_mutex_init(&f_lock, NULL) != 0)
	{ 
        	syslog(LOG_ERR, "mutex init has failed"); 
        	return -1; 
	} 
	
	fp = fopen(filepath, "w");
	if(fp != NULL)
	{
		fclose(fp);
	}

	sig_action.sa_handler = sig_handler;
	sig_action.sa_flags = 0;
	sigemptyset(&sig_action.sa_mask);

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


	syslog(LOG_INFO, "creating socket");
	socketfd = socket(AF_INET, SOCK_STREAM, 0);
	if(socketfd < 0)
	{
		syslog(LOG_ERR, "create socket failed");
		return -1;
	}

	ret = setsockopt(socketfd,
			SOL_SOCKET,
			SO_REUSEADDR | SO_REUSEPORT,
			&reuse_sock,
			sizeof(reuse_sock));
	if(ret != 0)
	{
		syslog(LOG_ERR, "setsockopt() failed, ret = %d", ret);
                ret = -1;
		goto error1;
	}

	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_INET;

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

	pthread_attr_init(&cli_thread_attr);
	pthread_attr_setdetachstate(&cli_thread_attr, PTHREAD_CREATE_JOINABLE);

	syslog(LOG_INFO, "Creating timer");
	if(create_timer(10 *1000, &timer_id) == -1) //10 secs
	{
		syslog(LOG_ERR, "Create timer failed");
		goto error2;
	}

	syslog(LOG_INFO, "timer id: %p", timer_id);

	syslog(LOG_INFO, "listening to socket");
	ret = listen(socketfd, 5);
	if(ret != 0)
	{
		syslog(LOG_ERR, "listen() failed, ret = %d", ret);
		ret = -1;
		goto error3;
	}

	while(!shutdown_flag)
	{
		// syslog(LOG_INFO, "accept new connection in the socket");
		inc_sockfd = accept(socketfd, &inc_sockaddr, &inc_sockaddr_len);
		if(inc_sockfd < 0)
		{
			syslog(LOG_ERR, "accept() failed");
			ret = -1;
			goto error3;
		}
		
		if(NULL == inet_ntop(AF_INET, inc_sockaddr.sa_data, addr, INET_ADDRSTRLEN))
		{
			syslog(LOG_ERR, "inet_ntop() failed");
                        ret = -1;
                        goto error3;
		}

		syslog(LOG_INFO, "new connection in the socket: %s:%d", addr, inc_sockfd);

		th = malloc(sizeof(struct node));
        	if(th == NULL)
        	{
            		fprintf(stderr, "malloc failed");
            		goto error3;
        	}
		
		th->inc_sock = inc_sockfd;
		// Create a new thread to handle the client
	        if (pthread_create(&(th->thread_id), &cli_thread_attr, handle_client, th) != 0)
		{
        	    	syslog(LOG_ERR, "Error creating thread");
            		close(inc_sockfd);
			goto error3;
        	}
		
		syslog(LOG_INFO, "created thread %ld for client %d", th->thread_id, inc_sockfd);

        	// Actually insert the node e into the queue at the end
        	TAILQ_INSERT_TAIL(&head, th, nodes);
        	th = NULL;
	}

error3:

	pthread_mutex_lock(&f_lock);
        if(timer_delete(timer_id) != 0)
        {
                syslog(LOG_ERR, "timer_delete failed %s\n",  strerror (errno));
        }
        pthread_mutex_unlock(&f_lock);

error2:
	pthread_attr_destroy(&cli_thread_attr);

	// free the elements from the queue
    	while (!TAILQ_EMPTY(&head))
    	{
        	th = TAILQ_FIRST(&head);
		//syslog(LOG_INFO, "th->thread_id = %ld", th->thread_id);
		pthread_join(th->thread_id, NULL);
        	TAILQ_REMOVE(&head, th, nodes);
        	free(th);
        	th = NULL;
    	}

	freeaddrinfo(addr_info);

error1:
	close(socketfd);

	pthread_mutex_destroy(&f_lock); 

	syslog(LOG_INFO, "Exiting program");
	return ret;
	exit(0);

}

void *handle_client(void *arg)
{
	int ret = -1;
	struct node *th = arg;
	int inc_sockfd = th->inc_sock;
	
	char *data_buffer = NULL;
	unsigned int data_buffer_size = 5 *1024;
	unsigned int recv_len;
	int flags = 0;
	FILE *fp;
	const char *filepath = "/var/tmp/aesdsocketdata";
	unsigned int write_len;
	unsigned int file_size;
	unsigned int read_len;
	unsigned int send_len;
	//char c;

	data_buffer = malloc(data_buffer_size);
	if(data_buffer == NULL)
	{
		goto error3;
	}

	while(!shutdown_flag)
	{
		memset(data_buffer, 0, data_buffer_size);
		recv_len = 0;

		while(!shutdown_flag && recv_len < data_buffer_size)
		{
			//ret = read(inc_sockfd, &c, sizeof(c));
			//if(ret <= 0)
			//{
			//	//syslog(LOG_ERR, "[%d] read() failed, ret = %d", inc_sockfd, ret);
			//	goto error3;
			//}

			//data_buffer[recv_len++] = c;
		
			//if(c == '\n')
			//{
			//	syslog(LOG_INFO, "[%d] data: %s data_buffer_len: %d", inc_sockfd, data_buffer, recv_len);
			//	break;
			//}
			ret = recv(inc_sockfd,
				&data_buffer[recv_len],
			 	(data_buffer_size - recv_len), flags);
			if(ret <= 0)
			{
			 	syslog(LOG_ERR, "[%d] recv() failed, ret = %d", inc_sockfd, ret);
			 	goto error3;
			}
			else
			{
			 	//syslog(LOG_INFO, "[%d] recv(), ret = %d", inc_sockfd, ret);
			 	recv_len += ret;
			 	if(strstr(data_buffer, "\n") != NULL)
			 	{
			 		//syslog(LOG_INFO, "[%d] data: %s\n data_buffer_len: %d", inc_sockfd, data_buffer, recv_len);
			 		//syslog(LOG_INFO, "data_buffer: %s", data_buffer);
			 		break;
			 	}
			}
		}

		if(recv_len > 0 && strstr(data_buffer, "\n") != NULL)
		//if(recv_len > 0)
		{
			pthread_mutex_lock(&f_lock);
			// syslog(LOG_INFO, "[%d] writing received data to file", inc_sockfd);
			write_len = 0;
			fp = fopen(filepath, "a+");
			if(fp == NULL)
			{
				syslog(LOG_ERR, "open file %s failed", filepath);
						ret = -1;
						pthread_mutex_unlock(&f_lock);
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
					pthread_mutex_unlock(&f_lock);
					goto error3;
				}
				write_len += ret;
			}

			file_size = ftell(fp);
			rewind(fp);
			syslog(LOG_INFO, "[%d] file_size = %d", inc_sockfd, file_size);
			
			while(file_size > 0)
			{
				// syslog(LOG_INFO, "[%d] reading file comtents", inc_sockfd);
				read_len = 0;
				memset(data_buffer, 0, data_buffer_size);
				ret = fread(data_buffer,
						sizeof(char),
						(data_buffer_size < file_size)? data_buffer_size:file_size,
						fp);
				if(ret <= 0)
				{
					syslog(LOG_ERR, "fread() failed, ret = %d", ret);
					fclose(fp);
					pthread_mutex_unlock(&f_lock);
					goto error3;
				}
				read_len = ret;
				file_size -= read_len;

				send_len = 0;
				while(send_len < read_len)
				{
					// syslog(LOG_INFO, "[%d] sending file contents", inc_sockfd);
					ret = send(inc_sockfd,
							&data_buffer[send_len],
							read_len - send_len,
								flags);
					if(ret <= 0)
					{
						syslog(LOG_ERR, "send() failed, ret = %d", ret);
						fclose(fp);
						pthread_mutex_unlock(&f_lock);
						goto error3;
					}
					send_len += ret;
				}

				if(send_len != read_len)
				{
					syslog(LOG_ERR, "send_len != read_len");
					fclose(fp);
					pthread_mutex_unlock(&f_lock);
					goto error3;
				}
			}

			fclose(fp);
			pthread_mutex_unlock(&f_lock);
		}
	}

error3:
	close(inc_sockfd);

	if(data_buffer != NULL)
	{
		free(data_buffer);
	}

	syslog(LOG_INFO, "Exiting thread for client %d.", inc_sockfd);
	//pthread_exit(NULL);
	return 0;

}

