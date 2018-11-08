#pragma once
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
	enum pool_type
	{
		normal_t,
		manager_t,
	};
	typedef struct _pool_t * pool_t;
	extern __mallocfunc void*  pool_malloc(pool_t, size_t);
	extern __mallocfunc void*  pool_calloc(pool_t, size_t, size_t);
	extern void*  pool_realloc(pool_t, void *, size_t);
	extern void   pool_free(pool_t, void *);
	extern pool_t pool_create(pool_type);
	extern void pool_destroy(pool_t);
#ifdef __cplusplus
}
#endif