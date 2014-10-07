/**
 * \file: texture_sharing_api.c
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

LOCAL test_params *gp_test_params;

/**
 * \func   texture_sharing_api_configure_event
 *
 * \param  p_data: user data
 * \param  p_share_surf: interface object
 * \param  id: surface ID of shared surface
 * \param  type: surface type
 * \param  width: surface width
 * \param  height: surface height
 * \param  stride: stride
 * \param  format: buffer format of shared surface
 *
 * \return none
 *
 * \see
 */
static void
texture_sharing_api_configure_event(void *p_data,
    struct wl_share_surface_ext *p_share_surf, uint32_t id, uint32_t type,
    uint32_t width, uint32_t height, uint32_t stride, uint32_t format)
{
    if (NULL != gp_test_params)
    {
        gp_test_params->received_configure = TRUE;
    }

    share_surface_configure(p_data, p_share_surf, id, type, width, height,
        stride, format);
}

/**
 * \func   texture_sharing_api_update_event
 *
 * \param  p_data: user data
 * \param  p_share_surf: interface object
 * \param  id: surface ID of shared surface
 * \param  name: global name of GBM surface
 *
 * \return none
 *
 * \see
 */
static void
texture_sharing_api_update_event(void *p_data,
    struct wl_share_surface_ext *p_share_surf, uint32_t id, uint32_t name)
{
    struct ShareSurfaceInfo *p_info = (struct ShareSurfaceInfo*)p_data;

    if (NULL != p_info)
    {
        if (NULL == p_info->p_share_surf)
        {
            /* Although share surface was destroyed, update event received */
            if (NULL != gp_test_params)
            {
                gp_test_params->received_update2 = FALSE;
            }
        }
    }

    if ((NULL != gp_test_params) && (FALSE == gp_test_params->received_update))
    {
        gp_test_params->received_update = TRUE;
    }

    share_surface_update(p_data, p_share_surf, id, name);
}

/**
 * \func   texture_sharing_api_input_caps_event
 *
 * \param  p_data: user data
 * \param  p_share_surf: interface object
 * \param  cpas: seat capabilities of the surface to share
 *
 * \return none
 *
 * \see
 */
static void
texture_sharing_api_input_caps_event(void *p_data,
    struct wl_share_surface_ext *p_share_surf, uint32_t caps)
{
    (void)p_data;
    (void)p_share_surf;
    (void)caps;

    if (NULL != gp_test_params)
    {
        gp_test_params->received_input_caps = TRUE;
    }
}

/**
 * \func   texture_sharing_api_alarm_handler
 *
 * \param  signum: signal number
 *
 * \return none
 *
 * \see
 */
LOCAL void
texture_sharing_api_alarm_handler(S32 signum)
{
    if (SIGALRM == signum)
    {
        texture_sharing_terminate();
    }
}

/**
 * \func   texture_sharing_api_main_01
 *
 * \param  p_test_params: test parameter
 * \param  sub_testcase: sub testcase No.
 *
 * \return int: return status
 *
 * \see
 */
LOCAL S32
texture_sharing_api_main_01(test_params *p_test_params, S32 sub_testcase)
{
    struct TextureSharing ts;
    struct ShareSurfaceInfo *p_share_info = NULL;
    S32 pid = 0;
    char window_title[32];
    S32 rc = 0;

    TEST_info("Enter texture_sharing_api_main_01: sub_testcase(%d)\n", sub_testcase);

    gp_test_params = p_test_params;

    switch (sub_testcase)
    {
    case 1:
        pid = p_test_params->share_params[0].pid;
        sprintf(window_title, "window_%d", p_test_params->share_params[0].surface_no);
        break;
    case 2:
        pid = 0;
        window_title[0] = '\0';
        break;
    default:
        TEST_error("Invalid sub testcase (%d)\n", sub_testcase);
        return -1;
    }

    /* Preparing Texture Sharing parameter */
    memset(&ts, 0x00, sizeof(struct TextureSharing));
    wl_list_init(&ts.share_surface_list);

    ts.width      = p_test_params->width;
    ts.height     = p_test_params->height;
    ts.surface_id = p_test_params->surface_id;
    ts.layer_id   = p_test_params->layer_id;
    ts.enable_listener = 1;
    ts.share_surface_listener.update    = texture_sharing_api_update_event;
    ts.share_surface_listener.configure = texture_sharing_api_configure_event;
    ts.share_surface_listener.input_capabilities
                                        = texture_sharing_api_input_caps_event;

    do
    {
        p_share_info =
            (struct ShareSurfaceInfo*)calloc(1, sizeof(struct ShareSurfaceInfo));
        if (NULL == p_share_info)
        {
            TEST_error("Insufficient memory\n");
            break;
        }
        p_share_info->p_share        = &ts;
        p_share_info->pid            = pid;
        p_share_info->p_window_title = (window_title[0] == '\0') ? NULL : strdup(window_title);
        wl_list_insert(ts.share_surface_list.prev, &p_share_info->link);

        rc = texture_sharing_main(&ts);
        if (rc == 0)
        {
            switch (sub_testcase)
            {
            case 1:
                rc = (FALSE == p_test_params->received_configure) ? 1 : 0;
                break;
            case 2:
                rc = (FALSE == p_test_params->received_configure) ? 0 : 1;
                break;
            default:
                /* improbable case */
                break;
            }
        }

    } while (FALSE);

    /* Cleanup */
    if (NULL != p_share_info)
    {
        free(p_share_info->p_window_title);
        free(p_share_info);
    }

    return rc;
}

/**
 * \func   texture_sharing_api_main_02
 *
 * \param  p_test_params: test parameter
 * \param  sub_testcase: sub testcase No.
 *
 * \return int: return status
 *
 * \see
 */
LOCAL S32
texture_sharing_api_main_02(test_params *p_test_params, S32 sub_testcase)
{
    struct TextureSharing ts;
    struct ShareSurfaceInfo *p_share_info = NULL;
    S32 pid = 0;
    char window_title[32];
    S32 rc = 0;

    TEST_info("Enter texture_sharing_api_main_02: sub_testcase(%d)\n", sub_testcase);

    gp_test_params = p_test_params;

    switch (sub_testcase)
    {
    case 1:
    case 2:
    case 3:
        pid = p_test_params->share_params[0].pid;
        sprintf(window_title, "window_%d", p_test_params->share_params[0].surface_no);
        break;
    default:
        TEST_error("Invalid sub testcase (%d)\n", sub_testcase);
        return -1;
    }

    /* Preparing Texture Sharing parameter */
    memset(&ts, 0x00, sizeof(struct TextureSharing));
    wl_list_init(&ts.share_surface_list);

    ts.width      = p_test_params->width;
    ts.height     = p_test_params->height;
    ts.surface_id = p_test_params->surface_id;
    ts.layer_id   = p_test_params->layer_id;
    ts.take_snapshot   = (2 == sub_testcase) ? 1 : 0;
    ts.p_tmp_dir       = (char*)gp_tmp_dir;
    ts.enable_listener = 1;
    ts.repeat_get_and_destroy = (3 == sub_testcase) ? 1 : 0;
    ts.share_surface_listener.update    = texture_sharing_api_update_event;
    ts.share_surface_listener.configure = texture_sharing_api_configure_event;
    ts.share_surface_listener.input_capabilities
                                        = texture_sharing_api_input_caps_event;

    do
    {
        p_share_info =
            (struct ShareSurfaceInfo*)calloc(1, sizeof(struct ShareSurfaceInfo));
        if (NULL == p_share_info)
        {
            TEST_error("Insufficient memory\n");
            break;
        }
        p_share_info->p_share        = &ts;
        p_share_info->pid            = pid;
        p_share_info->p_window_title = (window_title[0] == '\0') ? NULL : strdup(window_title);
        wl_list_insert(ts.share_surface_list.prev, &p_share_info->link);

        rc = texture_sharing_main(&ts);
        if (rc == 0)
        {
            switch (sub_testcase)
            {
            case 1:
                rc = (FALSE == p_test_params->received_configure) ? 1 : 0;
                break;
            case 2:
            case 3:
                if ((FALSE == p_test_params->received_configure) ||
                    (FALSE == p_test_params->received_update)    ||
                    (FALSE == p_test_params->received_update2))
                {
                    rc = 1;
                }
                else
                {
                    /* MD5 check */
                    rc = 0;
                }
                break;
            default:
                /* improbable case */
                break;
            }
        }

    } while (FALSE);

    /* Cleanup */
    if (NULL != p_share_info)
    {
        free(p_share_info->p_window_title);
        free(p_share_info);
    }

    return rc;
}

/**
 * \func   texture_sharing_api
 *
 * \param  main_testcase: main testcase No.
 * \param  sub_testcase: sub testcase No.
 * \param  duration: execution time
 *
 * \return int: return status
 *
 * \see
 */
S32
texture_sharing_api(S32 main_testcase, S32 sub_testcase, S32 duration)
{
    test_params test_params;
    siginfo_t siginfo;
    S32 rc = 0;
    S32 pid = 0;
    S32 argc = 7;
    char **p_argv = NULL;
    S32 i;
    BOOL take_snapshot = FALSE;
    S32 test_result = TEST_FAIL;

    TEST_info("Enter texture_sharing_api: main_testcase(%d)\n", main_testcase);

    memset(&test_params, 0x00, sizeof(test_params));

    /* Determine test function according to main_testcase */
    switch (main_testcase)
    {
    case 1:
        test_params.run = texture_sharing_api_main_01;
        break;
    case 2:
        test_params.run = texture_sharing_api_main_02;
        if (2 == sub_testcase)
        {
            take_snapshot = TRUE;
        }
        break;
    default:
        TEST_error("Invalid main testcase (%d)\n", main_testcase);
        return TEST_FAIL;
    }

    p_argv = (char**)calloc(argc, sizeof(char*));
    if (NULL == p_argv)
    {
        TEST_error("Insufficient memory\n");
        return TEST_FAIL;
    }
    argc = 0;
    p_argv[argc++] = strdup(TEST_TRIANGLES_APP);
    p_argv[argc++] = strdup("-t");
    p_argv[argc++] = strdup("1");
    p_argv[argc++] = strdup("-n");
    if ((TRUE == take_snapshot) && (NULL != gp_tmp_dir))
    {
        p_argv[argc++] = strdup("-s");
        p_argv[argc++] = strdup(gp_tmp_dir);
    }
    p_argv[argc] = NULL;

    do
    {
        /* Start shared application, Triangles */
        rc = start_shared_app(TEST_TRIANGLES_APP, (char* const*)p_argv, &pid);
        if ((0 > rc) || (0 >= pid))
        {
            break;
        }
        TEST_info("Start triangle_test: pid(%d)\n", pid);

        /* SWGKP-253: wait until application complete first drawing (1 second) */
        sleep(1);

        test_params.share_params[0].pid        = pid;
        test_params.share_params[0].surface_no = 1;

        test_params.n_share_surface     = 1;
        test_params.alarm_handler       = texture_sharing_api_alarm_handler;
        test_params.duration            = duration;
        test_params.width               = 250;
        test_params.height              = 250;
        test_params.received_update     = FALSE;
        test_params.received_configure  = FALSE;
        test_params.received_update2    = TRUE;
        test_params.received_input_caps = FALSE;
        test_params.surface_id          = SURFACE_ID;
        test_params.layer_id            = LAYER_ID;

        /* Preparing sharing app, this test app */
        rc = start_test(&test_params, sub_testcase, &pid);
        if ((0 > rc) || (0 >= pid))
        {
            /* Kill all shared applications, and test will finish as failed */
            break;
        }

        test_params.n_apps = 1;
        test_params.pids[0] = pid;

    } while (FALSE);

    /* Wait for exit test function */
    do
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

       switch (siginfo.si_code)
       {
       case CLD_EXITED:
           TEST_info("Process(%d) exited: %d\n",
               siginfo.si_pid, siginfo.si_status);

           if (siginfo.si_pid == test_params.pids[0])
          {
               test_result = (siginfo.si_status == 0) ? TEST_PASS : TEST_FAIL;
               killall_shared_app(&test_params);
           }
           break;
       case CLD_KILLED:
       case CLD_DUMPED:
           TEST_info("Process(%d) killed or dumped: %d\n",
               siginfo.si_pid, siginfo.si_status);

           if (siginfo.si_pid == test_params.pids[0])
           {
               test_result = TEST_FAIL;
               killall_shared_app(&test_params);
           }
           break;
       }

       usleep(100000000); /* 100msec */
    } while(FALSE);

    /* Cleanup */
    if (NULL != p_argv)
    {
        for (i = 0; i < argc; ++i)
        {
            if (NULL != p_argv[i])
            {
                free(p_argv[i]);
            }
        }
    }

    return test_result;
}
