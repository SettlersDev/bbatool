#include "stdbool.h"
#include "stdint.h"

#define SHoK_FEISTEL(sum, value0, value1, num, key)	SHoK_FEISTEL2(sum, value0, value1, num, key)
#define SHoK_FEISTEL2(s, v0, v1, n, k)				(((s ^ v0) + (v1 ^ k[((s >> 2) & 3) ^ n & 3])) ^ ((16 * v1 ^ (v0 >> 3)) + ((v1 >> 5) ^ 4 * v0)))
#define SHoK_KEY									{ 0x298F599D, 0x67AD005D, 0x2AF91C8D, 0x66433D6D }
#define SHoK_DELTA									0x61C88647

bool SHoK_Decrypt(uint32_t *data, uint32_t len);
bool SHoK_Encrypt(uint32_t *data, uint32_t len);
void SHoK_XXTEA_Decrypt(uint32_t *data, uint32_t blocks, const uint32_t *key);
void SHoK_XXTEA_Encrypt(uint32_t *data, uint32_t blocks, const uint32_t *key);