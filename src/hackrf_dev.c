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

/* Simulink */
#include "mex.h"

/* HackRF includes */
#include "hackrf.h"

#define HACKRF_RX_VGA_MAX_DB 62 
#define HACKRF_TX_VGA_MAX_DB 47
#define HACKRF_RX_LNA_MAX_DB 40
#define HACKRF_AMP_MAX_DB 14


#define CHECK_DEVICE_INDEX(param) \
if (!mxIsNumeric(param) || (mxGetScalar(param) < 0)) \
    {mexErrMsgTxt("Device Index must be numeric");return;}

#define CHECK_SAMPLE_RATE(param) \
if (!mxIsNumeric(param) || mxIsEmpty(param) || !(mxGetScalar(param) > 0)) \
    {mexErrMsgTxt("Samples per Second must be numeric and greater zero");return;}

#define CHECK_BANDWIDTH(param) \
if (!mxIsNumeric(param) || mxIsEmpty(param) || !(mxGetScalar(param) > 0)) \
    {mexErrMsgTxt("Bandwidth must be numeric and greater zero");return;}

#define CHECK_FREQUENCY(param) \
if (!mxIsNumeric(param) || !(mxGetScalar(param) > 0)) \
    {mexErrMsgTxt("Frequency must be numeric and greater zero");return;}

#define CHECK_BUF_LENGTH(param) \
if (!mxIsNumeric(param) || mxIsEmpty(param) || !(mxGetScalar(param) > 0)) \
    {mexErrMsgTxt("Buffer Length must be numeric and greater zero");return;}

/* interface */
#define DEVICE_INDEX  prhs[0]
#define DEVICE_MODE   prhs[1]
#define SAMPLE_RATE   prhs[2]
#define BANDWIDTH     prhs[3]
#define FREQUENCY     prhs[4]
#define GAIN          prhs[5]
#define BUF_LENGTH    prhs[6]
#define TX_DATA       prhs[6]
#define DEVICE_HANDLE plhs[0]
#define RECEIVE_DATA  plhs[0]
#define num_inputs    7
#define num_outputs   1

/* defines */
#define NUM_SUPPORT   20

/* global variables */
struct hackrf_device *_devices[NUM_SUPPORT];
#define MODE_RX     1
#define MODE_TX     2
int _device_modes[NUM_SUPPORT];
int _sample_rates[NUM_SUPPORT];
uint64_t _frequencies[NUM_SUPPORT];
int _bandwidths[NUM_SUPPORT];
int _lnagains[NUM_SUPPORT];
int _rxvga[NUM_SUPPORT];
int _txvga[NUM_SUPPORT];
int _amp[NUM_SUPPORT];
int _buf_lengths[NUM_SUPPORT];


typedef struct _CallbackData{
	int device_index;
	int  buf_length;
	double * outr;
	double * outi;
	size_t index;
}CallbackData;


int rx_callback(hackrf_transfer * transfer) {

	CallbackData * cb_data = (CallbackData *)transfer->rx_ctx;

	int buf_length = cb_data->buf_length;
	char *buf = (char *)transfer->buffer;
	size_t bytes_to_write = (transfer->valid_length >> 1);

	if ((cb_data->index + bytes_to_write) > buf_length) {
		bytes_to_write = buf_length - cb_data->index;
	}

	for (int i = 0; i < bytes_to_write; i++) {
		cb_data->outr[i + cb_data->index] = (double)(buf[i*2] / 127.0f);
		cb_data->outi[i + cb_data->index] = (double)(buf[i*2 + 1] / 127.0f);

	}
	cb_data->index += bytes_to_write;
	if (cb_data->index >= buf_length) {
		return -1;
	}
	return 0;
}


int tx_callback(hackrf_transfer * transfer) {

	CallbackData * cb_data = (CallbackData *)transfer->tx_ctx;
	int buf_length = cb_data->buf_length;
	char *buf = (char *)transfer->buffer;
	size_t bytes_to_read = transfer->valid_length >> 1;

	if ((cb_data->index + bytes_to_read) > buf_length) {
		bytes_to_read = buf_length - cb_data->index;
		memset(transfer->buffer, 0, transfer->valid_length);
	}

	for (int i = 0; i < bytes_to_read; i++) {
		buf[i*2] = (char)(cb_data->outr[i + cb_data->index] * 127.0);
		buf[i*2+ 1] = (char)(cb_data->outi[i + cb_data->index] * 127.0);

	}
	cb_data->index += bytes_to_read;
	if (cb_data->index >= buf_length) {
		return -1;
	}
	return 0;

}

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]) {
	int ret = HACKRF_SUCCESS;
	char errmsg[250];

	/* close device */

	if (nlhs == 0 && nrhs == 1){
		CHECK_DEVICE_INDEX(DEVICE_INDEX);
		/* get device index for input */
		int device_index = (int)mxGetScalar(DEVICE_INDEX);

		_devices[device_index] = NULL;
		/* reset setting */
		_sample_rates[device_index] = 1;
		_bandwidths[device_index] = 1;
		_frequencies[device_index] = 0;
		_lnagains[device_index] = 0;
		_rxvga[device_index] = 0;
		_txvga[device_index] = 0;
		_buf_lengths[device_index] = 1;

		mexPrintf("hackrf_close() done\n");

	}

	/* open device*/
	else if (nrhs == 1 && nlhs == 1){
		/* initialize device handle */
		ret = hackrf_init();
		if (ret != HACKRF_SUCCESS){
			sprintf(errmsg, "hackrf_init() filed\n");
			mexErrMsgTxt(errmsg);
			return;
		}
		DEVICE_HANDLE = mxCreateDoubleMatrix(1, 1, mxREAL);
		*mxGetPr(DEVICE_HANDLE) = -1;

		/* check input */
		CHECK_DEVICE_INDEX(DEVICE_INDEX);
		int device_index = (int)mxGetScalar(DEVICE_INDEX);

		/* check if device is used */
		if (_devices[device_index] != NULL) {
			mexPrintf("HackRF device #%d is already in use \n");
			sprintf(errmsg, "HackRF device #%d is already in use.\n", device_index);
			mexErrMsgTxt(errmsg);
		}
		/* device is not used */
		else {
			hackrf_device_list_t  *list = hackrf_device_list();

			if (list->devicecount < 1) {
				sprintf(errmsg, "No HackRF boards found.\n");
				mexErrMsgTxt(errmsg);
			}

			ret = hackrf_device_list_open(list, device_index, &_devices[device_index]);
			if (ret != HACKRF_SUCCESS){
				sprintf(errmsg, "hackrf_open() failed: %s (%d)\n", hackrf_error_name(ret), ret);
				mexErrMsgTxt(errmsg);
				return;
			}

			ret = hackrf_close(_devices[device_index]);
			if (ret != HACKRF_SUCCESS){
				sprintf(errmsg, "hackrf_close() failed: %s (%d)\n", hackrf_error_name(ret), ret);
				mexErrMsgTxt(errmsg);
				//	return;
			}


			*mxGetPr(DEVICE_HANDLE) = device_index;

			/* reset settings */
			_device_modes[device_index] = 0;
			_sample_rates[device_index] = 0;
			_frequencies[device_index] = 0;
			_buf_lengths[device_index] = -1;

			mexPrintf("Using HackRF device #%d\n", device_index);
		}
	}
	/* set device */
	else if (nrhs == num_inputs && nlhs == num_outputs) {
		int rx;
		char mode[10];

		/* check input */
		CHECK_DEVICE_INDEX(DEVICE_INDEX);
		CHECK_BANDWIDTH(BANDWIDTH);
		CHECK_SAMPLE_RATE(SAMPLE_RATE);
		CHECK_FREQUENCY(FREQUENCY);

		/* define RX mode */
		rx = 1;
		if (mxGetString(DEVICE_MODE, (char *)&mode, 3)) {
			rx = 1;
			CHECK_BUF_LENGTH(BUF_LENGTH);
		}
		else {
			if (!strcmp(mode, "TX"))
				rx = 0;
		}

		/* get device index from input */
		int device_index = (int)mxGetScalar(DEVICE_INDEX);

		/* check if device is already in use */
		if (_devices[device_index] == NULL) {
			sprintf(errmsg, "HackRF device #%d is not initialized.\n", device_index);
			mexErrMsgTxt(errmsg);
		}

		hackrf_device_list_t  *list = hackrf_device_list();

		ret = hackrf_device_list_open(list, device_index, &_devices[device_index]);
		if (ret != HACKRF_SUCCESS){
			sprintf(errmsg, "hackrf_open() failed: %s (%d)\n", hackrf_error_name(ret), ret);
			mexErrMsgTxt(errmsg);
			return;
		}

		if (_devices[device_index] == NULL) {

			ret = hackrf_open(&_devices[device_index]);
			if (ret != HACKRF_SUCCESS){
				sprintf(errmsg, "hackrf_open() failed: %s (%d)\n", hackrf_error_name(ret), ret);
				mexErrMsgTxt(errmsg);
				return;
			}
		}

		/* set sample rate */
		int sample_rate = (int)mxGetScalar(SAMPLE_RATE);

		if (sample_rate != _sample_rates[device_index]) {
			ret = hackrf_set_sample_rate(_devices[device_index], sample_rate);
			if (ret != HACKRF_SUCCESS) {
				sprintf(errmsg, "hackrf_sample_rate_set() failed: %s (%d)\n", hackrf_error_name(ret), ret);
				mexErrMsgTxt(errmsg);
			}
			else{
				mexPrintf("hackrf_sample_rate_set:%.03f MHzz/)\n", (float)sample_rate / 1000000);
			}
			_sample_rates[device_index] = sample_rate;
		}

		/* set bandwidth */
		int bandwidth = (int)mxGetScalar(BANDWIDTH);
		if (bandwidth != _bandwidths[device_index]) {
			ret = hackrf_set_baseband_filter_bandwidth(_devices[device_index], bandwidth);
			if (ret != HACKRF_SUCCESS) {
				sprintf(errmsg, "hackrf_set_baseband_filter_bandwidth() failed: %s (%d)\n", hackrf_error_name(ret), ret);
				mexErrMsgTxt(errmsg);
			}
			else{
				mexPrintf("hackrf_set_baseband_filter_bandwidth:%.03f MHz/)\n", (float)bandwidth / 1000000);
			}
			_bandwidths[device_index] = bandwidth;
		}
		/* set tuning frequency */

		uint64_t frequency = (uint64_t)mxGetScalar(FREQUENCY);
		if (frequency != _frequencies[device_index]) {
			ret = hackrf_set_freq(_devices[device_index], bandwidth);
			if (ret != HACKRF_SUCCESS) {
				sprintf(errmsg, "hackrf_set_freq() failed: %s (%d)\n", hackrf_error_name(ret), ret);
				mexErrMsgTxt(errmsg);
			}
			else{
				mexPrintf("hackrf_set_freq:%.03f MHz/)\n", (float)frequency / 1000000);
			}
			_frequencies[device_index] = frequency;
		}

		/*gains default value */
		_lnagains[device_index] = 16;
		_rxvga[device_index] = 16;
		_txvga[device_index] = 0;
		_amp[device_index] = 0;
		double *gains = mxGetPr(GAIN);
		if (rx &&mxGetN(GAIN) == 1){
			/*Custom gain*/
			int gain= (int)gains[0];

			if (gain <= 0) {
				_rxvga[device_index] = 0;
				_lnagains[device_index] = 0;
				_amp[device_index] = 0;
			}
			else if(gain <= (HACKRF_RX_LNA_MAX_DB / 2) + (HACKRF_RX_VGA_MAX_DB / 2)){
				_rxvga[device_index]= (gain / 3) & ~0x1;
				_lnagains[device_index]=gain - _rxvga[device_index];
				_amp[device_index] = 0;
			}
			else if (gain <= ((HACKRF_RX_LNA_MAX_DB / 2) + (HACKRF_RX_VGA_MAX_DB / 2) + HACKRF_AMP_MAX_DB))
			{
				_amp[device_index] = HACKRF_AMP_MAX_DB;
				_rxvga[device_index] = ((gain - _amp[device_index]) / 3) & ~0x1;
				_lnagains[device_index] = gain - _amp[device_index] - _rxvga[device_index];
			}
			else if (gain <= HACKRF_RX_LNA_MAX_DB + HACKRF_RX_VGA_MAX_DB + HACKRF_AMP_MAX_DB)
			{
				_amp[device_index] = HACKRF_AMP_MAX_DB;
				_rxvga[device_index] = (int)((gain - _amp[device_index]) * HACKRF_RX_LNA_MAX_DB * 1.0 / HACKRF_RX_VGA_MAX_DB)& ~0x1;;
				_lnagains[device_index] = gain - _amp[device_index] - _rxvga[device_index];
			}

		}
		else if (rx &&mxGetN(GAIN) == 2){
			_rxvga[device_index] = (int)gains[0] ;
			_lnagains[device_index] = (int)gains[1] ;
		}
		else if (rx &&mxGetN(GAIN) == 3){
			_rxvga[device_index] = (int)gains[0];
			_lnagains[device_index] = (int)gains[1];
			_amp[device_index] = (int)gains[2];
		}
		else if (!rx &&mxGetN(GAIN) == 1){
			int gain = (int)gains[0];

			if (gain <= 0)
			{
				_amp[device_index] = 0;
				_txvga[device_index] = 0;
			}
			else if (gain <= (HACKRF_TX_VGA_MAX_DB / 2))
			{
				_amp[device_index] = 0;
				_txvga[device_index] = _amp[device_index];
			}
			else if (gain <= HACKRF_TX_VGA_MAX_DB + HACKRF_AMP_MAX_DB)
			{
				_amp[device_index] = HACKRF_AMP_MAX_DB;
				_txvga[device_index] = gain - HACKRF_AMP_MAX_DB;
			}

		}if (!rx &&mxGetN(GAIN) == 2){
			_txvga[device_index] = (int)gains[0];
			_amp[device_index] = (int)gains[1];;
		}


		if (rx){
			ret = hackrf_set_amp_enable(_devices[device_index], _amp[device_index]>0?1:0);
			ret |= hackrf_set_lna_gain(_devices[device_index], _lnagains[device_index]);
			ret |= hackrf_set_vga_gain(_devices[device_index], _rxvga[device_index]);
			if (ret != HACKRF_SUCCESS) {
				sprintf(errmsg, "hackrf_set_gain() failed: %s (%d)\n", hackrf_error_name(ret), ret);
				mexErrMsgTxt(errmsg);
			}
		}
		else{
			ret = hackrf_set_amp_enable(_devices[device_index], _amp[device_index]);
			ret |= hackrf_set_txvga_gain(_devices[device_index], _txvga[device_index]);

			if (ret != HACKRF_SUCCESS) {
				sprintf(errmsg, "hackrf_set_gain() failed: %s (%d)\n", hackrf_error_name(ret), ret);
				mexErrMsgTxt(errmsg);
			}

		}

		/*device rx*/
		if (rx){
			if (!_device_modes[device_index] == MODE_RX){
				_device_modes[device_index] = MODE_RX;
			}

			/* set buffer length */
			int buff_length = (int)mxGetScalar(BUF_LENGTH);
			if (buff_length != _buf_lengths[device_index]){
				_buf_lengths[device_index] = buff_length;
			}

			/*create output data */
			RECEIVE_DATA = mxCreateDoubleMatrix(buff_length, 1, mxCOMPLEX);
			double *outr = (double*)mxGetPr(RECEIVE_DATA);
			double *outi = (double*)mxGetPi(RECEIVE_DATA);

			CallbackData * cb_data = (CallbackData *)malloc(sizeof(CallbackData));

			cb_data->device_index = device_index;
			cb_data->buf_length = buff_length;
			cb_data->outr = outr;
			cb_data->outi = outi;
			cb_data->index = 0;

			ret = hackrf_start_rx(_devices[device_index], rx_callback, (void*)cb_data);
			if (ret != HACKRF_SUCCESS) {
				sprintf(errmsg, "hackrf_start_rx() failed:: %s (%d)\n", hackrf_error_name(ret), ret);
				mexErrMsgTxt(errmsg);
				return;
			}
			while ((hackrf_is_streaming(_devices[device_index]) == HACKRF_TRUE));


			ret = hackrf_stop_rx(_devices[device_index]);
			if (ret != HACKRF_SUCCESS){
				sprintf(errmsg, "hackrf_stop_rx() failed: %s (%d)\n", hackrf_error_name(ret), ret);
				mexErrMsgTxt(errmsg);;
			}

			ret = hackrf_close(_devices[device_index]);
			if (ret != HACKRF_SUCCESS){
				sprintf(errmsg, "hackrf_close() failed: %s (%d)\n", hackrf_error_name(ret), ret);
				mexErrMsgTxt(errmsg);
			}



		}
		/*device tx*/
		else{
			if (!_device_modes[device_index] == MODE_TX){
				_device_modes[device_index] = MODE_TX;
			}

			int len = mxGetN(TX_DATA);
			///* Get pointers to data */
			double *outr = (double*)mxGetPr(TX_DATA);
			double *outi = (double*)mxGetPi(TX_DATA);

			/* create return data */
			RECEIVE_DATA = mxCreateDoubleMatrix(1, 1, mxREAL);
			int *_return = (int *)mxGetPr(RECEIVE_DATA);
			CallbackData * cb_data = (CallbackData *)malloc(sizeof(CallbackData));

			cb_data->device_index = device_index;
			cb_data->buf_length = len;
			cb_data->outr = outr;
			cb_data->outi = outi;
			cb_data->index = 0;


			ret = hackrf_start_tx(_devices[device_index], tx_callback, (void*)cb_data);
			if (ret != HACKRF_SUCCESS) {
				sprintf(errmsg, "hackrf_start_tx() failed:: %s (%d)\n", hackrf_error_name(ret), ret);
				mexErrMsgTxt(errmsg);
				return;
			}
			while ((hackrf_is_streaming(_devices[device_index]) == HACKRF_TRUE));

			*_return = ret;

			ret = hackrf_stop_tx(_devices[device_index]);
			if (ret != HACKRF_SUCCESS){
				sprintf(errmsg, "hackrf_stop_rx() failed: %s (%d)\n", hackrf_error_name(ret), ret);
				mexErrMsgTxt(errmsg);
			}

			ret = hackrf_close(_devices[device_index]);
			if (ret != HACKRF_SUCCESS){
				sprintf(errmsg, "hackrf_close() failed: %s (%d)\n", hackrf_error_name(ret), ret);
				mexErrMsgTxt(errmsg);
			}
		}
	}

	else{
		/* Usage */
		mexPrintf("\nUsage:"
			"\n\n"
			"     \t# Initialize HackRF device:\n\n"
			"     \t\thandle = hackrf_dev(index)\n\n"
			"     \t     handle - The returned handle used for addressing the initialized HackRF device.\n"
			"     \t      index - The device index (e.g. 0).\n\n\n"
			"     \t# Close HackRF device:\n\n"
			"     \t\thackrf_dev(handle)\n\n"
			"     \t     handle - The returned handle used for the initialized HackRF device.\n\n\n"
			"     \t# Receive IQ-samples from HackRF device:\n\n"
			"     \t\trxdata = HackRF_dev(handle,'RX',samplerate,bandwidth,frequency,gains,buf_length)\n\n\n"
			"     \t     rxdata - The received IQ-samples.\n"
			"     \t     handle - The returned handle used for the initialized HackRF device.\n"
			"     \t samplerate - The sampling rate of the device (8MHz - 20MHz) (e.g. 8e6 for 8 MHz samplerate).\n"
			"     \t bandwidth - The bandwidth of the device (1.75MHz - 28MHz) (e.g. 8e6 for 8 MHz bandwidth).\n"
			"     \t  frequency - The center frequency of the tuner (1MHz - 6GHz) (e.g. 100e6 for 100 MHz).\n"
			"     \t     rxgain -  A scalar unified gain from 0db to 116dB (e.g. 10 for 10 dB of RX gain).\n"
			"     \t            ` Alternate mode: 3 index array specifying VGA_GAIN( 0 to 62 dB), LNA_GAIN( 0 to 40 dB), and AMP(0 or 14 dB) gains expressed in dB (e.g. [ 20 8 14 ] ).\n"
			"     \t buf_length - The number of samples in the receive buffer (e.g. 1000).\n\n\n"

			"     \t# Transmit IQ-samples to HackRF device:\n\n"
			"     \t\tret = HackRF_dev(handle,'TX',samplerate,bandwidth,frequency,gains,txdata)\n\n\n"
			"     \t        ret - Return status of transmit operation. 0 on success.\n"
			"     \t     handle - The returned handle used for the initialized HackRF device.\n"
			"     \t samplerate - The sampling rate of the device (2MHz - 20MHz) (e.g. 1e6 for 1 MHz samplerate).\n"
			"     \t bandwidth - The bandwidth of the device (1.75MHz - 28MHz) (e.g. 8e6 for 8 MHz bandwidth).\n"
			"     \t  frequency - The center frequency of the tuner (1MHz - 6GHz) (e.g. 100e6 for 100 MHz).\n"
			"     \t     txgain - A scalar unified gain from 0dB to 61 (e.g. 5 for 5dB of TX gain).\n"
			"     \t            ` Alternate mode: 2 index array specifying VGA_GAIN(0 to 47 dB ), and AMP(0 or 14 dB) gains expressed in dB (e.g. [ 5 14 ] ).\n"
			"     \t     txdata - The transmitted IQ-samples.\n\n");
		return;


	}
}
