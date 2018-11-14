#include <jni.h>
#include <unordered_map>
using namespace std;
extern "C"
{
#include "../interpreter.h"
#include "../pch.h"
#include "../io.h"

	void RuntimeSetupFunc(Picoc*pc)
	{

	}


	void __InternalCall(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs);


	void RuntimeLogW(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
	{
		struct StdVararg PrintfArgs;
		PrintfArgs.Param = Param;
		PrintfArgs.NumArgs = NumArgs - 1;
		const char*format = (const char*)Param[0]->Val->Pointer;
		int print_length = (int)(strlen(format)*1.5f) + 1;
		char*print_buffer = new char[print_length];
		ReturnValue->Val->Integer = StdioBasePrintf(Parser, NULL, print_buffer, print_length, (char*)Param[1]->Val->Pointer, &PrintfArgs);
		LOGW(print_buffer);
		delete print_buffer;
	}

	void RuntimeLogI(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
	{
		struct StdVararg PrintfArgs;
		PrintfArgs.Param = Param;
		PrintfArgs.NumArgs = NumArgs - 1;
		const char*format = (const char*)Param[0]->Val->Pointer;
		int print_length = (int)(strlen(format)*1.5f) + 1;
		char*print_buffer = new char[print_length];
		ReturnValue->Val->Integer = StdioBasePrintf(Parser, NULL, print_buffer, print_length, (char*)Param[1]->Val->Pointer, &PrintfArgs);
		LOGI(print_buffer);
		delete print_buffer;
	}

	void RuntimeLineno(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
	{
		ReturnValue->Val->Integer = Parser->Line;
	}

	/* list of all library functions and their prototypes */
	struct LibraryFunction UnixFunctions[] =
	{
		{ RuntimeLineno,    "int lineno();" },
		{ RuntimeLogW,      "int logw(char *, ...);" },
		{ RuntimeLogI,      "int logi(char *, ...);" },
		{ __InternalCall,    "int __internal_call(void*,int,char *,void *, ...);" },
							 //Picoc地址(权限验证),下标,类型标识,返回值地址,参数列表
		{ NULL,         NULL }
	};

	void PlatformLibraryInit(Picoc *pc)
	{
		IncludeRegister(pc, "runtime.h", &RuntimeSetupFunc, &UnixFunctions[0], NULL);
	}
}