#include "../interpreter.h"

void UnixSetupFunc()
{    
}

void CLogw(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
{
	LOGW(Param[0]->Val->Pointer);
}

void CLogi(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
{
	LOGI(Param[0]->Val->Pointer);
}

void Clineno (struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs) 
{
    ReturnValue->Val->Integer = Parser->Line;
}

/* list of all library functions and their prototypes */
struct LibraryFunction UnixFunctions[] =
{
    { Clineno,      "int lineno();" },
	{ CLogw,        "void logw(char *);" },
	{ CLogi,        "void logi(char *);" },
    { NULL,         NULL }
};

void PlatformLibraryInit(Picoc *pc)
{
    IncludeRegister(pc, "picoc_unix.h", &UnixSetupFunc, &UnixFunctions[0], NULL);
}
