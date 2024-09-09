#ifndef LIBRARY_H_
#define LIBRARY_H_

#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#define HASH_SIZE 32
#define FILENAME_SIZE 50
#define MAX_BURST 20

#define MTU_SIZE 512
#define MAX_DATA_SIZE (MTU_SIZE - sizeof(unsigned int))

void calculate_sha256(const unsigned char *data, size_t data_len, unsigned char *sha256_hash);
void printHex(unsigned char *hash);
void compareHash(unsigned char *hash1, unsigned char *hash2);



struct file_info
{
    int size;
    char name[FILENAME_SIZE];
    unsigned char sha256_hash[HASH_SIZE]; 
};

struct file_page
{
    char data[MAX_DATA_SIZE];
    unsigned int pagenumber;
};

struct response {
    unsigned int pagenumber;
    signed char ack;
};
enum ACK { 
    RETRY = -1,
    ACK = 0,
};


#endif
