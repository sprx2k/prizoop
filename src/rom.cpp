
#include "platform.h"

#include "memory.h"
#include "debug.h"
#include "mbc.h"
#include "cgb.h"

#include "rom.h"

static char curRomFile[64];
static char curSaveFile[64];

unsigned char loadROM(const char *filename) {
	char name[17];
	enum mbcType type;
	int i;
	
	int length;
	
	unsigned char header[0x180];

	const char* dot = strrchr(filename, '.');
	size_t baseLen = dot ? (size_t)(dot - filename) : strlen(filename);
	if (strlen(filename) + 8 >= sizeof(curRomFile) || baseLen + 12 >= sizeof(curSaveFile)) {
		printf("Filename too long!\n");
		return 0;
	}

	strcpy(curRomFile, "\\\\fls0\\");
	strcat(curRomFile, filename);

	bool isCompressed = dot &&
		(dot[1] == 'g' || dot[1] == 'G') &&
		(dot[2] == 'b' || dot[2] == 'B') &&
		(dot[3] == 'z' || dot[3] == 'Z') &&
		dot[4] == '\0';

	strcpy(curSaveFile, "\\\\fls0\\");
	strncat(curSaveFile, filename, baseLen);
	strcat(curSaveFile, ".SAV");

	int hFile;
	unsigned short pFile[256];
	Bfile_StrToName_ncpy(pFile, (const char*)curRomFile, strlen(curRomFile)+2);
	
	hFile = Bfile_OpenFile_OS(pFile, READ, 0); // Get handle
	if (hFile < 0) {
		printf("Not found! %d\n", hFile);
		return 0;
	}

	length = Bfile_GetFileSize_OS(hFile);
	if(length < 0x180) {
		printf("ROM is too small! (%d bytes)\n", length);
		Bfile_CloseFile_OS(hFile);
		return 0;
	}

	if (length > 4 * 1024 * 1024) {
		// 4 MB max
		printf("ROM is too big! (%d kbytes)\n", length / 1024);
		Bfile_CloseFile_OS(hFile);
		return 0;
	}

	// read in the header and print out info
	if (Bfile_ReadFile_OS(hFile, header, 0x180, 0) != 0x180) {
		printf("Failed reading ROM header!\n");
		Bfile_CloseFile_OS(hFile);
		return 0;
	}
	
	memset(name, '\0', 17);
	for(i = 0; i < 16; i++) {
		if(header[i + ROM_OFFSET_NAME] == 0x80 || header[i + ROM_OFFSET_NAME] == 0xc0) name[i] = '\0';
		else name[i] = header[i + ROM_OFFSET_NAME];
	}

	bool isCGB = header[ROM_OFFSET_GBC] == 0x80 || header[ROM_OFFSET_GBC] == 0xc0;
	if (isCGB) {
		printf("CGB ROM name: %s %s\n", name, isCompressed ? "(gbz)" : "");
	} else {
		printf("GB ROM name: %s %s\n", name, isCompressed ? "(gbz)" : "");
	}

	resetMemoryMaps(isCGB);
	cpuReset();

	if (isCGB) {
		// init CGB mode
		cgbInitROM();
	} else {
		cgb.isCGB = false;
	}

	// determine mbc controller support and initialize
	type = (mbcType) header[ROM_OFFSET_TYPE];
	unsigned char romSizeByte = header[ROM_OFFSET_ROM_SIZE];
	unsigned char ramSizeByte = header[ROM_OFFSET_RAM_SIZE];

	if (!setupMBCType(type, romSizeByte, ramSizeByte, hFile)) {
		printf("Unsupported MBC: %s, (%02x ROM, %02x RAM)\n", getMBCTypeString(type), romSizeByte, ramSizeByte);
		mbc.batteryBacked = 0;
		unloadROM();
		return 0;
	}

	printf("MBC type: %s\n", getMBCTypeString(type));
	printf("RAM type: %s\n", getRAMTypeString(mbc.ramType));
	printf("Num ROM Banks: %d\n", mbc.numRomBanks);

	if (compressedPages) {
		free((void*) compressedPages);
		compressedPages = NULL;
	}
	numCompressedPages = 0;

	// set up compression if in use
	if (isCompressed) {
		mbc.compressed = 1;

		unsigned short numPages = 0;
		if (Bfile_ReadFile_OS(hFile, &numPages, 2, -1) != 2) {
			printf("Failed reading gbz page count!\n");
			mbc.batteryBacked = 0;
			unloadROM();
			return 0;
		}
		EndianSwap(numPages);

		if (numPages < 4 || numPages > 1024) {
			printf("Invalid gbz page count: %d\n", numPages);
			mbc.batteryBacked = 0;
			unloadROM();
			return 0;
		}

		//  N 2 byte words of compressed page sizes (4098 if left uncompressed)
		//  N zx7 compressed pages
		compressedPages = (int*) malloc(sizeof(int) * (numPages + 1));
		if (!compressedPages) {
			printf("Out of memory for gbz table!\n");
			mbc.batteryBacked = 0;
			unloadROM();
			return 0;
		}
		numCompressedPages = numPages;
		int curPos = Bfile_TellFile_OS(hFile) + 2 * numPages;
		compressedPages[0] = curPos;
		for (int i = 1; i <= numPages; i++) {
			unsigned short pageSize = 0;
			if (Bfile_ReadFile_OS(hFile, &pageSize, 2, -1) != 2) {
				printf("Failed reading gbz page table!\n");
				mbc.batteryBacked = 0;
				unloadROM();
				return 0;
			}
			EndianSwap(pageSize);

			if (pageSize == 0 || pageSize > 4098) {
				printf("Invalid gbz page size!\n");
				mbc.batteryBacked = 0;
				unloadROM();
				return 0;
			}
			
			curPos += pageSize;
			compressedPages[i] = curPos;
		}
		DebugAssert(Bfile_TellFile_OS(hFile) == compressedPages[0]);
	} else {
		mbc.compressed = 0;
	}

	mbcFileUpdate();
		
	// read permanent ROM Area in
	if (!mbcReadPage(0, &cart[0x0000], false) ||
		!mbcReadPage(1, &cart[0x1000], false) ||
		!mbcReadPage(2, &cart[0x2000], false) ||
		!mbcReadPage(3, &cart[0x3000], false)) {
		printf("Failed reading ROM pages!\n");
		mbc.batteryBacked = 0;
		unloadROM();
		return 0;
	}

	if (mbc.batteryBacked) {
		if (tryLoadSRAM(curSaveFile)) {
			printf("Save file loaded\n");
		}
		else {
			printf("No save file in system\n");
		}
	} else {
		printf("No save file, not battery backed\n");
	}

	return 1;
}

void unloadROM(void) {
	if (mbc.romFile >= 0) {
		Bfile_CloseFile_OS(mbc.romFile);
		mbc.romFile = -1;
	}

	saveRAM();
	mbc.batteryBacked = 0;

	if (compressedPages) {
		free((void*)compressedPages);
		compressedPages = NULL;
	}
	numCompressedPages = 0;

	// free up vram
	if (vram) {
		free((void*)vram);
		vram = NULL;
	}

	if (cgb.isCGB) {
		// clean up CGB mode stuff
		cgbCleanup();
	}
}

void saveRAM(void) {
	if (mbc.batteryBacked) {
		trySaveSRAM(curSaveFile);
	}
}