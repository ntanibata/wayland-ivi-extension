/**
 * \file: touch_event_util.h
 *
 * \version: $Id:$
 *
 * \release: $Name:$
 *
 * <brief description>.
 * <detailed description>
 * \component: <componentname>
 *
 * \author: <author>
 *
 * \copyright (c) 2012, 2013 Advanced Driver Information Technology.
 * This code is developed by Advanced Driver Information Technology.
 * Copyright of Advanced Driver Information Technology, Bosch, and DENSO.
 * All rights reserved.
 *
 * \see <related items>
 *
 * \history
 * <history item>
 * <history item>
 * <history item>
 *
 ***********************************************************************/
#ifndef TOUCH_EVENT_UTIL_H
#define TOUCH_EVENT_UTIL_H

#include <stdint.h>

/* Log Macro */
#ifndef LOG_ERROR
#define LOG_ERROR(...) {                     \
    fprintf(stderr, "ERROR : " __VA_ARGS__); \
}
#endif

#ifndef LOG_INFO
#define LOG_INFO(...) {                      \
    fprintf(stderr, "INFO  : " __VA_ARGS__); \
}
#endif

#ifndef LOG_WARNING
#define LOG_WARNING(...) {                   \
    fprintf(stderr, "WARN  : " __VA_ARGS__); \
}
#endif

enum
{
    TOUCH_DOWN   = (1 << 0),
    TOUCH_MOTION = (1 << 1),
    TOUCH_UP     = (1 << 2),
    TOUCH_FRAME  = (1 << 3),
    TOUCH_CANCEL = (1 << 4)
};

struct event_log
{
    uint32_t event;
    int32_t  id;
    float    x;
    float    y;
    uint32_t time;
    char     dmy[4];
};

struct event_log_array
{
    size_t            n_log;
    size_t            n_alloc;
    struct event_log *p_logs;
};

void log_array_init(struct event_log_array *p_array, int n_alloc);
void log_array_release(struct event_log_array *p_array);
void log_array_add(struct event_log_array *p_array, struct event_log *p_log);

#endif /* TOUCH_EVENT_UTIL */
