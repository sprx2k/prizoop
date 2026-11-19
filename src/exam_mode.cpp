#include "exam_mode.h"
#include "platform.h"
#include "config.h"

namespace {
struct exam_find_info_t {
	unsigned short id;
	unsigned short type;
	unsigned long fsize;
	unsigned long dsize;
	unsigned int property;
	unsigned long address;
};

static bool canAccessStoragePattern(const char* pattern) {
#if TARGET_WINSIM
	(void)pattern;
	return true;
#else
	unsigned short filter[0x100];
	unsigned short found[0x100];
	exam_find_info_t info;
	int handle = -1;

	Bfile_StrToName_ncpy(filter, pattern, 0x50);
	int ret = Bfile_FindFirst((const char*)filter, &handle, (char*)found, &info);
	if (handle >= 0) {
		Bfile_FindClose(handle);
	}
	return ret == 0;
#endif
}
} // namespace

bool isStorageLockedOrExamMode() {
#if TARGET_WINSIM
	return false;
#else
	// Any normal installation running Prizoop has at least prizoop_*.g3a (and mpm.bin
	// on fx-CG100) in root storage memory (\\fls0\). If neither wildcard returns any
	// entry, storage memory access is restricted (e.g., Examination Mode).
	// Note: callers holding an open Bfile handle should close it before probing.
	if (canAccessStoragePattern("\\\\fls0\\*.*")) {
		return false;
	}
	if (canAccessStoragePattern("\\\\fls0\\*")) {
		return false;
	}
	return true;
#endif
}

void showExamModeBlockedScreen(bool waitForKey) {
#if !TARGET_WINSIM
	memset(GetVRAMAddress(), 0, LCD_HEIGHT_PX * LCD_WIDTH_PX * 2);

	int x = 16;
	int y = 40;
	PrintMini(&x, &y, "ACCESS DISABLED (EXAM MODE)", 0x02, 0xffffffff, 0, 0, COLOR_RED, COLOR_BLACK, 1, 0);

	x = 16;
	y = 70;
	PrintMini(&x, &y, "Storage memory access is locked.", 0x02, 0xffffffff, 0, 0, COLOR_WHITE, COLOR_BLACK, 1, 0);

	x = 16;
	y = 92;
	PrintMini(&x, &y, "Prizoop cannot run during Exam Mode.", 0x02, 0xffffffff, 0, 0, COLOR_WHITE, COLOR_BLACK, 1, 0);

	x = 16;
	y = 130;
	PrintMini(&x, &y, "Exit Exam Mode to restore access.", 0x02, 0xffffffff, 0, 0, COLOR_SILVER, COLOR_BLACK, 1, 0);

	if (waitForKey) {
		x = 16;
		y = 164;
		PrintMini(&x, &y, PRIZOOP_SEL("Press MENU to exit...", "Press any key to exit..."), 0x02, 0xffffffff, 0, 0, COLOR_YELLOW, COLOR_BLACK, 1, 0);
	}

	DrawFrame(0x0000);
	Bdisp_PutDisp_DD();

	if (waitForKey) {
#if PRIZOOP_IS_CW
		int key = 0;
		GetKey(&key);
#else
		for (;;) {
			int key = 0;
			GetKey(&key);
		}
#endif
	}
#else
	(void)waitForKey;
#endif
}
