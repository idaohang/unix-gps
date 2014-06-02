#define _GNU_SOURCE 
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <arpa/inet.h>

#define ERR(source) (fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
                     perror(source),kill(0,SIGKILL),\
		     		     exit(EXIT_FAILURE))

#define PTHREAD_MUTEX_LOCK_ERR(mutex) if(pthread_mutex_lock(mutex)!=0) ERR("pthread_mutex_lock");
#define PTHREAD_MUTEX_UNLOCK_ERR(mutex) if(pthread_mutex_unlock(mutex)!=0) ERR("pthread_mutex_unlock");

#define MIN_PORT 1001
#define MAX_PORT 9999
#define MIN_LATITUDE -9000
#define MAX_LATITUDE 9000
#define MIN_LONGITUDE -18000
#define MAX_LONGITUDE 18000

#define COMM_REGISTER 1
#define COMM_REMOVE 2
//TODO MORE

int sethandler(void (*f)(int), int sigNo);
int make_socket(int domain, int type);
int bind_inet_socket(uint16_t port, int type, int active);
int randb(int min, int max);
int port_from_args(int argc, char const *argv[]);
char* addrtostr(struct sockaddr_in addr, char* buf, size_t size);