#include "headers/util.h"

int strcmp(const char* a, const char* b)
{
    int i = 0;

    while (a[i] && b[i]) {
        if (a[i] != b[i]) {
            return 1;
        }
        i++;
    }

    if (a[i] != b[i]) {
        return 1;
    }

    return 0;
}

unsigned int str_to_uint(const char* s)
{
    unsigned int result = 0;

    while (*s >= '0' && *s <= '9') {
        result = (result * 10) + (*s - '0');
        s++;
    }

    return result;
}

void str_copy(char* dst, const char* src)
{
    unsigned int i = 0;
    while (src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}
unsigned int str_len(const char* s)
{
    unsigned int len = 0;
    while (s[len]) {
        len++;
    }
    return len;
}
