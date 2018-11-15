#include <jni.h>
#include <unordered_map>
using namespace std;
extern "C"
{
#include "../interpreter.h"
#include "../pch.h"
#include "../print.h"

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
		char LogBuffer[1024];
		StdioBasePrintf(Parser, NULL, LogBuffer, 1024, (char*)Param[1]->Val->Pointer, &PrintfArgs);
		ReturnValue->Val->Integer = LOGW(LogBuffer);
	}

	void RuntimeLogI(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
	{
		struct StdVararg PrintfArgs;
		PrintfArgs.Param = Param;
		PrintfArgs.NumArgs = NumArgs - 1;
		const char*format = (const char*)Param[0]->Val->Pointer;
		char LogBuffer[1024];
		StdioBasePrintf(Parser, NULL, LogBuffer, 1024, (char*)Param[1]->Val->Pointer, &PrintfArgs);
		ReturnValue->Val->Integer = LOGI(LogBuffer);
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
		{ __InternalCall,   "int __internal_call(void*,int,char *,void *, ...);" },
		//带有“__”双下划线的都是运行时内部使用的代码（地址验证,下标,类型签名,返回值地址,参数列表）
		{ NULL,         NULL }
	};

	void PlatformLibraryInit(Picoc *pc)
	{
		IncludeRegister(pc, "runtime.h", &RuntimeSetupFunc, &UnixFunctions[0], NULL);
	}
}