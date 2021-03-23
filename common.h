#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

typedef uint8_t Byte;
typedef uint16_t Word;
typedef unsigned int u32;

typedef enum {
    BYTE,
    WORD,
    U32,
    INT,
    CHAR,
    STR
} TYPE;