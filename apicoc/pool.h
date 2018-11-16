#pragma once
#include "pch.h"
#include <stdlib.h>

BINGE_EXTERN_C
enum pool_type
{
	normal_t,
	manager_t,
};

typedef struct _pool_t * PoolManager;
extern void*  PoolMalloc(PoolManager m, size_t s);
extern void*  PoolCalloc(PoolManager m, size_t s1, size_t s2);
extern void*  PoolRealloc(PoolManager m, void *p, size_t s);
extern void   PoolFree(PoolManager m, void * p);
extern PoolManager PoolCreate(enum pool_type t);
extern void PoolDestroy(PoolManager m);

END_EXTERN_C