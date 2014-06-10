#include "lib.h"
#include <math.h>
#define MAX_VEHICLES 100
#define MAX_COMPUTATIONS 100
#define CHECKUP_INTERVAL 3
#define LOG_NAME "server"
#define LOG_DIR "logs"
#define LOG_SEPARATOR ":"
#define MSG_DISP_LEN 100
#define LOG_LINE_LEN 20
#define PATH_LEN 50

struct sockaddr_in veh[MAX_VEHICLES];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t log_mutexes[MAX_VEHICLES];

pthread_t comps[MAX_COMPUTATIONS];
bool comp_slots[MAX_COMPUTATIONS];


struct comp_result
{
    struct sockaddr_in veh;
    double distance;
};

int search_veh(struct sockaddr_in v)
{
    int i;
    for (i = 0; i < MAX_VEHICLES; i++)
        if (sockaddr_cmp(v, veh[i]) == 0)
            return i;
    return -1;
}



FILE *open_log(struct sockaddr_in veh_addr, int index, const char *mode)
{
    if(index>=MAX_VEHICLES)
    {
        fprintf(stderr, "Tried to access invalid index %d\n", index);
        exit(-1);
    }

    FILE *ret;
    char path[PATH_LEN];
    char addr_str[ADDR_STR_LEN];
    if (mkdir(LOG_DIR, S_IRWXU | S_IRWXG | S_IRWXO) != 0)
        if (errno != EEXIST)
            ERR("mkdir " LOG_DIR);
    sockaddr_to_ip(addr_str,ADDR_STR_LEN, veh_addr);
    snprintf(path, PATH_LEN, LOG_DIR "/%s", addr_str);
    PTHREAD_MUTEX_LOCK_ERR(&log_mutexes[index]);
    if((ret=fopen(path, mode))==NULL)
    {
        PTHREAD_MUTEX_UNLOCK_ERR(&log_mutexes[index]);
        return NULL;
    }
    else
    {
        return ret;
    }
}

int close_log(FILE *f, int index)
{
    PTHREAD_MUTEX_UNLOCK_ERR(&log_mutexes[index]);
    return fclose(f);
}

void *compute_worker(void *arg)
{

    struct comp_result *ret;
    if((ret=malloc(sizeof(struct comp_result)))==NULL)
        ERR("malloc");
    memset(ret, 0, sizeof(struct comp_result));
    struct sockaddr_in copy_veh[MAX_VEHICLES];
    struct sockaddr_in zero_veh;
    memset(&zero_veh, 0, sizeof(ret));
    int length=sizeof(struct sockaddr_in);

    /* Create a local copy of the vehicle table */
    PTHREAD_MUTEX_LOCK_ERR(&mutex);
    memcpy(copy_veh, veh, length * MAX_VEHICLES);
    PTHREAD_MUTEX_UNLOCK_ERR(&mutex);

    /* Iterate through the table */
    int i, lat, lng;
    char line[LOG_LINE_LEN];
    for(i=0; i<MAX_VEHICLES; i++)
    {
        if(sockaddr_cmp(copy_veh[i], zero_veh)!=0)
        {
            FILE *f=open_log(copy_veh[i],i, "r");
            if(f==NULL)
            {
                if(errno==ENOENT)
                {
                    /* The vehicle was probably removed
                       in the meantime */
                    continue;
                }
                else
                {
                    ERR("open_log");
                }
            }
            double curr_distance=0;
            ret->distance=0;
            int lat_last=0, lng_last=0;

            /* Process the vehicle log */
            if(fgets(line, LOG_LINE_LEN, f)==NULL)
                continue;
            sscanf(line, "%d" LOG_SEPARATOR "%d", &lat_last, &lng_last);
            int scanned;
            while(fgets(line, LOG_LINE_LEN, f)!=NULL)
            {
                if((scanned=sscanf(line, "%d" LOG_SEPARATOR "%d", &lat, &lng))!=2)
                    break;
                curr_distance+=sqrt(pow(lat-lat_last,2)+pow(lng-lng_last,2));
            }
            if(close_log(f,i)!=0)
                ERR("close_log");
            if(curr_distance>(ret->distance))
            {
                ret->distance=curr_distance;
                ret->veh=copy_veh[i];
            }
        }
    }

    return ret;
}

void *checkup_worker(void *arg)
{

    int i, j, sock, lat, lng;
    FILE *logfile;
    struct sockaddr_in copy_veh[MAX_VEHICLES];
    struct sockaddr_in zero_veh;
    socklen_t length = sizeof(struct sockaddr_in);
    uint32_t buf[3];
    char addr_str[ADDR_STR_LEN];

    memset(&zero_veh, 0, length);
    for (;;)
    {
        sleep(CHECKUP_INTERVAL);
        printf("Waking up!\n");

        /* Make a local copy of the vehicles table */
        PTHREAD_MUTEX_LOCK_ERR(&mutex);
        memcpy(copy_veh, veh, length * MAX_VEHICLES);
        PTHREAD_MUTEX_UNLOCK_ERR(&mutex);

        /* Gather info from vehicles in table */
        for (i = 0; i < MAX_VEHICLES; i++)
        {
            if (sockaddr_cmp(copy_veh[i], zero_veh) != 0)
            {
                sock = bind_inet_socket(0, SOCK_STREAM, 1);
                sockaddr_to_ip(addr_str, MSG_DISP_LEN, copy_veh[i]);
                printf("Connecting to %s ...\n", addr_str);
                if(connect(sock, &copy_veh[i], length))
                {
                    fprintf(stderr, "WARN Could not connect to %s\n", addr_str);
                    continue;
                }

                /* Get the coords */
                int bytes_read;
                if((bytes_read=bulk_read(sock, buf, UINT32_S * 3))<=0)
                {
                    if(bytes_read==0)
                        fprintf(stderr, "WARN no message from %s\n", addr_str);
                    continue;
                }
                if(close(sock)!=0)
                    ERR("close");

                /* Process the coords */
                for (j = 0; j < 3; j++)
                    buf[j] = ntohl(buf[j]);
                lat = buf[1] + MIN_LATITUDE;
                lng = buf[2] + MIN_LONGITUDE;

                if(lat<MIN_LATITUDE||lat>MAX_LATITUDE||
                    lng<MIN_LONGITUDE||lng>MAX_LONGITUDE)
                {
                    fprintf(stderr, "WARN got invalid coords from %s\n", addr_str);
                    continue;
                }

                /* Append to log */
                if ((logfile = open_log(copy_veh[i],i,"a")) == NULL)
                    ERR("open_log");
                fprintf(logfile, "%d" LOG_SEPARATOR "%d\n", lat, lng);
                if(close_log(logfile, i)!=0)
                    ERR("close_log");

                printf("Coords retrieved: %d, %d\n", lat, lng);
            }
        }
        printf("Going to sleep...\n");
    }
}

void usage(int argc, char const *argv[])
{
    printf("Usage: ./%s [port]\n",argv[0]);
}

void reset_log(struct sockaddr_in veh_addr, int index)
{
    FILE *f;
    if((f=open_log(veh_addr, index, "w"))==NULL)
        ERR("open_log");
    if(close_log(f, index)!=0)
        ERR("close_log");
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
        reset_log(veh[empty_index], empty_index);
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

    int i;
    PTHREAD_MUTEX_LOCK_ERR(&mutex);
    if ((i = search_veh(veh_addr)) == -1)
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
    struct sockaddr_in veh_addr = sockaddr_create(
                                      msg[2], (uint16_t)msg[3]);
    int i, pos, lat, lng;
    char line[LOG_LINE_LEN];
    if ((i = search_veh(veh_addr)) == -1)
    {
        response[1] = htonl(COMM_VEH_NEXISTS);
    }
    else
    {
        FILE *logfile;
        if((logfile=open_log(veh_addr, i,"r"))==NULL)
        {
            char addr_str[ADDR_STR_LEN];
            sockaddr_to_ip(addr_str, ADDR_STR_LEN, veh_addr);
            fprintf(stderr, "WARN No log exists for vehicle %s\n", addr_str);
            response[1]=htonl(COMM_FAILURE);
        }else
        {
            pos=2;
            while(pos<MAX_MESSAGE_LENGTH-1&&
                (fgets(line, LOG_LINE_LEN, logfile)!=NULL))
            {
                if(sscanf(line, "%d" LOG_SEPARATOR "%d", &lat, &lng)!=2)
                    break;
                response[pos]=htonl(lat-MIN_LATITUDE);
                response[pos+1]=htonl(lng-MIN_LONGITUDE);
                pos+=2;
            }
            if(close_log(logfile, i)!=0)
                ERR("close_log");
            response[0]=htonl(pos);
            response[1]=htonl(COMM_LOG);
        }
    }
    return 0;
}

int comm_req_compute_handler(uint32_t *msg, uint32_t *response)
{
    printf("Got compute request\n");
    uint32_t i;
    for(i=0;i<MAX_COMPUTATIONS;i++)
        if(comp_slots[i]==true)
            break;
    if(i==MAX_COMPUTATIONS)
    {
        response[1]=htonl(COMM_COMPUTATIONS_FULL);
    }else
    {
        pthread_create(&comps[i], NULL, compute_worker, NULL);
        comp_slots[i]=false;
        response[0]=htonl(3);
        response[1]=htonl(COMM_COMPUTATION_TOKEN);
        response[2]=htonl(i);
    }
    return 0;
}

int comm_get_compute_handler(uint32_t *msg, uint32_t *response)
{
    printf("Got compute result request\n");

    uint32_t token=ntohl(msg[2]);
    if(token>=MAX_COMPUTATIONS || comp_slots[token]==true)
    {
        response[1]=htonl(COMM_NO_COMPUTATION);
        return 0;
    }
    struct comp_result *res;
    if((errno=pthread_tryjoin_np(comps[token], (void**)&res))!=0)
    {
        if(errno==EBUSY)
        {
            response[1]=htonl(COMM_COMPUTING);
            return 0;
        }else
        {
            ERR("pthread_tryjoin_np");
        }
    }

    comp_slots[token]=true;

    response[0]=htonl(5);
    response[1]=htonl(COMM_COMPUTATION_RESULT);
    response[2]=htonl(res->veh.sin_addr.s_addr);
    response[3]=htons(res->veh.sin_port);
    response[4]=htonl((uint32_t)res->distance);

    free(res);

    return 0;
}
/* END OF MESSAGE HANDLERS */

int handle_msg(uint32_t *msg, uint32_t *response)
{
    uint32_t type = ntohl(msg[1]);

    char str[MSG_DISP_LEN];
    if(sprintmsg(str, MSG_DISP_LEN, msg, 8)<0)
        fprintf(stderr, "Got message: cannot display\n");
    else
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
    struct sockaddr_in addr;
    socklen_t addr_size=sizeof(addr);
    char addr_str[ADDR_STR_LEN];

    for (;;)
    {
        printf("Waiting for connection...\n");
        client = accept(socket, &addr, &addr_size);
        sockaddr_to_ip(addr_str, ADDR_STR_LEN, addr);
        printf("Connection from %s !\n", addr_str);

        /* Prepare default response */
        response[0] = htonl(2);
        response[1] = htonl(COMM_FAILURE);

        int bytes_read;
        if((bytes_read=bulk_read(client, buf, UINT32_S))<=0)
        {
            if(bytes_read==0)
            {
                fprintf(stderr, "WARN Got nothing from %s\n", addr_str);
                close_conn(client);
                continue;
            }else
            {
                ERR("bulk_read");
            }
        }
        length = ntohl(buf[0]);
        if (length > 1 && length <= MAX_MESSAGE_LENGTH)
        {
            uint32_t to_read = (length - 1) * UINT32_S;
            if (to_read != (bytes_read = bulk_read(client, buf + 1, to_read)))
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
        length = ntohl(response[0]);
        if(bulk_write(client, response, length * UINT32_S)<0)
            fprintf(stderr, "Error with writing\n");
        close_conn(client);
        printf("End of connection\n");
    }
}



void init_mutexes()
{
    int i;
    for (i = 0; i < MAX_VEHICLES; i++)
    {
        errno=pthread_mutex_init(&(log_mutexes[i]), NULL);
        if(errno!=0)
            ERR("pthread_mutex_init");
    }
    return;
}

int main(int argc, char const *argv[])
{
    int port, socket,i;
    pthread_t checkup;

    if ((port = port_from_args(argc, argv)) < 0)
    {
        if (port == -1)
            usage(argc, argv);
        else if (port == -2)
            fprintf(stderr, "Invalid port number. Port must be between %d and %d.\n", MIN_PORT, MAX_PORT);
        return -1;
    }

    /* Initializing static tables */
    memset(veh, 0, sizeof(struct sockaddr_in)*MAX_VEHICLES);
    memset(comps, 0, sizeof(pthread_t)*MAX_COMPUTATIONS);
    for(i=0;i<MAX_COMPUTATIONS;i++)
        comp_slots[i]=true;

    init_mutexes();
    if((errno=pthread_create(&checkup, NULL, checkup_worker, NULL))!=0)
        ERR("pthread_create");

    printf("Binding socket on port %d...\n", port);
    socket = bind_inet_socket(port, SOCK_STREAM, 0);

    do_work(socket);

    return 0;
}