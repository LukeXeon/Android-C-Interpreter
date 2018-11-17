#include "helper.h"
#include <functional>
#include <unordered_map>
#include <string>
using namespace std;

struct Transform
{
	Transform() = delete;
	Transform(const Transform&) = delete;
	Transform(function<jobject(JNIEnv*, union AnyValue*)>&&toJObject,
		function<void(JNIEnv*, Picoc*, union AnyValue*, jobject)>&&toAnyVlaue)
		:toJObject(toJObject), toAnyVlaue(toAnyVlaue) {}
	const function<jobject(JNIEnv*, union AnyValue*)> toJObject;
	const function<void(JNIEnv*, Picoc*, union AnyValue*, jobject)> toAnyVlaue;
};

struct
{
	//field
	JavaVM*javaVM;
	//method
	unordered_map<char, Transform*> transforms;
	function<void(JNIEnv*,const char*)> throwIllegalEx;
	function<char*(JNIEnv*, jobject)> getStringHandler;
	function<bool(JNIEnv*, jobject, int)> checkStringIndex;
	function<jobjectArray(JNIEnv*, int)> newJObjectArray;
	function<jobject(JNIEnv*, char*)> newStringWrapper;
	function<void(JNIEnv*, jobject, int)> setDescriptor;
	function<jobject(JNIEnv*, jobject, int, jobjectArray)> onInvokeHandler;
} Helper;

static void LoadHelper(JNIEnv*env)
{
	jclass illegalExClass = (jclass)env->NewGlobalRef(env->FindClass("java/lang/IllegalStateException"));
	Helper.throwIllegalEx = [illegalExClass](JNIEnv*env, const char*message)->void
	{
		env->ThrowNew(illegalExClass, message);
	};

	jclass jobjectClass = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Object"));
	Helper.newJObjectArray = [jobjectClass](JNIEnv*env, int length)->jobjectArray
	{
		return env->NewObjectArray(length, jobjectClass, NULL);
	};

	jclass stringWrapperClass = (jclass)env->NewGlobalRef(env->FindClass("edu/guet/apicoc/ScriptingString"));
	jmethodID stringWrapperInit = env->GetMethodID(stringWrapperClass, "<init>", "(JI)V");
	Helper.newStringWrapper = [stringWrapperClass, stringWrapperInit](JNIEnv*env, char*str)->jobject
	{
		return env->NewObject(stringWrapperClass, stringWrapperInit, (jlong)str, (jint)strlen(str));
	};
	jfieldID stringHandlerField = env->GetFieldID(stringWrapperClass, "handler", "J");
	Helper.getStringHandler = [stringHandlerField](JNIEnv*env, jobject obj)->char*
	{
		return (char*)env->GetLongField(obj, stringHandlerField);
	};
	jfieldID maxCapacityField = env->GetFieldID(stringWrapperClass, "maxCapacity", "I");
	jclass indexExClass = (jclass)env->NewGlobalRef(env->FindClass("java/lang/IndexOutOfBoundsException"));
	Helper.checkStringIndex = [indexExClass, maxCapacityField](JNIEnv*env, jobject obj, jint index)->bool
	{
		jint maxCapacity = env->GetIntField(obj, maxCapacityField);
		if (index<0 || index>maxCapacity)
		{
			string message = "error index = ";
			message += index;
			env->ThrowNew(indexExClass, message.c_str());
			return false;
		}
		return true;
	};

	jfieldID fdField = env->GetFieldID(env->FindClass("java/io/FileDescriptor"), "descriptor", "I");
	Helper.setDescriptor = [fdField](JNIEnv*env, jobject fdObject, int fd)->void
	{
		env->SetIntField(fdObject, fdField, fd);
	};
	jmethodID invokeMethod = env->GetMethodID(env->FindClass("edu/guet/apicoc/ScriptRuntime"), "onInvoke",
		"(I[Ljava/lang/Object;)Ljava/lang/Object;");
	Helper.onInvokeHandler = [invokeMethod](JNIEnv*env, jobject target, int index, jobjectArray args)->jobject
	{
		return env->CallObjectMethod(target, invokeMethod, index, args);
	};

	jclass Zclass = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Boolean"));
	jclass Iclass = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Integer"));
	jclass Jclass = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Long"));
	jclass Dclass = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Double"));
	jclass Fclass = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Float"));
	jclass Cclass = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Character"));
	jclass Sclass = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Short"));
	jclass Bclass = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Byte"));

	jmethodID Zmethod = env->GetStaticMethodID(Zclass, "valueOf", "(Z)Ljava/lang/Boolean;");
	jmethodID Imethod = env->GetStaticMethodID(Iclass, "valueOf", "(I)Ljava/lang/Integer;");
	jmethodID Jmethod = env->GetStaticMethodID(Jclass, "valueOf", "(J)Ljava/lang/Long;");
	jmethodID Dmethod = env->GetStaticMethodID(Dclass, "valueOf", "(D)Ljava/lang/Double;");
	jmethodID Fmethod = env->GetStaticMethodID(Fclass, "valueOf", "(F)Ljava/lang/Float;");
	jmethodID Cmethod = env->GetStaticMethodID(Cclass, "valueOf", "(C)Ljava/lang/Character;");
	jmethodID Smethod = env->GetStaticMethodID(Sclass, "valueOf", "(S)Ljava/lang/Short;");
	jmethodID Bmethod = env->GetStaticMethodID(Bclass, "valueOf", "(B)Ljava/lang/Byte;");

	jmethodID Zmethod2 = env->GetMethodID(Zclass, "booleanValue", "()Z");
	jmethodID Imethod2 = env->GetMethodID(Iclass, "intValue", "()I");
	jmethodID Jmethod2 = env->GetMethodID(Jclass, "longValue", "()J");
	jmethodID Dmethod2 = env->GetMethodID(Dclass, "doubleValue", "()D");
	jmethodID Fmethod2 = env->GetMethodID(Fclass, "floatValue", "()F");
	jmethodID Cmethod2 = env->GetMethodID(Cclass, "charValue", "()C");
	jmethodID Smethod2 = env->GetMethodID(Sclass, "shortValue", "()S");
	jmethodID Bmethod2 = env->GetMethodID(Bclass, "byteValue", "()B");

	Helper.transforms.emplace('Z', new Transform([Zclass, Zmethod](JNIEnv* env, union AnyValue* val)->jobject
	{
		return env->CallStaticObjectMethod(Zclass, Zmethod, (jboolean)val->Integer);
	},
		[Zmethod2](JNIEnv* env, Picoc*, union AnyValue*val, jobject obj)->void
	{
		*((int*)val->Pointer) = env->CallBooleanMethod(obj, Zmethod2);
	}));
	Helper.transforms.emplace('I', new Transform([Iclass, Imethod](JNIEnv* env, union AnyValue* val)->jobject
	{
		return env->CallStaticObjectMethod(Iclass, Imethod, (jint)val->Integer);
	},
		[Imethod2](JNIEnv* env, Picoc*, union AnyValue*val, jobject obj)->void
	{
		*((int*)val->Pointer) = env->CallIntMethod(obj, Imethod2);
	}));
	Helper.transforms.emplace('J', new Transform([Jclass, Jmethod](JNIEnv* env, union AnyValue* val)->jobject
	{
		return env->CallStaticObjectMethod(Jclass, Jmethod, (jlong)val->LongInteger);
	},
		[Jmethod2](JNIEnv* env, Picoc*, union AnyValue*val, jobject obj)->void
	{
		*((long*)val->Pointer) = env->CallLongMethod(obj, Jmethod2);
	}));
	Helper.transforms.emplace('D', new Transform([Dclass, Dmethod](JNIEnv* env, union AnyValue* val)->jobject
	{
		return env->CallStaticObjectMethod(Dclass, Dmethod, (jdouble)val->FP);
	},
		[Dmethod2](JNIEnv* env, Picoc*, union AnyValue*val, jobject obj)->void
	{
		*((double*)val->Pointer) = env->CallDoubleMethod(obj, Dmethod2);
	}));
	Helper.transforms.emplace('F', new Transform([Fclass, Fmethod](JNIEnv* env, union AnyValue* val)->jobject
	{
		return env->CallStaticObjectMethod(Fclass, Fmethod, (jfloat)val->FP);
	},
		[Fmethod2](JNIEnv* env, Picoc*, union AnyValue*val, jobject obj)->void
	{
		*((double*)val->Pointer) = env->CallFloatMethod(obj, Fmethod2);
	}));
	Helper.transforms.emplace('C', new Transform([Cclass, Cmethod](JNIEnv* env, union AnyValue* val)->jobject
	{
		return env->CallStaticObjectMethod(Cclass, Cmethod, (jchar)val->UnsignedShortInteger);
	},
		[Cmethod2](JNIEnv* env, Picoc*, union AnyValue*val, jobject obj)->void
	{
		*((unsigned short*)val->Pointer) = env->CallCharMethod(obj, Cmethod2);
	}));
	Helper.transforms.emplace('S', new Transform([Sclass, Smethod](JNIEnv* env, union AnyValue* val)->jobject
	{
		return env->CallStaticObjectMethod(Sclass, Smethod, (jshort)val->ShortInteger);
	},
		[Smethod2](JNIEnv* env, Picoc*, union AnyValue*val, jobject obj)->void
	{
		*((short*)val->Pointer) = env->CallShortMethod(obj, Smethod2);
	}));
	Helper.transforms.emplace('B', new Transform([Bclass, Bmethod](JNIEnv* env, union AnyValue* val)->jobject
	{
		return env->CallStaticObjectMethod(Bclass, Bmethod, (jbyte)val->Character);
	},
		[Bmethod2](JNIEnv* env, Picoc*, union AnyValue*val, jobject obj)->void
	{
		*((char*)val->Pointer) = env->CallByteMethod(obj, Bmethod2);
	}));
	Helper.transforms.emplace('L', new Transform([](JNIEnv* env, union AnyValue* val)->jobject
	{
		return NewStringWrapper(env,(char*)val->Pointer);
	},
		[](JNIEnv* env, Picoc*pc, union AnyValue*val, jobject obj)->void
	{
		if (obj != nullptr)
		{
			*((char**)val->Pointer) = GetStringHandler(env, obj);
		}
	}));
	Helper.transforms.emplace('V', new Transform([](JNIEnv* env, union AnyValue* val)->jobject {return nullptr; },
		[](JNIEnv* env, Picoc*, union AnyValue*val, jobject obj)->void {}));

}



EXTERN_C
JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void *reserved)
{
	LOGI("apicoc lib load");
	Helper.javaVM = vm;
	JNIEnv*env = nullptr;
	vm->GetEnv((void**)(&env), JNI_VERSION_1_6);
	LoadHelper(env);
	return JNI_VERSION_1_6;
}

BEGIN_EXTERN_C
jobject ToJObject(char sig, JNIEnv*env, union AnyValue*val)
{
	return Helper.transforms[sig]->toJObject(env, val);
}

void ToAnyValue(char sig, JNIEnv*env, Picoc*pc, union AnyValue*val, jobject obj)
{
	Helper.transforms[sig]->toAnyVlaue(env, pc, val, obj);
}

Picoc* ToPicocHandler(jlong handler)
{
	return (Picoc*)handler;
}

void InitIO(Picoc * pc, FILE * streams[3])
{
	pc->CStdIn = streams[0];
	pc->CStdOut = streams[1];
	pc->CStdErr = streams[2];
}

void DumpStdIOToPipes(int c_io[3])
{
	dup2(c_io[0], STDIN_FILENO);
	dup2(c_io[1], STDOUT_FILENO);
	dup2(c_io[2], STDERR_FILENO);
}

void OpenStream(int c_io[3], FILE * streams[3])
{
	streams[0] = fdopen(c_io[0], "r");
	streams[1] = fdopen(c_io[1], "w");
	streams[2] = fdopen(c_io[1], "w");
	setvbuf(streams[1], NULL, _IONBF, 0);
	setvbuf(streams[2], NULL, _IONBF, 0);
}

void MapingIOToPipes(JNIEnv * env, int c_io[3], jobject j_fd[3])
{
	int stdin_pipe[2];
	int stdout_pipe[2];
	int stderr_pipe[2];
	pipe(stdin_pipe);
	pipe(stdout_pipe);
	pipe(stderr_pipe);
	c_io[0] = stdin_pipe[0];//r
	c_io[1] = stdout_pipe[1];//w
	c_io[2] = stderr_pipe[1];//w
	Helper.setDescriptor(env, j_fd[0], stdin_pipe[1]);//w
	Helper.setDescriptor(env, j_fd[1], stdout_pipe[0]);//r
	Helper.setDescriptor(env, j_fd[2], stderr_pipe[0]);//r
}

jlong ToJLong(void*ptr)
{
	return (jlong)ptr;
}

JNIEnv*GetCurrentEnv()
{
	JNIEnv*env;
	Helper.javaVM->GetEnv((void**)&env, JNI_VERSION_1_6);
	return env;
}

jobjectArray NewJObjectArray(JNIEnv*env,jsize length)
{
	return Helper.newJObjectArray(env,length);
}

jobject OnInvokeHandler(JNIEnv* env, jobject obj, int index, jobjectArray arg)
{
	return Helper.onInvokeHandler(env, obj, index, arg);
}

jobject NewStringWrapper(JNIEnv* env, char*str)
{
	return str == nullptr ? nullptr : Helper.newStringWrapper(env, str);
}

char* GetStringHandler(JNIEnv*env, jobject obj)
{
	return env->IsSameObject(obj, NULL) ? nullptr : Helper.getStringHandler(env, obj);
}

bool CheckStringIndex(JNIEnv*env,jobject obj,int index)
{
	return Helper.checkStringIndex(env, obj, index);
}

void ThrowIllegalEx(JNIEnv*env, const char*message)
{
	Helper.throwIllegalEx(env, message);
}
END_EXTERN_C