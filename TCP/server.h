#ifndef SERVER_H_
#define SERVER_H_

#define HASH_SIZE 32

void calculate_sha256(const unsigned char *data, size_t data_len, unsigned char *sha256_hash);
void printHex(unsigned char *hash);
void compareHash(unsigned char *hash1, unsigned char *hash2);


struct file_info
{
    int size;
    char name[20];
    unsigned char sha256_hash[HASH_SIZE]; 
};


#endif
