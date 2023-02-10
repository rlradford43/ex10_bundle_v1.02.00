/*****************************************************************************
 *                                                                           *
 *  IMPINJ CONFIDENTIAL AND PROPRIETARY INFORMATION FOR INTERNAL USE ONLY    *
 *  Use, modification and/or reproduction strictly prohibited.               *
 *                                                                           *
 * Copyright (c) Impinj, Inc. 2020. All rights reserved.                     *
 *                                                                           *
 *****************************************************************************/
#pragma once

#include <assert.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Only flush gcda if coverage is enabled.
#ifdef CODE_COVERGE
#define COVR_FLUSH __gcov_flush();
extern void __gcov_flush(void);
#else
#define COVR_FLUSH
#endif

#define ASSERT(...) \
    COVR_FLUSH;     \
    assert(__VA_ARGS__);

#define ERRMSG_FAIL(...)          \
    fprintf(stderr, __VA_ARGS__); \
    COVR_FLUSH;                   \
    assert(0);

#ifdef __cplusplus
}
#endif
