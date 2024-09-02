/* A simple server in the internet domain using TCP
   The port number is passed as an argument */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>
#include "server.h"

#define BUFFER_SIZE 10000000

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }
    int sockfd, newsockfd, portno, clilen;
    char *buffer = malloc(BUFFER_SIZE);
    struct sockaddr_in serv_addr, cli_addr;
    int n, bytes_read, total_bytes;
    char response[256];

    unsigned char calculated_hash[HASH_SIZE];

    // info del archivo a enviar
    struct file_info file_info;

    // CREA EL FILE DESCRIPTOR DEL SOCKET PARA LA CONEXION
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    // AF_INET - FAMILIA DEL PROTOCOLO - IPV4 PROTOCOLS INTERNET
    // SOCK_STREAM - TIPO DE SOCKET

    if (sockfd < 0)
        error("ERROR opening socket");
    bzero((char *)&serv_addr, sizeof(serv_addr));
    // ASIGNA EL PUERTO PASADO POR ARGUMENTO
    // ASIGNA LA IP EN DONDE ESCUCHA (SU PROPIA IP)
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);


    int yes=1;
    // lose the pesky "Address already in use" error message
    if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes) == -1) {
        perror("setsockopt");
        exit(1);
    } 

    // VINCULA EL FILE DESCRIPTOR CON LA DIRECCION Y EL PUERTO
    if (bind(sockfd, (struct sockaddr *)&serv_addr,
             sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    // SETEA LA CANTIDAD QUE PUEDEN ESPERAR MIENTRAS SE MANEJA UNA CONEXION
    listen(sockfd, 10);

    clilen = sizeof(cli_addr);

    // Loop principal
    while (1)
    {
        // SE BLOQUEA A ESPERAR UNA CONEXION
        newsockfd = accept(sockfd,
                           (struct sockaddr *)&cli_addr,
                           &clilen);

        // DEVUELVE UN NUEVO DESCRIPTOR POR EL CUAL SE VAN A REALIZAR LAS COMUNICACIONES
        if (newsockfd < 0)
            error("ERROR on accept");
        bzero(buffer, BUFFER_SIZE);

        // la primera lectura será del tamaño del struct que representa el tamaño del archivo
        total_bytes = sizeof(file_info);

        bzero(&file_info, sizeof(file_info));
        // lee el struct file_size
        read(newsockfd, &file_info, sizeof(file_info));
        printf("Recibiendo archivo %s, tamaño %d bytes\n", file_info.name, file_info.size);
        printHex(file_info.sha256_hash);
        
        total_bytes = file_info.size;
        bytes_read = 0;

        // LEE EL MENSAJE DEL CLIENTE
        while ((n = read(newsockfd, buffer + bytes_read, total_bytes - bytes_read)) > 0)
        {

            if (n < 0)
                error("ERROR reading from socket");

            bytes_read += n;
        }
        printf("Recibidos %d bytes total \n", bytes_read);
        
   
        // calcula el hash del archivo recibido
        calculate_sha256(buffer, bytes_read, calculated_hash);
        printHex(calculated_hash);
     
        compareHash(file_info.sha256_hash, calculated_hash);

        // RESPONDE AL CLIENTE
        n = send(newsockfd, "I got your message", 18, 0);
        if (n < 0)
            error("ERROR writing to socket");

        // cerramos el socket de la conexion actual

        close(newsockfd);
    }

    free(buffer);
    return 0;
}

void compareHash(unsigned char *hash1, unsigned char *hash2)
{
    int i;
    char a[3], b [3];

    for (i = 0; i < 32; i++)
    {
        
        sprintf(a,"%02x", hash1[i]);
        sprintf(b,"%02x", hash2[i]);
        
        if (memcmp(a,b,2) != 0)
        {
            printf("Hashes no coinciden\n\n");
            return;
        }
    }
    printf("Hashes coinciden\n\n");
}

void printHex(unsigned char *hash)
{
    int i;
    printf("Hash: ");
    for (i = 0; i < 32; i++)
    {
        printf("%02x", hash[i]);
    }
    printf("\n");
}

// caclcula el hash sha256 de un buffer y guarda el resultado en sha256_hash
void calculate_sha256(const unsigned char *data, size_t data_len, unsigned char *sha256_hash)
{
    SHA256_CTX context;
    SHA256_Init(&context);
    SHA256_Update(&context, data, data_len);
    SHA256_Final(sha256_hash, &context);
}
