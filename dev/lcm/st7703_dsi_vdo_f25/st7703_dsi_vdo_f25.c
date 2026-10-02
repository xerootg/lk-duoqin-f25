/*
 * LCM driver for the Duoqin Qin F25 Pro panel.
 *
 * Panel: Sitronix ST7703 driver IC, 640x960, MIPI-DSI video mode, 4 lanes.
 * The F25 drives the ST7703 directly from MT6768 DSI0 -- there is NO MT6382
 * bridge and NO DSC on this board (the stock LK and board DTS carry MT6382
 * support as generic MT6768 platform baggage, but the F25 does not populate it;
 * the dtbo wires dsi0 straight to the panel).
 *
 * The panel is dual-sourced: device trees name "st7703,yuxing" and
 * "st7703,yihua". Both are ST7703 and share this driver; any vendor-specific
 * init divergence is marked TODO below.
 *
 * Init sequence porting note:
 *   The command stream below is ported from the mainline ST7703 "xbd599" vendor
 *   init (drivers/gpu/drm/panel/panel-sitronix-st7703.c), which is a 720-wide
 *   reference. The resolution/timing-sensitive registers (SETDISP 0xB2,
 *   SETRGBIF 0xB3 porches, SETMIPI 0xBA timing) and the LCM_PARAMS porch/PLL
 *   values are placeholders and MUST be validated for 640x960 against the stock
 *   F25 LK ST7703 table before this panel will light correctly. The driver
 *   structure, params shape, and GPIO wiring are correct; the pixel-exact init
 *   is the one piece that needs on-device confirmation.
 *
 * Board GPIOs (from the F25 dtbo panel node):
 *   reset-gpio     = GPIO45  (driven via lcm_util.set_reset_pin)
 *   pm-enable-gpio = GPIO12  (panel/bias enable -- see TODO in lcm_init)
 */

#ifdef BUILD_LK
#else
    #include <linux/string.h>
    #if defined(BUILD_UBOOT)
        #include <asm/arch/mt_gpio.h>
    #else
        #include <mt-plat/mt_gpio.h>
    #endif
#endif
#include "lcm_drv.h"

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH                     (640)
#define FRAME_HEIGHT                    (960)

#define REGFLAG_DELAY                   (0xFE)
#define REGFLAG_END_OF_TABLE            (0x100)  // END OF REGISTERS MARKER

#ifndef TRUE
    #define TRUE 1
#endif
#ifndef FALSE
    #define FALSE 0
#endif

// ---------------------------------------------------------------------------
//  Local Variables / util hooks
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))
#define UDELAY(n)           (lcm_util.udelay(n))
#define MDELAY(n)           (lcm_util.mdelay(n))

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
            lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
            lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define write_cmd(cmd)                  lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg_v2(cmd, buffer, buffer_size) \
            lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

struct LCM_setting_table {
    unsigned cmd;
    unsigned char count;
    unsigned char para_list[64];
};

// ---------------------------------------------------------------------------
//  ST7703 initialization table (ported from mainline xbd599 vendor sequence)
// ---------------------------------------------------------------------------

static struct LCM_setting_table lcm_initialization_setting[] = {
    /* SETEXTC: unlock manufacturer commands */
    {0xB9, 3, {0xF1, 0x12, 0x83} },

    /* SETMIPI: VC=0, 4 lanes, DSI LDO/term, HFP/HBP OSC, + vendor tail.
       TODO(640x960): timing fields here are from the 720-wide reference. */
    {0xBA, 27, {0x33, 0x81, 0x05, 0xF9, 0x0E, 0x0E, 0x20, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x25,
                0x00, 0x91, 0x0A, 0x00, 0x00, 0x02, 0x4F, 0x11,
                0x00, 0x00, 0x37} },

    /* SETPOWER_EXT */
    {0xB8, 4, {0x25, 0x22, 0x20, 0x03} },

    /* SETRGBIF porches. TODO(640x960): validate VBP/VFP/DE porches. */
    {0xB3, 10, {0x10, 0x10, 0x05, 0x05, 0x03, 0xFF, 0x00, 0x00,
                0x00, 0x00} },

    /* SETSCR: source driving */
    {0xC0, 9, {0x73, 0x73, 0x50, 0x50, 0x00, 0xC0, 0x08, 0x70,
               0x00} },

    /* SETVDC */
    {0xBC, 1, {0x4E} },

    /* SETPANEL: scan / RGB order */
    {0xCC, 1, {0x0B} },

    /* SETCYC: column inversion */
    {0xB4, 1, {0x80} },

    /* SETDISP: resolution select. TODO(640x960): the reference selects 720RGB
       (NL=240); confirm the correct NL/RESO_SEL for the F25 640x960 panel. */
    {0xB2, 3, {0xF0, 0x12, 0xF0} },

    /* SETEQ */
    {0xE3, 14, {0x00, 0x00, 0x0B, 0x0B, 0x10, 0x10, 0x00, 0x00,
                0x00, 0x00, 0xFF, 0x00, 0xC0, 0x10} },

    /* vendor command 0xC6 */
    {0xC6, 5, {0x01, 0x00, 0xFF, 0xFF, 0x00} },

    /* SETPOWER */
    {0xC1, 12, {0x74, 0x00, 0x32, 0x32, 0x77, 0xF1, 0xFF, 0xFF,
                0xCC, 0xCC, 0x77, 0x77} },

    /* SETBGP: reference voltage */
    {0xB5, 2, {0x07, 0x07} },
    {REGFLAG_DELAY, 20, {0} },

    /* SETVCOM */
    {0xB6, 2, {0x2C, 0x2C} },

    /* vendor command 0xBF */
    {0xBF, 3, {0x02, 0x11, 0x00} },

    /* SETGIP1 (forward GIP timing) */
    {0xE9, 63, {0x82, 0x10, 0x06, 0x05, 0xA2, 0x0A, 0xA5, 0x12,
                0x31, 0x23, 0x37, 0x83, 0x04, 0xBC, 0x27, 0x38,
                0x0C, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0C, 0x00,
                0x03, 0x00, 0x00, 0x00, 0x75, 0x75, 0x31, 0x88,
                0x88, 0x88, 0x88, 0x88, 0x88, 0x13, 0x88, 0x64,
                0x64, 0x20, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
                0x02, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} },

    /* SETGIP2 (backward GIP timing) */
    {0xEA, 61, {0x02, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x02, 0x46, 0x02, 0x88,
                0x88, 0x88, 0x88, 0x88, 0x88, 0x64, 0x88, 0x13,
                0x57, 0x13, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
                0x75, 0x88, 0x23, 0x14, 0x00, 0x00, 0x02, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x0A,
                0xA5, 0x00, 0x00, 0x00, 0x00} },

    /* SETGAMMA */
    {0xE0, 34, {0x00, 0x09, 0x0D, 0x23, 0x27, 0x3C, 0x41, 0x35,
                0x07, 0x0D, 0x0E, 0x12, 0x13, 0x10, 0x12, 0x12,
                0x18, 0x00, 0x09, 0x0D, 0x23, 0x27, 0x3C, 0x41,
                0x35, 0x07, 0x0D, 0x0E, 0x12, 0x13, 0x10, 0x12,
                0x12, 0x18} },

    /* Sleep out */
    {0x11, 0, {0} },
    {REGFLAG_DELAY, 120, {0} },

    /* Display on */
    {0x29, 0, {0} },
    {REGFLAG_DELAY, 20, {0} },

    {REGFLAG_END_OF_TABLE, 0, {0} },
};

static void push_table(struct LCM_setting_table *table, unsigned int count,
                       unsigned char force_update)
{
    unsigned int i;

    for (i = 0; i < count; i++) {
        unsigned cmd = table[i].cmd;

        switch (cmd) {
        case REGFLAG_DELAY:
            MDELAY(table[i].count);
            break;
        case REGFLAG_END_OF_TABLE:
            return;
        default:
            dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
        }
    }
}

// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
    memset(params, 0, sizeof(LCM_PARAMS));

    params->type   = LCM_TYPE_DSI;
    params->width  = FRAME_WIDTH;
    params->height = FRAME_HEIGHT;

    params->dbi.te_mode = LCM_DBI_TE_MODE_DISABLED;

    params->dsi.mode     = SYNC_PULSE_VDO_MODE;
    params->dsi.LANE_NUM = LCM_FOUR_LANE;

    params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
    params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
    params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
    params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

    params->dsi.packet_size           = 256;
    params->dsi.intermediat_buffer_num = 2;
    params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

    /* TODO(640x960): porch/sync values below are placeholders carried from the
       reference panel. Confirm against the stock F25 LK DDPDSI/LCM dump
       (vact/vbp/vfp, hact/hbp/hfp) and set PLL_CLOCK to match the measured
       DSI data rate before expecting a correct image. */
    params->dsi.vertical_sync_active   = 4;
    params->dsi.vertical_backporch     = 12;
    params->dsi.vertical_frontporch    = 15;
    params->dsi.vertical_active_line   = FRAME_HEIGHT;
    params->dsi.horizontal_sync_active = 40;
    params->dsi.horizontal_backporch   = 40;
    params->dsi.horizontal_frontporch  = 40;
    params->dsi.horizontal_active_pixel = FRAME_WIDTH;

    params->dsi.PLL_CLOCK = 230; /* TODO(640x960): set real DSI PLL/data rate */

    params->dsi.cont_clock = 0;
    params->dsi.ssc_disable = 1;
}

static void lcm_init(void)
{
    /* TODO: assert the panel/bias enable rail (dtbo pm-enable-gpio = GPIO12)
       before reset if the board needs it powered explicitly. On many MT6768
       boards the LCM bias is handled by the MT6370 PMU BLED path; validate. */

    SET_RESET_PIN(1);
    MDELAY(5);
    SET_RESET_PIN(0);
    MDELAY(10);
    SET_RESET_PIN(1);
    MDELAY(120); /* ST7703 needs >6ms after reset release */

    push_table(lcm_initialization_setting,
               sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table),
               1);
}

static void lcm_suspend(void)
{
    static struct LCM_setting_table lcm_suspend_setting[] = {
        {0x28, 0, {0} },              /* display off */
        {REGFLAG_DELAY, 20, {0} },
        {0x10, 0, {0} },              /* sleep in */
        {REGFLAG_DELAY, 120, {0} },
        {REGFLAG_END_OF_TABLE, 0, {0} },
    };

    push_table(lcm_suspend_setting,
               sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_resume(void)
{
    lcm_init();
}

/*
 * The F25 ships two ST7703-based panels (yuxing / yihua). They share this IC and
 * init. If a specific unit needs a vendor-specific tweak, branch here on the
 * panel-ID read / lcd-id GPIO as the stock LK does (AGN::ST7703 lcm_compare_id,
 * lcd_id_pin). For the scaffold we accept any ST7703.
 * TODO: implement the yuxing-vs-yihua discrimination if their init diverges.
 */
static unsigned int lcm_compare_id(void)
{
    return 1;
}

LCM_DRIVER st7703_dsi_vdo_f25 =
{
    .name        = "st7703_dsi_vdo_f25",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params  = lcm_get_params,
    .init        = lcm_init,
    .suspend     = lcm_suspend,
    .resume      = lcm_resume,
    .compare_id  = lcm_compare_id,
};
