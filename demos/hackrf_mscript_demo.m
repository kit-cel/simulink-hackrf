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

% Initialize hackrf device
handle=hackrf_dev(0);

% plot the spectrum scope
for i=1:100

    % receive data-chunks
    data=hackrf_dev(handle, 'RX', 8e6, 8e6, 915e6, 40, 1024);

    % plot periodogram
    plot(10*log10((abs(1/1024*fftshift(fft(data)))).^2))
    xlim([0 1024])
    ylim([-110 -10])
    grid on
    drawnow;
end

% close device handle
hackrf_dev(handle)