#ifndef _MM_LOG_
#define _MM_LOG_

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define _LOG_VERBOSE     1    //ALOGV
#define _LOG_DEBUG       2    //ALOGD
#define _LOG_WARNING     3    //ALOGW
#define _LOG_ERROR       4    //ALOGE
#define _LOG_FATAL       5    //ALOGF
#define _LOG_NONE        6

//#define MM_LOG_LEVEL 2 //defined by file.c
//#define OS_LINUX

//#ifdef HAVE_ANDROID_OS    // only for Android4.4(KitKat)
#ifdef __ANDROID__          // for all Android version

#include <utils/Log.h>

/*
#if MM_LOG_LEVEL == _LOG_VERBOSE

#elif MM_LOG_LEVEL == _LOG_DEBUG
    #define ALOGV
#elif MM_LOG_LEVEL == _LOG_WARNING
    #define ALOGV
    #define ALOGD
#elif MM_LOG_LEVEL == _LOG_ERROR
    #define ALOGV
    #define ALOGD
    #define ALOGW
#elif MM_LOG_LEVEL == _LOG_FATAL
    #define ALOGV
    #define ALOGD
    #define ALOGW
    #define ALOGE
#elif MM_LOG_LEVEL == _LOG_NONE
    #define ALOGV
    #define ALOGD
    #define ALOGW
    #define ALOGE
    #define ALOGF
#endif
*/

//#elif defined OS_LINUX
#else

//#define MM_LOG_LEVEL 3

#ifndef LOG_TAG
#define LOG_TAG "----"
#endif

#if (MM_LOG_LEVEL==_LOG_VERBOSE)
    #define ALOGV(...) MM_Log(_LOG_VERBOSE, __VA_ARGS__)
    #define ALOGD(...) MM_Log(_LOG_DEBUG, __VA_ARGS__)
    #define ALOGW(...) MM_Log(_LOG_WARNING, __VA_ARGS__)
    #define ALOGE(...) MM_Log(_LOG_ERROR, __VA_ARGS__)
    #define ALOGF(...) MM_Log(_LOG_FATAL, __VA_ARGS__)
#elif (MM_LOG_LEVEL==_LOG_DEBUG)
    #define ALOGV(...)
    #define ALOGD(...) MM_Log(_LOG_DEBUG, __VA_ARGS__)
    #define ALOGW(...) MM_Log(_LOG_WARNING, __VA_ARGS__)
    #define ALOGE(...) MM_Log(_LOG_ERROR, __VA_ARGS__)
    #define ALOGF(...) MM_Log(_LOG_FATAL, __VA_ARGS__)
#elif (MM_LOG_LEVEL==_LOG_WARNING)
    #define ALOGV(...)
    #define ALOGD(...)
    #define ALOGW(...) MM_Log(_LOG_WARNING, __VA_ARGS__)
    #define ALOGE(...) MM_Log(_LOG_ERROR, __VA_ARGS__)
    #define ALOGF(...) MM_Log(_LOG_FATAL, __VA_ARGS__)
#elif (MM_LOG_LEVEL==_LOG_ERROR)
    #define ALOGV(...)
    #define ALOGD(...)
    #define ALOGW(...)
    #define ALOGE(...) MM_Log(_LOG_ERROR, __VA_ARGS__)
    #define ALOGF(...) MM_Log(_LOG_FATAL, __VA_ARGS__)
#elif (MM_LOG_LEVEL==_LOG_FATAL)
    #define ALOGV(...)
    #define ALOGD(...)
    #define ALOGW(...)
    #define ALOGE(...)
    #define ALOGF(...) MM_Log(_LOG_FATAL, __VA_ARGS__)
#else
    #define ALOGV(...)
    #define ALOGD(...)
    #define ALOGW(...)
    #define ALOGE(...)
    #define ALOGF(...)
#endif

static void MM_Log(int logLevel, const char *msg, ...)
{
    va_list argptr;
    va_start(argptr, msg);

    char buf[128];
    switch (logLevel) {
    case _LOG_VERBOSE:
        snprintf(buf, 128, "[VERBOSE](%s):\t%s\n", LOG_TAG, msg);
        vprintf(buf, argptr);
        break;
    case _LOG_DEBUG:
        snprintf(buf, 128, "[ DEBUG ](%s):\t%s\n", LOG_TAG, msg);
        vprintf(buf, argptr);
        break;
    case _LOG_WARNING:
        snprintf(buf, 128, "[WARNING](%s):\t%s\n", LOG_TAG, msg);
        vprintf(buf, argptr);
        break;
    case _LOG_ERROR:
        snprintf(buf, 128, "[ ERROR ](%s):\t%s\n", LOG_TAG, msg);
        vprintf(buf, argptr);
        break;
    case _LOG_FATAL:
        snprintf(buf, 128, "[ FATAL ](%s):\t%s\n", LOG_TAG, msg);
        vprintf(buf, argptr);
        break;
    default:
        snprintf(buf, 128, "[-------](%s):\t%s\n", LOG_TAG, msg);
        vprintf(buf, argptr);
        break;
    }

    va_end(argptr);
}

//#else
//    #error "---------unsupported OS------------"
#endif // HAVE_ANDROID_OS

#ifdef __cplusplus
}
#endif

#endif // _MM_LOG_
