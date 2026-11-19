#include "emulator.h"
#include "memory.h"
#include "cgb.h"
#include "gpu.h"
#include "rom.h"
#include "exam_mode.h"
#include "snd/snd.h"

#include "screen_rom.h"
#include "screen_settings.h"
#include "screen_play.h"
#include "screen_faq.h"
#include "config.h"

emulator_type emulator;

static void setDefaultKeyMap(unsigned char* keyMap) {
	memset(keyMap, 0, 12);
	keyMap[emu_button::A] = PRIZOOP_SEL(78, 77);			// SHIFT (78 CG50, 77 CG100)
	keyMap[emu_button::B] = PRIZOOP_SEL(68, 67);			// OPTN (68 CG50), ALPHA (67 CG100)
	keyMap[emu_button::SELECT] = 39;						// F5 (CG50), >| (CG100)
	keyMap[emu_button::START] = 29;							// F6 (CG50), PGUP (CG100)
	keyMap[emu_button::RIGHT] = PRIZOOP_SEL(27, 38);
	keyMap[emu_button::LEFT] = PRIZOOP_SEL(38, 58);
	keyMap[emu_button::UP] = PRIZOOP_SEL(28, 49);
	keyMap[emu_button::DOWN] = PRIZOOP_SEL(37, 47);
	keyMap[emu_button::STATE_SAVE] = 43;					// 'x' multiply
	keyMap[emu_button::STATE_LOAD] = 25;					// '->' (CG50), ')' (CG100)

#if TARGET_WINSIM
	keyMap[emu_button::STATE_SAVE] = 59;	// maps to F3
	keyMap[emu_button::STATE_LOAD] = 49;	// maps to F4
#endif
}

// Main emulator functionality
void emulator_type::startUp() {
	curScreen = 0;
	pausePreviewValid = false;
	mbc.romFile = -1;

	if (!loadSettings()) {
		defaultSettings();
	}

	// set up each menu screen
	memset(screens, 0, sizeof(screens));
	screens[0] = new screen_rom;
	screens[1] = new screen_settings;
	screens[2] = new screen_play;
	screens[3] = new screen_faq;

	if (screens[0]) screens[0]->fKey = 1;
	if (screens[1]) screens[1]->fKey = 2;
	if (screens[2]) screens[2]->fKey = 6;
	if (screens[3]) screens[3]->fKey = 5;

	for (int i = 0; i < 6; i++) {
		if (screens[i]) {
			screens[i]->setup();
		}
	}

	curScreen = 0;
}

void emulator_type::shutDown() {
	if (curScreen >= 0 && curScreen < 6 && screens[curScreen]) {
		screens[curScreen]->deselect();
	}
	for (int i = 0; i < 6; i++) {
		delete screens[i];
		screens[i] = NULL;
	}
}

bool emulator_type::isCGBRomLoaded() {
	screen_play* play = (screen_play*) screens[2];
	if (mbc.romFile >= 0 && play && play->loadedROM[0] && !strcmp(play->loadedROM, emulator.settings.selectedRom)) {
		return cgb.isCGB;
	}

	return false;
}

void emulator_type::run() {
	if (curScreen < 0 || curScreen >= 6 || !screens[curScreen]) {
		return;
	}

	screens[curScreen]->select();

	int key = 0;
	do {
		GetKey(&key);

		if (!screens[curScreen]) {
			break;
		}

		switch (key) {
			case KEY_CTRL_LEFT:
				screens[curScreen]->handleLeft();
				break;
			case KEY_CTRL_RIGHT:
				screens[curScreen]->handleRight();
				break;
			case KEY_CTRL_UP:
				screens[curScreen]->handleUp();
				break;
			case KEY_CTRL_DOWN:
				screens[curScreen]->handleDown();
				break;
			case KEY_CTRL_SHIFT:
			case KEY_CTRL_EXE:
				screens[curScreen]->handleSelect();
				break;
			case PRIZOOP_SEL(KEY_CTRL_F1, 30075):	// PREVTAB (|<) on fx-CG100
				tryScreenChange(1);
				break;
			case PRIZOOP_SEL(KEY_CTRL_F2, 30037):	// SETTINGS on fx-CG100
				tryScreenChange(2);
				break;
			case PRIZOOP_SEL(KEY_CTRL_F3, 30066):	// PAGEDOWN on fx-CG100
				tryScreenChange(3);
				break;
			case PRIZOOP_SEL(KEY_CTRL_F4, 30001):	// X,T,theta on fx-CG100
				tryScreenChange(4);
				break;
			case PRIZOOP_SEL(KEY_CTRL_F5, 30074):	// NEXTTAB (>|) on fx-CG100
				tryScreenChange(5);
				break;
			case PRIZOOP_SEL(KEY_CTRL_F6, 30065):	// PAGEUP on fx-CG100
				tryScreenChange(6);
				break;
		}
#if PRIZOOP_IS_CW || TARGET_WINSIM
	} while (key != KEY_CTRL_EXIT);
#else
	} while (true);
#endif
}

void emulator_type::tryScreenChange(int targetFKey) {
	if (curScreen < 0 || curScreen >= 6 || !screens[curScreen])
		return;
	if (targetFKey == screens[curScreen]->fKey)
		return;

	for (int i = 0; i < 6; i++) {
		if (screens[i] && screens[i]->fKey == targetFKey) {
			screens[curScreen]->deselect();
			curScreen = i;
			screens[curScreen]->select();
			break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Custom color palettes

static const char* settingsDir = "Prizoop";
static const char* settingsFile = "Settings";

void emulator_type::defaultSettings() {
	// default settings
	memset(&settings, 0, sizeof(settings));
	settings.version = SETTINGS_VERSION;
	settings.selectedRom[0] = 0;
	settings.overclock = false;
	settings.scaleMode = emu_scale::LO_150;
	settings.frameSkip = -1;

	settings.clampSpeed = true;
	settings.useCGBColors = true;
	settings.sound = false;

	setDefaultKeyMap(settings.keyMap);

	settings.faqOffset = 0;

	// simulator only defaults
#if TARGET_WINSIM
	settings.frameSkip = 0;
#endif
}

bool emulator_type::loadSettings() {
	int len;
	if (!MCSGetDlen2((unsigned char*) settingsDir, (unsigned char*) settingsFile, &len) && len == sizeof(emulator.settings)) {
		emulator_settings readSettings;
		if (!MCSGetData1(0, sizeof(readSettings), &readSettings)) {
			if (readSettings.version == SETTINGS_VERSION) {
				memcpy(&emulator.settings, &readSettings, sizeof(emulator_settings));
				emulator.settings.selectedRom[sizeof(emulator.settings.selectedRom) - 1] = '\0';
				emulator.settings.overclock = emulator.settings.overclock ? 1 : 0;
				emulator.settings.useCGBColors = emulator.settings.useCGBColors ? 1 : 0;
				emulator.settings.clampSpeed = emulator.settings.clampSpeed ? 1 : 0;
#if PRIZOOP_IS_CW
				emulator.settings.sound = 0;
#else
				emulator.settings.sound = emulator.settings.sound ? 1 : 0;
#endif
				if (emulator.settings.scaleMode >= emu_scale::MAX) {
					emulator.settings.scaleMode = emu_scale::LO_150;
				}
				if (emulator.settings.frameSkip < -1 || emulator.settings.frameSkip > 4) {
					emulator.settings.frameSkip = -1;
				}
				if (emulator.settings.bgColorPalette >= numPalettes()) {
					emulator.settings.bgColorPalette = 0;
				}
				if (emulator.settings.obj1ColorPalette >= numPalettes()) {
					emulator.settings.obj1ColorPalette = 0;
				}
				if (emulator.settings.obj2ColorPalette >= numPalettes()) {
					emulator.settings.obj2ColorPalette = 0;
				}
				bool keyMapValid = true;
				for (int i = 0; i < emu_button::MAX; i++) {
					unsigned char kc = emulator.settings.keyMap[i];
					if (kc < 10 || kc >= 90 || (kc % 10) == 0) {
						keyMapValid = false;
						break;
					}
				}
				if (!keyMapValid) {
					setDefaultKeyMap(emulator.settings.keyMap);
				}
				return true;
			}
		}
	}
	return false;
}

void emulator_type::saveSettings() {
	DebugAssert(sizeof(emulator.settings) % 4 == 0);
	MCS_CreateDirectory((unsigned char*) settingsDir);
	emulator.settings.version = SETTINGS_VERSION;
	MCS_WriteItem((unsigned char*) settingsDir, (unsigned char*) settingsFile, 0, sizeof(emulator.settings), (int)(&emulator.settings));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Custom color palettes

static colorpalette_type palettes[] = {
		{ { COLOR_LIGHTCYAN,	COLOR_CYAN,				COLOR_DARKCYAN,		COLOR_BLACK		} },
		{ { COLOR_WHITE,		COLOR_CYAN,				COLOR_MEDIUMBLUE,	COLOR_BLACK		} },
		{ { COLOR_WHITE,		COLOR_LIGHTGRAY,		COLOR_DARKGRAY,		COLOR_BLACK		} },
		{ { COLOR_WHITE,		COLOR_LIGHTGRAY,		COLOR_SLATEGRAY,	COLOR_BLACK		} },
		{ { COLOR_LIGHTGREEN, COLOR_MEDIUMSEAGREEN,	COLOR_DARKGREEN,	COLOR_BLACK			} },
		{ { COLOR_WHITE,		COLOR_MEDIUMSEAGREEN,	COLOR_DARKGREEN,	COLOR_BLACK		} },
		{ { COLOR_WHITE,		COLOR_YELLOW,			COLOR_SANDYBROWN,	COLOR_BROWN		} },
		{ { COLOR_WHITE,		COLOR_ORANGE,			COLOR_RED,			COLOR_BLACK		} },
		{ { COLOR_LIGHTCYAN,	COLOR_CYAN,				COLOR_DARKCYAN,		COLOR_BLACK		} },
		{ { COLOR_WHITE,		COLOR_YELLOW,			COLOR_ORANGE,		COLOR_DARKRED	} },
};

unsigned char emulator_type::numPalettes() {
	return sizeof(palettes) / sizeof(colorpalette_type);
}

void emulator_type::getPalette(unsigned char paletteNum, colorpalette_type& intoColors) {
	if (paletteNum >= numPalettes()) {
		paletteNum = 0;
	}
	memcpy(&intoColors, &palettes[paletteNum], sizeof(colorpalette_type));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Save states

void fillSaveStatePath(unsigned short* pFile) {
	// replace ROM extension with .prz
	char filepath[256];
	strcpy(filepath, "\\\\fls0\\");
	strncat(filepath, emulator.settings.selectedRom, sizeof(filepath) - strlen(filepath) - 5);
	char* dot = strrchr(filepath, '.');
	if (dot && (size_t)(dot - filepath) + 5 <= sizeof(filepath)) {
		strcpy(dot, ".prz");
	} else {
		strcat(filepath, ".prz");
	}

	Bfile_StrToName_ncpy(pFile, (const char*)filepath, strlen(filepath) + 2);
}

// removing this for now... tend to want the sram to stick around..
#define SAVE_STATE_SRAM 0

int getSaveStateSize(unsigned int& withRAMSize) {
	int spaceNeeded = 4 + sizeof(cpu_type) + sizeof(mbc_state);							// main types
	spaceNeeded += sizeof(wram_perm) + sizeof(wram_gb) + sizeof(oam);					// various permanent work RAMS

	// video and additional work ram based on cgb type
	if (cgb.isCGB) {
		spaceNeeded += sizeof(cgb_type) + 0x4000 + sizeof(cgbworkram_type) * 6;
	} else {
		spaceNeeded += 0x2000;
	}


	withRAMSize = getRAMSize();

#if SAVE_STATE_SRAM
	// we only save state RAMS <= 8 KB to save space
	if (withRAMSize <= 8 * 1024) {
		spaceNeeded += withRAMSize;										// RAM
	}
#endif

	// 4 byte align
	if (spaceNeeded % 4 != 0) {
		spaceNeeded += 4 - (spaceNeeded % 4);
	}

	return spaceNeeded;
}

static void CompatSwapsState(cpu_type& c, mbc_state& m, cgb_type& g, bool isCGB) {
	EndianSwap(c.registers.af);
	EndianSwap(c.registers.bc);
	EndianSwap(c.registers.de);
	EndianSwap(c.registers.hl);
	EndianSwap(c.registers.pc);
	EndianSwap(c.registers.sp);
	EndianSwap(c.IME);
	EndianSwap(c.clocks);
	EndianSwap(c.halted);
	EndianSwap(c.stopped);
	EndianSwap(c.div);
	EndianSwap(c.divBase);
	EndianSwap(c.timer);
	EndianSwap(c.timerBase);
	EndianSwap(c.timerInterrupt);
	EndianSwap(c.gpuTick);
	EndianSwap((unsigned int&)m.type);
	EndianSwap((unsigned int&)m.ramType);
	EndianSwap(m.numRomBanks);

	if (isCGB) {
		EndianSwap((unsigned int&)g.selectedWRAM);
		EndianSwap((unsigned int&)g.selectedVRAM);
		EndianSwap(g.dmaSrc);
		EndianSwap(g.dmaDest);
		EndianSwap(g.dmaLeft);
		EndianSwap(g.curPalTarget);
	}
}

void emulator_type::saveState() {
	if (mbc.romFile < 0 || !vram) {
		return;
	}

#if !TARGET_WINSIM
	// flush DMA before making OS calls
	DmaWaitNext();
#endif

	if (isStorageLockedSafe()) {
		mbcFileUpdate();
		return;
	}

	// calculate space needed for state
	unsigned int RAMSize;
	int spaceNeeded = getSaveStateSize(RAMSize);
	size_t createSize = (size_t)spaceNeeded;

	unsigned short pFile[256];
	fillSaveStatePath(pFile);

	Bfile_DeleteEntry(pFile);
	if (Bfile_CreateEntry_OS(pFile, CREATEMODE_FILE, &createSize) < 0) {
		mbcFileUpdate();
		return;
	}

	int hFile = Bfile_OpenFile_OS(pFile, WRITE, 0); // Get handle
	if (hFile < 0) {
		Bfile_DeleteEntry(pFile);
		mbcFileUpdate();
		return;
	}

	CompatSwapsState(cpu, mbc, cgb, cgb.isCGB != 0);

	bool writeOk = true;

	// write rom bytes 0x14E-F (checksum) for sanity
	if (Bfile_WriteFile_OS(hFile, &cart[0x14E], 4) < 0) writeOk = false;

	// write cpu and mbc state
	if (writeOk && Bfile_WriteFile_OS(hFile, &cpu, sizeof(cpu_type)) < 0) writeOk = false;
	if (writeOk && Bfile_WriteFile_OS(hFile, &mbc, sizeof(mbc_state)) < 0) writeOk = false;

	if (writeOk && cgb.isCGB) {
		if (Bfile_WriteFile_OS(hFile, &cgb, sizeof(cgb_type)) < 0) writeOk = false;
	}

	// write various work rams
	if (writeOk && Bfile_WriteFile_OS(hFile, &wram_perm[0], sizeof(wram_perm)) < 0) writeOk = false;
	if (writeOk && Bfile_WriteFile_OS(hFile, &wram_gb[0], sizeof(wram_gb)) < 0) writeOk = false;

	if (writeOk && cgb.isCGB) {
		for (int i = 0; i < 6; i++) {
			if (!cgb_wram[i] || Bfile_WriteFile_OS(hFile, cgb_wram[i]->data, 0x1000) < 0) {
				writeOk = false;
				break;
			}
		}
	}

	// video RAM
	if (writeOk) {
		if (cgb.isCGB) {
			if (Bfile_WriteFile_OS(hFile, &vram[0], 0x4000) < 0) writeOk = false;
		} else {
			if (Bfile_WriteFile_OS(hFile, &vram[0], 0x2000) < 0) writeOk = false;
		}
	}

	if (writeOk && Bfile_WriteFile_OS(hFile, &oam[0], sizeof(oam)) < 0) writeOk = false;

#if SAVE_STATE_SRAM
	// only write sram for small ram sizes
	if (writeOk && RAMSize <= 8 * 1024) {
		if (Bfile_WriteFile_OS(hFile, &sram[0], RAMSize) < 0) writeOk = false;
	}
#endif

#if TARGET_WINSIM
	if (writeOk) {
		int amountWritten = Bfile_TellFile_OS(hFile);
		DebugAssert(amountWritten == spaceNeeded);
	}
#endif

	// done!
	Bfile_CloseFile_OS(hFile);

	CompatSwapsState(cpu, mbc, cgb, cgb.isCGB != 0);

	if (!writeOk) {
		Bfile_DeleteEntry(pFile);
	}

	mbcFileUpdate();

	if (curScreen >= 0 && curScreen < 6 && screens[curScreen]) {
		screens[curScreen]->postStateChange();
	}
}

bool emulator_type::loadState() {
	if (mbc.romFile < 0 || !vram) {
		return false;
	}

#if !TARGET_WINSIM
	// flush DMA before making OS calls
	DmaWaitNext();
	REG_TMU_TSTR &= ~(1 << 1);
#endif

	if (isStorageLockedSafe()) {
		mbcFileUpdate();
		return false;
	}

	// calculate space needed for state
	unsigned int RAMSize;
	int spaceNeeded = getSaveStateSize(RAMSize);

	unsigned short pFile[256];
	fillSaveStatePath(pFile);

	int hFile = Bfile_OpenFile_OS(pFile, READ, 0); // Get handle
	if (hFile < 0) {
		// not found
		mbcFileUpdate();
		return false;
	}

	if (Bfile_GetFileSize_OS(hFile) != spaceNeeded) {
		// wrong size (format must have changed, or a bad write)
		Bfile_CloseFile_OS(hFile);
		mbcFileUpdate();
		return false;
	}

	// read checksum and check it
	unsigned char checkSum[4];
	if (Bfile_ReadFile_OS(hFile, &checkSum[0], 4, -1) != 4 ||
		checkSum[0] != cart[0x14E] || checkSum[1] != cart[0x14F]) {
		// different ROM somehow
		Bfile_CloseFile_OS(hFile);
		mbcFileUpdate();
		return false;
	}

	bool wasCGB = cgb.isCGB != 0;
	cpu_type tmpCpu;
	mbc_state tmpMbc;
	cgb_type tmpCgb;
	memset(&tmpCgb, 0, sizeof(tmpCgb));

	if (Bfile_ReadFile_OS(hFile, &tmpCpu, sizeof(cpu_type), -1) != (int)sizeof(cpu_type) ||
		Bfile_ReadFile_OS(hFile, &tmpMbc, sizeof(mbc_state), -1) != (int)sizeof(mbc_state) ||
		(wasCGB && Bfile_ReadFile_OS(hFile, &tmpCgb, sizeof(cgb_type), -1) != (int)sizeof(cgb_type))) {
		Bfile_CloseFile_OS(hFile);
		mbcFileUpdate();
		return false;
	}

	CompatSwapsState(tmpCpu, tmpMbc, tmpCgb, wasCGB);

	// switch back to normal speed before continuing (cgb state loading expects it)
	if (cgb.isCGB && cgb.isDouble) {
		cgbSpeedSwitch();
	}

	// preserve ROM-intrinsic mbc fields
	tmpMbc.romFile = mbc.romFile;
	tmpMbc.compressed = mbc.compressed;
	tmpMbc.type = mbc.type;
	tmpMbc.ramType = mbc.ramType;
	tmpMbc.numRomBanks = mbc.numRomBanks;
	tmpMbc.numRamBanks = mbc.numRamBanks;
	tmpMbc.batteryBacked = mbc.batteryBacked;
	tmpMbc.rumblePack = mbc.rumblePack;

	if (tmpMbc.numRomBanks > 0 && tmpMbc.romBank >= tmpMbc.numRomBanks) {
		tmpMbc.romBank %= tmpMbc.numRomBanks;
	}
	if (tmpMbc.numRamBanks > 0 && tmpMbc.ramBank >= tmpMbc.numRamBanks) {
		tmpMbc.ramBank %= tmpMbc.numRamBanks;
	}
	tmpMbc.bankMode &= 1;
	tmpMbc.sramEnabled = tmpMbc.sramEnabled ? 1 : 0;

	if (wasCGB) {
		tmpCgb.isCGB = 1;
		tmpCgb.isDouble = tmpCgb.isDouble ? 1 : 0;
		tmpCgb.hblankDmaActive = tmpCgb.hblankDmaActive ? 1 : 0;
		tmpCgb.dirtyPalette = 1;
		if (tmpCgb.selectedWRAM < 1 || tmpCgb.selectedWRAM > 7) {
			tmpCgb.selectedWRAM = 1;
		}
		tmpCgb.selectedVRAM &= 1;
		tmpCgb.dmaSrc &= 0xFFF0;
		tmpCgb.dmaDest = 0x8000 | (tmpCgb.dmaDest & 0x1FF0);
		if (tmpCgb.dmaLeft > 0x800) {
			tmpCgb.dmaLeft = 0;
		}
		tmpCgb.curPalTarget &= 0x3F;
	}

	// commit validated state
	cpu = tmpCpu;
	mbc = tmpMbc;
	if (wasCGB) {
		cgb = tmpCgb;
	}

	// read various work rams
	bool readOk = true;
	if (Bfile_ReadFile_OS(hFile, &wram_perm[0], sizeof(wram_perm), -1) != (int)sizeof(wram_perm)) readOk = false;
	if (readOk && Bfile_ReadFile_OS(hFile, &wram_gb[0], sizeof(wram_gb), -1) != (int)sizeof(wram_gb)) readOk = false;

	if (readOk && cgb.isCGB) {
		for (int i = 0; i < 6; i++) {
			if (!cgb_wram[i] || Bfile_ReadFile_OS(hFile, cgb_wram[i]->data, 0x1000, -1) != 0x1000) {
				readOk = false;
				break;
			}
		}
	}

	// video RAM
	if (readOk) {
		if (cgb.isCGB) {
			if (Bfile_ReadFile_OS(hFile, &vram[0], 0x4000, -1) != 0x4000) readOk = false;
		} else {
			if (Bfile_ReadFile_OS(hFile, &vram[0], 0x2000, -1) != 0x2000) readOk = false;
		}
	}

	if (readOk && Bfile_ReadFile_OS(hFile, &oam[0], sizeof(oam), -1) != (int)sizeof(oam)) readOk = false;

#if SAVE_STATE_SRAM
	// only read sram for small ram sizes
	if (readOk && RAMSize <= 8 * 1024) {
		if (Bfile_ReadFile_OS(hFile, &sram[0], RAMSize, -1) != (int)RAMSize) readOk = false;
	}
#endif

	Bfile_CloseFile_OS(hFile);

	cpuResetRuntimeState();

	// memory bus controller has to invalidate some stuff, etc
	mbcOnStateLoad();

	mbcFileUpdate();

	// color gameboy needs to fix some stuff too
	if (cgb.isCGB) {
		cgbOnStateLoad();
	} else {
		resolveDMGBGPalette();
		resolveDMGOBJ0Palette();
		resolveDMGOBJ1Palette();
	}

	gpuSyncAfterStateLoad();
	sndResetSync();

	if (curScreen >= 0 && curScreen < 6 && screens[curScreen]) {
		screens[curScreen]->postStateChange();
	}

	return readOk;
}
