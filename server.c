#include "lib.h"
#define MAX_VEHICLES 100
#define CHECKUP_INTERVAL 3
#define LOG "server"

struct sockaddr_in veh[MAX_VEHICLES];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *checkup_worker(void *arg)
{
    int i, j, sock;
    int port = *(int *)arg;
    struct sockaddr_in copy_veh[MAX_VEHICLES];
    struct sockaddr_in zero_veh;
    socklen_t length = sizeof(struct sockaddr_in);
    int32_t buf[3];

    memset(&zero_veh, 0, length);

    for (;;)
    {
        sleep(CHECKUP_INTERVAL);
        printf("Waking up\n");
        PTHREAD_MUTEX_LOCK_ERR(&mutex);
        memcpy(copy_veh, veh, length * MAX_VEHICLES);
        PTHREAD_MUTEX_UNLOCK_ERR(&mutex);

        for (i = 0; i < MAX_VEHICLES; i++)
        {
            if (memcmp(&(copy_veh[i]), &zero_veh, length) > 0)
            {
                sock = bind_inet_socket(port, SOCK_STREAM, 1);
                connect(sock, &copy_veh[i], length);
                read(sock, buf, sizeof(int32_t) * 3);
                close(sock);
                for (j = 0; j < 3; j++)
                    buf[j] = ntohl(buf[j]);
                printf("Coords");
                //TODO logging coords to file
            }
        }
    }
}

void usage()
{
    printf("Usage: ./server [port]\n");
}

/* MESSAGE HANDLERS */

void comm_register_handler(int client, uint32_t *msg, uint32_t length)
{

}

void comm_remove_handler(int client, uint32_t *msg, uint32_t length)
{

}

void comm_get_log_handler(int client, uint32_t *msg, uint32_t length)
{

}

void comm_req_compute_handler(int client, uint32_t *msg, uint32_t length)
{

}

void comm_get_compute_handler(int client, uint32_t *msg, uint32_t length)
{

}
/* END OF MESSAGE HANDLERS */

void handle_msg(int client, uint32_t *msg, uint32_t length)
{
    uint32_t type = ntohl(msg[1]);
    switch (type)
    {
    case COMM_REGISTER:
        comm_register_handler(client, msg, length);
        break;
    case COMM_REMOVE:
        comm_remove_handler(client, msg, length);
        break;
    case COMM_GET_LOG:
        comm_get_log_handler(client, msg, length);
        break;
    case COMM_REQ_COMPUTE:
        comm_req_compute_handler(client, msg, length);
        break;
    case COMM_GET_COMPUTE:
        comm_get_compute_handler(client, msg, length);
        break;
    default:
        print_log(LOG, "Unknown message type: %d", type);
        break;
    }
}

void do_work(int socket)
{
    int client;
    uint32_t buf[MAX_MESSAGE_LENGTH];
    uint32_t length;

    for (;;)
    {
        client = accept(socket, NULL, NULL);
        read(client, (uint32_t*)&buf, UINT32_S);
        length = ntohl(buf[0]);
        if (length > 1 && length <= MAX_MESSAGE_LENGTH)
        {
            uint32_t to_read = (length - 1) * UINT32_S;
            uint32_t bytes_read;
            if (to_read != (bytes_read = read(client, (&buf) + 1, to_read)))
                print_log(LOG, "ERR truncated message, read %d of %d",
                          bytes_read, to_read);
            else
                handle_msg(client, buf, length);

        }
        else
        {
            print_log(LOG, "WARN Got message of invalid legth %d", length);
        }
        close_conn(client);
    }
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

    int checkup_port = port + 1;
    pthread_create(&checkup, NULL, checkup_worker, &checkup_port);

    socket = bind_inet_socket(port, SOCK_STREAM, 0);

    do_work(socket);

    return 0;
}