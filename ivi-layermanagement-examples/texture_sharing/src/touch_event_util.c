/**
 * \file: touch_event_util.c
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
#include "touch_event_util.h"

void
log_array_init(struct event_log_array *p_array, int n_alloc)
{
    if (NULL != p_array)
    {
        memset(p_array, 0x00, sizeof *p_array);
        if (0 < n_alloc)
        {
            p_array->p_logs =
                (struct event_log*)malloc(sizeof(struct event_log) * n_alloc);
            if (NULL == p_array->p_logs)
            {
                LOG_ERROR("Memory allocation for logs failed\n");
                exit(-1);
            }
            p_array->n_alloc = n_alloc;
        }
    }
}

void
log_array_release(struct event_log_array *p_array)
{
    if (NULL != p_array)
    {
        if ((0 < p_array->n_alloc) && (NULL != p_array->p_logs))
        {
            free(p_array->p_logs);
        }
        memset(p_array, 0x00, sizeof *p_array);
    }
}

void
log_array_add(struct event_log_array *p_array, struct event_log *p_log)
{
    if ((NULL != p_array) && (NULL != p_log))
    {
        if ((p_array->n_log + 1) > p_array->n_alloc)
        {
            p_array->n_alloc += 500;
            p_array->p_logs = (struct event_log*)
                realloc(p_array->p_logs,
                        sizeof(struct event_log) * p_array->n_alloc);
            if (NULL == p_array->p_logs)
            {
                LOG_ERROR("Memory allocation for logs failed\n");
                exit(-1);
            }
        }

        p_array->p_logs[p_array->n_log] = *p_log;
        ++(p_array->n_log);
    }
}
