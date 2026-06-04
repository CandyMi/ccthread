/*
 * ccthread_naming.c — Assign debug names to threads
 *
 * Demonstrates: ccthread_set_name.
 *
 * Named threads are easier to identify in debuggers (gdb, lldb, WinDbg)
 * and system tools (top -H, htop, Process Explorer, /proc).
 *
 * Compile (POSIX):   cc -o naming examples/ccthread_naming.c ccthread.c -lpthread
 * Compile (MSVC):    cl examples\ccthread_naming.c ccthread.c
 */

#include "ccthread.h"
#include <stdio.h>
#ifdef _WIN32
  #include <process.h>
#else
  #include <unistd.h>
#endif

static void* worker(void* arg) {
    const char* name = (const char*)arg;

    /* Name the current thread — passing NULL means "me" */
    ccthread_set_name(NULL, name);

    printf("  [%s] working (pid=%d — check /proc or Activity Monitor)\n",
           name, getpid());
    ccthread_sleep(500);
    printf("  [%s] done\n", name);
    return NULL;
}

int main(void) {
    /* Name the main thread */
    ccthread_set_name(NULL, "main");

    ccthread_t* t1 = ccthread_create(worker, "io-worker");
    ccthread_t* t2 = ccthread_create(worker, "db-worker");
    ccthread_t* t3 = ccthread_create(worker, "net-worker");

#ifdef __linux__
    /* On Linux you can also name a thread from *outside* it */
    ccthread_set_name(t1, "io-renamed");
#endif

    ccthread_join(t1, NULL);
    ccthread_join(t2, NULL);
    ccthread_join(t3, NULL);

    printf("All workers finished.\n");
    return 0;
}
