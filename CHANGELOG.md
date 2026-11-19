# Changelog

## Unreleased — fx-CG100 / Graph Math+ Port & Exam Mode Guard

### Added
- Full fxSDK (`FXCG50.cmake`) CMake build system supporting both **fx-CG100 / Graph Math+ (`-DPLATFORM=cw` -> `prizoop_cw.g3a`)** and **fx-CG50 (`-DPLATFORM=cg` -> `prizoop_cg.g3a`)**.
- `casiowin.yaml`, `libsyscall-cw.a`, and `libsyscall-cg.a` for direct CASIOWIN syscall dispatch supporting both OS `02.00.x202` and OS `02.10` on fx-CG100 / Graph Math+.
- `prizm.ld`, `src/fxlibc_hal.c`, `src/config.h.in`, `include/fxcg/*`, and vendored `utils/*` (`zx7`, `calctype`, `snd`, `scope_timer`, `ptune2_simple`).
- In-app Exam Mode / locked-storage guard (`src/exam_mode.h`, `src/exam_mode.cpp`) integrated into `main()`, `screen_rom`, and `screen_play`.
- Dual-target GitHub Actions CI build in `.github/workflows/build-g3a.yml` uploading `prizoop_cw.g3a` and `prizoop_cg.g3a`.
- Documentation in `README.md` and `docs/CG100_PORT.md`.

### Changed
- Rebound fx-CG100 (`PLATFORM=cw`) menu tab keys off OS-intercepted `HOME` (`30003`) to non-intercepted keys: `|<` (`30075`, Tab 1 Select ROM), `SETTINGS` (`30037`, Tab 2 Settings), `PGDN` (`30066`, Tab 3), `X,T,θ` (`30001`, Tab 4), `>|` (`30074`, Tab 5 FAQ Viewer), and `PGUP` (`30065`, Tab 6 Play).
- Replaced the global `_()` macro in `src/config.h.in` with `PRIZOOP_PLATFORM` / `PRIZOOP_IS_CW` / `PRIZOOP_SEL(cg, cw)`.
- Disabled 2.5mm serial/SCIF2 audio output and the Sound menu option on fx-CG100 (`PLATFORM=cw`).

### Removed
- Removed legacy PrizmSDK/Windows build scripts and stale artifacts (`Makefile`, `clean.bat`, `copy.bat`, `make.bat`, `make-DeviceRelease.bat`, `rebuild.bat`, legacy `prizoop.g3a`, `README.txt`, `.stackedit-trash/Welcome file.md`, and prebuilt `proj/Release/comp-gb.exe`).
- Removed unused `-lgint-cg` link dependency and updated `CMakeLists.txt` / `.github/workflows/build-g3a.yml`.

### Fixed
- **Linker, HAL & Platform:** Fixed `.data`/`.bss` 4-byte alignment and preserved `.init_array`/`.fini_array`/`.ctors` in `prizm.ld`; guarded `calloc` multiplication overflow and replaced unsafe variadic `printf` macro with `prizoop_printf()`; fixed `include/fxcg/registers.h` `MSTPCR0` bitfield (`0xA4150030`) and `GetVRAMAddress()` / `getDeviceType()`; restricted `Ptune2_direct.c` strictly to `DT_CG20` and fixed its `FRQCR` mask index; fixed `main()` exit trap on fx-CG50 and full resource teardown in `shutdown()`.
- **CPU, Memory, Timer, Interrupts & CGB:** Fixed `DAA` (subtraction & carry), `STOP` wakeup, `EI` 1-instruction delay, `HALT` bug, `LD HL, SP+e` / `ADD SP, e` flags, `LDH (C), A` / `LDH A, (C)` and `LD (a16), A` memory mapping, `readByte`/`writeByte` echo RAM, `OAM` DMA (`0xFF46`), `DIV`/`TIMA` overflow & `TAC` falling-edge reload, `JOYP` (`0xFF00`) active-low matrix & interrupt, `IF`/`IE` upper bits, CGB HDMA/GDMA & WRAM/VRAM banking, and CGB bootstrap ROM/palette setup.
- **Graphics, Display DMA, GPU, ZX7 & CalcType:** Cached `0xDA` LCD register selection (`0x2C` vs `0x202`), enabled `DMAC0` in `MSTPCR0` before DMA access, added `TeardownDisplayDriver()`, fixed frameskip bit-reversal and DMA wait on skipped frames, fixed LCD-off re-enable and pause-preview deadlock in `gpu.cpp`, fixed `display_preview.cpp` lineBuffer offset and CGB palette resolution, hardened `zx7` decompression against truncated/corrupted streams and buffer overflows, fixed scanline sprite priority/window bugs, and fixed `calctype.c` newline clipping and negative X floor division.
- **Storage, MBC, RTC, Save States, Audio & UI:** Added `isStorageLockedSafe()` to prevent open-file Exam Mode false positives; fixed MBC1 5+2-bit banking and mode-1 bank-0 switching, MBC2 bit-8 decode and 512-byte echo, MBC3 RTC rollover/leap-year/latch/endianness, `.gbz` bounds and 4 MB support, atomic delete+recreate saves for SRAM and save states with full validation and runtime resync, wrap-safe TMU1 audio pacing and APU sweep/wave/LFSR fixes, non-blocking edge-triggered volume keys, `screen_rom` directory/long-name filtering and preview redraw, `screen_settings` OS key buffer drain, `screen_faq` iterative scrolling (no recursion), `ScopeTimer` divide-by-zero guards, and `comp-gb.cpp` include/resource fixes.
