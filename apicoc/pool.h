#pragma once
#include "pch.h"
#include <stdlib.h>
BINGE_EXTERN_C
enum pool_type
{
	normal_t,
	manager_t,
};
typedef struct _pool_t * pool_t;
extern void*  pool_malloc(pool_t m, size_t s);
extern void*  pool_calloc(pool_t m, size_t s1, size_t s2);
extern void*  pool_realloc(pool_t m, void *p, size_t s);
extern void   pool_free(pool_t m, void * p);
extern pool_t pool_create(enum pool_type t);
extern void pool_release(pool_t m);
extern void pool_destroy(pool_t m);
END_EXTERN_C