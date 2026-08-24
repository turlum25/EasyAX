#ifndef SHELL_UTIL_H
#define SHELL_UTIL_H

// returns 0 if equal, non-zero otherwise
int strcmp(const char* a, const char* b);

// parses a base-10 unsigned integer from a string.
// stops at the first non-digit char. returns 0 for no leading digits.
unsigned int str_to_uint(const char* s);
unsigned int str_len(const char* s);

// copies src into dst (dst must be large enough) - simple strcpy
void str_copy(char* dst, const char* src);

#endif
