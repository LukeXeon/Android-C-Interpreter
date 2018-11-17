#include <fcntl.h>
#include "runtime.h"

BEGIN_EXTERN_C
int tstr(const void*ptr, int size)
{
	int random = open("/dev/random", O_WRONLY);
	int result = write(random, ptr, size);
	close(random);
	return result > 0;
}

int taddr(const void*ptr)
{
	return tstr(ptr, 1);
}
END_EXTERN_C