#ifndef UTIL_H
#define UTIL_H

/*
 * Convert byte to human readable.
 * If binary is non-zero, use 1024 as conversation number, or else
 * use 1000.
 * precision is the precision, or -1 for the default
 * Returns the characters written on success, or -1 on error (like out is too small)
 */
int util_byte2human(long int bytes, int binary, int precision, char* out, size_t out_len);

/*
 * Convert raw bytes in 'bytes' to hex format into out
 * out has to be at least bytes_len * 2 + 1 large
 */
void util_byte2hex(const unsigned char* bytes, int bytes_len, int uppercase, char* out);

#endif
