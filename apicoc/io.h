#pragma once
#include "pch.h"
#include <stdio.h>
#include "interpreter.h"
BINGE_EXTERN_C
//printf
/* our representation of varargs within picoc */
struct StdVararg
{
	struct Value **Param;
	int NumArgs;
};
int StdioBasePrintf(struct ParseState *Parser, FILE *Stream, char *StrOut, int StrOutLen, char *Format, struct StdVararg *Args);

END_EXTERN_C