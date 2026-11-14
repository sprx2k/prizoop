# Prizoop fx-CG100 / Graph Math+ Technical Port Guide

This document explains why the previous fx-CG100 build failed to compile or run games, and documents every change made in this port.

## 1. Root Causes of the Previous Failure

1. **Incomplete CMake / Stub Build Setup:**
   - The previous `CMakeLists.txt` referenced missing files (`cmake/platform_cg100.c`, `cmake/ubc_hook.c`, `cmake/main_glue.c`) and excluded assembly scanline renderers (`src/asm/*.S`).
   - `.github/workflows/build-g3a.yml` configured fxSDK without `-DPLATFORM=cw`.
   - Legacy `prizoop.g3a` binaries built for fx-CG50 with PrizmSDK relied on the `0x80020070` syscall jump table, which Casio removed in the fx-CG100 / Graph Math+ OS. Calling the missing table crashes or hangs when launching a game.

2. **Keypad Matrix & `GetKey` Code Changes:**
   - The fx-CG100 / Graph Math+ uses a redesigned ClassWiz 48-key layout (no `F1`–`F6` keys, `MENU` replaced by `HOME`, `EXIT` replaced by `BACK`, relocated D-pad/SHIFT/ALPHA matrix positions).
   - Menu tab switching (`GetKey`) returned `30037` (`SETTINGS`), `30003` (`HOME`), `30075` (`|<`), `30066` (`PGDN`), `30074` (`>|`), `30065` (`PGUP`) instead of `KEY_CTRL_F1`..`F6`.
   - In `screen_play.cpp`, the post-exit key-release wait checked hardcoded key matrix code `48` (`MENU` on fx-CG50, but `OK` on fx-CG100!) instead of `_(48, 69)` (`HOME` on fx-CG100), causing input/exit glitches.

3. **LCD Controller DMA Register Differences (`0x202` vs `0x2C`):**
   - Classic fx-CG50 and early fx-CG100 / Graph Math+ units use an **R61524**-compatible controller (`LCD_GRAM = 0x202`).
   - Newer fx-CG100 units (and 2025 bootcode fx-CG50 units) use a newer panel driver whose GRAM write command is **`0x2C`** rather than `0x202`. Direct DMA scanline rendering fails (blank/frozen display) if `0x202` is sent unconditionally.

## 2. Port Architecture (`PLATFORM=cw` / `PLATFORM=cg`)

- **Build & Linker Integration:**
  - `CMakeLists.txt` + `src/config.h.in` define `PLATFORM` (`cw` = `1` for fx-CG100/Math+, `cg` = `0` for fx-CG50) and helper macro `_(CG, CW)`.
  - `prizm.ld` maps `.text`/`.rodata` at `0x00300000`, `.bss`/`.data` at `0x08100000`, `.ilram` at `0xE5200000`, and `.xyram` at `0xE500E000`.
  - `src/fxlibc_hal.c` routes `fxlibc` heap calls (`fxlibc_hal_malloc/free/realloc`) to CASIOWIN `sys_malloc`/`sys_free`/`sys_realloc` and stubs POSIX `write`/`lseek`.
  - Vendored static utility libraries under `utils/`: `zx7`, `calctype`, `snd`, `scope_timer`, `ptune2_simple`.
  - Minimal Casio Prizm headers under `include/fxcg/*.h` and `include/fxcg_registers.h`.

- **Syscall Resolution (`casiowin.yaml`, `libsyscall-cw.a`, `libsyscall-cg.a`):**
  - Instead of jumping through `0x80020070`, `libsyscall-cw.a` inspects the OS version header at `0xA0020020` (selecting index `0` for OS `02.00.0202`/`02.00.2202` and index `1` for OS `02.10`) and dispatches every CASIOWIN function (`Bfile_*`, `Bdisp_*`, `MCS_*`, `GetKey`, `RTC_GetTicks`, `CMT_Delay_micros`, `OS_InnerWait_ms`, `Serial_*`, `SetQuitHandler`, `EnableStatusArea`, `PrintMini`, `SaveVRAM_1`/`LoadVRAM_1`, `DrawFrame`, `sys_malloc/free/realloc`) directly.

- **Adaptive LCD DMA Driver (`src/display_directlcd.cpp`):**
  - `SelectVRAMDataRegister()` queries LCD register `0xDA` (`*DISPLAY` at `0xB4000000`). If the panel reports `0x32` or `0x52`, it selects `0x2C`; otherwise it selects `0x202` (`LCD_GRAM`). This supports both older and newer fx-CG100 displays as well as 2025 fx-CG50 hardware.

- **Keyboard & Settings Compatibility:**
  - `src/emulator.h` separates `SETTINGS_VERSION` (`4` on `cg`, `0x104` on `cw`) so MCS settings saved by an older fx-CG50 build cannot poison fx-CG100 key bindings.
  - `src/emulator.cpp`, `src/keys.cpp`, `src/screen_play.cpp`, and `src/screen_settings.cpp` map all UI tabs, pause/cancel keys (`_(48, 69)`), and Game Boy buttons cleanly to verified fx-CG100 scan codes.
  - `src/cpu.cpp` replaces `exit(-1)` on undefined opcodes with a safe `GetKey` loop.

## 3. Exam Mode Enforcement (`src/exam_mode.h`, `src/exam_mode.cpp`)

1. **Bootloader / OS Lock:** MPM disables `HOME -> TOOLS` whenever Examination Mode is active on fx-CG100 / Graph Math+, preventing add-ins from starting. CASIOWIN similarly blocks `.g3a` execution on fx-CG50.
2. **In-App Guard (`isStorageLockedOrExamMode()`):** Because Casio OS locks `\\fls0\` storage access during Examination Mode, Prizoop checks root storage accessibility at startup (`main()`), in the ROM list (`screen_rom::select()`), and prior to ROM loading (`screen_play::initRom()`), refusing to browse or launch ROMs and displaying an Exam Mode access-disabled screen if storage is restricted. Note: Casio fx-CG100 OS does not expose a documented public cross-version Exam Mode status syscall, so MPM's launcher lock remains the primary enforcement boundary while Prizoop's storage-lock guard acts as secondary defense-in-depth.
