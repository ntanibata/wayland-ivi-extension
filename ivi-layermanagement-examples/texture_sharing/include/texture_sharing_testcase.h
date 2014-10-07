/**
 * \file: texture_sharing_testcase.h
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
#ifndef TEXTURE_SHARING_TESTCASE_H
#define TEXTURE_SHARING_TESTCASE_H

#include "texture_sharing_test.h"

#define T_API_TEST_TIME     10
#define T_STRESS_TEST_TIME 300

/** Test Functions */
S32 texture_sharing_api(S32 main_testcase, S32 sub_testcase, S32 duration);
S32 texture_sharing_stress(S32 main_testcase, S32 sub_testcase, S32 duration);
S32 texture_sharing_event(S32 main_testcase, S32 sub_testcase, S32 duration);

/** Testcases List */
const testcase_params g_testcase_list[] =
{
    {texture_sharing_api,    1, 1, T_API_TEST_TIME   },
    {texture_sharing_api,    1, 2, T_API_TEST_TIME   },
    {texture_sharing_api,    2, 1, T_API_TEST_TIME   },
    {texture_sharing_api,    2, 2, T_API_TEST_TIME   },
    {texture_sharing_api,    2, 3, T_API_TEST_TIME   },
    {texture_sharing_stress, 3, 1, T_STRESS_TEST_TIME},
    {texture_sharing_stress, 3, 2, T_STRESS_TEST_TIME},
    {texture_sharing_stress, 3, 3, T_STRESS_TEST_TIME},
    {texture_sharing_stress, 3, 4, T_STRESS_TEST_TIME},
    {texture_sharing_event,  4, 1, -1},
};

#endif
