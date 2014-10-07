/**
 * \file: texture_sharing_stress.c
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

#define SURFACE_ID_BASE 10
#define LAYER_ID_BASE   10
#define SCREEN_WIDTH    1080
#define SCREEN_HEIGHT   1920
#define SHARE_SURFACE_NUM MAX_TEXTURE_SHARING

/**
 * \func   texture_sharing_stress_alarm_handler
 *
 * \param  signum: signal number
 *
 * \return none
 *
 * \see
 */
LOCAL void
texture_sharing_stress_alarm_handler(S32 signum)
{
    if (SIGALRM == signum)
    {
        texture_sharing_terminate();
    }
}

LOCAL S32
check_pid(test_params *p_test_params, S32 pid)
{
    S32 rc = 0;
    S32 i = 0;
    S32 n_exited = 0;

    for (i = 0; i < p_test_params->n_share_surface; ++i)
    {
        if (p_test_params->share_params[i].pid == pid)
        {
            p_test_params->share_params[i].pid = -1;
        }

        if (-1 == p_test_params->share_params[i].pid)
        {
            ++n_exited;
        }
    }

    if (p_test_params->n_share_surface == n_exited)
    {
        rc |= 0x1;
    }

    n_exited = 0;

    for (i = 0; i < p_test_params->n_apps; ++i)
    {
        if (p_test_params->pids[i] == pid)
        {
            p_test_params->pids[i] = -1;
        }

        if (-1 == p_test_params->pids[i])
        {
            ++n_exited;
        }
    }

    if (p_test_params->n_apps == n_exited)
    {
        rc |= 0x2;
    }

    return rc;
}

/**
 * \func   texture_sharing_stress_run
 *
 * \param  p_test_param: test parameter
 * \param  sub_testcase: sub testcase No.
 *
 * \return S32: return status
 *
 * \see
 */
LOCAL S32
texture_sharing_stress_run(test_params *p_test_params, S32 sub_testcase)
{
    struct TextureSharing ts;
    struct ShareSurfaceInfo *p_share_info = NULL;
    struct ShareSurfaceInfo *p_next = NULL;
    char window_title[32];
    S32 i = 0;
    S32 rc = 0;

    _UNUSED_(sub_testcase);

    TEST_info("Enter texture_sharing_stress_run\n");

    memset(&ts, 0x00, sizeof(struct TextureSharing));
    wl_list_init(&ts.share_surface_list);

    ts.x               = p_test_params->dest_x;
    ts.y               = p_test_params->dest_y;
    ts.width           = p_test_params->width;
    ts.height          = p_test_params->height;
    ts.surface_id      = p_test_params->surface_id;
    ts.layer_id        = p_test_params->layer_id;
    ts.enable_listener = 0;

    if (sub_testcase == 1)
    {
        ts.repeat_get_and_destroy = 1;
    }

    for (i = 0; i < p_test_params->n_share_surface; ++i)
    {
        p_share_info =
            (struct ShareSurfaceInfo*)calloc(1, sizeof(struct ShareSurfaceInfo));
        if (NULL == p_share_info)
        {
            TEST_error("Insufficient memory\n");
            break;
        }

        sprintf(window_title, "window_%d",
            p_test_params->share_params[i].surface_no);
        p_share_info->p_share = &ts;
        p_share_info->pid = p_test_params->share_params[i].pid;
        p_share_info->p_window_title = strdup(window_title);

        TEST_info("Share info: PID(%d), WindowTitle(%s)\n",
            p_share_info->pid, window_title);

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
 * \func   texture_sharing_stress_0301
 *         number of offscreen app - 1 (creates 1 surface)
 *         number of texture sharing app - 1 (shares 1 surface)
 *
 * \param  p_test_param: test parameter
 *
 * \return S32: return status
 *
 * \see
 */
LOCAL S32
texture_sharing_stress_0301(test_params *p_test_params)
{
    S32 rc = 0;
    S32 pid = 0;
    char* const p_argv[] = {TEST_TRIANGLES_APP,
        "-t", "1", "-w", "1080", "-h", "1920", NULL};

    /* execute offscreen application */
    rc = start_shared_app(TEST_TRIANGLES_APP, p_argv, &pid);
    if ((0 > rc) || (0 >= pid))
    {
        p_test_params->test_result = TEST_FAIL;
        return -1;
    }

    /* SWGKP-253: wait until application complete first drawing (1 second) */
    sleep(1);

    p_test_params->n_share_surface            = 1;
    p_test_params->share_params[0].pid        = pid;
    p_test_params->share_params[0].surface_no = 1;

    p_test_params->width      = 1080;
    p_test_params->height     = 1920;
    p_test_params->dest_x     = 0;
    p_test_params->dest_y     = 0;
    p_test_params->surface_id = SURFACE_ID_BASE;
    p_test_params->layer_id   = LAYER_ID_BASE;

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
 * \func   texture_sharing_stress_0302
 *         number of offscreen app - 10 (creates 1 surface for each)
 *         number of texture sharing app - 1 (shares all surfaces)
 *
 * \param  p_test_param: test parameter
 *
 * \return S32: return status
 *
 * \see
 */
LOCAL S32
texture_sharing_stress_0302(test_params *p_test_params)
{
    S32 rc = 0;
    S32 i = 0;
    S32 pid = 0;
    S32 n_share = SHARE_SURFACE_NUM;
    char* const p_argv[] = {TEST_TRIANGLES_APP, "-t", "1", NULL};

    for (i = 0; i < n_share; ++i)
    {
        /* execute offscreen application */
        rc = start_shared_app(TEST_TRIANGLES_APP, p_argv, &pid);
        if ((0 > rc) || (0 >= pid))
        {
            p_test_params->test_result = TEST_FAIL;
            return -1;
        }

        p_test_params->share_params[i].pid        = pid;
        p_test_params->share_params[i].surface_no = 1;
        ++(p_test_params->n_share_surface);

        usleep(500000); /* wait 500 msec */
    }

    /* SWGKP-253: wait until application complete first drawing (1 second) */
    sleep(1);

    p_test_params->width      = SCREEN_WIDTH;
    p_test_params->height     = SCREEN_HEIGHT;
    p_test_params->dest_x     = 0;
    p_test_params->dest_y     = 0;
    p_test_params->surface_id = SURFACE_ID_BASE;
    p_test_params->layer_id   = LAYER_ID_BASE;

    /* execute texture sharing application */
    rc = start_test(p_test_params, 2, &pid);
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
 * \func   texture_sharing_stress_0303
 *         number of offscreen app - 1 (creates 1 surface)
 *         number of texture sharing app - 10 (shares 1 surface for each)
 *
 * \param  p_test_param: test parameter
 *
 * \return S32: return status
 *
 * \see
 */
LOCAL S32
texture_sharing_stress_0303(test_params *p_test_params)
{
    S32 rc = 0;
    S32 i = 0;
    S32 pid = 0;
    S32 n_apps = SHARE_SURFACE_NUM;
    S32 dest_x = 0;
    S32 dest_y = 0;
    U32 w = 250;
    U32 h = 250;
    char* const p_argv[] = {TEST_TRIANGLES_APP, "-t", "1", NULL};

    /* execute offscreen application */
    rc = start_shared_app(TEST_TRIANGLES_APP, p_argv, &pid);
    if ((0 > rc) || (0 >= pid))
    {
        p_test_params->test_result = TEST_FAIL;
        return -1;
    }

    /* SWGKP-253: wait until application complete first drawing (1 second) */
    sleep(1);

    p_test_params->n_share_surface            = 1;
    p_test_params->share_params[0].pid        = pid;
    p_test_params->share_params[0].surface_no = 1;

    for (i = 0; i < n_apps; ++i)
    {
        p_test_params->width      = w;
        p_test_params->height     = h;
        p_test_params->dest_x     = dest_x;
        p_test_params->dest_y     = dest_y;
        p_test_params->surface_id = SURFACE_ID_BASE + i;
        p_test_params->layer_id   = LAYER_ID_BASE + i;

        /* execute texture sharing application */
        rc = start_test(p_test_params, 3, &pid);
        if ((0 > rc) || (0 >= pid))
        {
            p_test_params->test_result = TEST_FAIL;
            return -1;
        }

        p_test_params->pids[i] = pid;
        ++(p_test_params->n_apps);

        if (SCREEN_WIDTH < (dest_x + w * 2))
        {
            dest_x  = 0;
            dest_y += h;
        }
        else
        {
            dest_x += w;
        }

        usleep(500000); /* wait 500 msec */
    }

    return 0;
}

/**
 * \func   texture_sharing_stress_0304
 *         number of offscreen app - 1 (creates 10 surfaces)
 *         number of texture sharing app - 10 (shares one surfaces for each)
 *
 * \param  p_test_param: test parameter
 *
 * \return S32: return status
 *
 * \see
 */
LOCAL S32
texture_sharing_stress_0304(test_params *p_test_params)
{
    S32 rc = 0;
    S32 i = 0;
    S32 pid = 0;
    S32 n_apps = SHARE_SURFACE_NUM;
    S32 dest_x = 0;
    S32 dest_y = 0;
    U32 w = 250;
    U32 h = 250;
    char* const p_argv[] = {TEST_TRIANGLES_APP, "-t", "10", NULL};

    /* execute offscreen application */
    rc = start_shared_app(TEST_TRIANGLES_APP, p_argv, &pid);
    if ((0 > rc) || (0 >= pid))
    {
        p_test_params->test_result = TEST_FAIL;
        return -1;
    }

    /* SWGKP-253: wait until application complete first drawing (1 second) */
    sleep(1);

    p_test_params->n_share_surface     = 1;
    p_test_params->share_params[0].pid = pid;

    for (i = 0; i < n_apps; ++i)
    {
        p_test_params->share_params[0].surface_no = i + 1;
        p_test_params->width      = w;
        p_test_params->height     = h;
        p_test_params->dest_x     = dest_x;
        p_test_params->dest_y     = dest_y;
        p_test_params->surface_id = SURFACE_ID_BASE + i;
        p_test_params->layer_id   = LAYER_ID_BASE + i;

        /* execute texture sharing application */
        rc = start_test(p_test_params, 4, &pid);
        if ((0 > rc) || (0 >= pid))
        {
            p_test_params->test_result = TEST_FAIL;
            return -1;
        }

        p_test_params->pids[i] = pid;
        ++(p_test_params->n_apps);

        if (SCREEN_WIDTH < (dest_x + w * 2))
        {
            dest_x  = 0;
            dest_y += h;
        }
        else
        {
            dest_x += w;
        }

        usleep(500000); /* wait 500 msec */
    }

    return 0;
}

/**
 * \func   texture_sharing_stress_main_03
 *
 * \param  sub_testcase: sub testcase No.
 * \param  duration: execution time
 *
 * \return S32: return status
 *
 * \see
 */
LOCAL S32
texture_sharing_stress_main_03(S32 sub_testcase, S32 duration)
{
    test_params test_params;
    siginfo_t siginfo;
    S32 rc = 0;
    S32 test_result = TEST_PASS;
    BOOL send_signal = FALSE;

    memset(&test_params, 0x00, sizeof(test_params));

    /* set common parameters */
    test_params.run                = texture_sharing_stress_run;
    test_params.alarm_handler      = texture_sharing_stress_alarm_handler;
    test_params.duration           = duration;
    test_params.received_update    = FALSE;
    test_params.received_configure = FALSE;

    switch (sub_testcase)
    {
    case 1:
        rc = texture_sharing_stress_0301(&test_params);
        break;
    case 2:
        rc = texture_sharing_stress_0302(&test_params);
        break;
    case 3:
        rc = texture_sharing_stress_0303(&test_params);
        break;
    case 4:
        rc = texture_sharing_stress_0304(&test_params);
        break;
    default:
        TEST_error("Invalid sub testcase (%d)\n", sub_testcase);
        return TEST_FAIL;
    }

    if (0 > rc)
    {
        /* Something failed. Kill all applications */
        killall_shared_app(&test_params);
        test_result = TEST_FAIL;
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

            /* If all texture_sharing_test exited, kill all triangles */
            rc = check_pid(&test_params, siginfo.si_pid);
            if ((rc == 0x2) && (FALSE == send_signal))
            {
                killall_shared_app(&test_params);
                send_signal = TRUE;
            }
        }

        usleep(100000); /* 100msec */
    }

    return test_result;
}

/**
 * \func   texture_sharing_stress
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
texture_sharing_stress(S32 main_testcase, S32 sub_testcase, S32 duration)
{
    S32 test_result = TEST_FAIL;

    TEST_info("Enter texture_sharing_stress: main_testcase(%d)\n", main_testcase);

    switch (main_testcase)
    {
    case 3:
        test_result = texture_sharing_stress_main_03(sub_testcase, duration);
        break;
    default:
        TEST_error("Invalid main testcase (%d)\n", main_testcase);
        break;
    }

    return test_result;
}
