#include "lib.h"

#define MOVE_INTERVAL 1

int pos[2];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void usage(int argc, char const *argv[])
{
    fprintf(stderr, "Usage: ./%s [port]\n",argv[0]);
}

void *move_worker(void *arg)
{
    for (;;)
    {
        sleep_solid(MOVE_INTERVAL);
        PTHREAD_MUTEX_LOCK_ERR(&mutex);
        pos[0] += randb(-2, 2);
        pos[1] += randb(-2, 2);

        printf("%d, %d\n",pos[0],pos[1]);
        
        PTHREAD_MUTEX_UNLOCK_ERR(&mutex);
    }
}

void prepare_packet(uint32_t *buf)
{
    PTHREAD_MUTEX_LOCK_ERR(&mutex);
    buf[0]=htonl(3);
    buf[1]=htonl(pos[0]-MIN_LATITUDE);
    buf[2]=htonl(pos[1]-MIN_LONGITUDE);
    PTHREAD_MUTEX_UNLOCK_ERR(&mutex);
}

int main(int argc, char const *argv[])
{
    srand(time(NULL)*getpid());
    int socket,client, port;
    pthread_t t;
    uint32_t buf[3];

    if((port=port_from_args(argc, argv))<0)
    {
        if(port==-1)
            usage(argc, argv);
        else if(port==-2)
            fprintf(stderr, "Invalid port number. Port must be between %d and %d.\n", MIN_PORT, MAX_PORT);
        return -1;
    }

    pos[0] = randb(MIN_LATITUDE, MAX_LATITUDE);
    pos[1] = randb(MIN_LONGITUDE, MAX_LONGITUDE);

    printf("Binding socket on port %d...\n", port);
    socket = bind_inet_socket(port, SOCK_STREAM,0);
    printf("Socket bound\n");

    /* Launching thread for changing position */
    if((errno=pthread_create(&t, NULL, move_worker, NULL))!=0)
        ERR("pthread_create");


    for(;;)
    {
    	client=accept(socket, NULL,NULL);
        printf("Connection accepted!\n");
        prepare_packet(buf);
        bulk_write(client, buf, sizeof(buf));
        close_conn(client);
        printf("Sent successfully.\n");
        close(client);

    }


    return 0;
}