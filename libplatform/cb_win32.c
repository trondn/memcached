#include <windows.h>
#include <assert.h>
#include "platform_internal.h"

struct thread_execute {
    cb_thread_main_func func;
    void *argument;
};

static DWORD WINAPI platform_thread_wrap(LPVOID arg) {
    struct thread_execute *ctx = arg;
	assert(ctx);
    ctx->func(ctx->argument);
    free(ctx);
    return 0;
}


__declspec(dllexport)
int cb_create_thread(cb_thread_t *id,
                     void (*func)(void*arg),
                     void *arg,
                     int detached) {
	HANDLE handle;
    struct thread_execute *ctx = malloc(sizeof(struct thread_execute));
    if (ctx == NULL) {
        return -1;
    }

    ctx->func = func;
    ctx->argument = arg;

	handle = CreateThread(NULL, 0, platform_thread_wrap, ctx, 0, id);
	if (handle == INVALID_HANDLE_VALUE) {
		free(ctx);
		return -1;
	} else {
		if (detached) {
			CloseHandle(handle);
		}
	}

    return 0;
}

__declspec(dllexport)
int cb_join_thread(cb_thread_t id) {
	HANDLE handle = OpenThread(SYNCHRONIZE, FALSE, id);
	if (handle == NULL) {
		return -1;
	}
	WaitForSingleObject(handle, INFINITE);
	CloseHandle(handle);
	return 0;
}

__declspec(dllexport)
cb_thread_t cb_thread_self(void)
{
    return GetCurrentThreadId();
}

__declspec(dllexport)
void cb_mutex_initialize(cb_mutex_t* mutex)
{
    InitializeCriticalSection(mutex);
}

__declspec(dllexport)
void cb_mutex_destroy(cb_mutex_t* mutex)
{
    DeleteCriticalSection(mutex);
}

__declspec(dllexport)
void cb_mutex_enter(cb_mutex_t* mutex)
{
    EnterCriticalSection(mutex);
}

__declspec(dllexport)
void cb_mutex_exit(cb_mutex_t* mutex)
{
    LeaveCriticalSection(mutex);
}

__declspec(dllexport)
void cb_cond_initialize(cb_cond_t*cond) {
    InitializeConditionVariable(cond);
}

__declspec(dllexport)
void cb_cond_destroy(cb_cond_t*cond) {
    (void)cond;
}

__declspec(dllexport)
void cb_cond_wait(cb_cond_t *cond, cb_mutex_t *mutex) {
    SleepConditionVariableCS(cond, mutex, INFINITE);
}

__declspec(dllexport)
void cb_cond_signal(cb_cond_t *cond) {
    WakeConditionVariable(cond);
}

__declspec(dllexport)
void cb_cond_broadcast(cb_cond_t *cond) {
    WakeAllConditionVariable(cond);
}
