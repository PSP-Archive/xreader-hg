#include <psptypes.h>
#include <stdio.h>
#include "rc4.h"

static void rc4_swap(u8 * a, u8 * b)
{
	u8 c;

	c = *a;
	*a = *b;
	*b = c;
}

void rc4_prepare_key(u8 * key_data, size_t key_length, rc4_key * key)
{
	size_t i, j;

	for (i = 0; i < 256; ++i) {
		key->s[i] = i;
	}

	j = 0;

	for (i = 0; i < 256; ++i) {
		j = (j + key->s[i] + key_data[i % key_length]) % 256;
		rc4_swap(&key->s[i], &key->s[j]);
	}

	key->i = key->j = 0;
}

u8 rc4_prga(rc4_key * key)
{
	size_t i, j;
	u8 *s;

	i = key->i;
	j = key->j;
	s = key->s;

	i = (i + 1) % 256;
	j = (j + s[i]) % 256;
	rc4_swap(&s[i], &s[j]);

	key->i = i;
	key->j = j;

	return s[(s[i] + s[j]) % 256];
}

void rc4_crypt(u8 * buf, size_t buf_size, rc4_key * key)
{
	size_t i, j, cnt;
	u8 *s;

	i = key->i;
	j = key->j;
	s = key->s;

	for (cnt = 0; cnt < buf_size; ++cnt) {
		i = (i + 1) % 256;
		j = (j + s[i]) % 256;
		rc4_swap(&s[i], &s[j]);
		*buf++ ^= s[(s[i] + s[j]) % 256];
	}

	key->i = i;
	key->j = j;
}

void rc4_encrypt(u8 * plain, size_t plain_size, u8 * key_data, size_t key_size)
{
	rc4_key key;

	rc4_prepare_key(key_data, key_size, &key);
	rc4_crypt(plain, plain_size, &key);
}
