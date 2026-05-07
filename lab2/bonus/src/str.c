#include "types.h"

size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len] != '\0') {//count until seeing \0
        len++;
    }
    return len;
}

int strcmp(const char *a, const char *b) {
    size_t i = 0;

    while (a[i] != '\0' && b[i] != '\0') {//until 1 string ends
        if ((unsigned char)a[i] != (unsigned char)b[i]) {
            return (unsigned char)a[i] - (unsigned char)b[i];//return difference of the first different char
        }
        i++;
    }

    return (unsigned char)a[i] - (unsigned char)b[i];//only 0 if both \0
}

char* strcpy(char *dst, const char *src) {
    size_t i = 0;

    while (1) {
        dst[i] = src[i];
        if (src[i] == '\0') {//copy until seeing \0
            break;
        }
        i++;
    }

    return dst;
}
//take int c becuase special character like EOF is represented as int
char* strchr(const char *s, int c) {//find first occurrence of c in s, return pointer to it, or NULL if not found
    char ch = (char)c;
    size_t i = 0;

    while (1) {
        if (s[i] == ch) {
            return (char*)(s + i);
        }
        if (s[i] == '\0') {
            break;
        }
        i++;
    }

    return NULL;
}

char* strrchr(const char *s, int c) {//find last occurrence of c in s, return pointer to it, or NULL if not found
    char ch = (char)c;
    const char *last = NULL;
    size_t i = 0;

    while (1) {//traverse whole string
        if (s[i] == ch) {
            last = s + i;
        }
        if (s[i] == '\0') {
            break;
        }
        i++;
    }

    return (char*)last;
}

void* memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char*)dst;
    const unsigned char *s = (const unsigned char*)src;

    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }

    return dst;
}