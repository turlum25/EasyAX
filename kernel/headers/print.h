#ifndef PRINT_H
#define PRINT_H

#include "colors.h"
#include "screen.h"

void print_text(const char* str);
void print_char(char c);
void print_uint(unsigned int n);
void print_hex(unsigned int n);
void clear_screen(unsigned char color);
void clear_screen_reset_cursor(void);

#endif
