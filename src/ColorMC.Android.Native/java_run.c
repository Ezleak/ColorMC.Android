/*
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */
#include <android/log.h>
#include <dlfcn.h>
#include <errno.h>
#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
 // Boardwalk: missing include
#include <string.h>

#include "log.h"

// Uncomment to try redirect signal handling to JVM
// #define TRY_SIG2JVM

#define FULL_VERSION "1.8.0-internal"
#define DOT_VERSION "1.8"

static const char* const_progname = "java";
static const char* const_launcher = "openjdk";
static const char** const_jargs = NULL;
static const char** const_appclasspath = NULL;
static const jboolean const_javaw = JNI_FALSE;
static const jboolean const_cpwildcard = JNI_TRUE;
static const jint const_ergo_class = 0; // DEFAULT_POLICY
static struct sigaction old_sa[NSIG];

void (*__old_sa)(int signal, siginfo_t* info, void* reserved);
int (*JVM_handle_linux_signal)(int signo, siginfo_t* siginfo, void* ucontext, int abort_if_unrecognized);

void android_sigaction(int signal, siginfo_t* info, void* reserved) 
{
    printf("process killed with signal %d code %p addr %p\n", signal, info->si_code, info->si_addr);
    if (JVM_handle_linux_signal == NULL) 
    { 
        // should not happen, but still
        __old_sa = old_sa[signal].sa_sigaction;
        __old_sa(signal, info, reserved);
        exit(1);
    }
    else 
    {
        // Preserve errno value over signal handler.
        int orig_errno = errno;  
        JVM_handle_linux_signal(signal, info, reserved, true);
        errno = orig_errno;
    }
}

typedef jint JNI_CreateJavaVM_func(JavaVM** pvm, void** penv, void* args);

typedef jint JLI_Launch_func(int argc, char** argv, /* main argc, argc */
    int jargc, const char** jargv,          /* java args */
    int appclassc, const char** appclassv,  /* app classpath */
    const char* fullversion,                /* full version defined */
    const char* dotversion,                 /* dot version defined */
    const char* pname,                      /* program name */
    const char* lname,                      /* launcher name */
    jboolean javaargs,                      /* JAVA_ARGS */
    jboolean cpwildcard,                    /* classpath wildcard*/
    jboolean javaw,                         /* windows-only javaw */
    jint ergo                               /* ergonomics class policy */
);

static jint launchJVM(int margc, char** margv) 
{
    void* libjli = dlopen("libjli.so", RTLD_LAZY | RTLD_GLOBAL);

    // Boardwalk: silence
    // LOGD("JLI lib = %x", (int)libjli);
    if (NULL == libjli) {
        LOGE("JLI lib = NULL: %s", dlerror());
        return -1;
    }
    LOGD("Found JLI lib");

    JLI_Launch_func* pJLI_Launch =
        (JLI_Launch_func*)dlsym(libjli, "JLI_Launch");
    // Boardwalk: silence
    // LOGD("JLI_Launch = 0x%x", *(int*)&pJLI_Launch);

    if (NULL == pJLI_Launch) {
        LOGE("JLI_Launch = NULL");
        return -1;
    }

    LOGD("Calling JLI_Launch");

    return pJLI_Launch(margc, margv,
        0, NULL, // sizeof(const_jargs) / sizeof(char *), const_jargs,
        0, NULL, // sizeof(const_appclasspath) / sizeof(char *), const_appclasspath,
        FULL_VERSION,
        DOT_VERSION,
        *margv, // (const_progname != NULL) ? const_progname : *margv,
        *margv, // (const_launcher != NULL) ? const_launcher : *margv,
        (const_jargs != NULL) ? JNI_TRUE : JNI_FALSE,
        const_cpwildcard, const_javaw, const_ergo_class);
    /*
       return pJLI_Launch(argc, argv,
           0, NULL, 0, NULL, FULL_VERSION,
           DOT_VERSION, *margv, *margv, // "java", "openjdk",
           JNI_FALSE, JNI_TRUE, JNI_FALSE, 0);
    */
}

int java_run_init(char* dir, char** argv, int argc)
{
    LOGD("to dir %s", dir);
    chdir(dir);
#ifdef TRY_SIG2JVM
    void* libjvm = dlopen("libjvm.so", RTLD_LAZY | RTLD_GLOBAL);
    if (NULL == libjvm) {
        LOGE("JVM lib = NULL: %s", dlerror());
        return -1;
    }
    JVM_handle_linux_signal = dlsym(libjvm, "JVM_handle_linux_signal");
#endif

    //Prepare the signal trapper
    struct sigaction catcher;
    memset(&catcher, 0, sizeof(sigaction));
    catcher.sa_sigaction = android_sigaction;
    catcher.sa_flags = SA_SIGINFO | SA_RESTART;
    // SA_RESETHAND;
#define CATCHSIG(X) sigaction(X, &catcher, &old_sa[X])
    CATCHSIG(SIGILL);
    //CATCHSIG(SIGABRT);
    CATCHSIG(SIGBUS);
    CATCHSIG(SIGFPE);
#ifdef TRY_SIG2JVM
    CATCHSIG(SIGSEGV);
#endif
    CATCHSIG(SIGSTKFLT);
    CATCHSIG(SIGPIPE);
    CATCHSIG(SIGXFSZ);
    //Signal trapper ready

    LOGD("Done processing args");

    LOGD("Set Args:");

    for (int i = 0; i < argc; i++)
    {
        LOGD("%s", argv[i]);
    }

    int res = launchJVM(argc, argv);

    LOGD("Free done");

    return res;
}