/**
 * \file: texture_sharing_test.h
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
#ifndef TEXTURE_SHARING_TEST_H
#define TEXTURE_SHARING_TEST_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

#define MAX_TEXTURE_SHARING 10
#define MAX_SHARE_SURFACE 10
#define TEXTURE_SHARING_LTP_TCID "./texture_sharing_test"
#define TEST_TRIANGLES_APP "./triangles_test"
#define TEST_TOUCH_EVENT_APP "touch_event_test"
#define TEST_PASS 0
#define TEST_FAIL 1

#define S32 int32_t
#define U32 uint32_t
#define BOOL int32_t
#define IMPORT extern
#define LOCAL static
#define TRUE true
#define FALSE false

/** Test Parameter */
typedef struct _testcase_params
{
    S32 (*test_func)(S32, S32, S32);
    S32 main_testcase;
    S32 sub_testcase;
    S32 duration;
} testcase_params;

typedef struct _share_surface_params
{
    S32 pid;
    S32 surface_no;
} share_surface_params;

typedef struct _test_params
{
    S32 (*run)(struct _test_params *, S32);
    void (*alarm_handler)(S32);
    void (*signal_handler)(S32);
    BOOL test_result;
    /* Texture Sharing parameter */
    S32 n_apps;
    S32 pids[MAX_TEXTURE_SHARING];
    U32 duration;
    U32 width;
    U32 height;
    BOOL received_update;
    BOOL received_update2;
    BOOL received_configure;
    BOOL received_input_caps;
    /* Share Surface parameter */
    S32 n_share_surface;
    share_surface_params share_params[MAX_SHARE_SURFACE];
    /* ILM params */
    U32 dest_x;
    U32 dest_y;
    U32 surface_id;
    U32 layer_id;
} test_params;

/** Helper Functions */
S32 start_test(test_params *p_test_param, S32 sub_testcase, S32 *p_pid);
S32 start_shared_app(const char *p_exec, char* const *p_argv, S32 *p_pid);
S32 start_event(const char *p_event_file, const char *p_device_path, S32 *p_pid);
void killall_shared_app(test_params *p_test_params);

IMPORT BOOL g_verbose;
IMPORT char *gp_tmp_dir;

/** Log macro */
#define TEST_error(...) {                                         \
    fprintf(stderr, "texture_sharing_test ERROR : " __VA_ARGS__); \
}

#define TEST_info(...) {                                          \
    fprintf(stdout, "texture_sharing_test INFO  : " __VA_ARGS__); \
}

#define TEST_debug(...) {                                             \
    if (g_verbose)                                                    \
    {                                                                 \
        fprintf(stdout, "texture_sharing_test DEBUG : " __VA_ARGS__); \
    }                                                                 \
}

#endif
