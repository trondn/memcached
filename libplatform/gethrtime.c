/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2010 Couchbase, Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

#include "config.h"
#include "platform_internal.h"
#include <assert.h>

#ifdef __APPLE__
#include <mach/mach_time.h>

/*
 * OS X doesn't have clock_gettime, but for monotonic, we can build
 * one with mach_absolute_time as shown below.
 *
 * Most of the idea came from
 * http://www.wand.net.nz/~smr26/wordpress/2009/01/19/monotonic-time-in-mac-os-x/
 */
#define CLOCK_MONOTONIC 192996728

static void mach_absolute_difference(uint64_t start, uint64_t end,
                              struct timespec *tp) {
    uint64_t difference = end - start;
    static mach_timebase_info_data_t info = {0,0};

    if (info.denom == 0) {
        mach_timebase_info(&info);
    }

    uint64_t elapsednano = difference * (info.numer / info.denom);

    tp->tv_sec = elapsednano * 1e-9;
    tp->tv_nsec = elapsednano - (tp->tv_sec * 1e9);
}

static int clock_gettime(int which, struct timespec *tp) {
    assert(which == CLOCK_MONOTONIC);

    static uint64_t epoch = 0;

    if (epoch == 0) {
        epoch = mach_absolute_time();
    }

    uint64_t now = mach_absolute_time();

    mach_absolute_difference(epoch, now, tp);

    return 0;
}
#endif

#if defined(linux) || defined(__APPLE__)
hrtime_t gethrtime(void) {
    struct timespec tm;
    if (clock_gettime(CLOCK_MONOTONIC, &tm) == -1) {
        abort();
    }
    return (((hrtime_t)tm.tv_sec) * 1000000000) + tm.tv_nsec;
}
#elif defined(WIN32)
hrtime_t gethrtime(void) {
    double ret;
    /*
    ** To fix the potential race condition for the local static variable,
    ** gethrtime should be called in a global static variable first.
    ** It will guarantee the local static variable will be initialized
    ** before any thread calls the function.
    */
    static int initialized;
    static LARGE_INTEGER pf;
    static double freq;
    LARGE_INTEGER currtime;

    if (initialized == 0) {
        memset(&pf, 0, sizeof(pf));
        if (!QueryPerformanceFrequency(&pf)) {
            /* @todo trond figure this one out! */
            abort();
        } else {
            assert(pf.QuadPart != 0);
            freq = 1.0e9 / (double)pf.QuadPart;
        }
    }

    if (!QueryPerformanceCounter(&currtime)) {
        abort();
    }

    ret = (double)currtime.QuadPart * freq ;
    return (hrtime_t)ret;
}

#if 0
__attribute__((constructor))
static void init_clock_win32(void) {
    gethrtime();
}
#endif

#elif !defined(__sun)
#error "I don't know how to build a highres clock..."
#endif
