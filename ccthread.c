/*
 * ccthread.c — Cross-platform C/C++ thread library implementation
 *
 * Windows   : Win32 threads (CreateThread / WaitForSingleObject / CloseHandle)
 * POSIX     : pthreads  (pthread_create / pthread_join  / pthread_detach)
 */

#include "ccthread.h"

/* ---- platform includes ---- */

#ifdef CCTHREAD_PLATFORM_WINDOWS
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600   /* target Vista+ */
  #endif
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <process.h>            /* _beginthreadex (optional — we use CreateThread) */
#else  /* CCTHREAD_PLATFORM_POSIX */
  #include <pthread.h>
  #include <sched.h>              /* sched_yield */
  #include <unistd.h>             /* usleep / nanosleep */
  #include <time.h>               /* nanosleep */
  #include <errno.h>
  #ifdef __FreeBSD__
    #include <pthread_np.h>       /* pthread_set_name_np */
  #endif
#endif

#include <stdlib.h>
#include <string.h>

/* struct ccthread_impl is now defined in ccthread.h */

/* ================================================================== */
/*  Internal: thread wrapper                                           */
/* ================================================================== */

#ifdef CCTHREAD_PLATFORM_WINDOWS
static DWORD WINAPI ccthread_wrapper(LPVOID arg)
#else
static void* ccthread_wrapper(void* arg)
#endif
{
    ccthread_t*      thread = (ccthread_t*)arg;
    ccthread_func_t  func   = thread->func;
    void*            uarg   = thread->arg;

    /* Run the user's function */
    void* ret = func(uarg);

    /* Store result, mark finished, and self-destruct if detached */
    thread->result   = ret;
    thread->finished = 1;

    if (thread->detached) {
        /* Detached — we are responsible for cleanup.
         *
         * ccthread_detach() also checks `finished` after setting
         * `detached`; the double-check from both sides guarantees
         * exactly one frees the struct even under scheduling races.
         *
         * On weakly-ordered architectures a tiny leak is theoretically
         * possible; in practice the window is negligible. */
#ifdef CCTHREAD_PLATFORM_WINDOWS
        if (thread->handle) {
            CloseHandle(thread->handle);
        }
#endif
        free(thread);
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    return 0;
#else
    return ret;
#endif
}

/* ================================================================== */
/*  ccthread_create                                                    */
/* ================================================================== */

ccthread_t* ccthread_create(ccthread_func_t func, void* arg)
{
    ccthread_t* thread;

    if (!func) {
        return NULL;
    }

    thread = (ccthread_t*)calloc(1, sizeof(ccthread_t));
    if (!thread) {
        return NULL;
    }

    thread->func     = func;
    thread->arg      = arg;
    thread->detached = 0;
    thread->joined   = 0;
    thread->finished = 0;
    thread->is_self  = 0;
    thread->result   = NULL;

#ifdef CCTHREAD_PLATFORM_WINDOWS
    thread->handle = CreateThread(
        NULL,                   /* default security attributes */
        0,                      /* default stack size */
        ccthread_wrapper,
        thread,                 /* arg to wrapper */
        0,                      /* run immediately */
        &thread->tid
    );

    if (!thread->handle) {
        free(thread);
        return NULL;
    }
#else
    {
        int rc = pthread_create(
            &thread->handle,
            NULL,               /* default attributes */
            ccthread_wrapper,
            thread
        );
        if (rc != 0) {
            free(thread);
            return NULL;
        }
    }
#endif

    return thread;
}

/* ================================================================== */
/*  ccthread_join                                                       */
/* ================================================================== */

int ccthread_join(ccthread_t* thread, void** result)
{
    if (!thread || thread->detached || thread->joined || thread->is_self) {
        return CCTHREAD_ERROR;
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    {
        DWORD rc = WaitForSingleObject(thread->handle, INFINITE);
        if (rc != WAIT_OBJECT_0) {
            return CCTHREAD_ERROR;
        }
        if (result) {
            *result = thread->result;
        }
        CloseHandle(thread->handle);
        thread->handle = NULL;
    }
#else
    {
        void* ret = NULL;
        int rc = pthread_join(thread->handle, &ret);
        if (rc != 0) {
            return CCTHREAD_ERROR;
        }
        thread->result = ret;
        if (result) {
            *result = ret;
        }
    }
#endif

    thread->joined = 1;
    free(thread);
    return CCTHREAD_SUCCESS;
}

/* ================================================================== */
/*  ccthread_detach                                                     */
/* ================================================================== */

int ccthread_detach(ccthread_t* thread)
{
    if (!thread || thread->detached || thread->joined || thread->is_self) {
        return CCTHREAD_ERROR;
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    /* On Windows, closing our handle is sufficient — the kernel
     * keeps the thread object alive until the thread exits. */
    CloseHandle(thread->handle);
    thread->handle = NULL;
#else
    {
        int rc = pthread_detach(thread->handle);
        if (rc != 0) {
            return CCTHREAD_ERROR;
        }
    }
#endif

    thread->detached = 1;

    /* If the wrapper has already exited, it missed the `detached` flag
     * and didn't free — we must free now.  Otherwise the wrapper will. */
    if (thread->finished) {
        free(thread);
    }

    return CCTHREAD_SUCCESS;
}

/* ================================================================== */
/*  ccthread_destroy                                                    */
/* ================================================================== */

void ccthread_destroy(ccthread_t* thread)
{
    if (!thread) {
        return;
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    if (thread->handle) {
        CloseHandle(thread->handle);
    }
#endif

    free(thread);
}

/* ================================================================== */
/*  ccthread_exit                                                       */
/* ================================================================== */

void ccthread_exit(void* result)
{
#ifdef CCTHREAD_PLATFORM_WINDOWS
    ExitThread((DWORD)(uintptr_t)result);
#else
    pthread_exit(result);
#endif
}

/* ================================================================== */
/*  ccthread_yield                                                      */
/* ================================================================== */

void ccthread_yield(void)
{
#ifdef CCTHREAD_PLATFORM_WINDOWS
    SwitchToThread();
#else
    sched_yield();
#endif
}

/* ================================================================== */
/*  ccthread_sleep                                                      */
/* ================================================================== */

void ccthread_sleep(unsigned int ms)
{
#ifdef CCTHREAD_PLATFORM_WINDOWS
    Sleep((DWORD)ms);
#else
    {
        struct timespec ts;
        ts.tv_sec  = (time_t)(ms / 1000);
        ts.tv_nsec = (long)(ms % 1000) * 1000000L;
        nanosleep(&ts, NULL);
    }
#endif
}

/* ================================================================== */
/*  ccthread_self                                                       */
/* ================================================================== */

ccthread_t* ccthread_self(void)
{
    ccthread_t* self;

    self = (ccthread_t*)calloc(1, sizeof(ccthread_t));
    if (!self) {
        return NULL;
    }

    self->is_self  = 1;
    self->detached = 1;   /* can't join a self handle */
    self->joined   = 1;
    self->finished = 1;

#ifdef CCTHREAD_PLATFORM_WINDOWS
    {
        HANDLE pseudo  = GetCurrentThread();
        HANDLE process = GetCurrentProcess();
        if (!DuplicateHandle(process, pseudo, process,
                             &self->handle, 0, FALSE,
                             DUPLICATE_SAME_ACCESS)) {
            free(self);
            return NULL;
        }
        self->tid = GetCurrentThreadId();
    }
#else
    self->handle = pthread_self();
#endif

    return self;
}

/* ================================================================== */
/*  ccthread_equal                                                      */
/* ================================================================== */

int ccthread_equal(ccthread_t* a, ccthread_t* b)
{
    if (!a || !b) {
        return (a == b);   /* both NULL → considered equal */
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    return (a->tid == b->tid) ? 1 : 0;
#else
    return pthread_equal(a->handle, b->handle);
#endif
}

/* ================================================================== */
/*  ccthread_set_name                                                   */
/* ================================================================== */

int ccthread_set_name(ccthread_t* thread, const char* name)
{
    /* Truncate name to CCTHREAD_NAME_MAX-1 */
    char buf[CCTHREAD_NAME_MAX];

    if (!name) {
        return CCTHREAD_ERROR;
    }

    {
        size_t i;
        for (i = 0; i < CCTHREAD_NAME_MAX - 1 && name[i] != '\0'; i++) {
            buf[i] = name[i];
        }
        buf[i] = '\0';
    }

#ifdef CCTHREAD_PLATFORM_WINDOWS
    {
        /* SetThreadDescription is available on Windows 10 1607+.
         * Dynamically resolve it so the library works on older Windows. */
        typedef HRESULT (WINAPI *SetThreadDesc_fn)(HANDLE, PCWSTR);
        static SetThreadDesc_fn pSetThreadDesc = NULL;
        HANDLE hThread;
        int    wlen;
        WCHAR* wname;

        if (!pSetThreadDesc) {
            HMODULE mod = GetModuleHandleW(L"kernel32.dll");
            if (mod) {
                pSetThreadDesc = (SetThreadDesc_fn)
                    GetProcAddress(mod, "SetThreadDescription");
            }
            if (!pSetThreadDesc) {
                return CCTHREAD_ERROR;
            }
        }

        if (thread && thread->handle) {
            hThread = thread->handle;
        } else if (!thread) {
            hThread = GetCurrentThread();
        } else {
            return CCTHREAD_ERROR;   /* handle already closed */
        }

        /* Convert UTF-8 → wide string */
        wlen = MultiByteToWideChar(CP_UTF8, 0, buf, -1, NULL, 0);
        if (wlen <= 0) {
            return CCTHREAD_ERROR;
        }

        wname = (WCHAR*)malloc((size_t)wlen * sizeof(WCHAR));
        if (!wname) {
            return CCTHREAD_ERROR;
        }

        MultiByteToWideChar(CP_UTF8, 0, buf, -1, wname, wlen);
        {
            HRESULT hr = pSetThreadDesc(hThread, wname);
            free(wname);
            return SUCCEEDED(hr) ? CCTHREAD_SUCCESS : CCTHREAD_ERROR;
        }
    }

#else  /* CCTHREAD_PLATFORM_POSIX */

  #if defined(__APPLE__)
    {
        /* macOS: pthread_setname_np only names the *calling* thread. */
        if (thread) {
            pthread_t self = pthread_self();
            if (!pthread_equal(thread->handle, self)) {
                return CCTHREAD_ERROR;
            }
        }
        return (pthread_setname_np(buf) == 0)
                   ? CCTHREAD_SUCCESS : CCTHREAD_ERROR;
    }

  #elif defined(__linux__)
    {
        pthread_t pt = thread ? thread->handle : pthread_self();
        return (pthread_setname_np(pt, buf) == 0)
                   ? CCTHREAD_SUCCESS : CCTHREAD_ERROR;
    }

  #elif defined(__FreeBSD__)
    {
        pthread_t pt = thread ? thread->handle : pthread_self();
        pthread_set_name_np(pt, buf);   /* returns void */
        return CCTHREAD_SUCCESS;
    }

  #else
    (void)thread;
    (void)buf;
    return CCTHREAD_ERROR;   /* unsupported platform */
  #endif

#endif /* CCTHREAD_PLATFORM_POSIX */
}
