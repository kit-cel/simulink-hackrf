%    	# Initialize HackRF device:
%
%     		handle = hackrf_dev(index)
%
%     	     handle - The returned handle used for addressing the initialized HackRF device.
%     	      index - The device index (e.g. 0).
%
%
%     	# Close HackRF device:
%
%     		hackrf_dev(handle)
%
%     	     handle - The returned handle used for the initialized HackRF device.
%
%
%     	# Receive IQ-samples from HackRF device:
%
%     		rxdata = HackRF_dev(handle,'RX',samplerate,bandwidth,frequency,gains,buf_length)
%
%
%     	     rxdata - The received IQ-samples.
%     	     handle - The returned handle used for the initialized HackRF device.
%     	 samplerate - The sampling rate of the device (1MHz - 20MHz) (e.g. 8e6 for 8 MHz samplerate).
%     	  bandwidth - The bandwidth of the device (1.75MHz - 28MHz) (e.g. 8e6 for 8 MHz bandwidth).
%     	  frequency - The center frequency of the tuner (1MHz - 6GHz) (e.g. 100e6 for 100 MHz).
%     	  	 rxgain -  A scalar unified gain from 0db to 116dB (e.g. 10 for 10 dB of RX gain).
%     	            ` Alternate mode: 3 index array specifying VGA_GAIN( 0 to 62 dB), LNA_GAIN( 0 to 40 dB), and AMP(0 or 14 dB) gains expressed in dB (e.g. [ 20 8 14 ] ).
%     	 buf_length - The number of samples in the receive buffer (e.g. 1000).
%
%
%     	# Transmit IQ-samples to HackRF device:
%
%     		ret = HackRF_dev(handle,'TX',samplerate,bandwidth,frequency,gains,txdata)
%
%
%     	        ret - Return status of transmit operation. 0 on success.
%     	     handle - The returned handle used for the initialized HackRF device.
%     	 samplerate - The sampling rate of the device (1MHz - 20MHz) (e.g. 1e6 for 1 MHz samplerate).
%     	  bandwidth - The bandwidth of the device (1.75MHz - 28MHz) (e.g. 8e6 for 8 MHz bandwidth)
%     	  frequency - The center frequency of the tuner (1MHz - 6GHz) (e.g. 100e6 for 100 MHz).
%     	     txgain - A scalar unified gain from 0dB to 61dB (e.g. 5 for 5dB of TX gain).
%     	            ` Alternate mode: 2 index array specifying VGA_GAIN(0 to 47 dB ), and AMP(0 or 14 dB) gains expressed in dB (e.g. [ 5 14 ] ).
%     	     txdata - The transmitted IQ-samples.