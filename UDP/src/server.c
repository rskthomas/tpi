

#define _XOPEN_SOURCE 600
#include "../include/library.h"

void validate_port(int argc, char *argv[]);
int create_and_bind_socket(char *port);
void handle_connection(int sockfd, struct sockaddr_storage their_addr,
                       socklen_t addr_len);
int recv_file_info(struct file_metadata *file_info, int sockfd,
                   struct sockaddr_storage their_addr, socklen_t addr_len);
void initialize_buffers(bool **ack_array, char **file_buf, int npages);
void receive_file(int sockfd, struct sockaddr_storage their_addr,
                  socklen_t addr_len, struct file_metadata *file_info,
                  bool *ack_array, char *file_buf, int npages);

int main(int argc, char *argv[]) {
  validate_port(argc, argv);
  char *port = argv[1];

  int sockfd = create_and_bind_socket(port);
  if (sockfd == -1) {
    fprintf(stderr, "listener: failed to bind socket\n");
    return 2;
  }

  printf("Servidor corriendo en puerto %s. Esperando conexiones...\n", port);
  struct sockaddr_storage their_addr;
  socklen_t addr_len = sizeof(their_addr);

  handle_connection(sockfd, their_addr, addr_len);

  close(sockfd);
  return 0;
}

/*
 * Simple port validation
 */
void validate_port(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "ERROR, no port provided\n");
    exit(EXIT_FAILURE);
  }

  int port = atoi(argv[1]);
  if (port < 1024 || port > 65535) {
    fprintf(stderr, "ERROR, invalid port number\n");
    exit(EXIT_FAILURE);
  }
}

/*
 * Socket initialization
 */
int create_and_bind_socket(char *port) {
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int rv;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return -1;
  }

  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("listener: socket");
      continue;
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("listener: bind");
      continue;
    }

    break;
  }

  freeaddrinfo(servinfo);

  if (p == NULL) {
    return -1;
  }

  return sockfd;
}

/*
 * Initialize buffers for file reception
 */
void initialize_buffers(bool **ack_array, char **file_buf, int npages) {
  *file_buf = malloc(npages * PAGE_SIZE);
  if (*file_buf == NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  memset(*file_buf, 0, npages * PAGE_SIZE);

  *ack_array = malloc(npages * sizeof(bool));
  if (*ack_array == NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  memset(*ack_array, 0, npages * sizeof(bool));
}

/*
 * Handle initial file transfer setup
 */
int recv_file_info(struct file_metadata *file_info, int sockfd,
                   struct sockaddr_storage their_addr, socklen_t addr_len) {
  int numbytes;

  memset(file_info, 0, sizeof(struct file_metadata));
  if ((numbytes = recvfrom(sockfd, file_info, sizeof(struct file_metadata), 0,
                           (struct sockaddr *)&their_addr, &addr_len)) == -1) {
    perror("recvfrom");
    exit(EXIT_FAILURE);
  }

  struct response response;
  memset(&response, 0, sizeof(response));
  response.pagenumber = -1;
  response.ack = ACK;

  printf("Aceptando archivo. Enviando respuesta al cliente\n");

  if ((numbytes = sendto(sockfd, &response, sizeof(response), 0,
                         (struct sockaddr *)&their_addr, addr_len)) == -1) {
    perror("sendto");
    exit(EXIT_FAILURE);
  }

  return (file_info->size + PAGE_SIZE - 1) / PAGE_SIZE;
}

/*
 * Handle the connection and receive the file
 */
void handle_connection(int sockfd, struct sockaddr_storage their_addr,
                       socklen_t addr_len) {
  struct file_metadata file_info;
  bool *ack_array = NULL;
  char *file_buf = NULL;

  int npages = recv_file_info(&file_info, sockfd, their_addr, addr_len);
  initialize_buffers(&ack_array, &file_buf, npages);

  printf("Recibiendo archivo %s, tamaño %zu bytes, %d páginas\n",
         file_info.name, file_info.size, npages);

  receive_file(sockfd, their_addr, addr_len, &file_info, ack_array, file_buf,
               npages);

  unsigned char hash[HASH_SIZE];
  calculate_sha256(file_buf, file_info.size, hash);
  printf("Hash calculado: ");
  printHex(hash);
  printf("Hash recibido: ");
  printHex(file_info.sha256_hash);
  compareHash(hash, file_info.sha256_hash);

  free(file_buf);
  free(ack_array);
}

/*
 * Receive the file from the client
 */
void receive_file(int sockfd, struct sockaddr_storage their_addr,
                  socklen_t addr_len, struct file_metadata *file_info,
                  bool *ack_array, char *file_buf, int npages) {
  int numbytes, recvd_pages = 0, tries_remaining = 5;
  struct file_page file_page;
  char reply[MTU_SIZE];
  struct response *response = (struct response *)reply;

  while (tries_remaining > 0 && recvd_pages < npages) {
    memset(&reply, 0, MTU_SIZE);
    memset(&file_page, 0, sizeof(struct file_page));

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);
    struct timeval timeout = {TIMEOUT_SEC, TIMEOUT_USEC};

    if ((numbytes = recvfrom(sockfd, &file_page, MTU_SIZE, 0,
                             (struct sockaddr *)&their_addr, &addr_len)) ==
        -1) {
      perror("recvfrom");
      tries_remaining--;
      continue;
    }

    if (file_page.pagenumber == -99) {
      break;
    }

    printf("Recibiendo página %d\n", file_page.pagenumber);

    if (!ack_array[file_page.pagenumber]) {
      ack_array[file_page.pagenumber] = true;
      size_t offset = file_page.pagenumber * PAGE_SIZE;
      memcpy(file_buf + offset, file_page.data, PAGE_SIZE);
      recvd_pages++;
    }

    response[0].pagenumber = file_page.pagenumber;
    response[0].ack = ACK;

    if ((numbytes = sendto(sockfd, reply, sizeof(reply), 0,
                           (struct sockaddr *)&their_addr, addr_len)) == -1) {
      perror("sendto");
    }
  }
}
