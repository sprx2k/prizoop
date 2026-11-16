# Changelog

## Unreleased — fx-CG100 / Graph Math+ Port & Exam Mode Guard

### Added
- Full fxSDK (`FXCG50.cmake`) CMake build system supporting both **fx-CG100 / Graph Math+ (`-DPLATFORM=cw` -> `prizoop_cw.g3a`)** and **fx-CG50 (`-DPLATFORM=cg` -> `prizoop_cg.g3a`)**.
- `casiowin.yaml`, `libsyscall-cw.a`, and `libsyscall-cg.a` for direct CASIOWIN syscall dispatch supporting both OS `02.00.x202` and OS `02.10` on fx-CG100 / Graph Math+.
- `prizm.ld`, `src/fxlibc_hal.c`, `src/config.h.in`, `include/fxcg/*`, and vendored `utils/*` (`zx7`, `calctype`, `snd`, `scope_timer`, `ptune2_simple`).
- In-app Exam Mode / locked-storage guard (`src/exam_mode.h`, `src/exam_mode.cpp`) integrated into `main()`, `screen_rom`, and `screen_play`.
- Dual-target GitHub Actions CI build in `.github/workflows/build-g3a.yml` uploading `prizoop_cw.g3a` and `prizoop_cg.g3a`.
- Documentation in `README.md` and `docs/CG100_PORT.md`.

### Fixed
- `src/display_directlcd.cpp`: Added `SelectVRAMDataRegister()` with `0xDA` ID register detection (`0x32` / `0x52` -> `0x2C`, otherwise `0x202`) and `DISPLAY` pointer (`0xB4000000`) so both older R61524 and newer LCD drivers render properly.
- `src/screen_play.cpp`: Fixed hardcoded pause-key release check `keyDown_fast(48)` (which is `OK` on fx-CG100) to `keyDown_fast(_(48, 69))` (`HOME` on fx-CG100).
- `src/emulator.cpp`, `src/keys.cpp`, `src/screen_settings.cpp`: Added fx-CG100 `GetKey` menu tab codes, `KEY_CTRL_EXIT` clean exit, `HOME` cancel key/prompt, and verified fx-CG100 default button mappings.
- `src/emulator.h`: Isolated `SETTINGS_VERSION` (`0x104` on `cw`) so stale fx-CG50 MCS settings do not break fx-CG100 controls.
- `src/platform.h`, `src/main.cpp`: Fixed backslash `#include` paths for Linux toolchains and added `shutdown()` call after `emulator.run()`.
- `src/cpu.cpp`: Replaced unsafe `exit(-1)` in `undefined()` with a `GetKey` loop.
