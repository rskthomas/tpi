/*
 * Send a file to a server using UDP.
 *
 * Author: Thomas Rusiecki
 */

#include "../include/library.h"

/*
 * Function to bind socket to server
 */
void init_connection(int *sockfd, struct addrinfo **res, const char *hostname,
                     const char *port) {
  int rv;
  struct addrinfo hints;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  if ((rv = getaddrinfo(hostname, port, &hints, res)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(EXIT_FAILURE);
  }

  for (struct addrinfo *p = *res; p != NULL; p = p->ai_next) {
    if ((*sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) ==
        -1) {
      perror("socket");
      continue;
    }
    break;
  }

  if (*sockfd == -1) {
    fprintf(stderr, "Failed to bind socket\n");
    exit(EXIT_FAILURE);
  }
}

/*
 * Load file onto @param **buffer
 * given @param *filemane
 * Saves its metadata onto @param *file_metadata
 */
void load_file(struct file_metadata *file_metadata, const char *filename,
               char **buffer) {
  FILE *file = fopen(filename, "r");
  if (file == NULL) {
    perror("Error opening file");
    exit(EXIT_FAILURE);
  }

  fseek(file, 0, SEEK_END);
  file_metadata->size = ftell(file);
  fseek(file, 0, SEEK_SET);

  const char *short_filename = strrchr(filename, '/');
  short_filename = (short_filename == NULL) ? filename : short_filename + 1;

  strncpy(file_metadata->name, short_filename, FILENAME_SIZE - 1);
  file_metadata->name[FILENAME_SIZE - 1] = '\0';

  file_metadata->npages = (file_metadata->size + PAGE_SIZE - 1) / PAGE_SIZE;

  *buffer = malloc(file_metadata->npages * PAGE_SIZE);
  if (*buffer == NULL) {
    perror("Error allocating memory");
    exit(EXIT_FAILURE);
  }
  memset(*buffer, 0, file_metadata->size);

  fread(*buffer, file_metadata->size, 1, file);
  fclose(file);
}

/*
 * Sends the file metadata to the server, and waits for reply
 */
int send_file_metadata(int sockfd, struct file_metadata *file_info, int flags,
                       const struct sockaddr *dest_addr, socklen_t addrlen) {
  int retries = 0;
  while (retries < MAX_RETRIES) {
    int nbytes = sendto(sockfd, file_info, sizeof(struct file_metadata), flags,
                        dest_addr, addrlen);
    printf("Sent %d bytes. ", nbytes);

    if (nbytes == -1) {
      perror("Error sending file metadata, retrying");
      retries++;
      continue;
    }
    printf("File metadata sent. ");

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);
    struct timeval timeout = {TIMEOUT_SEC, TIMEOUT_USEC};
    struct response response;

    int retval = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
    if (retval == -1) {
      perror("select");
      exit(EXIT_FAILURE);
    } else if (retval == 0) {
      printf("Timeout: No response from server in %d microseconds, "
             "retrying\n",
             TIMEOUT_USEC);
      retries++;
      continue;
    }

    if (recvfrom(sockfd, &response, sizeof(response), flags, NULL, NULL) !=
        -1) {
      if (response.ack == ACK || response.pagenumber == -1) {
        printf("ACK received\n");
        printf("Server ready to receive file\n");
        return 0;
      }
    }

    perror("Error receiving response from server, retrying");
    retries++;
  }
  fprintf(stderr, "Max retries reached. Exiting.\n");
  exit(EXIT_FAILURE);
}

/*
 * Sends in a burst of pages the file to the server
 * and checks for acks back
 */

void send_file(int sockfd, struct addrinfo *res, char *file_buffer,
               bool *ack_array, int npages) {

  int last_contiguous = -1;
  int remaining_pages = npages;
  int burst = BURST_SIZE;
  int runs = 0;
  int retries = 0;

  struct file_page page;

  while (remaining_pages > 0 || retries < MAX_RETRIES) {

    burst = (remaining_pages < BURST_SIZE) ? remaining_pages : BURST_SIZE;
    // printf("Run #%d. Sending %d pages: ", runs, burst);

    for (int i = 1; i <= burst; i++) {
      int current_page = i + last_contiguous;
      if (!ack_array[current_page]) {
        page.pagenumber = current_page;
        memcpy(page.data, file_buffer + current_page * PAGE_SIZE, PAGE_SIZE);

        if (sendto(sockfd, &page, sizeof(struct file_page), 0, res->ai_addr,
                   res->ai_addrlen) == -1) {
          perror("Error sending file page");
          exit(EXIT_FAILURE);
        }
        // printf(" %d,", page.pagenumber);
      } else {
        burst++;
      }
    }

    char reply[MTU_SIZE];
    struct response *response = (struct response *)reply;

    // loop till no more incoming datagrams on os buffer
    int current_page, numbytes, index, retval;

    while (1) {
      fd_set readfds;
      FD_ZERO(&readfds);
      FD_SET(sockfd, &readfds);

      struct timeval timeout = {TIMEOUT_SEC, 0};

      retval = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

      if (retval == -1) {
        perror("select");
      } else if (retval == 0) {
        // No more datagrams to read
        // printf("Timeout: No se recibieron más acks\n");
        break;
      }

      // Receive file_page
      numbytes = recvfrom(sockfd, reply, sizeof(reply) - 1, 0, NULL, NULL);
      if (numbytes == -1) {
        perror("recvfrom");
        exit(EXIT_FAILURE);
      }

      current_page = response[0].pagenumber;
      if (!ack_array[current_page] && response[0].ack == ACK) {

        ack_array[current_page] = true;
        remaining_pages--;
      }

      if (current_page == -99 && (response[00].ack == END_OF_TRANSMISSION)) {
        printf("EOT");
        return;
      }

      // update last_contigou state
      index = last_contiguous + 1;
      while (ack_array[index]) {
        last_contiguous++;
        index++;
      }

      // printf("Received ack: %d\n", current_page);
    }
    runs++;

    if (remaining_pages == 0 && last_contiguous + 1 == npages) {
      printf("DONE client side");
      return;
    }
  }
}

int main(int argc, char *argv[]) {

  if (argc != 4) {
    fprintf(stderr, "Usage: %s hostname port file\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  // socket file structures
  int sockfd = -1;
  struct addrinfo *res = NULL;

  // file info data
  struct file_metadata file_info;
  memset(&file_info, 0, sizeof(struct file_metadata));

  // heap buffers; file buffer could be as large as heap allows
  char *file_buffer = NULL;
  bool *ack_array = NULL;

  init_connection(&sockfd, &res, argv[1], argv[2]);

  set_socket_buffers(sockfd);
  load_file(&file_info, argv[3], &file_buffer);

  calculate_sha256(file_buffer, file_info.size, file_info.sha256_hash);
  printHex(file_info.sha256_hash);

  printf("Sending file %s, size %d bytes, %d pages\n", file_info.name,
         file_info.size, file_info.npages);

  /*
   * INIT TRANSMISSION
   */
  clock_t begin = clock();

  send_file_metadata(sockfd, &file_info, 0, res->ai_addr, res->ai_addrlen);

  ack_array = malloc(file_info.npages * sizeof(bool));
  if (ack_array == NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  memset(ack_array, 0, file_info.npages * sizeof(bool));

  send_file(sockfd, res, file_buffer, ack_array, file_info.npages);

  /*
   *Finished transmission
   */
  clock_t end = clock();
  double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;

  printf("Tiempo transcurrido por conexión: %f \n", (time_spent * 1000) / 2);

  free(file_buffer);
  free(ack_array);
  freeaddrinfo(res);
  close(sockfd);

  return 0;
}
