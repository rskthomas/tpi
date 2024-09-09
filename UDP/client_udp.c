#define _XOPEN_SOURCE 600
#include "library.h"
#include <netdb.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s hostname port file\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    FILE *file = NULL;
    int sockfd = -1;
    bool *ack_array = NULL;
    char *buffer = NULL;
    struct addrinfo *res = NULL;

    if ((file = fopen(argv[3], "r")) == NULL) // El último parámetro es el archivo a enviar
    {
        perror("Error al abrir el archivo");
        goto cleanup;
    }

    // Clip the file name to 20 characters, after last / if it exists
    char *filename = strrchr(argv[3], '/');
    if (filename == NULL)
    {
        filename = argv[3];
    }
    else
    {
        filename++;
    }

    /*
     * Declaración de variables
     */

    int portno;
    struct file_info file_info; // struct con info del archivo a enviar
    struct addrinfo hints;
    char reply[MTU_SIZE];
    //response es un array de structs response en la direccion de memoria de reply
    struct response *response = (struct response *)reply;

    /*
     * Obtener datos del archivo
     */
    memset(&file_info, 0, sizeof(file_info));

    // Obtener el tamaño del archivo y el nombre
    fseek(file, 0, SEEK_END);
    file_info.size = ftell(file);
    fseek(file, 0, SEEK_SET);

    strncpy(file_info.name, filename, FILENAME_SIZE - 1);
    file_info.name[FILENAME_SIZE - 1] = '\0'; // Ensure null-termination
    int const npages = (file_info.size + MAX_DATA_SIZE - 1) / MAX_DATA_SIZE;
    
    printf("Enviar archivo %s, tamaño %d bytes, %d páginas\n", file_info.name, file_info.size, npages);

    // Alocar memoria en el heap para el archivo
    buffer = malloc(npages * MAX_DATA_SIZE);
    if (buffer == NULL)
    {
        perror("Error al reservar memoria");
        goto cleanup;
    }
    memset(buffer, 0, file_info.size);

    // Cargar el archivo en el buffer y calcular el hash sha_256
    fread(buffer, file_info.size, 1, file);
    calculate_sha256(buffer, file_info.size, file_info.sha256_hash);
    printHex(file_info.sha256_hash);
    fclose(file);
    file = NULL;

    /*
     * Calcular la cantidad de ráfagas a enviar y alocar memoria para el array de ACK
     */
    int const burst = MAX_BURST;

    ack_array = malloc(npages * sizeof(bool));
    if (ack_array == NULL)
    {
        perror("Error al reservar memoria");
        goto cleanup;
    }
    memset(ack_array, 0, npages);

    /*
     * Inicializar socket UDP
     */
    memset(&hints, 0, sizeof hints);
    portno = atoi(argv[2]);
    hints.ai_family = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP

    if (getaddrinfo(argv[1], argv[2], &hints, &res) != 0)
    {
        fprintf(stderr, "Error en getaddrinfo\n");
        goto cleanup;
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1)
    {
        perror("Error al crear el socket");
        goto cleanup;
    }

    /*
     * Enviar metadata del archivo
     */
    if (sendto(sockfd, &file_info, sizeof(file_info), 0, res->ai_addr, res->ai_addrlen) == -1)
    {
        perror("Error al enviar el archivo");
        goto cleanup;
    }

    // Check if the server is ready to receive the file and answer is OK
    if (recvfrom(sockfd, reply, sizeof(reply), 0, res->ai_addr, &res->ai_addrlen) == -1)
    {
        perror("Error al recibir respuesta del servidor");
        goto cleanup;
    }

    if (response[0].ack != ACK || response[0].pagenumber != -1)
    {
        fprintf(stderr, "Error al recibir respuesta del servidor\n");
        goto cleanup;
    }
    else
    {
        printf("Servidor listo para recibir archivo\n");
    }

    // Una vez que el servidor esta listo para recibir, enviar el archivo de a ráfagas
    clock_t start = clock();

    int last_contiguous = -1;
    int iterate;
    int current_page;
    struct file_page page;
    int remaining_pages = npages;

    do
    {
        memset(reply, 0, sizeof(reply));
        
        iterate = (remaining_pages < burst) ? remaining_pages : burst;
        printf("## Enviando %d páginas: ", iterate);

        // Enviar ráfaga
        for (int i = 1; i <= iterate; i++)
        {
            current_page = i + last_contiguous;

            if (!ack_array[current_page])
            {
                page.pagenumber = current_page;
                memcpy(page.data, buffer + current_page * MAX_DATA_SIZE, MAX_DATA_SIZE);

                if (sendto(sockfd, &page, sizeof(struct file_page), 0, res->ai_addr, res->ai_addrlen) == -1)
                {
                    perror("Error al enviar el archivo");
                    goto cleanup;
                }
                printf(" %d,", page.pagenumber);

            }
            else
            {
                iterate++;
            }
        }
        printf("\n");

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        struct timeval timeout = {3, 0};

        printf("Esperando respuesta del servidor\n");
        int retval = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

        if (retval == -1)
        {
            perror("Error en select");
            goto cleanup;
        }
        else if (retval == 0)
        {
            printf("Timeout: No se recibió respuesta del servidor en 3 segundos\n");
        }
        else
        {
            if (recvfrom(sockfd, reply, sizeof(struct response) * burst, 0, res->ai_addr, &res->ai_addrlen) == -1)
            {
                perror("Error al recibir respuesta del servidor");
                goto cleanup;
            }
        }

        // Iterate over the reply until both fields are -99 (end of response)
        int i = 0;
        
        printf("Respuesta del servidor:\n");
        printf("Página %d: %s\n", response[i].pagenumber, response[i].ack == ACK ? "ACK" : "RETRY");
            
        while (i < burst && response[i].pagenumber != -99 && response[i].ack != -99)
        {
            
            int pagenumber = response[i].pagenumber;
            bool ack = response[i].ack == ACK;
            ack_array[pagenumber] = ack;

            if (ack)
            {
                remaining_pages--;
                if (last_contiguous + 1 == pagenumber)
                {
                    
                    last_contiguous++;
                }
            }
            
            i++;
            
        }

        printf("Last contiguous: %d\n", last_contiguous);

    } while (remaining_pages > 0);

    clock_t end = clock();
    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Tiempo transcurrido por conexión: %f ms\n", (time_spent * 1000) / 2);

    exit(EXIT_SUCCESS);

cleanup:
    if (file != NULL)
    {
        fclose(file);
    }
    if (sockfd >= 0)
    {
        close(sockfd);
    }
    if (ack_array != NULL)
    {
        free(ack_array);
    }
    if (buffer != NULL)
    {
        free(buffer);
    }
    if (res != NULL)
    {
        freeaddrinfo(res);
    }

    exit(EXIT_FAILURE);
}