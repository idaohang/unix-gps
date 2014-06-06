#include "lib.h"
#define MAX_VEHICLES 100
#define MAX_COMPUTATIONS 100
#define CHECKUP_INTERVAL 3
#define LOG_NAME "server"
#define LOG_DIR "logs"
#define LOG_SEPARATOR ":"
#define MSG_DISP_LEN 100
#define PATH_LEN 50

struct sockaddr_in veh[MAX_VEHICLES];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutexes[MAX_VEHICLES];

pthread_t comps[MAX_COMPUTATIONS];

typedef struct _comp_result
{
    struct sockaddr_in veh;
    uint32_t distance;
} comp_result;

void *checkup_worker(void *arg)
{
    int i, j, sock, lat, lng;
    FILE *logfile;
    struct sockaddr_in copy_veh[MAX_VEHICLES];
    struct sockaddr_in zero_veh;
    socklen_t length = sizeof(struct sockaddr_in);
    uint32_t buf[3];
    char addr_str[ADDR_STR_LEN], path[PATH_LEN];

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
            if (sockaddr_cmp(copy_veh[i], zero_veh) != 0)
            {
                sock = bind_inet_socket(0, SOCK_STREAM, 1);
                sockaddr_to_ip(addr_str, MSG_DISP_LEN, copy_veh[i]);
                printf("Connecting to %s ...\n", addr_str);
                connect(sock, &copy_veh[i], length);
                read(sock, buf, UINT32_S * 3);
                close(sock);
                for (j = 0; j < 3; j++)
                    buf[j] = ntohl(buf[j]);
                lat=buf[1]+MIN_LATITUDE;
                lng=buf[2]+MIN_LONGITUDE;

                /* Append to log */
                if(mkdir(LOG_DIR, S_IRWXU|S_IRWXG|S_IRWXO)!=0)
                    if(errno!=EEXIST)
                        ERR("mkdir " LOG_DIR);
                snprintf(path, PATH_LEN, LOG_DIR "/%s", addr_str);
                PTHREAD_MUTEX_LOCK_ERR(&log_mutexes[i]);
                if((logfile=fopen(path, "a"))==NULL)
                    ERR("fopen"); //TODO handle errors properly
                fprintf(logfile, "%d" LOG_SEPARATOR "%d\n", lat,lng);
                fclose(logfile);
                PTHREAD_MUTEX_UNLOCK_ERR(&log_mutexes[i]);

                printf("Coords retrieved: %d, %d", lat, lng);
            }
        }
        printf("Going to sleep...\n");
    }
}

void usage()
{
    printf("Usage: ./server [port]\n");
}

/* MESSAGE HANDLERS */

int comm_register_handler(uint32_t *msg, uint32_t *response)
{
    printf("Got register request\n");
    struct sockaddr_in veh_addr = sockaddr_create(
                                      msg[2], (uint16_t)msg[3]);
    struct sockaddr_in zero_addr;
    memset(&zero_addr, 0, sizeof(struct sockaddr_in));

    PTHREAD_MUTEX_LOCK_ERR(&mutex);
    int i = 0;
    int empty_index = -1;
    while (sockaddr_cmp(veh[i], veh_addr) != 0 && i < MAX_VEHICLES)
    {
        if (empty_index == -1 && sockaddr_cmp(veh[i], zero_addr) == 0)
            empty_index = i;
        i++;
    }
    if (i < MAX_VEHICLES)
    {
        response[1] = htonl(COMM_VEH_EXISTS);
    }
    else if (empty_index == -1)
    {
        response[1] = htonl(COMM_FULL);
    }
    else
    {
        veh[empty_index] = veh_addr;
        response[1] = htonl(COMM_SUCCESS);
    }
    PTHREAD_MUTEX_UNLOCK_ERR(&mutex);
    return 0;
}

int comm_remove_handler(uint32_t *msg, uint32_t *response)
{
    printf("Got remove request\n");
    struct sockaddr_in veh_addr = sockaddr_create(
                                      msg[2], (uint16_t)msg[3]);

    PTHREAD_MUTEX_LOCK_ERR(&mutex);
    int i = 0;
    while (sockaddr_cmp(veh[i], veh_addr) != 0 && i < MAX_VEHICLES)
        i++;
    if (i == MAX_VEHICLES)
    {
        response[1] = htonl(COMM_VEH_NEXISTS);
    }
    else
    {
        memset(veh + i, 0, sizeof(struct sockaddr_in));
        response[1] = htonl(COMM_SUCCESS);
    }
    PTHREAD_MUTEX_UNLOCK_ERR(&mutex);
    return 0;
}

int comm_get_log_handler(uint32_t *msg, uint32_t *response)
{
    printf("Got log request\n");
    return 0;
}

int comm_req_compute_handler(uint32_t *msg, uint32_t *response)
{
    printf("Got compute request\n");
    return 0;
}

int comm_get_compute_handler(uint32_t *msg, uint32_t *response)
{
    printf("Got compute result request");
    return 0;
}
/* END OF MESSAGE HANDLERS */

int handle_msg(uint32_t *msg, uint32_t *response)
{
    uint32_t type = ntohl(msg[1]);

    char str[MSG_DISP_LEN];
    sprintmsg(str, MSG_DISP_LEN, msg, 8);
    printf("Received message: %s\n", str);
    switch (type)
    {
    case COMM_REGISTER:
        return comm_register_handler(msg, response);
    case COMM_REMOVE:
        return comm_remove_handler(msg, response);
    case COMM_GET_LOG:
        return comm_get_log_handler(msg, response);
    case COMM_REQ_COMPUTE:
        return comm_req_compute_handler(msg, response);
    case COMM_GET_COMPUTE:
        return comm_get_compute_handler(msg, response);
    default:
        printf("Unknown message type: %d\n", type);
        return 0;
    }
}

void do_work(int socket)
{
    printf("Launching...\n");
    int client;
    uint32_t buf[MAX_MESSAGE_LENGTH];
    uint32_t response[MAX_MESSAGE_LENGTH];
    uint32_t length;

    for (;;)
    {
        printf("Waiting for connection...\n");
        client = accept(socket, NULL, NULL);
        printf("Connected!\n");

        /* Prepare default response */
        response[0] = htonl(2);
        response[1] = htonl(COMM_FAILURE);

        read(client, buf, UINT32_S);
        length = ntohl(buf[0]);
        if (length > 1 && length <= MAX_MESSAGE_LENGTH)
        {
            uint32_t to_read = (length - 1) * UINT32_S;
            uint32_t bytes_read;
            if (to_read != (bytes_read = read(client, buf + 1, to_read)))
                printf("ERR truncated message, read %d of %d",
                       bytes_read, to_read);
            else
            {
                handle_msg(buf, response);
            }
        }
        else
        {
            printf("WARN Got message of invalid length %d", length);
        }
        printf("Responding\n");
        length=ntohl(response[0]);
        write(client, response, length * UINT32_S);
        close_conn(client);
        printf("End of connection\n");
    }
}



void init_mutexes()
{
    int i;
    for(i=0; i<MAX_VEHICLES; i++)
    {
        pthread_mutex_init(&(log_mutexes[i]),NULL);
    }
    return;
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
    init_mutexes();
    pthread_create(&checkup, NULL, checkup_worker, NULL);

    printf("Binding socket on port %d...\n", port);
    socket = bind_inet_socket(port, SOCK_STREAM, 0);

    do_work(socket);

    return 0;
}