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

#include "mex.h"
#include "hackrf.h"

#define STR_EXPAND(tok) #tok
#define STR(tok) STR_EXPAND(tok)

#define Hackrf_assert(ret, msg) if (ret != HACKRF_SUCCESS) \
    mexErrMsgIdAndTxt("hackrf:finddevices", "%s (error %d)", msg, ret);


void mexFunction(int nlhs, mxArray *plhs[], int nrhs,
        const mxArray *prhs[]) 
{
    hackrf_device *device;
    int ret;
    enum hackrf_board_id board_id = BOARD_ID_INVALID;
    char version[255 + 1];
    read_partid_serialno_t data;

    mexPrintf("Simulink-HackRF version %s\n\n", STR(SIMULINK_HACKRF_VERSION));

    /* init hackrf and open device */
    Hackrf_assert(hackrf_init(), "Failed to init HackRF API");

    ret = hackrf_open(&device);
    Hackrf_assert(ret, (ret == HACKRF_ERROR_NOT_FOUND) ?
                       "No HackRF device found" : "Failed to open HackRF");

    /* read hackrf board id and version */
    ret = hackrf_board_id_read(device, (uint8_t*) &board_id);
    Hackrf_assert(ret, "Failed to get HackRF board id");
    ret = hackrf_version_string_read(device, &version[0], 255);
    Hackrf_assert(ret, "Failed to read version string");
    mexPrintf("Found %s device with firmware %s\n",
              hackrf_board_id_name(board_id), version);

    /* read part id and serial number */
    ret = hackrf_board_partid_serialno_read(device, &data);
    Hackrf_assert(ret, "Failed to read part ID number and serial number");
    mexPrintf("  Part ID Number: 0x%08x 0x%08x\n",
              data.part_id[0], data.part_id[1]);
    mexPrintf("  Serial Number: 0x%08x 0x%08x 0x%08x 0x%08x\n",
              data.serial_no[0], data.serial_no[1],
              data.serial_no[2], data.serial_no[3]);

    /* close hackrf device and exit */
    Hackrf_assert(hackrf_close(device), "Failed to close HackRF device");
    Hackrf_assert(hackrf_exit(), "Failed to exit HackRF API");
}
