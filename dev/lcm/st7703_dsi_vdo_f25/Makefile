#
# ST7703 640x960 DSI video-mode LCM for the Duoqin Qin F25 Pro.
#
# For the LK build, dev/lcm/rules.mk compiles this driver automatically when
# CUSTOM_LK_LCM selects "st7703_dsi_vdo_f25". This Makefile mirrors the other
# dev/lcm drivers for the kernel-style build path.
#

obj-y += st7703_dsi_vdo_f25.o

ccflags-$(CONFIG_MTK_LCM) += -I$(srctree)/drivers/misc/mediatek/lcm/inc
