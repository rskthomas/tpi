
#define _XOPEN_SOURCE 600
#include "server_udp.h"
#include <netdb.h>
#include <time.h>

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "usage %s hostname port file\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    FILE *file;                               // file descriptor
    if ((file = fopen(argv[3], "r")) == NULL) // El último parámetro es el archivo a enviar
    {
        printf("Error al abrir el archivo\n");
        exit(EXIT_FAILURE);
    }

    // clip the file name to 20 characters, after last / if it exists
    char *filename = strrchr(argv[3], '/');
    if (filename == NULL)
    {
        filename = argv[3];
    }
    else
        filename++;

    /*
     * Declaración de variables
     */

    int sockfd, n, portno;

    struct file_info file_info; // struct con info del archivo a enviar

    struct addrinfo hints, *res;

    char *buffer;
    char reply[MTU_SIZE];

    // Se podría utilizar un buffer de bits (unsigned char son 8 bits)
    // pero por simplicidad será un buffer de booleans (8bits c/u)

    bool *ack_array;

    /*
     * Obtener datos del archivo
     */
    memset(&file_info, 0, sizeof(file_info));

    // Obtener el tamaño del archivo y el nombre

    fseek(file, 0, SEEK_END);
    file_info.size = ftell(file);

    memcpy(file_info.name, filename, FILENAME_SIZE);
    printf("Enviar archivo %s, tamaño %d bytes\n", file_info.name, file_info.size);

    // Volver al inicio del archivo
    fseek(file, 0, SEEK_SET);

    // alocar memoria en el heap para el archivo
    if ((buffer = malloc(file_info.size)) == NULL)
    {
        printf("Error al reservar memoria\n");
        exit(EXIT_FAILURE);
    }
    memset(buffer, 0, file_info.size);

    // cargar el archivo en el buffer y calcular el hash sha_256
    fread(buffer, file_info.size, 1, file);
    calculate_sha256(buffer, file_info.size, file_info.sha256_hash);
    printHex(file_info.sha256_hash);
    fclose(file);

    /*
     * Calcular la cantidad de ráfagas a enviar y alocar memoria para el array de ACK
     */

    // calcular la cantidad total de paginas
    int const npages = (file_info.size + MTU_SIZE - 1) / MTU_SIZE;
    int const burst = sizeof(struct response) / MTU_SIZE;

    // malloc para el buffer de ACK
    ack_array = malloc(npages * sizeof(bool));

    if (ack_array == NULL)
    {
        printf("Error al reservar memoria\n");
        exit(EXIT_FAILURE);
    }
    memset(ack_array, 0, npages);

    /*
     * Inicializar socket UDP
     */

    memset(&hints, 0, sizeof hints);
    portno = atoi(argv[2]);
    hints.ai_family = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP

    // obtener info del servidor y guardar el resultado en *res
    if (getaddrinfo(argv[1], argv[2], &hints, &res) != 0)
    {
        fprintf(stderr, "Error en getaddrinfo\n");
        exit(EXIT_FAILURE);
    }

    // crear el socket usando el primer resultado de getaddrinfo
    if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
    {
        perror("Error al crear el socket");
        exit(EXIT_FAILURE);
    }

    /*
     * Enviar metadata del archivo
     */

    if (sendto(sockfd, &file_info, sizeof(file_info), 0, res->ai_addr, res->ai_addrlen) == -1)
    {
        perror("Error al enviar el archivo");
        exit(EXIT_FAILURE);
    }

    // check if the server is ready to receive the file and answer is OK
    if (recvfrom(sockfd, reply, sizeof(reply), 0, res->ai_addr, &res->ai_addrlen) == -1)
    {
        perror("Error al recibir respuesta del servidor");
        exit(EXIT_FAILURE);
    }

    struct response *response = (struct response *)reply;
    if (response->pagenumber != -1 || response->ack != ACK)
    {
        printf("Error al recibir respuesta del servidor\n");
        exit(EXIT_FAILURE);
    }
    else
        printf("Servidor listo para recibir archivo\n");

    // una vez que el servidor esta listo para recibir, enviar el archivo de a ráfagas
    clock_t start = clock();

    int last_contiguous = -1;
    int iterate;

    struct file_page *page = malloc(sizeof(struct file_page));

    do
    {
        iterate = burst;
        // enviar ráfaga
        for (int i = 1; i <= iterate; i++)
        {
            // if the page is not acked, send it
            if (!ack_array[i])
            {
                page->pagenumber = i + last_contiguous;
                memcpy(page->data, buffer + (i - 1) * MTU_SIZE, MTU_SIZE);

                if (sendto(sockfd, page, sizeof(struct file_page), 0, res->ai_addr, res->ai_addrlen) == -1)
                {
                    perror("Error al enviar el archivo");
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                iterate++;
                break;
            }
        }

        // receive one datagram with acks for the whole burst
        if (recvfrom(sockfd, reply, sizeof(struct response) * burst, 0, res->ai_addr, &res->ai_addrlen) == -1)
        {
            perror("Error al recibir respuesta del servidor");
            exit(EXIT_FAILURE);
        }
        // iterate over the reply and set the acks in the buffer
        for (int i = 0; i < burst; i++)
        {
            struct response *response = (struct response *)reply + i;
            ack_array[response->pagenumber] = response->ack;
            if (response->ack == ACK && ack_array[response->pagenumber - 1] == ACK)
            {
                last_contiguous = response->pagenumber;
            }
        }

    } while (last_contiguous < npages);

    clock_t end = clock();
    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Tiempo transcurrido por conexión: %f \n", (time_spent * 1000) / 2);

    // cleanup
    free(buffer);
    free(ack_array);
    free(page);
    freeaddrinfo(res);
    pclose(sockfd);
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