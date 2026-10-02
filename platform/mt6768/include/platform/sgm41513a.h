/*
 * SGM41513A single-cell Li-ion switching charger - LK driver header.
 *
 * The Duoqin Qin F25 Pro uses an SGM41513A charger on I2C7 at 7-bit address
 * 0x6B (write byte 0xD6). Register map follows the SGM4154x family. Register
 * semantics and the F25 init values were confirmed against the stock F25 LK
 * boot log (sgm41513a_* register writes) and the SGM41513/A/D datasheet.
 */
#ifndef _SGM41513A_SW_H_
#define _SGM41513A_SW_H_

#include <platform/mt_typedefs.h>

/* I2C: bus 7, 7-bit slave address 0x6B (write 0xD6 / read 0xD7) */
#define SGM41513A_I2C_ID            I2C7
#define SGM41513A_SLAVE_ADDR_WRITE  0xD6
#define SGM41513A_SLAVE_ADDR_READ   0xD7

/* Registers */
#define SGM_REG_00  0x00  /* EN_HIZ, input current limit (IINDPM) */
#define SGM_REG_01  0x01  /* WD_RST, OTG, charge-enable (CHG_CONFIG), SYS_MIN */
#define SGM_REG_02  0x02  /* BOOST_LIM, fast charge current (ICHG) */
#define SGM_REG_03  0x03  /* precharge / termination current */
#define SGM_REG_04  0x04  /* charge voltage limit (VREG), recharge */
#define SGM_REG_05  0x05  /* EN_TERM, watchdog, charge timer */
#define SGM_REG_06  0x06  /* OVP, boost voltage, input voltage limit (VINDPM) */
#define SGM_REG_07  0x07  /* IINDET, BATFET control */
#define SGM_REG_08  0x08  /* system status (charge/power-good state) */
#define SGM_REG_09  0x09  /* fault */
#define SGM_REG_0A  0x0A  /* VBUS good / dpm status */
#define SGM_REG_0B  0x0B  /* REG_RST, part number, device revision */
#define SGM_REG_0C  0x0C
#define SGM_REG_0D  0x0D
#define SGM_REG_0E  0x0E  /* input source detection */
#define SGM_REG_0F  0x0F
#define SGM_REG_NUM 16

/* REG00 - input current limit */
#define SGM_EN_HIZ_MASK         0x01
#define SGM_EN_HIZ_SHIFT        7
#define SGM_IINDPM_MASK         0x1F   /* [4:0] */
#define SGM_IINDPM_SHIFT        0
#define SGM_IINDPM_BASE_MA      100
#define SGM_IINDPM_STEP_MA      100

/* REG01 - charge enable / watchdog reset / OTG */
#define SGM_WD_RST_MASK         0x01
#define SGM_WD_RST_SHIFT        6
#define SGM_OTG_EN_MASK         0x01
#define SGM_OTG_EN_SHIFT        5
#define SGM_CHG_EN_MASK         0x01
#define SGM_CHG_EN_SHIFT        4

/* REG02 - fast charge current */
#define SGM_ICHG_MASK           0x3F   /* [5:0] */
#define SGM_ICHG_SHIFT          0
#define SGM_ICHG_STEP_MA        60

/* REG05 - watchdog timer (00 = disabled) */
#define SGM_WDT_MASK            0x03   /* [5:4] */
#define SGM_WDT_SHIFT           4
#define SGM_WDT_DISABLE         0x00

/* REG06 - input voltage limit (MIVR / VINDPM) */
#define SGM_VINDPM_MASK         0x0F   /* [3:0] */
#define SGM_VINDPM_SHIFT        0
#define SGM_VINDPM_BASE_MV      3900
#define SGM_VINDPM_STEP_MV      100

/* REG0B - part number detect. Stock F25 reads 0x08 -> PN field = 0x1 */
#define SGM_PN_MASK             0x0F   /* [6:3] */
#define SGM_PN_SHIFT            3
#define SGM_PN_SGM41513A        0x01

extern int sgm41513a_chg_probe(void);

#endif /* _SGM41513A_SW_H_ */
