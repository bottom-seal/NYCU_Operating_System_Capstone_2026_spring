#ifndef STR_H
#define STR_H

#include "types.h"

size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
char*  strcpy(char *dst, const char *src);
char*  strchr(const char *s, int c);
char*  strrchr(const char *s, int c);
void*  memcpy(void *dst, const void *src, size_t n);

#endif