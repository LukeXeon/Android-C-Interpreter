#include <jni.h>
#include <unordered_map>
using namespace std;
extern "C"
{
#include "../interpreter.h"
#include "../pch.h"
#include "../cstdlib/stdio0.h"

	void RuntimeSetupFunc(Picoc*pc)
	{

	}

	void Runtime__Handler(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
	{
		JNIEnv *env = nullptr;
		Parser->pc->JVM->GetEnv((void**)&env, JNI_VERSION_1_6);
		if (env == nullptr)
		{
			return;
		}
	}

	void Runtime__LogW(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
	{
		struct StdVararg PrintfArgs;
		PrintfArgs.Param = Param + 1;
		PrintfArgs.NumArgs = NumArgs - 2;
		const char*format = (const char*)Param[0]->Val->Pointer;
		int print_length = (int)(strlen(format)*1.5f) + 1;
		char*print_buffer = new char[print_length];
		ReturnValue->Val->Integer = StdioBasePrintf(Parser, NULL, print_buffer, print_length, (char*)Param[1]->Val->Pointer, &PrintfArgs);
		LOGW(print_buffer);
		delete print_buffer;
	}

	void Runtime__LogI(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
	{
		struct StdVararg PrintfArgs;
		PrintfArgs.Param = Param + 1;
		PrintfArgs.NumArgs = NumArgs - 2;
		const char*format = (const char*)Param[0]->Val->Pointer;
		int print_length = (int)(strlen(format)*1.5f) + 1;
		char*print_buffer = new char[print_length];
		ReturnValue->Val->Integer = StdioBasePrintf(Parser, NULL, print_buffer, print_length, (char*)Param[1]->Val->Pointer, &PrintfArgs);
		LOGI(print_buffer);
		delete print_buffer;
	}

	void Runtime__Lineno(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
	{
		ReturnValue->Val->Integer = Parser->Line;
	}

	/* list of all library functions and their prototypes */
	struct LibraryFunction UnixFunctions[] =
	{
		{ Runtime__Lineno,    "int __lineno();" },
		{ Runtime__LogW,      "int __logw(char *, ...);" },
		{ Runtime__LogI,      "int __logi(char *, ...);" },
		{ Runtime__Handler,    "int __handler(char *,void *, ...);" },
		{ NULL,         NULL }
	};

	void PlatformLibraryInit(Picoc *pc)
	{
		IncludeRegister(pc, "runtime.h", &RuntimeSetupFunc, &UnixFunctions[0], NULL);
	}
}