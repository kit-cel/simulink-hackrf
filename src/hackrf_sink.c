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

#define S_FUNCTION_NAME hackrf_sink
#define S_FUNCTION_LEVEL 2

#include "common.h"


/* S-function params */
enum SFcnParamsIndex_and_RWorkIndex {
    FREQUENCY, BANDWIDTH, TXVGA_GAIN,
    NUM_PARAMS
};
enum PWorkIndex {
    DEVICE = 0, SBUF, P_WORK_LENGTH
};


/* ======================================================================== */
#if defined(MATLAB_MEX_FILE)
#define MDL_CHECK_PARAMETERS
void mdlCheckParameters(SimStruct *S)
/* ======================================================================== */
{
    Assert_is_numeric(S, FREQUENCY);
    Assert_is_numeric(S, BANDWIDTH);
    Assert_is_numeric(S, TXVGA_GAIN);
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
    ssSetSFcnParamTunable(S, FREQUENCY,   SS_PRM_SIM_ONLY_TUNABLE);
    ssSetSFcnParamTunable(S, BANDWIDTH,   SS_PRM_NOT_TUNABLE);
    ssSetSFcnParamTunable(S, TXVGA_GAIN,  SS_PRM_SIM_ONLY_TUNABLE);

    /* ports */
    ssSetNumSampleTimes(S, 1);
    if (!ssSetNumOutputPorts(S, 0) || !ssSetNumInputPorts(S, 1)) return;
    ssSetInputPortWidth(S, 0, DYNAMICALLY_SIZED);
    ssSetInputPortComplexSignal(S, 0, COMPLEX_YES);
    ssSetInputPortDataType(S, 0, SS_INT8);
    ssSetInputPortDirectFeedThrough(S, 0, true);
    ssSetInputPortOptimOpts(S, 0, SS_REUSABLE_AND_LOCAL);

    /* work Vectors */
    ssSetNumPWork(S, P_WORK_LENGTH);
    ssSetNumIWork(S, 0);
    ssSetNumRWork(S, NUM_PARAMS);
    ssSetNumModes(S, 0);

    ssSetNumNonsampledZCs(S, 0);
    ssSetSimStateCompliance(S, USE_DEFAULT_SIM_STATE);
    ssSetOptions(S, 0);
}


/* ========================================================================*/
#if defined(MATLAB_MEX_FILE)
#define MDL_SET_DEFAULT_PORT_DIMENSION_INFO
void mdlSetDefaultPortDimensionInfo(SimStruct *S)
/* ========================================================================*/
{
    if (ssGetInputPortWidth(S, 0) == DYNAMICALLY_SIZED)
        ssSetInputPortWidth(S, 0, BUFFER_SIZE / BYTES_PER_SAMPLE);
}
#endif


/* ========================================================================*/
#if defined(MATLAB_MEX_FILE)
#define MDL_SET_INPUT_PORT_DIMENSION_INFO
void mdlSetInputPortDimensionInfo(SimStruct *S, int_T port, const DimsInfo_T *dimsInfo)
/* ========================================================================*/
{
    if (dimsInfo->numDims >= 2 && dimsInfo->dims[1] > 1)
        ssSetErrorStatus(S, "Wrong port dimensions")
    else if (BUFFER_SIZE / BYTES_PER_SAMPLE % dimsInfo->dims[0])
        ssSetErrorStatus(S, "Frame size must be a power of two (<= 2^18)")
    else
        ssSetInputPortDimensionInfo(S, port, dimsInfo);
}
#endif



/* ======================================================================== */
#define MDL_INITIALIZE_SAMPLE_TIMES
static void mdlInitializeSampleTimes(SimStruct *S)
/* ======================================================================== */
{
    ssSetSampleTime(S, 0, INHERITED_SAMPLE_TIME);
    ssSetOffsetTime(S, 0, 0.0);
}


static void startHackrfTx(SimStruct *S, bool print_info);
void mdlProcessParameters(SimStruct *S);
static int hackrf_tx_callback(hackrf_transfer *transfer);


/* ======================================================================== */
#define MDL_START
void mdlStart(SimStruct *S)
/* ======================================================================== */
{
    int i = 0; for (; i < P_WORK_LENGTH; ++i) ssSetPWorkValue(S, i, NULL);

    ssSetPWorkValue(S, SBUF, sample_buffer_new());

    Hackrf_assert(S, hackrf_init(), "Failed to initialize HackRF API");
    startHackrfTx(S, true);
}


/* ======================================================================== */
static void startHackrfTx(SimStruct *S, bool print_info)
/* ======================================================================== */
{
    double sample_rate = (1.0 / ssGetSampleTime(S, 0)) * ssGetInputPortDimensions(S, 0)[0];
    hackrf_device *device = startHackrf(S, sample_rate, GetParam(BANDWIDTH), print_info);
    ssSetPWorkValue(S, DEVICE, device);
    if (ssGetErrorStatus(S)) return;

    int i = 0; for (; i < NUM_PARAMS; i++) ssSetRWorkValue(S, i, NAN);
    mdlProcessParameters(S);

    sample_buffer_reset((SampleBuffer*) ssGetPWorkValue(S, SBUF));
    int ret = hackrf_start_tx(device, hackrf_tx_callback, S);
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
    Hackrf_set_param(S, hackrf_set_txvga_gain, uint32_t, TXVGA_GAIN,
                     "Failed to set TXVGA gain (range 0-47 step 1db)");
}


/* ======================================================================== */
static int hackrf_tx_callback(hackrf_transfer *transfer)
/* ======================================================================== */
{
    SimStruct *S = transfer->tx_ctx;
    SampleBuffer *sbuf = ssGetPWorkValue(S, SBUF);

    if (transfer->valid_length != BUFFER_SIZE) {
        sbuf->error = SB_SIZE_MISSMATCH;
        return -1;
    }

    if (sbuf->startup_skip) {
        memset(transfer->buffer, 0, (size_t) transfer->valid_length);
        sbuf->startup_skip--;

    } else if (sbuf->ready) {
        memcpy(transfer->buffer, sbuf->buffers[sbuf->head], BUFFER_SIZE);
        if (++sbuf->head >= NUMBER_OF_BUFFERS) sbuf->head = 0;
        pthread_mutex_lock(&sbuf->mutex);
        sbuf->ready--;
        pthread_cond_signal(&sbuf->cond_var);
        pthread_mutex_unlock(&sbuf->mutex);

    } else {  /* underrun, no buffers ready */
        memset(transfer->buffer, 0, (size_t) transfer->valid_length);
        sbuf->error = SB_UNDERRUN;
        sbuf->had_error = true;
    }
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
        ssPrintf(sample_buffer_error_names[sbuf->error]);
        /* not in callback, due to issues with Simulink */
        sbuf->error = SB_NO_ERROR;
    }

    pthread_mutex_lock(&sbuf->mutex);
    if(sbuf->ready == NUMBER_OF_BUFFERS) {
        hackrf_device* device = ssGetPWorkValue(S, DEVICE);
        if (hackrf_is_streaming(device) == HACKRF_TRUE) {
            pthread_cond_wait(&sbuf->cond_var, &sbuf->mutex);
        } else {
            ssSetErrorStatus(S, "Streaming to device stopped");
            pthread_mutex_unlock(&sbuf->mutex);
            return;
        }
    }
    pthread_mutex_unlock(&sbuf->mutex);

    size_t len_in = 2 * (size_t) ssGetInputPortWidth(S, 0);
    memcpy(sbuf->buffers[sbuf->tail] + sbuf->offset,
           ssGetInputPortSignalPtrs(S, 0)[0], len_in);
    sbuf->offset += len_in;

    if (sbuf->offset >= BUFFER_SIZE) {
        sbuf->offset = 0;
        if (++sbuf->tail >= NUMBER_OF_BUFFERS) sbuf->tail = 0;
        pthread_mutex_lock(&sbuf->mutex);
        sbuf->ready += 1;
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
        startHackrfTx(S, false);
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
}


#ifdef  MATLAB_MEX_FILE    /* Is this file being compiled as a MEX-file? */
#include "simulink.c"      /* MEX-file interface mechanism */
#else
#include "cg_sfun.h"       /* Code generation registration function */
#endif
