Raspberry Pi Pico - Coloured Text Trial
=======================================

This code is a quick trial of generating VGA coloured text using the RPi scanvideo code
https://github.com/raspberrypi/pico-extras/tree/master/src/common/pico_scanvideo

As described in the above reference, it fills the scanvideo_scanline_buffer->data buffer
with the pixels to be displayed. A single RAW_RUN is used for 641 pixels, including a black
pixel at the end of the run.

It proved not possible to fill the data buffers fast enough to output 640X480x60 scanlines.
Therefore a 640x240x60 mode was defined, in which each horizontal row is displayed twice
in succession to fill the screen.

At present the code just displays the contents of tbuf[], which is filled with a demo pattern.
However the code is only using one core, so it will be possible to use the second core to
generate or update the text to be displayed.
