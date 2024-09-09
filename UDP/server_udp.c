#define _XOPEN_SOURCE 600
#include "library.h"
#include <netdb.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void handle_connection(int sockfd, struct sockaddr_storage their_addr, socklen_t addr_len);

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(EXIT_FAILURE);
    }

    // Check if port is a number from 1024 to 65535
    int port = atoi(argv[1]);
    if (port < 1024 || port > 65535)
    {
        fprintf(stderr, "ERROR, invalid port number\n");
        exit(EXIT_FAILURE);
    }
    char *portn = argv[1];

    /*
     * Networking variables
     */
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

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

    // Bind on first result
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("listener: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("listener: bind");
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }

    freeaddrinfo(servinfo);

    printf("Servidor corriendo en puerto %s. Esperando conexiones...\n", portn);

    struct sockaddr_storage their_addr;
    socklen_t addr_len = sizeof their_addr;
    handle_connection(sockfd, their_addr, addr_len);

    close(sockfd);
    return 0;
}

void handle_connection(int sockfd, struct sockaddr_storage their_addr, socklen_t addr_len)
{
    int numbytes;
    bool *ack_array = NULL;
    struct file_info file_info;
    struct file_page file_page;
    char *file_buf = NULL;
    char reply[MTU_SIZE];
    struct response *response = (struct response *)reply;

    // 2. Receive file_info
    if ((numbytes = recvfrom(sockfd, &file_info, sizeof(file_info), 0,
                             (struct sockaddr *)&their_addr, &addr_len)) == -1)
    {
        perror("recvfrom");
        goto cleanup;
    }

    int const npages = (file_info.size + MAX_DATA_SIZE - 1) / MAX_DATA_SIZE;

    printf("Recibiendo archivo %s, tamaño %d bytes, %d páginas\n",
           file_info.name, file_info.size, npages);

    // 3. Send response
    memset(&reply, 0, sizeof(reply));
    response[0].pagenumber = -1;
    response[0].ack = ACK;
    printf("Aceptando archivo. Enviando respuesta al cliente\n");

    if ((numbytes = sendto(sockfd, reply, sizeof(reply), 0,
                           (struct sockaddr *)&their_addr, addr_len)) == -1)
    {
        perror("talker: sendto");
        goto cleanup;
    }

    // 4. Receive file and allocate memory
    file_buf = malloc(npages * MAX_DATA_SIZE);

    if (file_buf == NULL)
    {
        perror("malloc");
        goto cleanup;
    }

    memset(file_buf, 0, file_info.size);

    // Inicializando el buffer de acks para llevar la cuenta de las páginas recibidas correctamente
    ack_array = malloc(npages * sizeof(bool));
    if (ack_array == NULL)
    {
        perror("malloc");
        goto cleanup;
    }
    for (int i = 0; i < npages; i++)
    {
        ack_array[i] = false;
    }

    printf("Recibiendo archivo\n");

    int remaining_pages = npages;
read: // Label para regresar a leer páginas

    memset(&reply, 0, sizeof(reply));
    memset(&file_page, 0, sizeof(file_page));
    int received_pages = 0;
    printf("Recibiendo página: ");
    while (remaining_pages >= 0)
    {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        struct timeval timeout = {0, 0}; // Wait 0.1 sec

        int retval = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

        if (retval == -1)
        {
            perror("select");
            goto cleanup;
        }
        else if (retval == 0)
        {
            // No more datagrams to read
            printf("Timeout: No se recibió respuesta del cliente en 1 segundo\n");
            break;
        }

        // Receive file_page
        if ((numbytes = recvfrom(sockfd, &file_page, sizeof(file_page), 0,
                                 (struct sockaddr *)&their_addr, &addr_len)) == -1)
        {
            perror("recvfrom");
            goto cleanup;
        }

        // checkeamos que no hayamos recibido anteriormente la página
        if (ack_array[file_page.pagenumber])
        {
            continue;
        }

        printf("%d, ", file_page.pagenumber);

        // Como se recibió la página correctamente, se marca en el buffer de acks
        ack_array[file_page.pagenumber] = true;

        // y se copia en memoria
        size_t offset = file_page.pagenumber * MAX_DATA_SIZE;
        memcpy(file_buf + offset, file_page.data, MAX_DATA_SIZE);

        response[received_pages].pagenumber = file_page.pagenumber;
        response[received_pages].ack = ACK;

        received_pages++;
        remaining_pages--;
    }
    //

    response[received_pages].pagenumber = -99;
    response[received_pages].ack = -99;

    // Send response to client
    if ((numbytes = sendto(sockfd, reply, sizeof(reply), 0,
                           (struct sockaddr *)&their_addr, addr_len)) == -1)
    {
        perror("talker: sendto");
        goto cleanup;
    }
    printf("remaining pages: %d\n", remaining_pages);

    if (remaining_pages > 0)
    {
        goto read;
    }

    // 5. Calculate hash and compare
    unsigned char hash[HASH_SIZE];
    calculate_sha256(file_buf, file_info.size, hash);
    printf("Hash calculado: ");
    printHex(hash);
    printf("Hash recibido: ");
    printHex(file_info.sha256_hash);
    compareHash(hash, file_info.sha256_hash);

cleanup:
    if (file_buf != NULL)
    {
        free(file_buf);
    }
    if (ack_array != NULL)
    {
        free(ack_array);
    }
}
