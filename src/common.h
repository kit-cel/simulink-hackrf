/*
* Copyright 2015 Communications Engineering Lab, KIT
*
* This is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 3, or (at your option)
* any later version.
*
* This software is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this software; see the file COPYING. If not, write to
* the Free Software Foundation, Inc., 51 Franklin Street,
* Boston, MA 02110-1301, USA.
*/

#ifndef HACKRF_COMMON_H
#define HACKRF_COMMON_H

#include <math.h>  /* NAN */
#include <stdio.h>
#include <stdlib.h>

#include "simstruc.h"

#include "pthread.h"
#include "hackrf.h"


/* ======================================================================== */


#define BUFFER_SIZE        (16 * 32 * 512)
#define NUMBER_OF_BUFFERS  16
#define BYTES_PER_SAMPLE   2  /* device delivers 8 bit I and Q samples */

enum SampleBufferError {
    SB_NO_ERROR = 0,
    SB_OVERRUN = 1,
    SB_UNDERRUN = 2,
    SB_SIZE_MISSMATCH = 3
};
static char *sample_buffer_error_names[] = {"\0", "O", "U", "M"};


typedef struct {
    unsigned char *buffers[NUMBER_OF_BUFFERS];  /* array of buffers */
    int head, tail;                             /* next buffer to be read / written */
    volatile int ready;                         /* number of buffers in use */

    size_t offset;                              /* offset in current */
    int startup_skip;

    enum SampleBufferError error;
    bool had_error;

    pthread_mutex_t mutex;
    pthread_cond_t cond_var;
} SampleBuffer;


SampleBuffer* sample_buffer_new();
void sample_buffer_reset(SampleBuffer* sbuf);
void sample_buffer_free(SampleBuffer* sbuf);


/* ======================================================================== */


#define GetParam(index) mxGetScalar(ssGetSFcnParam(S, index))

static char error_msg[512];
#define ssSetErrorStatusf(S, msg, ...) do { \
    snprintf(error_msg, sizeof(error_msg), msg, __VA_ARGS__); \
    ssSetErrorStatus(S, error_msg); \
} while(0);

#define Assert_is_numeric(S, param) \
    if (!mxIsNumeric(ssGetSFcnParam(S, param)) || mxIsEmpty(ssGetSFcnParam(S, param))) { \
        ssSetErrorStatusf(S, "Parameter '%s' must be numeric", #param); \
        return; \
    }


/* ======================================================================== */


#define Hackrf_assert(S, ret, msg, ...) \
    if (ret != HACKRF_SUCCESS) { \
        ssSetErrorStatusf(S, "%s: %s (%d)", msg, hackrf_error_name(ret), ret); \
        return __VA_ARGS__; \
    }

#define Hackrf_set_param(S, action, cast, index, msg) do { \
    double value = GetParam(index), value_last = ssGetRWorkValue(S, index); \
    if (value != value_last) { \
        ssSetRWorkValue(S, index, value); \
        int ret = action((hackrf_device*) ssGetPWorkValue(S, DEVICE), (cast) value); \
        if (isnan(value_last)) Hackrf_assert(S, ret, msg); \
        if (ret != HACKRF_SUCCESS) { \
            snprintf(error_msg, sizeof(error_msg), "%s: %s (%d)", msg, hackrf_error_name(ret), ret); \
            ssWarning(S, error_msg); \
        } \
    } \
} while(0);


hackrf_device *startHackrf(SimStruct *S,
                           double sample_rate, double bandwidth,
                           bool print_info);
void stopHackRf(SimStruct *S, int device_index);

#endif /* HACKRF_COMMON_H */