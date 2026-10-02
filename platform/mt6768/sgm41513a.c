/*
 * SGM41513A single-cell Li-ion switching charger - LK driver.
 *
 * Duoqin Qin F25 Pro: SGM41513A on I2C7 @ 0x6B. New-arch MTK charger driver
 * (registers a struct mtk_charger_info via sgm41513a_chg_probe()). The probe is
 * fail-safe: a single bounded I2C read detects the part; if absent it returns
 * without registering, so a board without this charger never stalls boot.
 *
 * Init values mirror the stock F25 LK sequence captured from its boot log
 * (REG00=0x04, REG02=0x9E, REG03=0xDD, REG04=0x89, REG05=0x8F, REG06=0xE5) and
 * explicitly disable the charger watchdog so it cannot reset the system.
 */

#include <platform/mt_typedefs.h>
#include <platform/mt_reg_base.h>
#include <platform/mt_i2c.h>
#include <platform/mtk_charger_intf.h>
#include <platform/sgm41513a.h>
#include <platform/errno.h>
#include <printf.h>
#include <string.h>

#define SGM_TAG "[sgm41513a] "

struct sgm41513a_charger_info {
	struct mtk_charger_info mchr_info;
	struct mt_i2c_t i2c;
	int i2c_log_level;
};

static struct sgm41513a_charger_info g_info;

/* ---- raw I2C (bounded: one transaction, no retry loop) ---- */
static kal_uint32 sgm_write_byte(kal_uint8 reg, kal_uint8 val)
{
	kal_uint8 buf[2];
	kal_uint32 len = 2;
	kal_int32 ret;

	buf[0] = reg;
	buf[1] = val;
	g_info.i2c.id = SGM41513A_I2C_ID;
	g_info.i2c.addr = (SGM41513A_SLAVE_ADDR_WRITE >> 1);
	g_info.i2c.mode = ST_MODE;
	g_info.i2c.speed = 100;

	ret = i2c_write(&g_info.i2c, buf, len);
	if (ret)
		dprintf(INFO, SGM_TAG "write reg 0x%x = 0x%x failed: %d\n", reg, val, ret);
	return ret;
}

static kal_uint32 sgm_read_byte(kal_uint8 reg, kal_uint8 *val)
{
	kal_uint8 buf[1];
	kal_uint32 len = 1;
	kal_int32 ret;

	buf[0] = reg;
	g_info.i2c.id = SGM41513A_I2C_ID;
	g_info.i2c.addr = (SGM41513A_SLAVE_ADDR_READ >> 1);
	g_info.i2c.mode = ST_MODE;
	g_info.i2c.speed = 100;

	ret = i2c_write_read(&g_info.i2c, buf, len, len);
	if (ret) {
		dprintf(INFO, SGM_TAG "read reg 0x%x failed: %d\n", reg, ret);
		return ret;
	}
	*val = buf[0];
	return 0;
}

/* read-modify-write a bitfield */
static kal_uint32 sgm_config(kal_uint8 reg, kal_uint8 val, kal_uint8 mask, kal_uint8 shift)
{
	kal_uint8 tmp = 0;
	kal_uint32 ret;

	ret = sgm_read_byte(reg, &tmp);
	if (ret)
		return ret;
	tmp &= ~(mask << shift);
	tmp |= ((val & mask) << shift);
	return sgm_write_byte(reg, tmp);
}

/* ---- detection ---- */
static bool sgm_is_hw_exist(void)
{
	kal_uint8 regval = 0;
	kal_uint8 pn;

	if (sgm_read_byte(SGM_REG_0B, &regval))
		return false; /* no ACK -> not present, do not stall */

	pn = (regval >> SGM_PN_SHIFT) & SGM_PN_MASK;
	dprintf(CRITICAL, SGM_TAG "REG0B=0x%x PN=0x%x\n", regval, pn);
	return (pn == SGM_PN_SGM41513A);
}

/* ---- ops ---- */
static int sgm_dump_register(struct mtk_charger_info *info)
{
	kal_uint8 i, v;

	for (i = 0; i <= SGM_REG_0B; i++) {
		if (sgm_read_byte(i, &v) == 0)
			dprintf(CRITICAL, SGM_TAG "[0x%x]=0x%x\n", i, v);
	}
	return 0;
}

static int sgm_enable_charging(struct mtk_charger_info *info, bool en)
{
	dprintf(CRITICAL, SGM_TAG "enable_charging=%d\n", en);
	return sgm_config(SGM_REG_01, en ? 1 : 0, SGM_CHG_EN_MASK, SGM_CHG_EN_SHIFT);
}

static int sgm_set_ichg(struct mtk_charger_info *info, unsigned int ichg_ua)
{
	unsigned int ichg_ma = ichg_ua / 1000;
	kal_uint8 reg = ichg_ma / SGM_ICHG_STEP_MA;

	if (reg > SGM_ICHG_MASK)
		reg = SGM_ICHG_MASK;
	return sgm_config(SGM_REG_02, reg, SGM_ICHG_MASK, SGM_ICHG_SHIFT);
}

static int sgm_get_ichg(struct mtk_charger_info *info, unsigned int *ichg_ua)
{
	kal_uint8 v = 0;

	if (sgm_read_byte(SGM_REG_02, &v))
		return -EIO;
	*ichg_ua = ((v >> SGM_ICHG_SHIFT) & SGM_ICHG_MASK) * SGM_ICHG_STEP_MA * 1000;
	return 0;
}

static int sgm_set_aicr(struct mtk_charger_info *info, unsigned int aicr_ua)
{
	unsigned int aicr_ma = aicr_ua / 1000;
	kal_uint8 reg;

	if (aicr_ma < SGM_IINDPM_BASE_MA)
		aicr_ma = SGM_IINDPM_BASE_MA;
	reg = (aicr_ma - SGM_IINDPM_BASE_MA) / SGM_IINDPM_STEP_MA;
	if (reg > SGM_IINDPM_MASK)
		reg = SGM_IINDPM_MASK;
	return sgm_config(SGM_REG_00, reg, SGM_IINDPM_MASK, SGM_IINDPM_SHIFT);
}

static int sgm_get_aicr(struct mtk_charger_info *info, unsigned int *aicr_ua)
{
	kal_uint8 v = 0;

	if (sgm_read_byte(SGM_REG_00, &v))
		return -EIO;
	*aicr_ua = (SGM_IINDPM_BASE_MA +
		    ((v >> SGM_IINDPM_SHIFT) & SGM_IINDPM_MASK) * SGM_IINDPM_STEP_MA) * 1000;
	return 0;
}

static int sgm_set_mivr(struct mtk_charger_info *info, unsigned int mivr_uv)
{
	unsigned int mivr_mv = mivr_uv / 1000;
	kal_uint8 reg;

	if (mivr_mv < SGM_VINDPM_BASE_MV)
		mivr_mv = SGM_VINDPM_BASE_MV;
	reg = (mivr_mv - SGM_VINDPM_BASE_MV) / SGM_VINDPM_STEP_MV;
	if (reg > SGM_VINDPM_MASK)
		reg = SGM_VINDPM_MASK;
	return sgm_config(SGM_REG_06, reg, SGM_VINDPM_MASK, SGM_VINDPM_SHIFT);
}

static int sgm_enable_wdt(struct mtk_charger_info *info, bool en)
{
	/* 0 = watchdog disabled; keep it off in LK so it cannot reset us */
	return sgm_config(SGM_REG_05, en ? 0x1 : SGM_WDT_DISABLE, SGM_WDT_MASK, SGM_WDT_SHIFT);
}

static int sgm_reset_wdt(struct mtk_charger_info *info)
{
	return sgm_config(SGM_REG_01, 1, SGM_WD_RST_MASK, SGM_WD_RST_SHIFT);
}

static struct mtk_charger_ops sgm41513a_mchr_ops = {
	.dump_register = sgm_dump_register,
	.enable_charging = sgm_enable_charging,
	.get_ichg = sgm_get_ichg,
	.set_ichg = sgm_set_ichg,
	.get_aicr = sgm_get_aicr,
	.set_aicr = sgm_set_aicr,
	.set_mivr = sgm_set_mivr,
	.enable_wdt = sgm_enable_wdt,
	.reset_wdt = sgm_reset_wdt,
};

static struct sgm41513a_charger_info g_info = {
	.mchr_info = {
		.name = "primary_charger",
		.alias_name = "sgm41513a",
		.device_id = -1,
		.mchr_ops = &sgm41513a_mchr_ops,
	},
	.i2c = {
		.id = SGM41513A_I2C_ID,
		.addr = (SGM41513A_SLAVE_ADDR_WRITE >> 1),
		.mode = ST_MODE,
		.speed = 100,
	},
	.i2c_log_level = INFO,
};

/* Replicate the stock F25 LK init and force the charger watchdog off. */
static int sgm_init_setting(void)
{
	int ret = 0;

	ret |= sgm_write_byte(SGM_REG_00, 0x04); /* input current limit ~500mA, HIZ off */
	ret |= sgm_write_byte(SGM_REG_02, 0x9E); /* BOOST_LIM + ICHG ~1.8A */
	ret |= sgm_write_byte(SGM_REG_03, 0xDD); /* precharge / termination */
	ret |= sgm_write_byte(SGM_REG_04, 0x89); /* charge voltage limit + recharge */
	ret |= sgm_write_byte(SGM_REG_05, 0x8F); /* EN_TERM on, watchdog bits [5:4]=0 (disabled) */
	ret |= sgm_write_byte(SGM_REG_06, 0xE5); /* OVP / boost / input voltage limit */

	/* belt-and-braces: ensure the charger watchdog stays disabled */
	sgm_config(SGM_REG_05, SGM_WDT_DISABLE, SGM_WDT_MASK, SGM_WDT_SHIFT);
	/* ensure charging is enabled */
	sgm_config(SGM_REG_01, 1, SGM_CHG_EN_MASK, SGM_CHG_EN_SHIFT);

	return ret;
}

/*
 * Charger-capability hook consumed by mt_pmic_dlpt.c (normally provided by the
 * active charger driver). SGM41513A has a power path, but LK does not need the
 * DLPT power-path branch, so report false to keep that logic inert.
 */
bool is_power_path_supported(void)
{
	return false;
}

int sgm41513a_chg_probe(void)
{
	if (!sgm_is_hw_exist()) {
		dprintf(INFO, SGM_TAG "not present, skip\n");
		return 0; /* absent is not an error for the probe list */
	}

	dprintf(CRITICAL, SGM_TAG "detected on I2C7 @ 0x6B\n");
	sgm_init_setting();
	mtk_charger_set_info(&g_info.mchr_info);
	return 0;
}
