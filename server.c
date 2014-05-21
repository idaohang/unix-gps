#include "lib.h"
#define MAX_VEHICLES 100
#define CHECKUP_INTERVAL 3

struct sockaddr_in veh[MAX_VEHICLES];
int logs[MAX_VEHICLES];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *checkup_worker(void *arg)
{
    int i,j, sock;
    int port=*(int*)arg;
    struct sockaddr_in copy_veh[MAX_VEHICLES];
    struct sockaddr_in zero_veh;
    socklen_t length=sizeof(struct sockaddr_in);
    int32_t buf[3];

    memset(&zero_veh, 0, length);

    for (;;)
    {
        sleep(CHECKUP_INTERVAL);
        printf("Waking up\n");
        PTHREAD_MUTEX_LOCK_ERR(&mutex);
        memcpy(copy_veh, veh, length*MAX_VEHICLES);
        PTHREAD_MUTEX_UNLOCK_ERR(&mutex);

        for (i = 0; i < MAX_VEHICLES; i++)
        {
            if (memcmp(&(copy_veh[i]),&zero_veh, length)>0)
            {   
                sock=bind_inet_socket(port, SOCK_STREAM, 1);
                connect(sock, &copy_veh[i], length);
                read(sock, buf, sizeof(int32_t)*3);
                close(sock);
                for(j=0; j<3; j++)
                    buf[j]=ntohl(buf[j]);
                printf("Coords");

            }
        }
    }
}

void usage()
{
    printf("Usage: ./server [port]\n");
}

int main(int argc, char const *argv[])
{
    int port, socket;
    pthread_t checkup;

              if ((port = port_from_args(argc, argv)) < 0)
    {
        if (port == -1)
            usage();
        else if (port == -2)
            fprintf(stderr, "Invalid port number. Port must be between %d and %d.\n", MIN_PORT, MAX_PORT);
        return -1;
    }
    memset(veh, 0, sizeof(struct sockaddr_in)*MAX_VEHICLES);
    memset(logs, 0, sizeof(int)*MAX_VEHICLES);

    int checkup_port=port+1;
    pthread_create(&checkup, NULL, checkup_worker,&checkup_port);

    socket = bind_inet_socket(port, SOCK_STREAM, 0);

    return 0;
}