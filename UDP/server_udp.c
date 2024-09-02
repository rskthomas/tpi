#define _XOPEN_SOURCE 600
#include "server_udp.h"
#include <netdb.h>
#include <time.h>

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }

    // check if port is a number from 1024 to 65535
    if (atoi(argv[1]) < 1024 || atoi(argv[1]) > 65535)
    {
        fprintf(stderr, "ERROR, invalid port number\n");
        exit(1);
    }
    char *portn = argv[1];

    /*
     *  un buffer para el archivo que se aloca cuando se recive el primer datagrama
     *  un struct de tipo response para enviar la respuesta al cliente
     * un struct de tipo file_page para recibir los datagramas
     * un struct de tipo file_info para recibir la informacion del archivo
     */

    /*
     * Networking variables
     */
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    socklen_t addr_len;

    /*
     * Application variables
     */
    struct file_info file_info;
    struct file_page file_page;
    struct response response;

    char *file_buf;

    printf("Servidor corriendo en puerto %s. ", portn);
    /*
     * 1. Create a socket
     */
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, portn, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }


    //bind on first result
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            perror("listener: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            pclose(sockfd);
            perror("listener: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }

    freeaddrinfo(servinfo);

    printf("Esperando conexiones...\n");
    // 2. Receive file_info
    addr_len = sizeof their_addr;

    if ((numbytes = recvfrom(sockfd, &file_info, sizeof(file_info), 0,
                             (struct sockaddr *)&their_addr, &addr_len)) == -1)
    {
        perror("recvfrom");
        exit(1);
    }

    printf("Recibiendo archivo %s, tamaño %d bytes\n", file_info.name, file_info.size);
    
    // 3. Send response
    response.pagenumber = -1;
    response.ack = ACK;

    if ((numbytes = sendto(sockfd, &response, sizeof(response), 0,
                           (struct sockaddr *)&their_addr, addr_len)) == -1)
    {
        perror("talker: sendto");
        exit(1);
    }
    printf("Aceptando archivo. Enviando respuesta al cliente\n");

    exit(0);
    free(file_buf);
    pclose(sockfd);
    


}
