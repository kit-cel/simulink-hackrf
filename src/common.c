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


#include "common.h"


SampleBuffer* sample_buffer_new()
{
    SampleBuffer *sbuf = malloc(sizeof(SampleBuffer));
    sample_buffer_reset(sbuf);
    int i = 0; for (; i < NUMBER_OF_BUFFERS; i++)
        sbuf->buffers[i] = malloc(BUFFER_SIZE);
    pthread_mutex_init(&sbuf->mutex, NULL);
    pthread_cond_init(&sbuf->cond_var, NULL);
    return sbuf;
}

void sample_buffer_reset(SampleBuffer* sbuf)
{
    sbuf->head = sbuf->tail = sbuf->ready = 0;
    sbuf->offset = 0;
    sbuf->startup_skip = 2;
    sbuf->error = SB_NO_ERROR;
    sbuf->had_error = false;
}


void sample_buffer_free(SampleBuffer* sbuf)
{
    int i = 0;
    for (; i < NUMBER_OF_BUFFERS; ++i)
        if (sbuf->buffers[i]) free(sbuf->buffers[i]);
    pthread_mutex_destroy(&sbuf->mutex);
    pthread_cond_destroy(&sbuf->cond_var);
    free(sbuf);
}


/* ========================================================================*/


hackrf_device *startHackrf(SimStruct *S,
                           double sample_rate, double bandwidth,
                           bool print_info)
{
    hackrf_device *device;
    int ret = hackrf_open(&device);
    Hackrf_assert(S, ret, "Failed to open HackRF device", device);

    /* show device info */
    if (print_info) {
        enum hackrf_board_id board_id = BOARD_ID_INVALID;
        ret = hackrf_board_id_read(device, (uint8_t *) &board_id);
        Hackrf_assert(S, ret, "Failed to get HackRF board id", device);
        char version[255 + 1];
        ret = hackrf_version_string_read(device, &version[0], 255);
        Hackrf_assert(S, ret, "Failed to read version string", device);
        ssPrintf("Using %s with firmware %s\n",
                 hackrf_board_id_name(board_id), version);
    }
    /* set sample rate */
    ret = hackrf_set_sample_rate(device, sample_rate);
    Hackrf_assert(S, ret, "Failed to set sample rate", device);
    if (print_info)
        if (sample_rate >= 1e6)
            ssPrintf("Sampling at %.6f MSps\n", sample_rate / 1e6);
        else if (sample_rate >= 1e3)
            ssPrintf("Sampling at %.3f kSps\n", sample_rate / 1e3);
        else
            ssPrintf("Sampling at %.0f Sps\n", sample_rate);

    /* set filter bandwidth (0 means automatic filter selection) */
    if (bandwidth == 0.0) bandwidth = sample_rate * 0.75;
    uint32_t bw = hackrf_compute_baseband_filter_bw((uint32_t) bandwidth);
    ret = hackrf_set_baseband_filter_bandwidth(device, bw);
    Hackrf_assert(S, ret, "Failed to set filter bandwidth", device);

    return device;
}


void stopHackRf(SimStruct *S, int device_index)
{
    hackrf_device *device = ssGetPWorkValue(S, device_index);
    if (!device) return;
    if (hackrf_is_streaming(device))
        Hackrf_assert(S, hackrf_stop_rx(device), "Failed to stop RX streaming");
    Hackrf_assert(S, hackrf_close(device), "Failed to close HackRF");
    ssSetPWorkValue(S, device_index, NULL);
}