#ifndef RC4_H
#define RC4_H

typedef struct _rc4_key
{
	u8 s[256];
	int i, j;
} rc4_key;

// low level;
void rc4_prepare_key(u8 * key_data, size_t key_length, rc4_key * key);
u8 rc4_prga(rc4_key * key);
void rc4_crypt(u8 * buf, size_t buf_size, rc4_key * key);

// high level
void rc4_encrypt(u8 * plain, size_t plain_size, u8 * key_data, size_t key_size);

#endif
