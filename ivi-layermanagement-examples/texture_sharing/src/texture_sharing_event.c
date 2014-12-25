/**
 * \file: texture_sharing_event.c
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

#include "texture_sharing_test.h"
#include "texture_sharing.h"

#define SURFACE_ID 10
#define LAYER_ID   10
#define SCREEN_WIDTH  1080
#define SCREEN_HEIGHT 960

static const int g_touch_event_data = 4;
static const char *gp_touch_event_data[] = {
    "touch_event_data_s1.dat",
    "touch_event_data_m2.dat",
    "touch_event_data_m3.dat",
    "touch_event_data_m4.dat"
};

extern char *gp_tmp_dir;

/**
 * \func   texture_sharing_event_signal_handler
 *
 * \param  signum: signal number
 *
 * \return none
 *
 * \see
 */
LOCAL void
texture_sharing_event_signal_handler(S32 signum)
{
    switch (signum)
    {
    case SIGUSR1:
        TEST_info("Caught SIGUSR1: evaluate event log\n");
        texture_sharing_evaluate_event_log();
        TEST_info("Done\n");
        break;
    case SIGINT:
        texture_sharing_terminate();
        break;
    }
}

/**
 * \func   texture_sharing_event_run
 *
 * \param  p_test_param: test parameter
 * \param  sub_testcase: sub testcase No.
 *
 * \return S32: return status
 *
 * \see
 */
LOCAL S32
texture_sharing_event_run(test_params *p_test_params, S32 sub_testcase)
{
    struct TextureSharing ts;
    struct ShareSurfaceInfo *p_share_info = NULL;
    struct ShareSurfaceInfo *p_next = NULL;
    S32 i = 0;
    S32 rc = 0;

    _UNUSED_(sub_testcase);

    TEST_info("Enter texture_sharing_event_run\n");

    memset(&ts, 0x00, sizeof(struct TextureSharing));
    wl_list_init(&ts.share_surface_list);

    ts.x               = p_test_params->dest_x;
    ts.y               = p_test_params->dest_y;
    ts.width           = p_test_params->width;
    ts.height          = p_test_params->height;
    ts.surface_id      = p_test_params->surface_id;
    ts.layer_id        = p_test_params->layer_id;
    ts.p_tmp_dir       = (char*)gp_tmp_dir;
    ts.bind_seat       = 1;
    ts.enable_listener = 0;

    for (i = 0; i < p_test_params->n_share_surface; ++i)
    {
        p_share_info =
            (struct ShareSurfaceInfo*)calloc(1, sizeof(struct ShareSurfaceInfo));
        if (NULL == p_share_info)
        {
            TEST_error("Insufficient memory\n");
            break;
        }

        p_share_info->p_share = &ts;
        p_share_info->pid = p_test_params->share_params[i].pid;
        p_share_info->p_window_title = strdup(TEST_TOUCH_EVENT_APP);

        TEST_info("Share info: PID(%d), WindowTitle(%s)\n",
            p_share_info->pid, p_share_info->p_window_title);

        wl_list_insert(ts.share_surface_list.prev, &p_share_info->link);
    }

    rc = texture_sharing_main(&ts);

    /* Cleanup */
    wl_list_for_each_safe(p_share_info, p_next, &ts.share_surface_list, link)
    {
        free(p_share_info->p_window_title);
        free(p_share_info);
    }

    return rc;
}

/**
 * \func   texture_sharing_event_0401
 *
 * \param  p_test_param: test parameter
 *
 * \return S32: return status
 *
 * \see
 */
LOCAL S32
texture_sharing_event_0401(test_params *p_test_params)
{
    S32 rc = 0;
    S32 pid = 0;
    char* const p_argv[] = {TEST_TOUCH_EVENT_APP, gp_tmp_dir, NULL};

    /* execute offscreen application */
    rc = start_shared_app(TEST_TOUCH_EVENT_APP, p_argv, &pid);
    if ((0 > rc) || (0 >= pid))
    {
        p_test_params->test_result = TEST_FAIL;
        return -1;
    }

    sleep(1);

    p_test_params->n_share_surface            = 1;
    p_test_params->share_params[0].pid        = pid;
    p_test_params->share_params[0].surface_no = 1;

    p_test_params->width      = 1080;
    p_test_params->height     =  960;
    p_test_params->dest_x     = 0;
    p_test_params->dest_y     = 0;
    p_test_params->surface_id = SURFACE_ID;
    p_test_params->layer_id   = LAYER_ID;

    /* execute texture sharing application */
    rc = start_test(p_test_params, 1, &pid);
    if ((0 > rc) || (0 >= pid))
    {
        p_test_params->test_result = TEST_FAIL;
        return -1;
    }

    p_test_params->n_apps = 1;
    p_test_params->pids[0] = pid;

    return 0;
}

/**
 * \func   texture_sharing_event_main_04
 *
 * \param  sub_testcase: sub testcase No.
 * \param  duration: execution time
 *
 * \return S32: return status
 *
 * \see
 */
LOCAL S32
texture_sharing_event_main_04(S32 sub_testcase, S32 duration)
{
    test_params test_params;
    siginfo_t siginfo;
    S32 rc = 0;
    S32 pid = 0;
    S32 test_result = TEST_PASS;
    S32 i;
    char *p_device_path;
    char *p_env;
    char touch_event_data[256];

    memset(&test_params, 0x00, sizeof(test_params));

    /* set common parameters */
    test_params.run                = texture_sharing_event_run;
    test_params.signal_handler     = texture_sharing_event_signal_handler;
    test_params.duration           = duration; /* should be -1 */
    test_params.received_update    = FALSE;
    test_params.received_configure = FALSE;

    switch (sub_testcase)
    {
    case 1:
        rc = texture_sharing_event_0401(&test_params);
        break;
    default:
        TEST_error("Invalid sub testcase (%d)\n", sub_testcase);
        return TEST_FAIL;
    }

    if (0 > rc)
    {
        /* a certain error occurred. kill all applications */
        killall_shared_app(&test_params);
        test_result = TEST_FAIL;
    }


    if (TEST_FAIL == test_result)
    {
        if (0 < test_params.pids[0])
        {
            kill(test_params.pids[0], SIGINT);
        }
    }
    else
    {
        sleep(1);

        p_device_path = getenv("T_SCREEN0_TOUCH_DEVICE");
        p_env = getenv("T_TOUCH_EVENT_DATA_PATH");

        /* generate touch event */
        for (i = 0; i < g_touch_event_data; ++i)
        {
            if (NULL != p_env)
            {
                sprintf(touch_event_data, "%s/%s", p_env, gp_touch_event_data[i]);
            }
            else
            {
                strcpy(touch_event_data, gp_touch_event_data[i]);
            }

            /* generate touch event */
            start_event(touch_event_data, p_device_path, &pid);

            /* wait until application is exited */
            waitpid(pid, &rc, 0);
            if (WIFEXITED(rc))
            {
                if (0 != (rc = WEXITSTATUS(rc)))
                {
                    TEST_error("touch_event_generator failed\n");
                    test_result = TEST_FAIL;
                }
            }

            sleep(1);

            /* send a signal to output touch event log to shared application */
            kill(test_params.share_params[0].pid, SIGUSR1);

            sleep(1);

            /* send a signal to evaluate touch event log to sharing application */
            kill(test_params.pids[0], SIGUSR1);

            sleep(1);
        }

        /* exit test */
        kill(test_params.share_params[0].pid, SIGINT);
        kill(test_params.pids[0], SIGINT);
    }

    /* Wait for exit test function */
    while (TRUE)
    {
        siginfo.si_pid = 0;

        if (0 != waitid(P_ALL, 0, &siginfo, WEXITED | WNOHANG))
        {
            if (ECHILD == errno)
            {
                /* No more child process */
                TEST_info("No more child process. End of test.\n");
                break;
            }
        }

        if (0 < siginfo.si_pid)
        {
            switch (siginfo.si_code)
            {
            case CLD_EXITED:
                TEST_info("Process(%d) exited: %d\n",
                    siginfo.si_pid, siginfo.si_status);

                /* Check status */
                if (TEST_PASS == test_result)
                {
                    if (0 != siginfo.si_status)
                    {
                        test_result = TEST_FAIL;
                    }
                }
                break;
            case CLD_KILLED:
            case CLD_DUMPED:
                TEST_info("Process(%d) killed or dumped: %d\n",
                    siginfo.si_pid, siginfo.si_status);

                /* Check status */
                if (TEST_PASS == test_result)
                {
                    test_result = TEST_FAIL;
                }
                break;
            }
        }

        usleep(100000); /* 100msec */
    }

    return test_result;
}

/**
 * \func   texture_sharing_event
 *
 * \param  main_testcase: main testcase No.
 * \param  sub_testcase: sub testcase No.
 * \param  duration: execution time
 *
 * \return S32: return status
 *
 * \see
 */
S32
texture_sharing_event(S32 main_testcase, S32 sub_testcase, S32 duration)
{
    S32 test_result = TEST_FAIL;

    TEST_info("Enter texture_sharing_event: main_testcase(%d)\n", main_testcase);

    switch (main_testcase)
    {
    case 4:
        test_result = texture_sharing_event_main_04(sub_testcase, duration);
        break;
    default:
        TEST_error("Invalid main testcase (%d)\n", main_testcase);
        break;
    }

    return test_result;
}
