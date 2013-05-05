#include <windows.h>
#include <assert.h>
#include <stdio.h>
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

static const char *get_dll_name(const char *path, char *buffer) {
    char *ptr = strstr(path, ".dll");
    if (ptr != NULL) {
        return path;
    }

    strcpy(buffer, path);

    ptr = strstr(buffer, ".so");
    if (ptr != NULL) {
        sprintf(ptr, ".dll");
        return buffer;
    }

    strcat(buffer, ".dll");
    return buffer;
}

__declspec(dllexport)
cb_dlhandle_t cb_dlopen(const char *library, char **errmsg) {
    cb_dlhandle_t handle;
    char *buffer;

    if (library == NULL) {
        if (errmsg != NULL) {
            *errmsg = _strdup("Open self is not supported");
        }
        return NULL;
    }

    buffer = malloc(strlen(library) + 20);
    if (buffer == NULL) {
        if (*errmsg) {
            *errmsg = _strdup("Failed to allocate memory");
        }
        return NULL;
    }

    handle = LoadLibrary(get_dll_name(library, buffer));
    if (handle == NULL && errmsg != NULL) {
        DWORD err = GetLastError();
        LPVOID error_msg;

        if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                          FORMAT_MESSAGE_FROM_SYSTEM |
                          FORMAT_MESSAGE_IGNORE_INSERTS,
                          NULL, err, 0, (LPTSTR)&error_msg, 0, NULL) != 0) {
            *errmsg = _strdup(error_msg);
            LocalFree(error_msg);
        } else {
            *errmsg = _strdup("Failed to get error message");
        }
    }

    free(buffer);

    return handle;
}

__declspec(dllexport)
void *cb_dlsym(cb_dlhandle_t handle, const char *symbol, char **errmsg) {
    void *ret = GetProcAddress(handle, symbol);
    if (ret == NULL && errmsg) {
        DWORD err = GetLastError();
        LPVOID error_msg;

        if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                          FORMAT_MESSAGE_FROM_SYSTEM |
                          FORMAT_MESSAGE_IGNORE_INSERTS,
                          NULL, err, 0, (LPTSTR)&error_msg, 0, NULL) != 0) {
            *errmsg = _strdup(error_msg);
            LocalFree(error_msg);
        } else {
            *errmsg = _strdup("Failed to get error message");
        }
    }

    return ret;
}

__declspec(dllexport)
void cb_dlclose(cb_dlhandle_t handle) {
    FreeLibrary(handle);
}
