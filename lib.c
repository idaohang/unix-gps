#include "lib.h"

ssize_t bulk_read(int fd, void *buf, size_t count){
    int c;
    size_t len=0;
    do{
        c=read(fd,buf,count);
        if(c<0){
            if(EINTR==errno) continue;
            return c;
        }
        if(0==c) return len;
        buf+=c;
        len+=c;
        count-=c;
    }while(count>0);
    return len ;
}

ssize_t bulk_write(int fd, const void *buf, size_t count){
    int c;
    size_t len=0;
    do{
        c=TEMP_FAILURE_RETRY(write(fd,buf,count));
        if(c<0) return c;
        buf+=c;
        len+=c;
        count-=c;
    }while(count>0);
    return len ;
}

int sethandler( void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        return -1;
    return 0;
}

int make_socket(int domain, int type)
{
    int sock;
    sock = socket(domain, type, 0);
    if (sock < 0) ERR("socket");
    return sock;
}

int bind_inet_socket(uint16_t port, int type, int active)
{
    struct sockaddr_in addr;
    int socketfd, t = 1;
    socketfd = make_socket(PF_INET, type);
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t))) ERR("setsockopt");
    if (bind(socketfd, (struct sockaddr *) &addr, sizeof(addr)) < 0)  ERR("bind");
    if (SOCK_STREAM == type && active == 0)
        if (listen(socketfd, 100) < 0) ERR("listen");
    return socketfd;
}

int randb(int min, int max)
{
    return (rand() % (max - min)) + min;
}

int port_from_args(int argc, char const *argv[])
{
    int port;
    if (argc == 1)
    {
        port = randb(MIN_PORT, MAX_PORT);
        return port;
    }
    else if (argc == 2)
    {
        port = atoi(argv[1]);
        if (port < MIN_PORT || port > MAX_PORT)
            return -2;
        else
            return port;

    }
    else
    {
        return -1;
    }
}

char *addrtostr(struct sockaddr_in addr, char *buf, size_t size)
{
    char address[20];
    inet_ntop(AF_INET, &(addr.sin_addr), address, 20);
    snprintf(buf, size, "%s:%d", address, ntohs(addr.sin_port));
    return buf;
}

void print_log(char *name, const char *format, ... )
{
    va_list arglist;
    char buf[50];
    sprintf(buf, "%s.txt", name);
    FILE *f = fopen(buf, "a+");
    if (f == NULL)
    {
        printf("LOG: Error opening file!\n");
        exit(1);
    }

    /* Getting time */
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    struct timeval tv;
    if(gettimeofday(&tv, NULL)!=0)
        ERR("gettimeofday");

    fprintf(f, "[%d](%d:%d:%d.%ld): ", getpid(), tm.tm_hour, tm.tm_min,
            tm.tm_sec, (tv.tv_usec / 1000));

    va_start(arglist, format);
    vfprintf(f, format, arglist);
    va_end(arglist);

    fprintf(f, "\n");

    if(fclose(f)!=0)
        ERR("fclose");
}

void close_conn(int socket)
{
    if(shutdown(socket, SHUT_WR)!=0)
    {
        if(errno==ENOTCONN)
            fprintf(stderr, "WARN Attempted to shutdown non-connected socket\n");
        else
            ERR("shutdown");
    }
    while (read(socket, NULL, 0) != 0);
}

/* Message field bytes should be in network order. */
int sprintmsg(char *str, size_t size, uint32_t *msg, uint32_t count)
{
    if (msg == NULL)
        return GPS_ERR_GENERIC;
    uint32_t len = ntohl(msg[0]);
    if (len < 1 || len > MAX_MESSAGE_LENGTH)
        return GPS_ERR_INVALID_MSG;
    if (len < count)
        count = len;
    if (count > MAX_MESSAGE_LENGTH)
        count = MAX_MESSAGE_LENGTH;

    int i, cur = 0;
    for (i = 0; i < count; i++)
        cur += snprintf(str + cur, size - cur, "%" PRIu32 ":", ntohl(msg[i]));
    if (len > count)
        snprintf(str + cur, size - cur, "...");
    return 0;
}

/* Converts ddd.ddd.ddd.ddd:port to sockaddr */
int ip_to_sockaddr(const char *address, size_t size, struct sockaddr_in *addr)
{
    char *ip;
    if((ip=malloc(size))==NULL)
        ERR("malloc");
    char port_str[6];
    uint16_t port;
    strncpy(ip, address, size);
    char *delim;
    if((delim = strtok(ip, ":"))==NULL)
        return -1;
    if((delim = strtok(NULL, ":"))==NULL)
        return -1;
    strncpy(port_str, delim, 6);
    if(sscanf(port_str, "%" SCNu16 "", &port)<1)
        return -1;

    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    if(inet_aton(ip, &(addr->sin_addr))==0)
        return -1;
    free(ip);
    return 0;
}

/* Converts sockaddr to ddd.ddd.ddd.ddd:port */
void sockaddr_to_ip(char *str, size_t size, struct sockaddr_in address)
{
    char *ip = inet_ntoa(address.sin_addr);
    snprintf(str, size, "%s:%" PRIu16 "", ip, ntohs(address.sin_port));

    return;
}

int sockaddr_cmp(struct sockaddr_in a, struct sockaddr_in b)
{
    if (a.sin_family == b.sin_family &&
            a.sin_port == b.sin_port &&
            a.sin_addr.s_addr == b.sin_addr.s_addr &&
            memcmp(a.sin_zero, b.sin_zero, 8) == 0)
        return 0;
    else
        return -1;
}

struct sockaddr_in sockaddr_create(uint32_t ip, uint16_t port)
{
    struct sockaddr_in ret;
    memset(&ret, 0, sizeof(ret));
    ret.sin_family = AF_INET;
    ret.sin_port = port;
    ret.sin_addr.s_addr = ip;
    return ret;
}

void sleep_solid(unsigned int seconds)
{
    unsigned int left = seconds;
    do
    {
        left = sleep(left);
    }
    while (left > 0);
}