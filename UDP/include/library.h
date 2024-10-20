#ifndef LIBRARY_H_
#define LIBRARY_H_

#include <netdb.h>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define HASH_SIZE 32

#define FILENAME_SIZE 50
#define BURST_SIZE 40

#define MTU_SIZE 1500
#define PAGE_SIZE (MTU_SIZE - sizeof(int))

#define TIMEOUT_SEC 0
#define TIMEOUT_USEC 100000

#define MAX_RETRIES 3

/*
 * Hashing function declarations
 * TODO: update SHA functions-- deprecated as of openssl 3.0
 */

void set_socket_buffers(int sockfd);
void calculate_sha256(const unsigned char *data, size_t data_len,
                      unsigned char *sha256_hash);
void printHex(unsigned char *hash);
void compareHash(unsigned char *hash1, unsigned char *hash2);

/*
 *Basic metadata for each file, along with its hash string
 */
struct file_metadata {
  unsigned int size;
  unsigned int npages;
  int page_size;
  char name[FILENAME_SIZE];
  unsigned char sha256_hash[HASH_SIZE];
};

typedef struct {
  char data[PAGE_SIZE];
  int pagenumber;
} file_page_t;

struct file_page {
  char data[PAGE_SIZE];
  int pagenumber;
};

struct response {
  int pagenumber;
  signed char ack;
};

enum ACK {
  ACK = 1,
  END_OF_TRANSMISSION = -1,
};

#endif
