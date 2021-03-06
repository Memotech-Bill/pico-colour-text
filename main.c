/*  main.c - The main program

Demonstration of 80x24 text buffer and minimal keyboard handling.

*/

#include "pico.h"
#include "pico/stdlib.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/composable_scanline.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "monprom.h"
#include "bsp/board.h"
#include "tusb.h"
#include "class/hid/hid.h"
#include <stdio.h>

#define ROWS 24
#define COLS 80
#define VID_CORE    1
// #define DEBUG

struct st_char
    {
    uint16_t  fg;
    uint16_t  bg;
    uint8_t  ch;
    } tbuf[ROWS+1][COLS];

static int iTopRow = 0;
static int iCsrRow = ROWS - 1;
static int iCsrCol = COLS - 1;

static uint16_t colours[64];

void hid_task(void);

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
        // bool bEnd = ( iScan == ( ROWS * GLYPH_HEIGHT - 1 ) );
        int iRow = iScan / GLYPH_HEIGHT;
        iScan -= GLYPH_HEIGHT * iRow;
		iRow += iTopRow;
        if ( iRow > ROWS ) iRow -= ROWS + 1;
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
        /*
        if ( bEnd )
            {
            tuh_task();
            hid_task();
            }
        */
        }
    }

void scroll (uint16_t fg, uint16_t bg)
    {
    int iBotRow = iTopRow - 1;
    if ( iBotRow < 0 ) iBotRow = ROWS;
    struct st_char *pch = tbuf[iBotRow];
    for (int i = 0; i < COLS; ++i)
        {
        pch->fg = fg;
        pch->bg = bg;
        pch->ch = ' ';
        ++pch;
        }
    if ( ++iTopRow > ROWS ) iTopRow = 0;
    --iCsrRow;
    }

void putstr (const char *ps, uint16_t fg, uint16_t bg)
    {
    int nch = strlen (ps);
    if ( iCsrCol + nch >= COLS )
        {
        iCsrCol = 0;
        if ( ++iCsrRow >= ROWS ) scroll (fg, bg);
        }
    int iRow = iTopRow + iCsrRow;
    if ( iRow > ROWS ) iRow -= ROWS + 1;
    struct st_char *pch = &tbuf[iRow][iCsrCol];
    iCsrCol += nch;
    while (nch)
        {
        pch->fg = fg;
        pch->bg = bg;
        pch->ch = *ps;
        ++pch;
        ++ps;
        --nch;
        }
    }

static uint8_t led_flags = 0;

void set_leds (uint8_t leds)
    {
    uint8_t const addr = 1;
    led_flags = leds;

    tusb_control_request_t ledreq = {
        .bmRequestType_bit.recipient = TUSB_REQ_RCPT_INTERFACE,
        .bmRequestType_bit.type = TUSB_REQ_TYPE_CLASS,
        .bmRequestType_bit.direction = TUSB_DIR_OUT,
        .bRequest = HID_REQ_CONTROL_SET_REPORT,
        .wValue = HID_REPORT_TYPE_OUTPUT << 8,
        .wIndex = 0,    // Interface number
        .wLength = sizeof (led_flags)
        };
    
    tuh_control_xfer (addr, &ledreq, &led_flags, NULL);
    }

void key_event (uint8_t key, bool bPress)
    {
    uint8_t leds = led_flags;
    char sEvent[5];
#ifdef DEBUG
    printf ("%s key 0x%02X\n", bPress ? "Press" : "Release", key);
#endif
    sprintf (sEvent, "%c%02hhX ", bPress ? 'P' : 'R', key);
    putstr (sEvent, bPress ? colours[12] : colours[3], colours[0]);
    if ( bPress )
        {
        switch (key)
            {
            case HID_KEY_NUM_LOCK:
                leds ^= KEYBOARD_LED_NUMLOCK;
                set_leds (leds);
                break;
            case HID_KEY_CAPS_LOCK:
                leds ^= KEYBOARD_LED_CAPSLOCK;
                set_leds (leds);
                break;
            case HID_KEY_SCROLL_LOCK:
                leds ^= KEYBOARD_LED_SCROLLLOCK;
                set_leds (leds);
                break;
            default:
                break;
            }
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

// look up new key in previous keys
static inline int find_key_in_report(hid_keyboard_report_t const *p_report, uint8_t keycode)
    {
    for (int i = 0; i < 6; i++)
        {
        if (p_report->keycode[i] == keycode) return i;
        }
    return -1;
    }

static inline void process_kbd_report(hid_keyboard_report_t const *p_new_report)
    {
    static hid_keyboard_report_t prev_report = {0, 0, {0}}; // previous report to check key released
    bool held[6];
    for (int i = 0; i < 6; ++i) held[i] = false;
    for (int i = 0; i < 6; ++i)
        {
        uint8_t key = prev_report.keycode[i];
        if ( key )
            {
            int kr = find_key_in_report(p_new_report, key);
            if ( kr >= 0 )
                {
                held[kr] = true;
                }
            else
                {
                key_event (key, false);
                }
            }
        }
    int old_mod = prev_report.modifier;
    int new_mod = p_new_report->modifier;
    int bit = 0x01;
    for (int i = 0; i < 8; ++i)
        {
        if ((old_mod & bit) && !(new_mod & bit)) key_event (HID_KEY_CONTROL_LEFT + i, false);
        bit <<= 1;
        }
    bit = 0x01;
    for (int i = 0; i < 8; ++i)
        {
        if (!(old_mod & bit) && (new_mod & bit)) key_event (HID_KEY_CONTROL_LEFT + i, true);
        bit <<= 1;
        }
    for (int i = 0; i < 6; ++i)
        {
        uint8_t key = p_new_report->keycode[i];
        if (( ! held[i] ) && ( key ))
            {
            key_event (key, true);
            }
        }

    prev_report = *p_new_report;
    }

#if PICO_SDK_VERSION_MAJOR == 1
#if PICO_SDK_VERSION_MINOR < 2
CFG_TUSB_MEM_SECTION static hid_keyboard_report_t usb_keyboard_report;

void hid_task(void)
	{
    uint8_t const addr = 1;

    if (tuh_hid_keyboard_is_mounted(addr))
		{
        if (!tuh_hid_keyboard_is_busy(addr))
			{
            process_kbd_report(&usb_keyboard_report);
            tuh_hid_keyboard_get_report(addr, &usb_keyboard_report);
			}
		}
	}

void tuh_hid_keyboard_mounted_cb(uint8_t dev_addr)
    {
    // application set-up
#ifdef DEBUG
    printf("A Keyboard device (address %d) is mounted\r\n", dev_addr);
#endif
    tuh_hid_keyboard_get_report(dev_addr, &usb_keyboard_report);
    putstr ("Ins ", colours[63], colours[0]);
    }

void tuh_hid_keyboard_unmounted_cb(uint8_t dev_addr)
    {
    // application tear-down
#ifdef DEBUG
    printf("A Keyboard device (address %d) is unmounted\r\n", dev_addr);
#endif
    putstr ("Rem ", colours[63], colours[0]);
    }

// invoked ISR context
void tuh_hid_keyboard_isr(uint8_t dev_addr, xfer_result_t event)
    {
    (void) dev_addr;
    (void) event;
    }

#else   // PICO_SDK_VERSION_MINOR >= 2
void hid_task (void)
    {
    }

// Each HID instance can has multiple reports

#define MAX_REPORT  4
static uint8_t kbd_addr;
static uint8_t _report_count;
static tuh_hid_report_info_t _report_info_arr[MAX_REPORT];

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
    {
    // Interface protocol
    uint8_t const interface_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    if ( interface_protocol == HID_ITF_PROTOCOL_KEYBOARD )
        {
        kbd_addr = dev_addr;
#ifdef DEBUG
        printf ("Keyboard mounted: dev_addr = %d\n", dev_addr);
#endif
    
        _report_count = tuh_hid_parse_report_descriptor(_report_info_arr, MAX_REPORT,
            desc_report, desc_len);
#ifdef DEBUG
        printf ("%d reports defined\n", _report_count);
#endif
        putstr ("Ins ", colours[63], colours[0]);
        }
    }

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t __attribute__((unused)) instance)
    {
#ifdef DEBUG
    printf ("Device %d unmounted\n");
#endif
    if ( dev_addr == kbd_addr )
        {
        kbd_addr = 0;
        putstr ("Rem ", colours[63], colours[0]);
        }
    }

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t __attribute__((unused)) instance,
    uint8_t const* report, uint16_t len)
    {
    if ( dev_addr != kbd_addr ) return;

    uint8_t const rpt_count = _report_count;
    tuh_hid_report_info_t* rpt_info_arr = _report_info_arr;
    tuh_hid_report_info_t* rpt_info = NULL;

    if ( rpt_count == 1 && rpt_info_arr[0].report_id == 0)
        {
        // Simple report without report ID as 1st byte
        rpt_info = &rpt_info_arr[0];
        }
    else
        {
        // Composite report, 1st byte is report ID, data starts from 2nd byte
        uint8_t const rpt_id = report[0];

        // Find report id in the arrray
        for(uint8_t i=0; i<rpt_count; i++)
            {
            if (rpt_id == rpt_info_arr[i].report_id )
                {
                rpt_info = &rpt_info_arr[i];
                break;
                }
            }

        report++;
        len--;
        }

    if (!rpt_info)
        {
#ifdef DEBUG
        printf("Couldn't find the report info for this report !\r\n");
#endif
        return;
        }

    if ( rpt_info->usage_page == HID_USAGE_PAGE_DESKTOP )
        {
        switch (rpt_info->usage)
            {
            case HID_USAGE_DESKTOP_KEYBOARD:
                // Assume keyboard follow boot report layout
                process_kbd_report( (hid_keyboard_report_t const*) report );
                break;

            default:
                break;
            }
        }
    }

#endif  // PICO_SDK_VERSION_MINOR
#endif  // PICO_SDK_VERSION_MAJOR

void keyboard_loop (void)
    {
    tusb_init();
    while ( true )
        {
        tuh_task();
        hid_task();
        }
    }

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
    render_loop ();
    }

int main (void)
    {
    set_sys_clock_khz (150000, true);
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
    printf ("Building screen.\n");
#endif
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
    // tusb_init();
#if VID_CORE == 0
    multicore_launch_core1 (keyboard_loop);
    setup_video ();
#else // VID_CORE == 1
    multicore_launch_core1 (setup_video);
    keyboard_loop ();
#endif
    }
