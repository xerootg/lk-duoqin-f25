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
MTK_CHARGER_NEW_ARCH := yes
MTK_PUMP_EXPRESS_PLUS_SUPPORT := no
MTK_CHARGER_INTERFACE := yes
MTK_MT6370_PMU_CHARGER_SUPPORT := yes
MTK_MT6370_PMU_BLED_SUPPORT := yes
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
DEFINES += MTK_MT6370_PMU
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
