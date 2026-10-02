#
# LK project for the Duoqin Qin F25 Pro (MediaTek MT6768).
#
# Derived from the MT6768 reference project k68v1_64.mk. Differences versus the
# reference are called out inline. This project targets the real F25 hardware:
#   SoC     : MT6768 (platform/mt6768)
#   PMIC    : MT6358
#   Charger : MT6370 (PMU charger + backlight) -- same as the k68 reference
#   Panel   : Sitronix ST7703, 640x960 MIPI-DSI video mode, dual-sourced
#             (yuxing / yihua), driven directly by DSI0 (no MT6382 bridge, no DSC)
#   Layout  : A/B (boot_a/boot_b, lk_a/lk_b, vbmeta_a/_b, ...) with a single super
#   Secure  : SBC (secure boot) is fused OFF on this device, so an unsigned LK boots.
#
LOCAL_DIR := $(GET_LOCAL_DIR)
# Reuse the board-generic MT6768 target (target/k68v1_64), which sets
# PLATFORM := mt6768 and resolves display geometry at runtime from
# CFG_DISPLAY_* / DISP_GetScreenWidth()/Height(), so the F25's 640x960 panel is
# still honored. There is no target/f25_64 tree; PROJECT stays f25_64 so every
# knob set in this file still applies.
TARGET := k68v1_64
MODULES += app/mt_boot \
           dev/lcm
MTK_EMMC_SUPPORT = yes
MTK_MMC_COMBO_DRV = yes
MTK_KERNEL_POWER_OFF_CHARGING = yes
MTK_SMI_SUPPORT = yes
DEFINES += MTK_NEW_COMBO_EMMC_SUPPORT
DEFINES += MTK_GPT_SCHEME_SUPPORT
# In this tree rules.mk gates ALL charger drivers + mt6battery + mtk_charger_intf
# under `ifneq(MTK_CHARGER_NEW_ARCH, yes)`, with no else branch. So NEW_ARCH=yes
# (the k68v1_64 default) compiles ZERO charger code. Set it to no so the SGM41513A
# charger, battery, and the charger interface are actually built.
MTK_CHARGER_NEW_ARCH := no
MTK_PUMP_EXPRESS_PLUS_SUPPORT := no
MTK_CHARGER_INTERFACE := yes
# F25 charger is an SGM41513A on I2C7 @ 0x6B (confirmed from the stock LK boot log),
# not the MT6370. Backlight is native MTK PWM (per HARDWARE.md), not MT6370 BLED.
MTK_MT6370_PMU_CHARGER_SUPPORT := no
MTK_MT6370_PMU_BLED_SUPPORT := no
MTK_SGM41513A_CHARGER_SUPPORT := yes

# --- BRING-UP SAFETY: skip LK battery/charger init -------------------------
# platform.c battery init (both the MTK_CHARGER_NEW_ARCH=yes path and the
# legacy mt65xx_bat_init() path this project uses) contains several
# mt_power_off() guards keyed on battery-voltage / charger / PMIC BATON reads
# (mt_battery.c:542/567/578; common/power/mtk_charger.c:335/386/406/576/661).
# These were VALIDATED on stock (stock built MTK_CHARGER_NEW_ARCH=yes) but the
# F25's actual charger IC is not in any LK probe list (the stock PRELOADER log
# probes eta6937, not SGM41513A), so an unvalidated read here can power the
# device off and look like a boot-loop that leaves nothing in expdb.
# LK does not need to charge in order to hand off to the kernel; the kernel
# re-inits the charger. Skip battery init for the first-boot milestone, then
# remove this once the charger IC identity + voltage/BATON reads are confirmed.
DEFINES += NO_BAT_INIT
# ---------------------------------------------------------------------------

# --- NO-UART OBSERVABILITY: progress beacon to expdb -----------------------
# Our lk never persists its log because stock mt6768 mt_pmic.c omitted the
# save_pllk_log() call its sibling platforms have (now restored). On a WDT
# reset the power-off path is never reached, so this flushes the pl+lk log
# ring to expdb's last-2MB log_store region at platform_init stage boundaries
# (##F25-BEACON## markers). After a failed boot, dump expdb over BROM with
# mtkclient and `strings | grep F25-BEACON` to see how far LK got.
# Remove once the boot is understood.
DEFINES += CUSTOM_LK_LOG_BEACON
# ---------------------------------------------------------------------------

MTK_LCM_PHYSICAL_ROTATION = 0

# F25 panel: ST7703 640x960 video-mode LCM driver added in dev/lcm/st7703_dsi_vdo_f25.
# CUSTOM_LK_LCM is uppercased by dev/lcm/rules.mk into the define that selects the
# driver entry in dev/lcm/mt65xx_lcm_list.c (here: ST7703_DSI_VDO_F25).
CUSTOM_LK_LCM = "st7703_dsi_vdo_f25"
LCM_WIDTH = 640
LCM_HEIGHT = 960

MTK_SECURITY_SW_SUPPORT = yes
MTK_VERIFIED_BOOT_SUPPORT = no
MTK_SEC_FASTBOOT_UNLOCK_SUPPORT = yes
SPM_FW_USE_PARTITION = yes

# 640x960 does not match any stock logo asset set (fwvga/hd720/fhdplus). hd720 is the
# closest; the LK logo renderer scales/positions using LCM_WIDTH/HEIGHT. Replace with a
# correctly sized logo.bin or a matching BOOT_LOGO value once the panel is validated.
BOOT_LOGO := hd720

DEBUG := 2
DEFINES += WITH_DEBUG_UART=1
CUSTOM_LK_USB_UNIQUE_SERIAL = no
MTK_TINYSYS_SCP_SUPPORT = yes
MTK_PROTOCOL1_RAT_CONFIG = C/Lf/Lt/W/T/G
MTK_GOOGLE_TRUSTY_SUPPORT = no
# MT6370 PMU disabled on F25 (charger is SGM41513A; backlight is native PWM).
# DEFINES += MTK_MT6370_PMU
DEVELOP_STAGE = SB
MTK_TINYSYS_SSPM_SUPPORT = yes
MTK_VPU_SUPPORT = no
MTK_DM_VERITY_OFF = no
MTK_DYNAMIC_CCB_BUFFER_GEAR_ID =
MTK_AVB20_SUPPORT := yes
MTK_SMC_ID_MGMT = yes

# The F25 is an A/B device (boot_a/boot_b, lk_a/lk_b, vbmeta_a/_b, dtbo_a/_b, ...).
# The reference k68v1_64 sets this to "no"; it MUST be "yes" here or the LK will use the
# non-A/B boot path and fail to locate/verify the active slot.
MTK_AB_OTA_UPDATER := yes

# Owner convenience: skip the orange "device is unlocked" warning screen on a normal
# boot. This ONLY suppresses the local on-screen nag for the orange (user-unlocked)
# case in platform/common/boot/vboot_state.c. (This fork's orange_state_warning() already
# has its 5s mdelay commented out, so there is no boot delay to remove -- only the
# on-screen print is skipped.) It deliberately does NOT alter the boot state reported to
# the OS cmdline or to the TEE Root of Trust -- the device still reports its true
# unlocked/orange state everywhere a verifier can read it. Yellow/red (real integrity
# failure) warnings are left untouched.
DEFINES += CUSTOM_LK_SKIP_UNLOCK_WARNING
