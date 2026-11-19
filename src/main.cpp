#include "platform.h"
#include "config.h"

#include "emulator.h"
#include "rom.h"
#include "display.h"
#include "snd/snd.h"
#include "ptune2_simple/Ptune2_direct.h"
#include "debug.h"
#include "exam_mode.h"

#include "calctype/fonts/arial_small/arial_small.c"		// For Menus
#include "calctype/fonts/consolas_intl/consolas_intl.c"	// for FAQS

static bool isShuttingDown = false;

void shutdown() {
	if (isShuttingDown) {
		return;
	}
	isShuttingDown = true;

#if !TARGET_WINSIM
	SetQuitHandler(NULL);
#endif

	TeardownDisplayDriver();

	if (bSoundEnabled) {
		sndCleanup();
	}

	if (getDeviceType() == DT_CG20 && Ptune2_GetSetting() != PT2_DEFAULT) {
		Ptune2_LoadSetting(PT2_DEFAULT);
	}

	emulator.shutDown();
	unloadROM();
	sndShutdown();
}

#if TARGET_WINSIM
int simmain(void) {
#else
int main(void) {
#endif
	// prepare for full color mode
	Bdisp_EnableColor(1);
	EnableStatusArea(3);

	if (isStorageLockedOrExamMode()) {
		showExamModeBlockedScreen(true);
#if !PRIZOOP_IS_CW && !TARGET_WINSIM
		for (;;) {
			int k = 0;
			GetKey(&k);
		}
#endif
		return 0;
	}

	// allocate cached mbc banks on the stack
	ALLOCATE_CACHED_BANKS();

	reset_printf();
	memset(GetVRAMAddress(), 0, LCD_HEIGHT_PX * LCD_WIDTH_PX * 2);
	printf("Prizoop Initializing...");
	DrawFrame(0x0000);
	Bdisp_PutDisp_DD();

	emulator.startUp();
#if !TARGET_WINSIM
	SetQuitHandler(shutdown);
#endif
	emulator.run();
	shutdown();

#if !PRIZOOP_IS_CW && !TARGET_WINSIM
	for (;;) {
		int k = 0;
		GetKey(&k);
	}
#endif

	return 0;
}

#if !TARGET_WINSIM
void* operator new(unsigned int size){
	return malloc(size);
}
void operator delete(void* addr) {
	free(addr);
}
void* operator new[](unsigned int size) {
	return malloc(size);
}
void operator delete[](void* addr) {
	free(addr);
}
void operator delete(void* addr, unsigned int size) {
	free(addr);
}
void operator delete[](void* addr, unsigned int size) {
	free(addr);
}
#endif