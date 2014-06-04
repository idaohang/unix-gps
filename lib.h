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
#include <stdarg.h>
#include <inttypes.h>
#include <assert.h>

#define ERR(source) (fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
                     perror(source),kill(0,SIGKILL),\
                     exit(EXIT_FAILURE))

#define PTHREAD_MUTEX_LOCK_ERR(mutex) if(pthread_mutex_lock(mutex)!=0) ERR("pthread_mutex_lock");
#define PTHREAD_MUTEX_UNLOCK_ERR(mutex) if(pthread_mutex_unlock(mutex)!=0) ERR("pthread_mutex_unlock");


#define MIN_PORT 1001
#define MAX_PORT 9999

/* GPS definitions */
#define MIN_LATITUDE -9000
#define MAX_LATITUDE 9000
#define MIN_LONGITUDE -18000
#define MAX_LONGITUDE 18000

/* Client-server message types */
#define COMM_INVALID 0
#define COMM_REGISTER 1
#define COMM_REMOVE 2
#define COMM_GET_LOG 3
#define COMM_REQ_COMPUTE 4
#define COMM_GET_COMPUTE 5
#define COMM_SUCCESS 6
#define COMM_FAILURE 7
#define COMM_COMPUTING 8
#define COMM_VEH_EXISTS 9
#define COMM_VEH_NEXISTS 10
#define COMM_LONGEST 11
#define COMM_LOG 12
#define COMM_NO_COMPUTATION 13
#define COMM_FULL 14


/* Error codes */
#define GPS_ERR_GENERIC -1
#define GPS_ERR_INVALID_MSG -2

/* Max int count in message */
#define MAX_MESSAGE_LENGTH 1000

#define ADDR_STR_LEN 21

#define UINT32_S sizeof(int32_t)

int sethandler(void (*f)(int), int sigNo);
int make_socket(int domain, int type);
int bind_inet_socket(uint16_t port, int type, int active);
int randb(int min, int max);
int port_from_args(int argc, char const *argv[]);
char *addrtostr(struct sockaddr_in addr, char *buf, size_t size);
void print_log(char *name, const char *format, ... );
void close_conn(int socket);
int sprintmsg(char *str, size_t size, uint32_t *msg, uint32_t count);
struct sockaddr_in ip_to_sockaddr(const char* address, size_t size);
void sockaddr_to_ip(char* str, size_t size, struct sockaddr_in address);
int sockaddr_cmp(struct sockaddr_in a, struct sockaddr_in b);
struct sockaddr_in sockaddr_create(uint32_t ip, uint16_t port);
