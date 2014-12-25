/**
 * \file: texture_sharing_test_runner.c
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
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <getopt.h>

#include "texture_sharing_testcase.h"

char *TCID = TEXTURE_SHARING_LTP_TCID;

BOOL g_verbose = FALSE;
char *gp_tmp_dir = NULL;

static const char *TOUCH_EVENT_GENERATOR = "touch_event_generator";

#define MAX_PATH 256
#define CHECKSUM_LENGTH 32

/**
 * \func   start_test
 *
 * \param  p_test: test parameter
 * \param  sub_testcase: sub testcase No.
 *
 * \return S32: return status
 *
 * \see
 */
S32
start_test(test_params *p_test_param, S32 sub_testcase, S32 *p_pid)
{
    printf("[%s]-[%s]\n", __FILE__, __func__);
    S32 rc;
    pid_t pid;
    struct sigaction sigact;

    pid = fork();
    if (0 > pid)
    {
        TEST_error("Execute test failed: %m\n");
        return -1;
    }
    else if (0 == pid)
    {
        /* Setup alarm */
        if (NULL != p_test_param->alarm_handler)
        {
            sigact.sa_handler = p_test_param->alarm_handler;
            sigemptyset(&sigact.sa_mask);
            sigact.sa_flags = SA_RESETHAND;
            sigaction(SIGALRM, &sigact, NULL);
        }
        else if (NULL != p_test_param->signal_handler)
        {
            sigact.sa_handler = p_test_param->signal_handler;
            sigemptyset(&sigact.sa_mask);
            sigact.sa_flags = 0;
            sigaction(SIGUSR1, &sigact, NULL);
            sigaction(SIGUSR2, &sigact, NULL);
            sigaction(SIGINT,  &sigact, NULL);
        }

        if (0 < p_test_param->duration)
        {
            alarm(p_test_param->duration);
        }

        rc = p_test_param->run(p_test_param, sub_testcase);
        exit(rc);
    }

    *p_pid = (S32)pid;

    return 0;
}

/**
 * \func   start_shared_app
 *
 * \param  p_exec: the name of executable file
 * \param  p_argv: the argument list available to the executed program
 * \param  p_pid: process ID
 *
 * \return S32: return status
 *
 * \see
 */
S32
start_shared_app(const char *p_exec, char* const *p_argv, S32 *p_pid)
{
    pid_t pid;

    pid = fork();
    if (0 > pid)
    {
        TEST_error("Execute %s failed: %m\n", p_exec);
        return -1;
    }
    else if (0 == pid)
    {
        printf("exec_fileName = %s\n", p_exec);
        printf("execvp = %d\n", execvp(p_exec, p_argv));
        exit(0);
    }

    *p_pid = (S32)pid;

    return 0;
}

/**
 * \func   start_event
 *
 * \param  p_event_file: main testcase No.
 *         p_device_path: sub testcase No.
 *
 * \return S32: return status
 *
 * \see
 */
S32
start_event(const char *p_event_file, const char *p_device_path, S32 *p_pid)
{
    pid_t pid;
    S32 rc;

    pid = fork();
    if (0 > pid)
    {
        TEST_error("Execute touch_event_generator failed: %m\n");
        return -1;
    }
    else if (0 == pid)
    {
        rc = execlp(TOUCH_EVENT_GENERATOR, TOUCH_EVENT_GENERATOR,
            p_event_file, p_device_path, NULL);
        if (0 > rc)
        {
            TEST_error("Execute touch_event_generator failed (%d): %m\n", errno);
        }
        exit(rc);
    }

    *p_pid = (S32)pid;

    return 0;
}

/**
 * \func   killall_shared_app
 *
 * \param  p_test_params: test parameter
 *
 * \return none
 *
 * \see
 */
void
killall_shared_app(test_params *p_test_params)
{
    S32 i = 0;
    printf("p_test_params->n_share_surface = %d\n", p_test_params->n_share_surface);
    for (i = 0; i < p_test_params->n_share_surface; ++i)
    {
        printf("p_test_params->share_params[%d].pid =%d\n", i, p_test_params->share_params[i].pid);
        kill(p_test_params->share_params[i].pid, SIGINT);
    }
}

/**
 * \func   find_test
 *
 * \param  main_testcase: main testcase No.
 * \param  sub_testcase: sub testcase No.
 *
 * \return testcase_params*: testcase parameter
 *
 * \see
 */
LOCAL testcase_params *
find_test(S32 main_testcase, S32 sub_testcase)
{
    S32 n_testcase;
    S32 i;
    testcase_params *p_testcase = NULL;
    S32 size = sizeof(testcase_params);

    n_testcase = sizeof(g_testcase_list) / size;

    for (i = 0; i < n_testcase; ++i)
    {
        if ((g_testcase_list[i].main_testcase == main_testcase) &&
            (g_testcase_list[i].sub_testcase == sub_testcase))
        {
            p_testcase = (testcase_params*)calloc(1, size);
            if (NULL == p_testcase)
            {
                TEST_error("Insufficient memory\n");
                break;
            }
            memcpy(p_testcase, &g_testcase_list[i], size);
            break;
        }
    }

    return p_testcase;
}

/**
 * \func   usage
 *
 * \param  none
 *
 * \return none
 *
 * \see
 */
LOCAL S32
check_md5sum()
{
    S32 rc, stat;
    pid_t pid;
    FILE *p_fp;
    char command[MAX_PATH];
    char buff[MAX_PATH];
    const char *p_check_file = "check.txt";
    char check_file[MAX_PATH];
    char check_sum[2][40];
    int i;

    pid = fork();
    if (0 > pid)
    {
        TEST_error("fork() failed: %m\n");
        return -1;
    }
    else if (0 == pid)
    {
        sprintf(command, "/usr/bin/md5sum -b %s/*.bmp > %s/%s",
            gp_tmp_dir, gp_tmp_dir, p_check_file);
        execlp("sh", "sh", "-c", command, NULL);
        exit(0);
    }

    rc = waitpid(pid, &stat, 0);
    if (0 > rc)
    {
        TEST_error("md5sum failed: %m\n");
        return -1;
    }
    else
    {
        TEST_info("md5sum status: %d\n", WEXITSTATUS(stat));
        if (0 != WEXITSTATUS(stat))
        {
            return 1;
        }
    }

    sprintf(check_file, "%s/%s", gp_tmp_dir, p_check_file);

    if (NULL == (p_fp = fopen(check_file, "r")))
    {
        TEST_error("md5sum check file open failed: %m\n");
        return -1;
    }

    i = 0;
    while (NULL != fgets(buff, sizeof(buff), p_fp))
    {
        TEST_info("%s", buff);
        memset(check_sum[i], '\0', sizeof(check_sum[i]));
        strncpy(check_sum[i], buff, CHECKSUM_LENGTH);
        if (++i == 2)
        {
            break;
        }
    }
    fclose(p_fp);

    if ((2 != i) || (0 != strcmp(check_sum[0], check_sum[1])))
    {
        TEST_error("md5 check sum test failed\n");
        return 1;
    }

    return 0;
}

/**
 * \func   usage
 *
 * \param  none
 *
 * \return none
 *
 * \see
 */
LOCAL void
usage(S32 error_code)
{
    printf("[%s]-[%s]\n", __FILE__, __func__);
    TEST_error("Usage: texture_sharing_test [options] [main testcase No] [sub testcase No]\n"
               "Options:\n"
               " -t duration, --time=[duration]   Execute the test for given duration (seconds).\n"
               " -h,          --help              This help text.\n");
    exit(error_code);
}

/**
 * \func   main
 *  usage: texture_sharing_test <main testcase No> <sub testcase No>
 *
 * \param  argc: number of argument
 * \param  argv: array of argument
 *
 * \return int: return status
 *
 * \see
 */
#ifdef USE_LTP
int
main(int argc, char **argv)
{
    S32 main_testcase = 0;
    S32 sub_testcase = 0;
    S32 rc = -1;
    BOOL exec_md5sum = FALSE;
    testcase_params *p_testcase = NULL;
    S32 i, c;
    S32 duration = 0;
    static const struct option longopts[] = {
        {"time", required_argument, NULL, 't'},
        {0, 0, NULL, 0}
    };

    while ((c = getopt_long(argc, argv, "t:h", longopts, &i)) != -1)
    {
        switch (c)
        {
        case 't':
            duration = optarg ? atoi(optarg) : 0;
            break;
        case 'h':
            usage(EXIT_SUCCESS);
            break;
        case '?':
            TEST_error("Unknown option `-%c'.\n", optopt);
            usage(EXIT_FAILURE);
            break;
        default:
            usage(EXIT_FAILURE);
            break;
        }
    }

    if (2 > (argc - optind))
    {
        TEST_error("Expected argument after options\n");
        usage(EXIT_FAILURE);
    }

    do
    {
        main_testcase = atoi(argv[optind]);
        sub_testcase  = atoi(argv[optind + 1]);

        p_testcase = find_test(main_testcase, sub_testcase);
        if (NULL == p_testcase)
        {
            TEST_error("find_test failed: No such test case\n");
            break;
        }

        if (0 >= duration)
        {
            duration = p_testcase->duration;
        }

        rc = p_testcase->test_func(main_testcase, sub_testcase, duration);

    } while (FALSE);

    /* cleanup */
    if (NULL != p_testcase)
    {
        free(p_testcase);
    }
    if (NULL != gp_tmp_dir)
    {
        free(gp_tmp_dir);
    }

    return rc;
}
#endif
