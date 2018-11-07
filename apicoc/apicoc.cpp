#include "apicoc.h"
#include "interpreter.h"

#define PICOC_STACK_SIZE (128*1024)              /* space for the the stack */


struct CatchInfo {
	struct sigaction oldAction;
	sigjmp_buf jumpBuffer;
};

static struct {
	JavaVM*javaVM;
	jfieldID descriptorField;
	jclass accessExClass;
	mutex catchInfoMutex;
	map<pid_t, CatchInfo> catchInfo;
} caches;

static void SetFd(JNIEnv *env, jobject fileDescriptor, int value) {
	env->SetIntField(fileDescriptor, caches.descriptorField, value);
}

static Picoc*ToPtr(jlong ptr) {
	return (Picoc *)ptr;
}

static jlong ToLong(Picoc*ptr) {
	return (jlong)ptr;
}

EXTERN_C
JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void *reserved)
{
	LOGI("%s", "apicoc lib load");
	caches.javaVM = vm;
	JNIEnv*env = nullptr;
	vm->GetEnv((void**)(&env), JNI_VERSION_1_6);
	caches.descriptorField =
		env->GetFieldID(env->FindClass("java/io/FileDescriptor"), "descriptor", "I");
	caches.accessExClass =
		(jclass)env->NewGlobalRef(env->FindClass("java/lang/IllegalAccessException"));
	return JNI_VERSION_1_6;
}

EXTERN_C
JNIEXPORT jint JNICALL
Java_edu_guet_apicoc_Interpreter_createSub0(JNIEnv *env, jclass type, jobjectArray srcNames,
	jobjectArray srcOrFile, jobjectArray args,
	jobject stdinFd, jobject stdoutFd, jobject stderrFd) {
	int child_pid;
	int stdin_pipe[2];
	int stdout_pipe[2];
	int stderr_pipe[2];
	int src_or_file_count = env->GetArrayLength(srcOrFile);
	int argc = env->GetArrayLength(args);
	char **c_args = new char *[argc];
	char **src_or_file_path = new char *[src_or_file_count];
	char **c_src_names = nullptr;
	pipe(stdin_pipe);
	pipe(stdout_pipe);
	pipe(stderr_pipe);
	SetFd(env, stdinFd, stdin_pipe[1]);
	SetFd(env, stdoutFd, stdout_pipe[0]);
	SetFd(env, stderrFd, stderr_pipe[0]);
	if (srcNames != nullptr) {
		c_src_names = new char *[src_or_file_count];
		for (int i = 0; i < src_or_file_count; ++i) {
			jstring jstr = (jstring)env->GetObjectArrayElement(srcNames, i);
			const char *jstr_chars = env->GetStringUTFChars(jstr, 0);
			c_src_names[i] = strcpy(new char[strlen(jstr_chars) + 1], jstr_chars);
			env->ReleaseStringUTFChars(jstr, jstr_chars);
		}
	}
	for (int i = 0; i < src_or_file_count; ++i) {
		jstring jstr = (jstring)env->GetObjectArrayElement(srcOrFile, i);
		const char *jstr_chars = env->GetStringUTFChars(jstr, 0);
		src_or_file_path[i] = strcpy(new char[strlen(jstr_chars) + 1], jstr_chars);
		env->ReleaseStringUTFChars(jstr, jstr_chars);
	}
	for (int i = 0; i < argc; ++i) {
		jstring jstr = (jstring)env->GetObjectArrayElement(args, i);
		const char *jstr_chars = env->GetStringUTFChars(jstr, 0);
		c_args[i] = strcpy(new char[strlen(jstr_chars) + 1], jstr_chars);
		env->ReleaseStringUTFChars(jstr, jstr_chars);
	}
	while ((child_pid = fork()) == -1);
	if (child_pid != 0) {
		if (c_src_names != nullptr) {
			for (int i = 0; i < src_or_file_count; ++i) {
				delete c_src_names[i];
			}
			delete c_src_names;
		}
		for (int i = 0; i < src_or_file_count; ++i) {
			delete src_or_file_path[i];
		}
		delete src_or_file_path;
		for (int i = 0; i < argc; ++i) {
			delete c_args[i];
		}
		delete c_args;
		return child_pid;
	}
	else {
		Picoc *picoc = new Picoc();
		PicocInitialise(picoc, getenv("STACKSIZE") ? atoi(getenv("STACKSIZE")) : PICOC_STACK_SIZE);
		PicocIncludeAllSystemHeaders(picoc);
		picoc->CStdIn = fdopen(stdin_pipe[0], "r");
		dup2(stdin_pipe[0], STDIN_FILENO);
		picoc->CStdOut = fdopen(stdout_pipe[1], "w");
		setvbuf(picoc->CStdOut, NULL, _IONBF, 0);
		dup2(stdout_pipe[1], STDOUT_FILENO);
		picoc->CStdErr = fdopen(stderr_pipe[1], "w");
		setvbuf(picoc->CStdErr, NULL, _IONBF, 0);
		dup2(stderr_pipe[1], STDERR_FILENO);//重定向到管道
		if (c_src_names != nullptr) {
			for (int i = 0; i < src_or_file_count; ++i) {
				PicocParse(picoc, c_src_names[i],
					src_or_file_path[i],
					(int)strlen(src_or_file_path[i]),
					true, false, true, true);
			}
		}
		else {
			for (int i = 0; i < src_or_file_count; ++i) {
				PicocPlatformScanFile(picoc, src_or_file_path[i]);
			}
		}
		PicocCallMain(picoc, argc, c_args);
		exit(picoc->PicocExitValue);
		//不用做清理内存操作
	}
}

EXTERN_C
JNIEXPORT jint JNICALL
Java_edu_guet_apicoc_Interpreter_waitSub0(JNIEnv *env, jclass type, jint pid) {
	int status;
	waitpid(pid, &status, 0);
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

EXTERN_C
JNIEXPORT void JNICALL
Java_edu_guet_apicoc_Interpreter_killSub0(JNIEnv *env, jclass type, jint pid) {
	kill(pid, SIGKILL);
}

EXTERN_C
JNIEXPORT jlong JNICALL
Java_edu_guet_apicoc_Interpreter_init0(JNIEnv *env, jclass type, jobject stdinFd, jobject stdoutFd,
	jobject stderrFd) {
	LOGI("picoc init");
	Picoc *picoc = new Picoc();
	PicocInitialise(picoc, getenv("STACKSIZE") ? atoi(getenv("STACKSIZE")) : PICOC_STACK_SIZE);
	PicocIncludeAllSystemHeaders(picoc);
	int stdin_pipe[2];
	int stdout_pipe[2];
	int stderr_pipe[2];
	pipe(stdin_pipe);
	pipe(stdout_pipe);
	pipe(stderr_pipe);
	picoc->CStdIn = fdopen(stdin_pipe[0], "r");
	SetFd(env, stdinFd, stdin_pipe[1]);
	picoc->CStdOut = fdopen(stdout_pipe[1], "w");
	setvbuf(picoc->CStdOut, NULL, _IONBF, 0);
	SetFd(env, stdoutFd, stdout_pipe[0]);
	picoc->CStdErr = fdopen(stderr_pipe[1], "w");
	setvbuf(picoc->CStdErr, NULL, _IONBF, 0);
	SetFd(env, stderrFd, stderr_pipe[0]);
	return ToLong(picoc);
}

EXTERN_C
JNIEXPORT void JNICALL
Java_edu_guet_apicoc_Interpreter_addSource0(JNIEnv *env, jclass type, jlong ptr, jstring ramdonName_, jstring source_) {
	LOGI("add source");
	const char *ramdonName = env->GetStringUTFChars(ramdonName_, 0);
	const char *source = env->GetStringUTFChars(source_, 0);
	PicocParse(ToPtr(ptr), ramdonName,
		env->GetStringUTFChars(source_, 0),
		env->GetStringLength(source_),
		true, false, true, true);
	env->ReleaseStringUTFChars(ramdonName_, ramdonName);
	env->ReleaseStringUTFChars(source_, source);
}

EXTERN_C
JNIEXPORT void JNICALL
Java_edu_guet_apicoc_Interpreter_cleanup0(JNIEnv *env, jclass type, jlong ptr) {
	if (ptr == 0) {
		return;
	}
	LOGI("picoc cleanup");
	Picoc *picoc = ToPtr(ptr);
	PicocCleanup(picoc);
}

EXTERN_C
JNIEXPORT void JNICALL
Java_edu_guet_apicoc_Interpreter_close0(JNIEnv *env, jclass type, jlong ptr) {
	if (ptr == 0) {
		return;
	}
	LOGI("picoc close");
	Picoc *pc = ToPtr(ptr);
	close(fileno(pc->CStdIn));
	close(fileno(pc->CStdOut));
	close(fileno(pc->CStdErr));
	Java_edu_guet_apicoc_Interpreter_cleanup0(env, type, ptr);
	delete pc;
}

EXTERN_C
JNIEXPORT jint JNICALL
Java_edu_guet_apicoc_Interpreter_callMain0(JNIEnv *env, jclass type, jlong ptr, jobjectArray args) {
	LOGI("picoc call main");
	struct sigaction act;
	pid_t pid = getpid();
	Picoc *picoc = ToPtr(ptr);
	caches.catchInfoMutex.lock();
	CatchInfo &catchInfo = caches.catchInfo.emplace(pid, CatchInfo()).first->second;
	caches.catchInfoMutex.unlock();
	act.sa_handler = [](int sig) -> void {
		CatchInfo &info = caches.catchInfo[getpid()];
		sigaction(SIGSEGV, &(info.oldAction), NULL);
		siglongjmp(info.jumpBuffer, 1);
	};
	sigaction(SIGSEGV, &act, &(catchInfo.oldAction));
	jsize size = env->GetArrayLength(args);
	char **cargs = new char *[size];
	for (jsize i = 0; i < size; i++) {
		cargs[i] = (char *)env->GetStringUTFChars((jstring)env->GetObjectArrayElement(args, i),
			JNI_FALSE);
	}
	if (sigsetjmp(catchInfo.jumpBuffer, 1) == 0) {
		PicocCallMain(ToPtr(ptr), size, cargs);
	}
	else {
		for (jsize i = 0; i < size; i++) {
			env->ReleaseStringUTFChars((jstring)env->GetObjectArrayElement(args, i), cargs[i]);
			cargs[i] = nullptr;
		}
		delete cargs;
		caches.catchInfoMutex.lock();
		caches.catchInfo.erase(pid);
		caches.catchInfoMutex.unlock();
		env->ThrowNew(caches.accessExClass, "signal : SIGSEGV");
	}
	for (jsize i = 0; i < size; i++) {
		env->ReleaseStringUTFChars((jstring)env->GetObjectArrayElement(args, i), cargs[i]);
		cargs[i] = nullptr;
	}
	delete cargs;
	return picoc->PicocExitValue;
}

EXTERN_C
JNIEXPORT void JNICALL
Java_edu_guet_apicoc_Interpreter_addFile0(JNIEnv *env, jclass type, jlong ptr, jstring path_) {
	LOGI("add File");
	const char *path = env->GetStringUTFChars(path_, 0);
	PicocPlatformScanFile(ToPtr(ptr), path);
	env->ReleaseStringUTFChars(path_, path);
}