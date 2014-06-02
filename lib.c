#include "lib.h"

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
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    fprintf(f, "[%d](%d:%d:%d.%d): ", getpid(), tm.tm_hour, tm.tm_min, tm.tm_sec, (ts.tv_nsec / 1000));

    va_start(arglist, format);
    vfprintf(f, format, arglist);
    va_end(arglist);

    fprintf(f, "\n");

    fclose(f);
}