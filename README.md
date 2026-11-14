![Options Screen](https://github.com/tswilliamson/prizoop/raw/master/Screens/Options.png?raw=true)

Prizoop v1.1 (fx-CG100 / Graph Math+ & fx-CG50)
================================================

Prizoop is a Game Boy and Game Boy Color emulator for Casio graphing calculators, focused on high speed on SH7305 hardware. This fork adds full **Casio fx-CG100 / Graph Math+ (`PLATFORM=cw`, MPM)** support alongside **fx-CG50 / fx-CG10 / fx-CG20 (`PLATFORM=cg`)** using **fxSDK**.

For deep technical details on what failed previously and how the fx-CG100 port, LCD driver detection, syscall mapping, and Exam Mode enforcement work, see [docs/CG100_PORT.md](docs/CG100_PORT.md) and [CHANGELOG.md](CHANGELOG.md).

## Supported Calculators & Requirements

- **Casio fx-CG100 / Graph Math+ (`prizoop_cw.g3a`):**
  - Requires **MPM (Math Plus Mod)** installed on the calculator.
  - Supports OS **02.00.0202 / 02.00.2202** and **02.10** via `casiowin.yaml` / `libsyscall-cw.a` dual-version syscall dispatch.
  - Supports both older **R61524 (`0x202`)** and newer **(`0x2C`)** LCD controllers automatically via `0xDA` ID register detection.
- **Casio fx-CG50 / Graph 90+E (`prizoop_cg.g3a`):**
  - Built with `-DPLATFORM=cg`, preserving native fx-CG50 key mappings and `libsyscall-cg.a`.

## Install

1. Download the `.g3a` for your calculator from the **GitHub Actions** (`Build G3A` workflow) artifact:
   - `prizoop-cg100-g3a` -> `prizoop_cw.g3a` for **fx-CG100 / Graph Math+**
   - `prizoop-cg50-g3a` -> `prizoop_cg.g3a` for **fx-CG50 / Graph 90+E**
2. Connect your calculator to your PC via USB (select USB Flash / Mass Storage mode).
3. Copy `prizoop_cw.g3a` (and `mpm.bin` if not already installed) to the calculator's **root directory** (`\\fls0\`).
4. Copy your Game Boy (`.gb`), Game Boy Color (`.gbc`), or compressed (`.gbz`) ROM files into the **root directory** (`\\fls0\`). Keep filenames short and simple (8.3 / under 12 characters, e.g., `Tetris.gb`, `LinksA.gbc`).
5. Safely eject the calculator before unplugging USB.
6. On **fx-CG100 / Graph Math+**, go to **HOME** and press **TOOLS** to open the MPM add-in menu, then select **Prizoop**.

> **Storage Note:** The fx-CG100 exposes ~4.5 MB of USB storage memory by default. Large GBC ROMs (1–4 MB) fit more easily if compressed to `.gbz` with `comp-gb/comp-gb.exe` on Windows.

## Exam Mode Restriction

Prizoop enforces Examination Mode restrictions at two layers:

1. **System / Bootloader Lock (Primary):**
   - On **fx-CG100 / Graph Math+**, MPM checks Examination Mode and disables the `HOME -> TOOLS` add-in launcher (`mpm.bin` will not run while Exam Mode is active).
   - On **fx-CG50**, CASIOWIN blocks third-party `.g3a` add-ins during Examination Mode.
2. **In-App Locked-Storage Guard (Defense-in-Depth):**
   - Because Casio OS restricts `\\fls0\` storage memory access in Examination Mode, Prizoop verifies storage accessibility at startup (`src/exam_mode.cpp`), in the ROM browser (`src/screen_rom.cpp`), and before ROM initialization (`src/screen_play.cpp`).
   - If storage memory is locked/restricted, Prizoop displays an `"ACCESS DISABLED (EXAM MODE)"` notice and blocks browsing or launching ROMs.

## Controls

### Menu Navigation (fx-CG100 / Graph Math+)

- **D-Pad (Arrows):** Navigate ROMs, settings, and FAQ
- **SHIFT** or **EXE / OK:** Select / Launch ROM / Toggle option
- **SETTINGS:** Tab 1 — Select ROM
- **HOME:** Tab 2 — Settings
- **>| (END):** Tab 5 — FAQ Viewer
- **PGUP:** Tab 6 — Play / Load ROM
- **BACK:** Exit Prizoop and return to OS / MPM

*(On fx-CG50, the top function keys `F1`–`F6` correspond to Tabs 1–6 and `MENU` exits.)*

### In-Game Default Controls (fx-CG100 / Graph Math+)

All buttons can be remapped in **Settings -> Map Keys** (press **HOME** during mapping to cancel; on fx-CG50 press **MENU**):

| Game Boy Button | fx-CG100 / Graph Math+ Key | fx-CG50 Key |
|---|---|---|
| D-Pad | Arrow Keys | Arrow Keys |
| A | `SHIFT` | `SHIFT` |
| B | `ALPHA` | `OPTN` |
| Select | `>\|` (END) | `F5` |
| Start | `PGUP` | `F6` |
| Save State | `×` (Multiply) | `×` (Alpha `S`) |
| Load State | `)` | `→` (Alpha `L`) |
| Pause / Options | `HOME` | `MENU` |
| Volume + / - | `+` / `-` | `+` / `-` |

> **Note:** Hardware overclock (`Ptune2`) is automatically disabled on fx-CG100 and fx-CG50 (`Ptune2` timings are specific to fx-CG10/CG20). Do not force CG20 overclock registers on CG100.

## Building from Source (fxSDK)

Prizoop is built via CMake and **fxSDK** (`FXCG50.cmake`) with `gint`/`fxlibc` installed via `giteapc`:

```bash
# fx-CG100 / Graph Math+
cmake -B build-cw -S . -DCMAKE_TOOLCHAIN_FILE=$HOME/.local/lib/cmake/fxsdk/FXCG50.cmake -DPLATFORM=cw
cmake --build build-cw -j

# fx-CG50 / Graph 90+E
cmake -B build-cg -S . -DCMAKE_TOOLCHAIN_FILE=$HOME/.local/lib/cmake/fxsdk/FXCG50.cmake -DPLATFORM=cg
cmake --build build-cg -j
```

See `.github/workflows/build-g3a.yml` for the automated Ubuntu 24.04 + `giteapc` build pipeline.

## Credits & Acknowledgements

- Original **Prizoop** by `tswilliamson`, based on **Cinoop** by `CTurt`.
- **fxSDK / gint / fxlibc / MPM** by `Lephenixnoir` and the Planète Casio community.
- **fx-CG100 / ClassWiz syscall mapping (`casiowin.yaml`) and LCD `0xDA` DMA detection** adapted from community fxSDK porting work (`bju12290/prizoop-50` and `0v0katai/fxcraft`).