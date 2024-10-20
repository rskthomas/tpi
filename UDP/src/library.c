#include "../include/library.h"

void printHex(unsigned char *hash) {
  int i;
  printf("Hash: ");
  for (i = 0; i < 32; i++) {
    printf("%02x", hash[i]);
  }
  printf("\n");
}

void set_socket_buffers(int sockfd) {
  int rcvbuf = 4 * 1024 * 1024; // 4 MB
  int sndbuf = 4 * 1024 * 1024; // 4 MB

  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) ==
      -1) {
    perror("setsockopt SO_RCVBUF");
  }

  if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf)) ==
      -1) {
    perror("setsockopt SO_SNDBUF");
  }
}

// caclcula el hash sha256 de un buffer y guarda el resultado en sha256_hash
void calculate_sha256(const unsigned char *data, size_t data_len,
                      unsigned char *sha256_hash) {
  SHA256_CTX context;
  SHA256_Init(&context);
  SHA256_Update(&context, data, data_len);
  SHA256_Final(sha256_hash, &context);
}

void compareHash(unsigned char *hash1, unsigned char *hash2) {
  int i;
  char a[3], b[3];

  for (i = 0; i < 32; i++) {

    sprintf(a, "%02x", hash1[i]);
    sprintf(b, "%02x", hash2[i]);

    if (memcmp(a, b, 2) != 0) {
      printf("Hashes no coinciden\n\n");
      return;
    }
  }
  printf("Hashes coinciden\n\n");
}
