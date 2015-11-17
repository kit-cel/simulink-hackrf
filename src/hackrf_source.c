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

#define S_FUNCTION_NAME hackrf_source
#define S_FUNCTION_LEVEL 2

#include "common.h"


/* S-function params */
enum SFcnParamsIndex_and_RWorkIndex {
    SAMPLE_RATE = 0, FREQUENCY, BANDWIDTH,
    AMP_ENABLE, LNA_GAIN, VGA_GAIN, FRAME_SIZE, USE_DOUBLE,
    NUM_PARAMS
};

enum PWorkIndex {
    DEVICE = 0, SBUF, LUT,
    P_WORK_LENGTH
};


/* ======================================================================== */
#if defined(MATLAB_MEX_FILE)
#define MDL_CHECK_PARAMETERS
void mdlCheckParameters(SimStruct *S)
/* ======================================================================== */
{
    Assert_is_numeric(S, SAMPLE_RATE);
    Assert_is_numeric(S, FREQUENCY);
    Assert_is_numeric(S, BANDWIDTH);
    Assert_is_numeric(S, AMP_ENABLE);
    Assert_is_numeric(S, LNA_GAIN);
    Assert_is_numeric(S, VGA_GAIN);
    Assert_is_numeric(S, FRAME_SIZE);
    Assert_is_numeric(S, USE_DOUBLE);

    if (BUFFER_SIZE / BYTES_PER_SAMPLE % (int) GetParam(FRAME_SIZE)) {
        ssSetErrorStatus(S, "Frame size must be a power of two (<= 2^18)")
        return;
    }
}
#endif /* MDL_CHECK_PARAMETERS */


/* ======================================================================== */
#define MDL_INITIAL_SIZES
void mdlInitializeSizes(SimStruct *S)
/* ======================================================================== */
{
    /* parameters */
    ssSetNumSFcnParams(S, NUM_PARAMS);
    #if defined(MATLAB_MEX_FILE)
    if (ssGetNumSFcnParams(S) == ssGetSFcnParamsCount(S)) {
        mdlCheckParameters(S);
        if (ssGetErrorStatus(S) != NULL) return;
    } else
        return;
    #endif
    ssSetSFcnParamTunable(S, SAMPLE_RATE, SS_PRM_NOT_TUNABLE);
    ssSetSFcnParamTunable(S, FREQUENCY,   SS_PRM_SIM_ONLY_TUNABLE);
    ssSetSFcnParamTunable(S, BANDWIDTH,   SS_PRM_NOT_TUNABLE);
    ssSetSFcnParamTunable(S, VGA_GAIN,    SS_PRM_SIM_ONLY_TUNABLE);
    ssSetSFcnParamTunable(S, LNA_GAIN,    SS_PRM_SIM_ONLY_TUNABLE);
    ssSetSFcnParamTunable(S, AMP_ENABLE,  SS_PRM_SIM_ONLY_TUNABLE);
    ssSetSFcnParamTunable(S, FRAME_SIZE,  SS_PRM_NOT_TUNABLE);

    /* ports */
    ssSetNumSampleTimes(S, 1);
    if (!ssSetNumInputPorts(S, 0) || !ssSetNumOutputPorts(S, 1)) return;
    ssSetOutputPortWidth(S, 0, (int) GetParam(FRAME_SIZE));
    ssSetOutputPortComplexSignal(S, 0, COMPLEX_YES);
    ssSetOutputPortDataType(S, 0, (GetParam(USE_DOUBLE)) ? SS_DOUBLE : SS_INT8);
    ssSetOutputPortOptimOpts(S, 0, SS_REUSABLE_AND_LOCAL);

    /* work vectors */
    ssSetNumPWork(S, P_WORK_LENGTH);
    ssSetNumIWork(S, 0);
    ssSetNumRWork(S, NUM_PARAMS);
    ssSetNumModes(S, 0);

    ssSetNumNonsampledZCs(S, 0);
    ssSetSimStateCompliance(S, USE_DEFAULT_SIM_STATE);
    ssSetOptions(S, 0);
}


/* ======================================================================== */
#define MDL_INITIALIZE_SAMPLE_TIMES
void mdlInitializeSampleTimes(SimStruct *S)
/* ======================================================================== */
{
    ssSetSampleTime(S, 0, GetParam(FRAME_SIZE) / GetParam(SAMPLE_RATE));
    ssSetOffsetTime(S, 0, 0.0);
}


static void startHackrfRx(SimStruct *S, bool print_info);
void mdlProcessParameters(SimStruct *S);
static int hackrf_rx_callback(hackrf_transfer *transfer);


/* ======================================================================== */
#define MDL_START
void mdlStart(SimStruct *S)
/* ======================================================================== */
{
    int i = 0; for (; i < P_WORK_LENGTH; i++) ssSetPWorkValue(S, i, NULL);

    ssSetPWorkValue(S, SBUF, sample_buffer_new());

    if (GetParam(USE_DOUBLE)) {
        real_T *lut = malloc(256 * sizeof(real_T));
        for (i = 0; i < 256; i++) lut[i] = (char) i / 128.0;
        ssSetPWorkValue(S, LUT, lut);
    }

    Hackrf_assert(S, hackrf_init(), "Failed to initialize HackRF");
    startHackrfRx(S, true);
}


/* ======================================================================== */
static void startHackrfRx(SimStruct *S, bool print_info)
/* ======================================================================== */
{
    hackrf_device *device = startHackrf(S, (int) GetParam(SAMPLE_RATE),
                                        GetParam(BANDWIDTH), print_info);
    ssSetPWorkValue(S, DEVICE, device);
    if (ssGetErrorStatus(S)) return;

    int i = 0; for (; i < NUM_PARAMS; i++) ssSetRWorkValue(S, i, NAN);
    mdlProcessParameters(S);

    sample_buffer_reset((SampleBuffer*) ssGetPWorkValue(S, SBUF));
    int ret = hackrf_start_rx(device, hackrf_rx_callback, S);
    Hackrf_assert(S, ret, "Failed to start RX streaming");
}


/* ========================================================================*/
#define MDL_PROCESS_PARAMETERS
void mdlProcessParameters(SimStruct *S)
/* ========================================================================*/
{
    if(!ssGetPWorkValue(S, DEVICE)) return;

    Hackrf_set_param(S, hackrf_set_freq, uint64_t, FREQUENCY,
                     "Failed to set center frequency");
    Hackrf_set_param(S, hackrf_set_amp_enable, uint8_t, AMP_ENABLE,
                     "Failed to enable external amp");
    Hackrf_set_param(S, hackrf_set_lna_gain, uint32_t, LNA_GAIN,
                     "Failed to set LNA gain (range 0-40 step 8db)");
    Hackrf_set_param(S, hackrf_set_vga_gain, uint32_t, VGA_GAIN,
                     "Failed to set VGA gain (range 0-62 step 2db)");
}


/* ======================================================================== */
static int hackrf_rx_callback(hackrf_transfer *transfer)
/* ======================================================================== */
{
    SimStruct *S = transfer->rx_ctx;
    SampleBuffer *sbuf = ssGetPWorkValue(S, SBUF);

    if (transfer->valid_length != BUFFER_SIZE) {
        sbuf->error = SB_SIZE_MISSMATCH;
        return -1;
    }

    if (sbuf->startup_skip) {
        sbuf->startup_skip--;
        return 0;
    }

    memcpy(sbuf->buffers[sbuf->tail], transfer->buffer,
           (size_t) transfer->valid_length);

    pthread_mutex_lock(&sbuf->mutex);
    if (sbuf->ready == NUMBER_OF_BUFFERS) {
        sbuf->had_error = true;
        sbuf->error = SB_OVERRUN;
    } else {
        if (++sbuf->tail >= NUMBER_OF_BUFFERS) sbuf->tail = 0;
        sbuf->ready++;
    }
    pthread_cond_signal(&sbuf->cond_var);
    pthread_mutex_unlock(&sbuf->mutex);
    return 0;
}


/* ======================================================================== */
#define MDL_OUTPUTS
void mdlOutputs(SimStruct *S, int_T tid)
/* ======================================================================== */
{
    UNUSED_ARG(tid);
    SampleBuffer *sbuf = ssGetPWorkValue(S, SBUF);

    if (sbuf->error) {
        /* not in callback, due to issues with Simulink */
        ssPrintf(sample_buffer_error_names[sbuf->error]);
        sbuf->error = SB_NO_ERROR;
    }

    pthread_mutex_lock(&sbuf->mutex);
    if (!sbuf->ready) {
        hackrf_device *device = ssGetPWorkValue(S, DEVICE);
        if (hackrf_is_streaming(device) == HACKRF_TRUE) {
            pthread_cond_wait(&sbuf->cond_var, &sbuf->mutex);
        } else {
            ssSetErrorStatus(S, "Device stopped streaming");
            pthread_mutex_unlock(&sbuf->mutex);
            return;
        }
    }
    pthread_mutex_unlock(&sbuf->mutex);

    size_t len_out = 2 * (size_t) ssGetOutputPortWidth(S, 0);
    unsigned char *in = sbuf->buffers[sbuf->head] + sbuf->offset;
    if (GetParam(USE_DOUBLE)) {
        real_T *lut = ssGetPWorkValue(S, LUT),
               *out = ssGetOutputPortRealSignal(S, 0);
        int i=0; for(; i < len_out; i++) out[i] = lut[in[i]];
    } else
        memcpy(ssGetOutputPortSignal(S, 0), in, len_out);
    sbuf->offset += len_out;

    if (sbuf->offset >= BUFFER_SIZE) {
        sbuf->offset = 0;
        if (++sbuf->head >= NUMBER_OF_BUFFERS) sbuf->head = 0;
        pthread_mutex_lock(&sbuf->mutex);
        sbuf->ready--;
        pthread_mutex_unlock(&sbuf->mutex);
    }
}


/* ========================================================================*/
#if defined(MATLAB_MEX_FILE)
#define MDL_SIM_STATUS_CHANGE
void mdlSimStatusChange(SimStruct *S, ssSimStatusChangeType simStatus)
/* ========================================================================*/
{
    if (simStatus == SIM_PAUSE) {
        SampleBuffer *sbuf = ssGetPWorkValue(S, SBUF);
        if (sbuf->had_error) ssPrintf("\n");
        stopHackRf(S, DEVICE);

    } else if (simStatus == SIM_CONTINUE)
        startHackrfRx(S, false);
}
#endif


/* ======================================================================== */
void mdlTerminate(SimStruct *S)
/* ======================================================================== */
{
    stopHackRf(S, DEVICE);
    Hackrf_assert(S, hackrf_exit(), "Failed to exit HackRF API");

    SampleBuffer *sbuf = ssGetPWorkValue(S, SBUF);
    if (sbuf) {
        if (sbuf->had_error) ssPrintf("\n");
        sample_buffer_free(sbuf);
        ssSetPWorkValue(S, SBUF, NULL);
    }
    double_t *lut = ssGetPWorkValue(S, LUT);
    if (lut) {
        free(lut);
        ssSetPWorkValue(S, LUT, NULL);
    }
}

#ifdef  MATLAB_MEX_FILE    /* Is this file being compiled as a MEX-file? */
#include "simulink.c"      /* MEX-file interface mechanism */
#else
#include "cg_sfun.h"       /* Code generation registration function */
#endif
