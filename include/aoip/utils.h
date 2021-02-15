#pragma once

#define MAX_IPV4_ASCII_SIZE 15
#define MAX_DEVICE_NAME_SIZ 32

#if defined(__x86_64__)
#define rmb() asm("lfence;");
#define wmb() asm("sfence;");
#else
#define rmb()
#define wmb()
#endif
