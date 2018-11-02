#include "apicoc.h"
#include "interpreter.h"

#define PICOC_STACK_SIZE (128*1024)              /* space for the the stack */


static struct Caches {
	//fd
	jfieldID descriptorField;
	//
} caches;

int main(int argc,char **argv) {
	printf("%s", "1212121");
	fflush(stdout);
	return 10086;
}


static void SetFileDescriptorOfFD(JNIEnv *env, jobject fileDescriptor, int value) {
	env->SetIntField(fileDescriptor, caches.descriptorField, value);
}

static Picoc*Cast(jlong ptr) {
	return (Picoc *) ptr;
}

static jlong ToLong(Picoc*ptr) {
	return (jlong) ptr;
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
	SetFileDescriptorOfFD(env, stdinFd, stdin_pipe[1]);
	picoc->CStdOut = fdopen(stdout_pipe[1], "w");
	SetFileDescriptorOfFD(env, stdoutFd, stdout_pipe[0]);
	picoc->CStdErr = fdopen(stderr_pipe[1], "w");
	SetFileDescriptorOfFD(env, stderrFd, stderr_pipe[0]);
	return ToLong(picoc);
}

EXTERN_C
JNIEXPORT void JNICALL
Java_edu_guet_apicoc_Interpreter_staticInit0(JNIEnv *env, jclass type) {
	jclass fileDescriptorClass =
			(jclass) (env->NewGlobalRef(env->FindClass("java/io/FileDescriptor")));
	caches.descriptorField =
			env->GetFieldID(fileDescriptorClass, "descriptor", "I");
}



EXTERN_C
JNIEXPORT void JNICALL
Java_edu_guet_apicoc_Interpreter_addSource0(JNIEnv *env, jclass type, jlong ptr, jstring ramdonName_, jstring source_) {
	LOGI("add source");
	const char *ramdonName = env->GetStringUTFChars(ramdonName_, 0);
	const char *source = env->GetStringUTFChars(source_, 0);
	PicocParse(Cast(ptr), ramdonName,
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
	Picoc *picoc = Cast(ptr);
	PicocCleanup(picoc);
}

EXTERN_C
JNIEXPORT void JNICALL
Java_edu_guet_apicoc_Interpreter_close0(JNIEnv *env, jclass type, jlong ptr) {
	if (ptr == 0) {
		return;
	}
	LOGI("picoc close");
	Picoc *pc = Cast(ptr);
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
	Picoc *picoc = Cast(ptr);
	jsize size = env->GetArrayLength(args);
	char **cargs = new char *[size];
	for (jsize i = 0; i < size; i++) {
		cargs[i] = (char *) env->GetStringUTFChars((jstring) env->GetObjectArrayElement(args, i),
												   JNI_FALSE);
	}
	picoc->JavaEnv = env;
	PicocCallMain(Cast(ptr), size, cargs);
	for (jsize i = 0; i < size; i++) {
		env->ReleaseStringUTFChars((jstring) env->GetObjectArrayElement(args, i), cargs[i]);
		cargs[i] = nullptr;
	}
	picoc->JavaEnv = nullptr;
	delete cargs;
	return picoc->PicocExitValue;
}

EXTERN_C
JNIEXPORT void JNICALL
Java_edu_guet_apicoc_Interpreter_addFile0(JNIEnv *env, jclass type, jlong ptr, jstring path_) {
	LOGI("add File");
	const char *path = env->GetStringUTFChars(path_, 0);
	PicocPlatformScanFile(Cast(ptr), path);
	env->ReleaseStringUTFChars(path_, path);
}