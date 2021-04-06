/*  main.c - The main program

    WJB  2/ 4/21 Attempting to develop a version of MEMU on an RPi Pico
                 As a first step attempt to reproduce Propeller video

*/

#include "pico.h"
#include "pico/stdlib.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/composable_scanline.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "monprom.h"
#include <stdio.h>

#define ROWS 24
#define COLS 80

struct st_char
    {
    uint16_t  fg;
    uint16_t  bg;
    uint8_t  ch;
    } tbuf[ROWS][COLS];

static uint16_t colours[64];

void __time_critical_func(render_loop) (void)
    {
#ifdef DEBUG
    printf ("Starting render\n");
#endif
    while (true)
        {
        struct scanvideo_scanline_buffer *buffer = scanvideo_begin_scanline_generation (true);
        uint16_t *pix = (uint16_t *) buffer->data;
        int iScan = scanvideo_scanline_number (buffer->scanline_id);
        int iRow = iScan / GLYPH_HEIGHT;
        iScan -= GLYPH_HEIGHT * iRow;
        pix += 1;
        for ( int iCol = 0; iCol < COLS; ++iCol )
            {
            struct st_char *ch = &tbuf[iRow][iCol];
            uint16_t fg = ch->fg;
            uint16_t bg = ch->bg;
            uint8_t bits = mon_alpha_prom[ch->ch][iScan];
            ++pix;
            if ( bits & 0x80 ) *pix = fg;
            else *pix = bg;
            ++pix;
            if ( bits & 0x40 ) *pix = fg;
            else *pix = bg;
            ++pix;
            if ( bits & 0x20 ) *pix = fg;
            else *pix = bg;
            ++pix;
            if ( bits & 0x10 ) *pix = fg;
            else *pix = bg;
            ++pix;
            if ( bits & 0x08 ) *pix = fg;
            else *pix = bg;
            ++pix;
            if ( bits & 0x04 ) *pix = fg;
            else *pix = bg;
            ++pix;
            if ( bits & 0x02 ) *pix = fg;
            else *pix = bg;
            ++pix;
            if ( bits & 0x01 ) *pix = fg;
            else *pix = bg;
            }
        ++pix;
        *pix = 0;
        ++pix;
        *pix = COMPOSABLE_EOL_ALIGN;
        pix = (uint16_t *) buffer->data;
        pix[0] = COMPOSABLE_RAW_RUN;
        pix[1] = pix[2];
        pix[2] = COLS * GLYPH_WIDTH - 2;
        buffer->data_used = ( COLS * GLYPH_WIDTH + 4 ) / 2;
        scanvideo_end_scanline_generation (buffer);
        }
    }

const scanvideo_mode_t vga_mode_640x240_60 =
        {
                .default_timing = &vga_timing_640x480_60_default,
                .pio_program = &video_24mhz_composable,
                .width = 640,
                .height = 240,
                .xscale = 1,
                .yscale = 2,
        };

void setup_video (void)
    {
#ifdef DEBUG
    printf ("System clock speed %d kHz\n", clock_get_hz (clk_sys) / 1000);
    printf ("Starting video\n");
#endif
    scanvideo_setup(&vga_mode_640x240_60);
    scanvideo_timing_enable(true);
#ifdef DEBUG
    printf ("System clock speed %d kHz\n", clock_get_hz (clk_sys) / 1000);
#endif
    }

int main (void)
    {
#ifdef DEBUG
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    stdio_init_all();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    for (int i = 10; i > 0; --i )
        {
        gpio_put(LED_PIN, 1);
        sleep_ms(500);
        gpio_put(LED_PIN, 0);
        sleep_ms(500);
        printf ("%d seconds to start\n", i);
        }
#endif
    // set_sys_clock_khz (200000, true);
    printf ("Building screen.\n");
    int i = 0;
    for ( int b = 0; b <= 0xFF; b += 0x55 )
        {
        for ( int g = 0; g <= 0xFF; g += 0x55 )
            {
            for ( int r = 0; r <= 0xFF; r += 0x55 )
                {
                colours[i] = PICO_SCANVIDEO_PIXEL_FROM_RGB8(r, g, b);
                ++i;
                }
            }
        }
    for ( int iRow = 0; iRow < ROWS; ++iRow )
        {
        for ( int iCol = 0; iCol < COLS; ++iCol )
            {
            struct st_char *ch = &tbuf[iRow][iCol];
            ch->ch = iCol + 0x20;
            ch->fg = colours[( iRow + iCol ) & 0x3F];
            ch->bg = colours[iRow & 0x3F];
            }
        }
    setup_video ();
    render_loop ();
    }
