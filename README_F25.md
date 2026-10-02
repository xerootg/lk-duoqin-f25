# Duoqin Qin F25 Pro — LK (Little Kernel) port

A custom Little Kernel bootloader project for the **Duoqin Qin F25 Pro**
(MediaTek **MT6768**), built on the `svoboda18/lk` MT6768 platform and the
`k68v1_64` reference project.

This is a **scaffold**: its full build graph resolves (verified with
`make f25_64 -n`, which links through `target/k68v1_64` on `platform/mt6768`),
but the display init and some timing values need on-device validation (see
*Validation status*).

## What this adds

- `project/f25_64.mk` — the F25 board project (derived from `project/k68v1_64.mk`).
- `dev/lcm/st7703_dsi_vdo_f25/` — a Sitronix **ST7703, 640×960** MIPI-DSI
  video-mode LCM driver (the F25 drives the panel directly from DSI0; there is
  **no MT6382 bridge and no DSC** on this board).
- Registration of that driver in `dev/lcm/mt65xx_lcm_list.c`.
- A build-time option `CUSTOM_LK_SKIP_UNLOCK_WARNING` (enabled by the F25
  project) that suppresses the cosmetic orange "device is unlocked" on-screen
  warning in `platform/common/boot/vboot_state.c`. (This fork's
  `orange_state_warning()` already has its 5-second `mdelay` commented out, so
  there is no boot delay to remove — only the on-screen print is skipped.)

## Hardware summary

| Block     | Part / value |
|-----------|--------------|
| SoC       | MediaTek MT6768 (`platform/mt6768`) |
| PMIC      | MT6358 (pwrap `0x1000d000`) |
| Charger   | MT6370 (PMU charger + backlight) |
| Panel     | Sitronix ST7703, 640×960, MIPI-DSI video, 4-lane, dual-sourced (yuxing / yihua) |
| Display   | MT6768 DSI0 → ST7703 **directly** (no MT6382 bridge, no DSC) |
| DRAM/EMI  | brought up by the preloader, not LK |
| Secure boot | SBC fused **off** — an unsigned, self-built LK boots |
| Layout    | A/B (`boot_a/boot_b`, `lk_a/lk_b`, `vbmeta_a/_b`, `dtbo_a/_b`, …) + single `super` |

Partition order (from the MT6768 scatter):

```
preloader pgpt misc para expdb frp
vbmeta_a vbmeta_system_a vbmeta_vendor_a vbmeta_b vbmeta_system_b vbmeta_vendor_b
md_udc metadata nvcfg nvdata protect1 protect2 seccfg
md1img_a spmfw_a scp_a sspm_a gz_a lk_a boot_a vendor_boot_a dtbo_a tee_a
sec1 proinfo boot_para nvram logo
md1img_b spmfw_b scp_b sspm_b gz_b lk_b boot_b vendor_boot_b dtbo_b tee_b
super userdata otp flashinfo sgpt
```

Because the device is A/B, `project/f25_64.mk` sets `MTK_AB_OTA_UPDATER := yes`
(the `k68v1_64` reference had it off).

## Building in an Android build tree

`svoboda18/lk` is designed to be built as part of an Android/MTK vendor tree via
its `Android.mk` (it produces `$(PRODUCT_OUT)/lk.img`). Place this repository at
the LK path your device expects (commonly
`vendor/mediatek/proprietary/bootable/bootloader/lk`) and point the device at the
`f25_64` project:

1. In the device makefile (e.g. `device/duoqin/f25/device.mk` or the board
   config), set:

   ```make
   LK_PROJECT    := f25_64
   LCM_WIDTH     := 640
   LCM_HEIGHT    := 960
   ```

   (The project also sets `LCM_WIDTH`/`LCM_HEIGHT`; exporting them from the device
   makefile keeps the logo renderer and `Android.mk` consistent.)

2. Ensure the LK toolchain variables the `Android.mk` consumes are set by your
   tree: `TARGET_TOOLS_PREFIX` / `SOONG_CLANG` (the arm cross toolchain). The repo
   also bundles a `gcc/` toolchain for the standalone path.

3. Build the LK target from the tree root:

   ```sh
   make lk           # or: make bootimage / make droidcore, which depends on lk
   ```

   The result is `$(PRODUCT_OUT)/lk.img` (the built `lk.img` is the LK binary
   concatenated with the LK DTB, per `Android.mk`).

### Standalone build (quick syntax/iteration check)

From the repo root, with an arm cross toolchain available, the project can be
built directly:

```sh
make f25_64 TOOLCHAIN_PREFIX=<arm-none-eabi-> LCM_WIDTH=640 LCM_HEIGHT=960
```

This path is mainly useful for catching compile errors while iterating on the
project/LCM files; the Android-tree path above is the supported build.

## Flashing

Flash the resulting `lk.img` to the **active** slot (`lk_a` or `lk_b`) with
mtkclient (Write Partition, nothing else selected). Keep a dump of the stock
`lk_a`/`lk_b` as a rollback — reflash it if the device misbehaves. SBC is off, so
no signing is required.

## Validation status

| Area | Status |
|------|--------|
| Project config (platform, charger, A/B, AVB2.0, GPT, unsigned) | Derived from the MT6768 reference + confirmed F25 partition layout |
| Boot / kernel hand-off | Expected to work headless (the kernel brings up the display) |
| Orange-warning skip | Straightforward gated change in `vboot_state.c` (on-screen print only; the delay was already removed upstream in this fork) |
| **ST7703 init sequence** | **Needs on-device validation.** Ported from the mainline ST7703 "xbd599" reference (720-wide). Resolution/timing registers (`SETDISP 0xB2`, `SETRGBIF 0xB3`, `SETMIPI 0xBA`) and the `LCM_PARAMS` porches/`PLL_CLOCK` are placeholders for 640×960 and should be confirmed against the stock F25 LK ST7703 table. |
| yuxing vs yihua panel discrimination | Not implemented; `lcm_compare_id()` accepts any ST7703. Add the lcd-id branch if the two vendors' init diverges. |
| Boot logo | `BOOT_LOGO := hd720` placeholder; 640×960 has no stock asset set — supply a correctly sized `logo.bin`. |

Display is not required for first boot — bring the board up headless, confirm the
kernel hand-off, then tune the panel.

## Scope / what this does NOT do

This project intentionally contains **no bootloader-attestation changes**. It does
not forge the TEE Root of Trust and does not spoof the device lock state to the OS
or to remote verifiers. The boot state reported to the kernel cmdline
(`androidboot.verifiedbootstate`) and to the TEE reflects the device's true
locked/unlocked state. The only unlock-related change here is cosmetic: skipping
the local orange warning screen on boot.
