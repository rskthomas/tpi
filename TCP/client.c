#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <netinet/tcp.h>
#include <string.h>
#include <openssl/sha.h>
#include <time.h>
#include "server.h"

#define DATA_SIZE_TO_SEND 20000

void error(char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        fprintf(stderr, "usage %s hostname port file\n", argv[0]);
        exit(0);
    }

    int sockfd, portno, n;
    FILE *file; // file descritor
    // info del archivo a enviar
    struct file_info file_info;
    // sockaddr_in replaces sockaddr,it is easier to use- with sockaddr you would have to write the ip adress bytes in an ordered manner <<
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char *buffer;
    char response[256];

    // El último parámetro es el archivo a enviar
    file = fopen(argv[3], "r");
    if (file == NULL)
    {
        printf("Error al abrir el archivo\n");
        exit(1);
    }

    bzero(&file_info, sizeof(file_info));
    // Posicionarse al final del archivo
    fseek(file, 0, SEEK_END);

    // Obtener el tamaño del archivo y el nombre y guardarlo en el struct
    file_info.size = ftell(file);

    memcpy(file_info.name, argv[3], strlen(argv[3]));
    printf("Enviar archivo %s, tamaño %d bytes\n", file_info.name, file_info.size);

    // Volver al inicio del archivo
    fseek(file, 0, SEEK_SET);

    // reservar memoria para el archivo y struct y setear en 0
    buffer = malloc(file_info.size + sizeof(file_info));
    if (buffer == NULL)
    {
        printf("Error al reservar memoria\n");
        exit(1);
    }
    
    bzero(buffer, file_info.size + sizeof(file_info));

    // carga el archivo a partir de la posicion del file_info
    fread(buffer + sizeof(file_info), file_info.size, 1, file);

    calculate_sha256(buffer + sizeof(file_info), file_info.size, file_info.sha256_hash);

    // copiar el struct al principio del buffer
    memcpy(buffer, &file_info, sizeof(file_info));

    printHex(file_info.sha256_hash);

    // TOMA EL NUMERO DE PUERTO DE LOS ARGUMENTOS
    portno = atoi(argv[2]);

    // CREA EL FILE DESCRIPTOR DEL SOCKET PARA LA CONEXION
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    // AF_INET - FAMILIA DEL PROTOCOLO - IPV4 PROTOCOLS INTERNET
    // SOCK_STREAM - TIPO DE SOCKET

    if (sockfd < 0)
        error("ERROR opening socket");

    // TOMA LA DIRECCION DEL SERVER DE LOS ARGUMENTOS
    // gethostbyname is deprecated, use getaddrinfo()
    server = gethostbyname(argv[1]);
    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }
    bzero((char *)&serv_addr, sizeof(serv_addr));
    

    serv_addr.sin_family = AF_INET;

    // COPIA LA DIRECCION IP Y EL PUERTO DEL SERVIDOR A LA ESTRUCTURA DEL SOCKET
    bcopy((char *)server->h_addr_list[0],
          (char *)&serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(portno);

    // DESCRIPTOR - DIRECCION - TAMAÑO DIRECCION
    if (connect(sockfd, &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR connecting");

    // Inicio cronometro ----------------------------
    clock_t begin = clock();

    long int bytes_sent = 0;
    // cantidad total de bytes a enviar
    const int TOTAL_BYTES = file_info.size + sizeof(file_info);
    printf("Total bytes a enviar: %d \n", TOTAL_BYTES);
    int bytes_to_send;
  
    while (bytes_sent < TOTAL_BYTES)
    {
        // Si quedan más de DATA_SIZE_TO_SEND bytes por enviar, envía DATA_SIZE_TO_SEND bytes, sino los que falten
        if ((TOTAL_BYTES - bytes_sent) > DATA_SIZE_TO_SEND)
            bytes_to_send = DATA_SIZE_TO_SEND;
        else
            bytes_to_send = TOTAL_BYTES - bytes_sent;

        n = write(sockfd, buffer + bytes_sent, bytes_to_send);
        
        //if there is an error writing, we should try again but lets not add -1 to bytes_sent
        if (n < 0)
            error("ERROR writing to socket1");
        else bytes_sent += n;
    }

    printf("Archivo %s, escritos en socket %ld bytes \n", argv[3], bytes_sent);
    
    if (n < 0)
        error("ERROR writing to socket");

    // ESPERA RECIBIR UNA RESPUESTA
    n = recv(sockfd, buffer, 255, 0);
    if (n < 0)
        error("ERROR reading from socket");

    close(sockfd);

    // Terminó la conexión, finaliza el cronometro
    clock_t end = clock();
    double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;

    printf("Tiempo transcurrido por conexión: %f \n", (time_spent * 1000) / 2);

    printf("%s\n", response);
    // terminamos de usar el socket, liberamos memoria y cerramos el archivo
    free(buffer);
    fclose(file);
    return 0;
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