#pragma once
#include "../pch.h"
#include "../interpreter.h"
#include <stdio.h>
BEGIN_EXTERN_C
//addr
int tstr(const void * ptr, int size);

int taddr(const void * ptr);

//printf
/* our representation of varargs within picoc */
struct StdVararg
{
	struct Value **Param;
	int NumArgs;
};
END_EXTERN_C