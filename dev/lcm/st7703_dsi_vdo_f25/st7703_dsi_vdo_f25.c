/*
 * LCM driver for the Duoqin Qin F25 Pro panel.
 *
 * Panel: Sitronix ST7703 driver IC, 640x960, MIPI-DSI VIDEO mode, 4 lanes.
 * The F25 drives the ST7703 directly from MT6768 DSI0 -- there is NO MT6382
 * bridge and NO DSC on this board.
 *
 * ==========================================================================
 *  THIS DRIVER'S INIT STREAM, DSI TIMING AND RESET SEQUENCE WERE EXTRACTED
 *  FROM THE STOCK F25 LK (lk.img / lk_a.bin) BY STATIC DISASSEMBLY, not from a
 *  generic reference. Provenance:
 *    - LCM_DRIVER "ST7703_YIHUA_4313RQ_640_960_MIPI4" @ vaddr 0x4c4ae988
 *        get_params @0x4c436034  init @0x4c4360f4  (init table @0x4c4ae238)
 *    - LCM_DRIVER "ST7703_YUXING_4313R_640_960_MIPI4_BOE" @ vaddr 0x4c4ad8bc
 *        get_params @0x4c435bf0  init @0x4c435ca8  (init table @0x4c4ad938)
 *  The device we have evidence for reports YIHUA (kernel cmdline fragment
 *  "lcm=1-ST7703_YIHUA_4313RQ_640_960_MIPI4"), so YIHUA is the primary table.
 *  The YUXING/BOE second-source table is kept below for the dual-source case;
 *  the stock LK selects between them via a panel-ID read (lcd_id_pin).
 * ==========================================================================
 *
 * Board GPIO:  reset-gpio = GPIO45  (stock init() toggles 0x8000002d directly:
 *   set_gpio_mode(45,0 GPIO); set_gpio_dir(45,1 out); set_gpio_out(45,val)).
 *   The stock init() does NOT touch any separate bias/pm-enable GPIO, so the
 *   earlier GPIO12 "pm-enable" TODO is not needed in LK.
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

/* Build the YUXING/BOE second-source panel instead of YIHUA. */
/* #define ST7703_F25_PANEL_YUXING 1 */

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
//  ST7703 initialization table -- EXACT bytes from the stock F25 LK
// ---------------------------------------------------------------------------

#ifndef ST7703_F25_PANEL_YUXING
/* ST7703_YIHUA_4313RQ_640_960_MIPI4  (stock init table @ vaddr 0x4c4ae238) */
static struct LCM_setting_table lcm_initialization_setting[] = {
    {0xB9, 3, {0xF1, 0x12, 0x87} },
    {0xB2, 3, {0xF0, 0x04, 0x70} },
    {0xB3, 10, {0x10, 0x10, 0x28, 0x28, 0x03, 0xFF, 0x00, 0x00, 0x00, 0x00} },
    {0xB4, 1, {0x80} },
    {0xB5, 2, {0x06, 0x06} },
    {0xB6, 2, {0xB1, 0xB1} },
    {0xB8, 4, {0x26, 0x22, 0xF0, 0x13} },
    {0xBA, 27, {0x33, 0x81, 0x05, 0xF9, 0x0E, 0x0E, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x25, 0x00, 0x91, 0x0A, 0x00, 0x00, 0x01, 0x4F, 0x01, 0x00, 0x00, 0x37} },
    {0xBC, 1, {0x47} },
    {0xBF, 5, {0x02, 0x10, 0x00, 0x80, 0x04} },
    {0xC0, 9, {0x73, 0x73, 0x50, 0x50, 0x00, 0x00, 0x12, 0x73, 0x00} },
    {0xC1, 17, {0x25, 0x00, 0x32, 0x32, 0x99, 0xE4, 0x77, 0x77, 0xCC, 0xCC, 0xFF, 0xFF, 0x11, 0x11, 0x00, 0x00, 0x32} },
    {0xC7, 12, {0x10, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0xED, 0xC5, 0x00, 0xA5} },
    {0xC8, 4, {0x10, 0x40, 0x1E, 0x03} },
    {0xCC, 1, {0x0B} },
    {0xE0, 34, {0x00, 0x18, 0x1C, 0x2A, 0x3A, 0x3F, 0x4A, 0x3A, 0x07, 0x0C, 0x0D, 0x11, 0x14, 0x12, 0x14, 0x10, 0x18, 0x00, 0x18, 0x1C, 0x2A, 0x3A, 0x3F, 0x4A, 0x3A, 0x07, 0x0C, 0x0D, 0x11, 0x14, 0x12, 0x14, 0x10, 0x18} },
    {0xE1, 7, {0x11, 0x11, 0x91, 0x00, 0x00, 0x00, 0x00} },
    {0xE3, 14, {0x07, 0x07, 0x0B, 0x0B, 0x0B, 0x0B, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x04, 0xC0, 0x10} },
    {0xE9, 63, {0xC8, 0x10, 0x0A, 0x03, 0xC5, 0x80, 0x28, 0x12, 0x31, 0x23, 0x4F, 0x86, 0x80, 0x28, 0x47, 0x08, 0x3C, 0x00, 0xE0, 0x0C, 0x00, 0x00, 0x3C, 0x00, 0xE0, 0x0C, 0x00, 0x00, 0x88, 0x8F, 0xF9, 0x94, 0x44, 0x66, 0x00, 0x22, 0x88, 0xAA, 0x02, 0x88, 0x8F, 0xF9, 0x94, 0x55, 0x77, 0x11, 0x33, 0x88, 0xAA, 0x13, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} },
    {0xEA, 61, {0x00, 0x1A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8F, 0xF8, 0x89, 0x94, 0x33, 0x11, 0x77, 0x55, 0x88, 0xAA, 0x31, 0x8F, 0xF8, 0x89, 0x94, 0x22, 0x00, 0x66, 0x44, 0x88, 0xAA, 0x20, 0x23, 0x00, 0x00, 0x01, 0xB0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xCC, 0x00, 0x00, 0x40, 0x80, 0x38, 0x40, 0x80, 0x81, 0x00} },
    {0xEF, 3, {0xFF, 0xFF, 0x01} },
    {0x11, 1, {0x00} },             /* sleep out */
    {REGFLAG_DELAY, 250, {0} },
    {0x29, 1, {0x00} },             /* display on */
    {REGFLAG_DELAY, 50, {0} },
    {REGFLAG_END_OF_TABLE, 0, {0} },
};
#else
/* ST7703_YUXING_4313R_640_960_MIPI4_BOE  (stock init table @ vaddr 0x4c4ad938) */
static struct LCM_setting_table lcm_initialization_setting[] = {
    {0xB9, 3, {0xF1, 0x12, 0x83} },
    {0xB1, 5, {0x00, 0x00, 0x00, 0xDA, 0x80} },
    {0xB2, 3, {0x78, 0x03, 0x70} },
    {0xB3, 10, {0x10, 0x10, 0x28, 0x28, 0x03, 0xFF, 0x00, 0x00, 0x00, 0x00} },
    {0xB4, 1, {0x80} },
    {0xB5, 2, {0x0A, 0x0A} },
    {0xB6, 2, {0xA6, 0xA6} },
    {0xB8, 4, {0x26, 0x22, 0xF0, 0x13} },
    {0xBA, 27, {0x33, 0x81, 0x05, 0xF9, 0x0E, 0x0E, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x25, 0x00, 0x90, 0x0A, 0x00, 0x00, 0x01, 0x4F, 0x01, 0x00, 0x00, 0x37} },
    {0xBC, 1, {0x47} },
    {0xBF, 3, {0x02, 0x11, 0x00} },
    {0xC0, 9, {0x73, 0x73, 0x50, 0x50, 0x00, 0x00, 0x12, 0x70, 0x00} },
    {0xC1, 12, {0x24, 0x40, 0x32, 0x32, 0x77, 0xE4, 0xFC, 0xFC, 0xC7, 0xC7, 0x73, 0x73} },
    {0xC6, 6, {0x82, 0x00, 0xBF, 0xFF, 0x00, 0xFF} },
    {0xC7, 6, {0xB8, 0x00, 0x0A, 0x00, 0x00, 0x00} },
    {0xC8, 4, {0x10, 0x40, 0x1E, 0x02} },
    {0xCC, 1, {0x0B} },
    {0xE0, 34, {0x00, 0x25, 0x2C, 0x2A, 0x3A, 0x3F, 0x53, 0x3F, 0x06, 0x0C, 0x0C, 0x10, 0x13, 0x11, 0x13, 0x11, 0x19, 0x00, 0x25, 0x2C, 0x2A, 0x3A, 0x3F, 0x53, 0x3F, 0x06, 0x0C, 0x0C, 0x10, 0x13, 0x11, 0x13, 0x11, 0x19} },
    {0xE3, 14, {0x07, 0x07, 0x0B, 0x0B, 0x0B, 0x0B, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xC0, 0x10} },
    {0xE9, 63, {0xC8, 0x10, 0x0A, 0x03, 0xC5, 0x80, 0x38, 0x12, 0x31, 0x23, 0x4F, 0x86, 0x80, 0x38, 0x47, 0x08, 0x3C, 0x00, 0xE0, 0x0C, 0x00, 0x00, 0x3C, 0x00, 0xE0, 0x0C, 0x00, 0x00, 0x88, 0x8F, 0xF9, 0x94, 0x44, 0x66, 0x00, 0x22, 0x88, 0xAA, 0x02, 0x88, 0x8F, 0xF9, 0x94, 0x55, 0x77, 0x11, 0x33, 0x88, 0xAA, 0x13, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} },
    {0xEA, 61, {0x00, 0x1A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8F, 0xF8, 0x89, 0x94, 0x33, 0x11, 0x77, 0x55, 0x88, 0xAA, 0x31, 0x8F, 0xF8, 0x89, 0x94, 0x22, 0x00, 0x66, 0x44, 0x88, 0xAA, 0x20, 0x23, 0x00, 0x00, 0x01, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xCC, 0x00, 0x00, 0x40, 0x80, 0x38, 0x40, 0x80, 0x81, 0x00} },
    {0xEF, 3, {0xFF, 0xFF, 0x01} },
    {0x11, 1, {0x00} },             /* sleep out */
    {REGFLAG_DELAY, 250, {0} },
    {0x29, 1, {0x00} },             /* display on */
    {REGFLAG_DELAY, 120, {0} },
    {REGFLAG_END_OF_TABLE, 0, {0} },
};
#endif

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

    /* Stock F25 LK uses SYNC_EVENT video mode (dsi.mode == 2), NOT sync-pulse. */
    params->dsi.mode     = SYNC_EVENT_VDO_MODE;
    params->dsi.LANE_NUM = LCM_FOUR_LANE;

    params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
    params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
    params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
    params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

    params->dsi.packet_size            = 256;
    params->dsi.intermediat_buffer_num = 2;
    params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

    /* Porches + PLL extracted from the stock F25 LK get_params() (see header). */
#ifndef ST7703_F25_PANEL_YUXING
    /* YIHUA (ST7703_YIHUA_4313RQ) */
    params->dsi.vertical_sync_active    = 4;
    params->dsi.vertical_backporch      = 21;
    params->dsi.vertical_frontporch     = 16;
    params->dsi.vertical_active_line    = FRAME_HEIGHT;
    params->dsi.horizontal_sync_active  = 35;
    params->dsi.horizontal_backporch    = 45;
    params->dsi.horizontal_frontporch   = 45;
    params->dsi.horizontal_active_pixel = FRAME_WIDTH;
    params->dsi.PLL_CLOCK = 147;        /* stock get_params: 0x93 */
#else
    /* YUXING_BOE (ST7703_YUXING_4313R..BOE) */
    params->dsi.vertical_sync_active    = 4;
    params->dsi.vertical_backporch      = 20;
    params->dsi.vertical_frontporch     = 20;
    params->dsi.vertical_active_line    = FRAME_HEIGHT;
    params->dsi.horizontal_sync_active  = 20;
    params->dsi.horizontal_backporch    = 40;
    params->dsi.horizontal_frontporch   = 40;
    params->dsi.horizontal_active_pixel = FRAME_WIDTH;
    params->dsi.PLL_CLOCK = 135;        /* stock get_params: 0x87 */
#endif

    params->dsi.cont_clock  = 0;
    params->dsi.ssc_disable = 1;
}

static void lcm_init(void)
{
    /* Reset waveform copied from the stock F25 LK init() (GPIO45):
     *   MDELAY(50); RST=1; MDELAY(10); RST=0; MDELAY(30); RST=1; MDELAY(120). */
    MDELAY(50);
    SET_RESET_PIN(1);
    MDELAY(10);
    SET_RESET_PIN(0);
    MDELAY(30);
    SET_RESET_PIN(1);
    MDELAY(120);

    push_table(lcm_initialization_setting,
               sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table),
               1);
}

static void lcm_suspend(void)
{
    static struct LCM_setting_table lcm_suspend_setting[] = {
        {0x28, 1, {0x00} },           /* display off */
        {REGFLAG_DELAY, 20, {0} },
        {0x10, 1, {0x00} },           /* sleep in */
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
 * The F25 ships two ST7703-based panels (yuxing / yihua). The stock LK selects
 * between its two LCM_DRIVERs via a panel-ID read (AGN::ST7703 lcm_compare_id,
 * lcd_id_pin). This scaffold builds YIHUA by default (matches the observed
 * kernel cmdline); define ST7703_F25_PANEL_YUXING to build the BOE/yuxing panel.
 * Returning 1 accepts whichever table is compiled in.
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
