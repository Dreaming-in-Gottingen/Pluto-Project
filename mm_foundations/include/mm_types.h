#ifndef _MM_TYPES_H_
#define _MM_TYPES_H_

typedef unsigned char MM_U8;
typedef signed char MM_S8;
typedef unsigned short MM_U16;
typedef short MM_S16;
typedef unsigned int MM_U32;
typedef int MM_S32;
typedef unsigned long long MM_U64;
typedef signed long long MM_S64;
typedef char MM_BOOL;
typedef void* MM_PTR;

#ifndef NULL
#define NULL ((void*)0)
#endif

#define MM_FALSE  0
#define MM_TRUE   1

enum {
    MM_OK               =   0,
    MM_ERROR            =   -1,

    MM_ERROR_UNSUPPORT  =   -2,
    MM_ERROR_NO_MEMORY  =   -3,
};

struct list_head {
    struct list_head *next, *prev;
};

#endif
