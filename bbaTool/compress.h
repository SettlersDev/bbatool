
#include <stdio.h>

#define LOLCRYPT_XOR	0x56
#define CHUNK 16384
extern "C" {
int inf(FILE *source, FILE *dest, bool lolCrypt);
int def(FILE *source, FILE *dest, int level, unsigned int *compressedSize, unsigned int *decompressedSize, unsigned int *dataAdler, bool lolCrypt);
}