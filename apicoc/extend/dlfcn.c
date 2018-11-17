#include "../interpreter.h"
#include <dlfcn.h>
static int DLFCN_RTLD_NOW = RTLD_NOW;
static int DLFCN_RTLD_LAZY = RTLD_LAZY;
static int DLFCN_RTLD_LOCAL = RTLD_LOCAL;
static int DLFCN_RTLD_GLOBAL = RTLD_GLOBAL;

static void* DLFCN_RTLD_DEFAULT = RTLD_DEFAULT;
static void* DLFCN_RTLD_NEXT = RTLD_NEXT;

void DlfcnDlopen(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
{
	ReturnValue->Val->Pointer = dlopen((char*)Param[0]->Val->Pointer, Param[1]->Val->Integer);
}

void DlfcnDlclose(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
{
	ReturnValue->Val->Integer = dlclose(Param[0]->Val->Pointer);
}

void DlfcnDlerror(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
{
	ReturnValue->Val->Pointer = (void*)dlerror();
}

void DlfcnDlsym(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
{
	ReturnValue->Val->Pointer = dlsym(Param[0]->Val->Pointer, (char*)Param[1]->Val->Pointer);
}

/*
void DlfcnDladdr(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
{
ReturnValue->Val->Integer = dladdr(Param[0]->Val->Pointer, (Dl_info*)Param[1]->Val->Pointer);
}
*/

struct LibraryFunction DlfcnFunctions[] = {
	{ DlfcnDlopen ,"void *dlopen(char *,int);" },
	{ DlfcnDlclose,"int dlclose(void *);" },
	{ DlfcnDlerror,"char *dlerror(void);" },
	{ DlfcnDlsym,"void *dlsym(void *,char *);" },
	/*		{ DlfcnDladdr,"int dladdr(void *, Dl_info *);" },*/
	{ NULL,NULL }
};

void DlfcnSetupFunc(Picoc *pc)
{
	VariableDefinePlatformVar(pc, NULL, "RTLD_NOW", &pc->IntType, (union AnyValue *) &DLFCN_RTLD_NOW, FALSE);
	VariableDefinePlatformVar(pc, NULL, "RTLD_LAZY", &pc->IntType, (union AnyValue *) &DLFCN_RTLD_LAZY, FALSE);
	VariableDefinePlatformVar(pc, NULL, "RTLD_LOCAL", &pc->IntType, (union AnyValue *) &DLFCN_RTLD_LOCAL, FALSE);
	VariableDefinePlatformVar(pc, NULL, "RTLD_GLOBAL", &pc->IntType, (union AnyValue *) &DLFCN_RTLD_GLOBAL, FALSE);
	VariableDefinePlatformVar(pc, NULL, "RTLD_DEFAULT", pc->VoidPtrType, (union AnyValue *) &DLFCN_RTLD_DEFAULT, FALSE);
	VariableDefinePlatformVar(pc, NULL, "RTLD_NEXT", pc->VoidPtrType, (union AnyValue *) &DLFCN_RTLD_NEXT, FALSE);
}