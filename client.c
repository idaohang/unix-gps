#include "lib.h"

void usage(int argc, char const *argv[])
{
    fprintf(stderr, "Usage: %s command [args] <server address>\n", argv[0]);
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "    reg <address> - register a vehicle\n");
    fprintf(stderr, "    rm <address> - remove a vehicle\n");
    fprintf(stderr, "    log <address> - get vehicle log\n");
    fprintf(stderr, "    comp - compute which vehicle has longest route\n");
    fprintf(stderr, "    get <token> - get computations result or status\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Type IP addresses in aaa.bbb.ccc.ddd:port form.\n");
}

int print_log_msg(uint32_t *response)
{
    int count = ntohl(response[0]);
    if (count > MAX_MESSAGE_LENGTH)
    {
        fprintf(stderr, "Invalid message length: %d\n", count);
        return -1;
    }
    int lat, lng, i;
    for (i = 2; i < count; i += 2)
    {
        lat = MIN_LATITUDE + ntohl(response[i]);
        lng = MIN_LONGITUDE + ntohl(response[i + 1]);
        printf("LAT: %d LNG: %d\n", lat, lng);
    }
    return (i - 2) / 2;
}

int send_and_recv(int sock, struct sockaddr_in addr, uint32_t *msg, uint32_t *response)
{
    ssize_t read;
    uint32_t count = ntohl(msg[0]);
    printf("Connecting...\n");
    if (connect(sock, &addr, sizeof(addr)) == -1)
        ERR("connect");
    printf("Connected!\n");
    /* Sending */
    if (bulk_write(sock, msg, count * UINT32_S) < 0)
        ERR("bulk_write");
    /* Waiting for response */
    read = bulk_read(sock, response, UINT32_S);
    if (read <= 0)
    {
        if(read==0)
            fprintf(stderr, "Server closed connection without response.\n");
        return -1;
    } 


    count = ntohl(response[0]);
    if (count > 1 && count <= MAX_MESSAGE_LENGTH)
    {
        uint32_t to_read = (count - 1) * UINT32_S;
        uint32_t bytes_read;
        if (to_read != (bytes_read = bulk_read(sock, response + 1, to_read)))
        {
            fprintf(stderr, "ERR truncated message, read %d of %d",
                    bytes_read, to_read);
            return -1;
        }
        return 0;
    }
    else
    {
        return -1;
    }
}

int main(int argc, char const *argv[])
{
    if (argc < 3)
    {
        usage(argc, argv);
        return EXIT_FAILURE;
    }

    int sock = bind_inet_socket(0, SOCK_STREAM, 1);
    int command_index = 1;
    const char *command = argv[command_index];
    struct sockaddr_in server_addr;
    if (ip_to_sockaddr(argv[argc - 1], ADDR_STR_LEN, &server_addr) != 0)
    {
        fprintf(stderr, "Invalid server IP.\n");
        return EXIT_FAILURE;
    }

    uint32_t msg[MAX_MESSAGE_LENGTH];
    uint32_t response[MAX_MESSAGE_LENGTH];
    uint32_t temp;
    char addr_str[ADDR_STR_LEN];
    struct sockaddr_in temp_addr;

    /* Parse the command */
    if (strcmp(command, "reg") == 0)
    {
        /* Register */

        if (argc != 4)
        {
            usage(argc, argv);
            return EXIT_FAILURE;
        }
        msg[0] = htonl(4);
        msg[1] = htonl(COMM_REGISTER);
        if (ip_to_sockaddr(argv[command_index + 1], ADDR_STR_LEN, &temp_addr) != 0)
        {
            fprintf(stderr, "Invalid vehicle address.\n");
            return EXIT_FAILURE;
        }
        msg[2] = temp_addr.sin_addr.s_addr;
        msg[3] = (uint32_t)temp_addr.sin_port;
    }
    else if (strcmp(command, "rm") == 0)
    {
        /* Remove */

        if (argc != 4)
        {
            usage(argc, argv);
            return EXIT_FAILURE;
        }
        msg[0] = htonl(4);
        msg[1] = htonl(COMM_REMOVE);
        if (ip_to_sockaddr(argv[command_index + 1], ADDR_STR_LEN, &temp_addr) != 0)
        {
            fprintf(stderr, "Invalid vehicle address.\n");
            return EXIT_FAILURE;
        }
        msg[2] = temp_addr.sin_addr.s_addr;
        msg[3] = (uint32_t)temp_addr.sin_port;
    }
    else if (strcmp(command, "log") == 0)
    {
        /* Get log */

        if (argc != 4)
        {
            usage(argc, argv);
            return EXIT_FAILURE;
        }
        msg[0] = htonl(4);
        msg[1] = htonl(COMM_GET_LOG);
        if (ip_to_sockaddr(argv[command_index + 1], ADDR_STR_LEN, &temp_addr) != 0)
        {
            fprintf(stderr, "Invalid vehicle address.\n");
            return EXIT_FAILURE;
        }
        msg[2] = temp_addr.sin_addr.s_addr;
        msg[3] = (uint32_t)temp_addr.sin_port;
    }
    else if (strcmp(command, "comp") == 0)
    {
        /* Request computation */

        if (argc != 3)
        {
            usage(argc, argv);
            return EXIT_FAILURE;
        }
        msg[0] = htonl(2);
        msg[1] = htonl(COMM_REQ_COMPUTE);
    }
    else if (strcmp(command, "get") == 0)
    {
        /* Get computation result */

        if (argc != 4)
        {
            usage(argc, argv);
            return EXIT_FAILURE;
        }
        msg[0] = htonl(3);
        msg[1] = htonl(COMM_GET_COMPUTE);
        msg[2] = htonl((uint32_t)atoi(argv[2]));
    }
    else
    {
        /* Unknown command */

        usage(argc, argv);
        return EXIT_FAILURE;
    }


    /* Communication */
    if (send_and_recv(sock, server_addr, msg, response) < 0)
    {
        fprintf(stderr, "Communication failed.\n");
        return EXIT_FAILURE;
    }

    /* Process response */
    uint32_t type = ntohl(response[1]);
    char msg_str[200];
    printf("Server: ");
    switch (type)
    {
    case COMM_SUCCESS:
        printf("Operation successful!\n");
        break;

    case COMM_FAILURE:
        printf("Operation unsuccessful. Something went wrong.\n");
        break;

    case COMM_VEH_EXISTS:
        printf("Vehicle already registered.\n");
        break;

    case COMM_VEH_NEXISTS:
        printf("Vehicle not registered.\n");
        break;

    case COMM_LOG:
        printf("Got log:\n");
        print_log_msg(response);
        break;

    case COMM_NO_COMPUTATION:
        printf("No computation with this token or somebody "
               "already retrieved the result.\n");
        break;

    case COMM_FULL:
        printf("Can't register, full house.\n");
        break;

    case COMM_COMPUTATIONS_FULL:
        printf("Computations service at full load, try again later.\n");
        break;

    case COMM_COMPUTATION_TOKEN:
        temp = ntohl(response[2]);
        printf("Computation request posted successfully!\n");
        printf("Your token is: %" PRIu32 "\n", temp);
        printf("Please use this token to retrieve your result.\n");
        break;

    case COMM_COMPUTATION_RESULT:
        printf("Longest distance was traversed by vehicle with address: ");
        memset(&temp_addr, 0, sizeof(struct sockaddr_in));
        temp_addr.sin_addr.s_addr = ntohl(response[2]);
        temp_addr.sin_port = ntohs(response[3]);
        sockaddr_to_ip(addr_str, ADDR_STR_LEN, temp_addr);
        printf("%s\n", addr_str);
        printf("Distance traversed: about %d\n", ntohl(response[4]));
        break;

    default:
        sprintmsg(msg_str, 200, response, 10);
        printf("Got unrecognized message.\n"
               "First 10 fields of the message:\n"
               "%s", msg_str);
        break;
    }


    return 0;
}