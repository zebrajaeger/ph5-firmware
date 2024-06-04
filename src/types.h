#pragma once

#include <Arduino.h>

typedef union {
    uint8_t uint8;
    int8_t int8;
    uint8_t bytes[1];
} zj_u8_t;

typedef union {
    uint16_t uint16;
    int16_t int16;
    uint8_t bytes[2];
} zj_u16_t;

typedef union {
    uint32_t uint32;
    int32_t int32;
    float float32;
    float double32;
    uint8_t bytes[4];
} zj_u32_t;

typedef union {
    uint64_t uint64;
    int64_t int64;
    uint8_t bytes[8];
} zj_u64_t;


typedef struct
{
    boolean focus : 1;
    boolean trigger : 1;
} Camera_t;