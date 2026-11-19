#include "platform.h"

#include "mbc.h"
#include "rom.h"
#include "keys.h"
#include "memory.h"
#include "emulator.h"
#include "snd/snd.h"

#include "zx7/zx7.h"

// cached banks
mbc_bankcache* cachedBanks[NUM_CACHED_BANKS];
unsigned int cachedBankIndex[NUM_CACHED_BANKS];
unsigned int lastCacheRequestIndex[NUM_CACHED_BANKS];
unsigned int cacheIndex = 0;

// for higher RAM requirements, we use cached banks to store our ram, the rest our stored starting with firstRomCache, as indicated by this var
unsigned int firstRomCache = 0;

// sram hash (for checking dirty state, need to save)
unsigned int sramHash = 0;

// the memory bus controller object
mbc_state mbc;

// MBC1 internal 5-bit/2-bit register state and mode-1 bank 0 selection
static unsigned char mbc1Low = 0;
static unsigned char mbc1High = 0;
static unsigned char mbc1Bank0 = 0;

// the real time clock object
rtc_state rtc;
unsigned char rtcMap[256] ALIGN(256);

// compressed page locations for each rom file page
int* compressedPages = NULL;
int numCompressedPages = 0;

const char* getMBCTypeString(mbcType type) {
	switch (type) {
		case ROM_PLAIN: return "ROM_PLAIN";
		case ROM_MBC1: return "ROM_MBC1";
		case ROM_MBC1_RAM: return "ROM_MBC1_RAM";
		case ROM_MBC1_RAM_BATT: return "ROM_MBC1_RAM_BATT";
		case ROM_MBC2: return "ROM_MBC2";
		case ROM_MBC2_BATTERY: return "ROM_MBC2_BATTERY";
		case ROM_RAM: return "ROM_RAM";
		case ROM_RAM_BATTERY: return "ROM_RAM_BATTERY";
		case ROM_MMM01: return "ROM_MMM01";
		case ROM_MMM01_SRAM: return "ROM_MMM01_SRAM";
		case ROM_MMM01_SRAM_BATT: return "ROM_MMM01_SRAM_BATT";
		case ROM_MBC3_TIMER_BATT: return "ROM_MBC3_TIMER_BATT";
		case ROM_MBC3_TIMER_RAM_BATT: return "ROM_MBC3_TIMER_RAM_BATT";
		case ROM_MBC3: return "ROM_MBC3";
		case ROM_MBC3_RAM: return "ROM_MBC3_RAM";
		case ROM_MBC3_RAM_BATT: return "ROM_MBC3_RAM_BATT";
		case ROM_MBC5: return "ROM_MBC5";
		case ROM_MBC5_RAM: return "ROM_MBC5_RAM";
		case ROM_MBC5_RAM_BATT: return "ROM_MBC5_RAM_BATT";
		case ROM_MBC5_RUMBLE: return "ROM_MBC5_RUMBLE";
		case ROM_MBC5_RUMBLE_SRAM: return "ROM_MBC5_RUMBLE_SRAM";
		case ROM_MBC5_RUMBLE_SRAM_BATT: return "ROM_MBC5_RUMBLE_SRAM_BATT";
		case ROM_POCKET_CAMERA: return "ROM_POCKET_CAMERA";
		case ROM_BANDAI_TAMA5: return "ROM_BANDAI_TAMA5";
		case ROM_HUDSON_HUC3: return "ROM_HUDSON_HUC3";
		case ROM_HUDSON_HUC1: return "ROM_HUDSON_HUC1";
		default: return "UNKNOWN";
	}
}

// given a ramSizeType returns a string description
const char* getRAMTypeString(ramSizeType type) {
	switch (type) {
		case RAM_NONE: return "RAM_NONE";
		case RAM_2KB: return "RAM_2KB";
		case RAM_8KB: return "RAM_8KB";
		case RAM_32KB: return "RAM_32KB";
		case RAM_128KB: return "RAM_128KB";
		case RAM_64KB: return "RAM_64KB";
		case RAM_MBC2: return "RAM_MBC2";
		default:  return "RAM_UNKNOWN";
	}
}

unsigned int getRAMSize() {
	switch (mbc.ramType) {
		case RAM_NONE: return 0;
		case RAM_2KB: return 1024 * 2;
		case RAM_8KB: return 1024 * 8;
		case RAM_32KB: return 1024 * 32;
		case RAM_128KB: return 1024 * 128;
		case RAM_64KB: return 1024 * 64;
		case RAM_MBC2: return 512;
		default:  return 0;
	}
}

bool supportedRAM(ramSizeType type) {
	switch (type) {
		case RAM_NONE: 
		case RAM_2KB:
		case RAM_8KB:
		case RAM_MBC2:
			// sram will do
			mbc.ramBank = 0;
			mbc.numRamBanks = 1;
			firstRomCache = 0;
			return true;
		case RAM_32KB:
			// need 8 4KB pages:
			mbc.ramBank = 0;
			mbc.numRamBanks = 4;
			firstRomCache = 8;
			return true;
		case RAM_64KB:
			// need 16 4KB pages:
			mbc.ramBank = 0;
			mbc.numRamBanks = 8;
			firstRomCache = 16;
			return true;
		case RAM_128KB:
			// need 32 4KB pages:
			mbc.ramBank = 0;
			mbc.numRamBanks = 16;
			firstRomCache = 32;
			return true;
	}

	return false;
}

// number of RAM 256-byte pages to allocate to the memory mapper when using SRAM
unsigned char ramNibbleCount(ramSizeType type) {
	switch (type) {
		case RAM_NONE: return 0;
		case RAM_2KB: return 8;
		case RAM_32KB:
		case RAM_64KB:
		case RAM_128KB:
		case RAM_8KB: return 32;
		case RAM_MBC2: return 2;
	}

	// unsupported
	return 0;
}

// support up to a 4 MB ROM (+ 8 KB .gbz header/table)
#define MAX_ROM_BLOCKS 1026
static unsigned char* BlockAddresses[MAX_ROM_BLOCKS] = { 0 };
static unsigned int romFileSize = 0;

void mbcFileUpdate() {
	memset(BlockAddresses, 0, sizeof(BlockAddresses));
	romFileSize = 0;
	if (mbc.romFile < 0)
		return;
	int fileSize = Bfile_GetFileSize_OS(mbc.romFile);
	if (fileSize < 0)
		return;
	romFileSize = (unsigned int)fileSize;
	int numBlocks = min((fileSize + 4095) / 4096, MAX_ROM_BLOCKS);
	for (int i = 0; i < numBlocks; i++) {
		int ret = Bfile_GetBlockAddress(mbc.romFile, i * 0x1000, &BlockAddresses[i]);
		if (ret < 0)
			// error!
			return;
	}
}

static int mbcReadFile(unsigned char* into, unsigned int size, unsigned int offset) {
	if (offset >= romFileSize) {
		return 0;
	}
	if (size > romFileSize - offset) {
		size = romFileSize - offset;
	}

	// use block addresses to do direct copy
	unsigned int sizeLeft = size;
	unsigned int block = offset / 4096;
	unsigned int blockAddr = offset % 4096;
	while (sizeLeft) {
		if (block >= MAX_ROM_BLOCKS || !BlockAddresses[block]) {
			return (int)(size - sizeLeft);
		}
		unsigned int blockRem = min(4096U - blockAddr, sizeLeft);
		memcpy(into, BlockAddresses[block] + blockAddr, blockRem);
		into += blockRem;
		sizeLeft -= blockRem;

		block++;
		blockAddr = 0;

		// update sound around all file reads
		condSoundUpdate();
	}
	return (int)size;
}

bool mbcReadPage(unsigned int bankIndex, unsigned char* target, bool instrOverlap) {
	TIME_SCOPE();

	unsigned int overlapBytes = instrOverlap ? 2 : 0;

	if (mbc.compressed) {
		if (!compressedPages || bankIndex >= (unsigned int)numCompressedPages) {
			return false;
		}
		int compSize = compressedPages[bankIndex + 1] - compressedPages[bankIndex];
		if (compSize <= 0 || compSize > 4098) {
			return false;
		}

		if (compSize == 4098) {
			return mbcReadFile(target, 0x1000 + overlapBytes, compressedPages[bankIndex]) == (int)(0x1000 + overlapBytes);
		} else {
			static unsigned char compBuffer[4098];
			if (mbcReadFile(compBuffer, (unsigned int)compSize, compressedPages[bankIndex]) != compSize) {
				return false;
			}
			if (instrOverlap) {
				return ZX7Decompress(compBuffer, (unsigned int)compSize, target, 4098) == 0;
			} else {
				static unsigned char uncompBuffer[4098];
				if (ZX7Decompress(compBuffer, (unsigned int)compSize, uncompBuffer, 4098) != 0) {
					return false;
				}
				memcpy(target, uncompBuffer, 4096);
				return true;
			}
		}
	} else {
		unsigned int read = (unsigned int)mbcReadFile(target, 0x1000 + overlapBytes, bankIndex * 0x1000);
		DebugAssert(read == 0x1000 + overlapBytes);
		if (read == 0x1000 + overlapBytes) {
			return true;
		}
	}

	return false;
}

// returns the cached rom bank (or caches it) with the given index (which is a factor of the number of caches per rom bank)
mbc_bankcache* cacheBank(unsigned int index) {
	cacheIndex++;

	// rare case: on overflow, the cache system needs a reset
	if (cacheIndex == 0) {
		for (int i = firstRomCache; i < NUM_CACHED_BANKS; i++) {
			lastCacheRequestIndex[i] = 0;
		}
		cacheIndex = 1;
	}

	int minSlot = firstRomCache;
	for (int i = firstRomCache; i < NUM_CACHED_BANKS; i++) {
		if (cachedBankIndex[i] == index) {
			lastCacheRequestIndex[i] = cacheIndex;
			return cachedBanks[i];
		} 
		if (lastCacheRequestIndex[i] < lastCacheRequestIndex[minSlot]) {
			minSlot = i;
		}
	}

	// uncached! using minimum cache request index, read into slot from file and return
	if (!mbcReadPage(index, cachedBanks[minSlot]->bank, index != unsigned(mbc.numRomBanks * 4 - 1))) {
		cachedBankIndex[minSlot] = 0xFFFFFFFF;
		lastCacheRequestIndex[minSlot] = 0;
		// attempt to escape
		keys.exit = true;
		return cachedBanks[firstRomCache];
	} else {
		cachedBankIndex[minSlot] = index;
		lastCacheRequestIndex[minSlot] = cacheIndex;

		return cachedBanks[minSlot];
	}
}

// total number of rom banks calculation (16k per bank)
unsigned short numSwitchableBanksFromType(unsigned char romSizeByte) {
	if (romSizeByte <= 7) {
		return 2 << romSizeByte;
	}
	switch (romSizeByte) {
		case 0x52: return 72;
		case 0x53: return 80;
		case 0x54: return 96;
		default: return 0;
	}
}

// selects the given rom bank (masked to cartridge ROM size)
void selectRomBank(unsigned char bankNum) {
	if (mbc.numRomBanks == 0) {
		return;
	}
	unsigned char masked = (unsigned char)((unsigned int)bankNum % mbc.numRomBanks);
	if (masked != mbc.romBank) {
		mbc.romBank = masked;

		// invalidate addresses in memory map for reading
		for (int i = 0x40; i <= 0x7f; i++) {
			specialMap[i] |= 0x10;
		}
	}
}

static void selectMBC1Bank0(unsigned char bankNum) {
	if (mbc.numRomBanks == 0) {
		return;
	}
	unsigned char masked = (unsigned char)((unsigned int)bankNum % mbc.numRomBanks);
	if (masked == mbc1Bank0) {
		return;
	}
	mbc1Bank0 = masked;
	if (mbc1Bank0 == 0) {
		for (int i = 0x00; i <= 0x3f; i++) {
			memoryMap[i] = &cart[i << 8];
			specialMap[i] &= ~0x10;
		}
	} else {
		for (int i = 0x00; i <= 0x3f; i++) {
			specialMap[i] |= 0x10;
		}
	}
}

unsigned char mbcRead(unsigned short address) {
	DebugAssert(address <= 0x7FFF);

	unsigned char highNibble = address >> 12;
	unsigned int bankIndex;
	if (address < 0x4000) {
		bankIndex = (unsigned int)mbc1Bank0 * 4 + highNibble;
	} else {
		bankIndex = (unsigned int)mbc.romBank * 4 + highNibble - 4;
	}

	mbc_bankcache* cache = cacheBank(bankIndex);
	if (keys.exit) {
		return 0xFF;
	}

	for (int i = 0; i < 16; i++) {
		// map the cache
		memoryMap[(highNibble << 4) + i] = &cache->bank[256 * i];
		// we've validated the highest nibble
		specialMap[(highNibble << 4) + i] &= ~0x10;
	}

	return memoryMap[address >> 8][address & 0xFF];
}

// selects the given ram bank
void selectRamBank(unsigned char bankNum, bool force = false) {
	if (mbc.numRamBanks == 0) {
		return;
	}
	bankNum = (unsigned char)(bankNum % mbc.numRamBanks);
	if (bankNum != mbc.ramBank || force) {
		mbc.ramBank = bankNum;
		if (mbc.sramEnabled && mbc.numRamBanks > 1) {
			// map memory to our usurped rom cache area
			for (int i = 0; i < 16; i++) {
				memoryMap[0xa0 + i] = &cachedBanks[bankNum * 2]->bank[i << 8];
			}
			for (int i = 0; i < 16; i++) {
				memoryMap[0xb0 + i] = &cachedBanks[bankNum * 2 + 1]->bank[i << 8];
			}
		}
	}
}

void rtcSelectReg(unsigned char reg);

// enable sram by updating memory map
void enableSRAM() {
	mbc.sramEnabled = 1;
	if (mbcIsRTC() && rtc.rtcReg >= 0x08 && rtc.rtcReg <= 0x0C) {
		rtcSelectReg(rtc.rtcReg);
		return;
	}
	if (mbc.ramType == RAM_MBC2) {
		// MBC2 has 512 bytes echoed across 0xA000-0xBFFF
		for (int i = 0; i < 32; i++) {
			memoryMap[0xa0 + i] = &sram[(i & 1) << 8];
		}
	} else if (mbc.numRamBanks <= 1) {
		int nibbleCount = ramNibbleCount(mbc.ramType);
		for (int i = 0; i < 32; i++) {
			memoryMap[0xa0 + i] = (i < nibbleCount) ? &sram[i << 8] : &disabledArea[0];
		}
	} else {
		selectRamBank(mbc.ramBank, true);
	}
}

// disable sram by updating memory map
void disableSRAM() {
	mbc.sramEnabled = 0;
	for (int i = 0xa0; i <= 0xbf; i++) {
		memoryMap[i] = &disabledArea[0];
	}
}

static void mbc1UpdateBanks() {
	unsigned char low = (mbc1Low == 0) ? 1 : mbc1Low;
	unsigned char romBank = (unsigned char)((mbc1High << 5) | low);
	selectRomBank(romBank);
	if (mbc.bankMode == 1) {
		selectMBC1Bank0((unsigned char)(mbc1High << 5));
		selectRamBank(mbc1High);
	} else {
		selectMBC1Bank0(0);
		selectRamBank(0);
	}
}

// MBC3 RTC support

#if TARGET_PRIZM
static inline unsigned int FromBCD(unsigned int x) {
	return (x & 0xF) + ((x & 0xF0) >> 4) * 10 + ((x & 0xF00) >> 8) * 100 + ((x & 0xF000) >> 12) * 1000;
}
#define RTCSeconds() FromBCD(*((volatile unsigned char*)(RTC_port + 2)))
#define RTCMinute()  FromBCD(*((volatile unsigned char*)(RTC_port + 4)))
#define RTCHour()    FromBCD(*((volatile unsigned char*)(RTC_port + 6)))
#define RTCDay()     FromBCD(*((volatile unsigned char*)(RTC_port + 10)))
#define RTCMonth()   FromBCD(*((volatile unsigned char*)(RTC_port + 12)))
#define RTCYear()    FromBCD(*((volatile unsigned short*)(RTC_port + 14)))
#endif

// converts current rtc values to seconds
unsigned int rtcToSeconds() {
	unsigned int sec, min, hour, day, month, year;

#if TARGET_PRIZM
	// Retry if seconds rolled over mid-read to prevent torn time values
	for (int attempt = 0; attempt < 4; attempt++) {
		unsigned int sec1 = RTCSeconds();
		min = RTCMinute();
		hour = RTCHour();
		day = RTCDay();
		month = RTCMonth();
		year = RTCYear();
		sec = RTCSeconds();
		if (sec == sec1) {
			break;
		}
	}
#else
	time_t t = time(0);
	tm* cal = localtime(&t);
	sec = (unsigned int)cal->tm_sec;
	min = (unsigned int)cal->tm_min;
	hour = (unsigned int)cal->tm_hour;
	day = (unsigned int)cal->tm_mday;
	month = (unsigned int)(cal->tm_mon + 1);
	year = (unsigned int)(cal->tm_year + 1900);
#endif

	// calculate days given years and month (16-year modulo window covers 2000-2099 leap years accurately)
	unsigned int y = year % 16;
	unsigned int totalDays = (y > 0) ? (y * 365U + (y - 1U) / 4U + 1U) : 0U;
	switch (month) {
		case 12: totalDays += 30;
		case 11: totalDays += 31;
		case 10: totalDays += 30;
		case  9: totalDays += 31;
		case  8: totalDays += 31;
		case  7: totalDays += 30;
		case  6: totalDays += 31;
		case  5: totalDays += 30;
		case  4: totalDays += 31;
		case  3: totalDays += 28; if ((y % 4) == 0) totalDays++;
		case  2: totalDays += 31;
	}
	if (day > 0) {
		totalDays += (day - 1U);
	}

	// calculate seconds and return
	return sec + min * 60U + hour * 3600U + totalDays * 86400U;
}

bool mbcIsRTC() {
	return mbc.type == ROM_MBC3_TIMER_BATT || mbc.type == ROM_MBC3_TIMER_RAM_BATT;
}

#define RTC_SEC 0x08
#define RTC_MIN 0x09
#define RTC_HOUR 0x0A
#define RTC_DAYS 0x0B
#define RTC_CTL 0x0C

void rtcLatch() {
	// latch (frozen while halted)
	if (!rtc.isHalted) {
		rtc.curRTC = rtcToSeconds() - rtc.rtcBase;
	}
}

int rtcGetLatched(int forReg) {
	switch (forReg) {
		case RTC_SEC:
			return rtc.curRTC % 60;
		case RTC_MIN:
			return (rtc.curRTC / 60) % 60;
		case RTC_HOUR:
			return (rtc.curRTC / 3600) % 24;
		case RTC_DAYS:
			return (rtc.curRTC / (3600 * 24)) % 256;
		case RTC_CTL: {
			// bit 0 : hi day bit (bit 8 of 9-bit day counter)
			// bit 6 : halt
			// bit 7 : day carry (>= 512 days)
			unsigned int days = rtc.curRTC / (3600U * 24U);
			return
				((days & 0x100) ? 1 : 0) |
				(rtc.isHalted ? (1 << 6) : 0) |
				((days >= 512) ? (1 << 7) : 0);
		}
	}

	return 0;
}

void rtcCheckDirty() {
	// check for dirty rtc value if a register is selected and SRAM/RTC is enabled
	if (mbc.sramEnabled && rtc.rtcReg >= RTC_SEC && rtc.rtcReg <= RTC_CTL) {
		unsigned char newValue = rtc.rtcValue;

		for (int i = 0; i < 256; i++) {
			if (rtcMap[i] != rtc.rtcValue) {
				newValue = rtcMap[i];
				break;
			}
		}

		if (newValue != rtc.rtcValue) {
			// update rtc base (or halt) according to type of value selected
			int diff = (int)newValue - (int)rtc.rtcValue;
			switch (rtc.rtcReg) {
				case RTC_SEC:
					rtc.curRTC += diff;
					rtc.rtcBase -= diff;
					break;
				case RTC_MIN:
					rtc.curRTC += diff * 60;
					rtc.rtcBase -= diff * 60;
					break;
				case RTC_HOUR:
					rtc.curRTC += diff * 3600;
					rtc.rtcBase -= diff * 3600;
					break;
				case RTC_DAYS:
					rtc.curRTC += diff * 3600 * 24;
					rtc.rtcBase -= diff * 3600 * 24;
					break;
				case RTC_CTL:
				{
					// remove 512 day carry flag?
					if ((rtc.rtcValue & 0x80) && (newValue & 0x80) == 0) {
						unsigned int newVal = rtc.curRTC % (3600U * 24U * 512U);
						int d = (int)(newVal - rtc.curRTC);
						rtc.curRTC += d;
						rtc.rtcBase -= d;
					}
					// toggle 256 day flag?
					if ((rtc.rtcValue & 0x01) != (newValue & 0x01)) {
						unsigned int newVal;
						if (newValue & 0x01) {
							newVal = rtc.curRTC + 3600U * 24U * 256U;
						} else {
							newVal = rtc.curRTC % (3600U * 24U * 256U);
						}
						int d = (int)(newVal - rtc.curRTC);
						rtc.curRTC += d;
						rtc.rtcBase -= d;
					}
					// halt flag
					if ((rtc.rtcValue & 0x40) != (newValue & 0x40)) {
						if (newValue & 0x40) {
							// halt: freeze curRTC at current elapsed time
							rtc.curRTC = rtcToSeconds() - rtc.rtcBase;
							rtc.isHalted = 1;
						} else {
							// unhalt
							rtc.isHalted = 0;
							rtc.rtcBase = rtcToSeconds() - rtc.curRTC;
						}
					}
				}
			}

			rtc.rtcValue = newValue;
			memset(rtcMap, newValue, sizeof(rtcMap));
		}
	}
}

void rtcSelectReg(unsigned char reg) {
	rtc.rtcReg = reg;

	rtc.rtcValue = rtcGetLatched(reg);

	if (!mbc.sramEnabled) {
		return;
	}
	
	// update rtcmap and point memory to it
	memset(rtcMap, rtc.rtcValue, sizeof(rtcMap));
	for (int i = 0xa0; i <= 0xbf; i++) {
		memoryMap[i] = &rtcMap[0];
	}
}

static unsigned int curRAMHash(int size);

bool setupMBCType(mbcType type, unsigned char romSizeByte, unsigned char ramSizeByte, int fileID) {
	memset(&mbc, 0, sizeof(mbc));
	memset(&rtc, 0, sizeof(rtc));
	memset(sram, 0, sizeof(sram));
	memset(cachedBankIndex, 0xFF, sizeof(cachedBankIndex));
	memset(lastCacheRequestIndex, 0, sizeof(lastCacheRequestIndex));
	cacheIndex = 0;
	firstRomCache = 0;
	mbc1Low = 0;
	mbc1High = 0;
	mbc1Bank0 = 0;

	for (int i = 0x00; i <= 0x3f; i++) {
		memoryMap[i] = &cart[i << 8];
		specialMap[i] &= ~0x10;
	}

	mbc.romFile = fileID;
	mbc.type = type;
	mbc.romBank = 0;

	switch (type) {
		case ROM_PLAIN:
			// sram is sometimes in use with these, just enable it:
			mbc.ramType = RAM_8KB;
			mbc.numRomBanks = 2;
			mbc.numRamBanks = 1;
			selectRomBank(1);
			enableSRAM();
			sramHash = curRAMHash(getRAMSize());
			return true;
		case ROM_MBC2_BATTERY:
			mbc.batteryBacked = 1;
		case ROM_MBC2:
			mbc.ramType = RAM_MBC2;
			mbc.numRomBanks = numSwitchableBanksFromType(romSizeByte);
			if (mbc.numRomBanks < 2) {
				mbc.batteryBacked = 0;
				return false;
			}
			selectRomBank(1);
			break;
		// support for MBC 1/3/5
		case ROM_MBC1_RAM_BATT:
		case ROM_MBC3_RAM_BATT:
		case ROM_MBC3_TIMER_BATT:
		case ROM_MBC3_TIMER_RAM_BATT:
		case ROM_MBC5_RAM_BATT:
		case ROM_MBC5_RUMBLE_SRAM_BATT:
			mbc.batteryBacked = 1;
		case ROM_MBC1_RAM:
		case ROM_MBC3_RAM:
		case ROM_MBC5_RAM:
		case ROM_MBC5_RUMBLE_SRAM:
			mbc.ramType = (ramSizeType)ramSizeByte;
		case ROM_MBC1:
		case ROM_MBC3:
		case ROM_MBC5:
		case ROM_MBC5_RUMBLE:
			mbc.numRomBanks = numSwitchableBanksFromType(romSizeByte);
			if (mbc.numRomBanks < 2) {
				mbc.batteryBacked = 0;
				return false;
			}
			selectRomBank(1);
			break;
		default:
			return false;
	}

	if (type == ROM_MBC5_RUMBLE || type == ROM_MBC5_RUMBLE_SRAM || type == ROM_MBC5_RUMBLE_SRAM_BATT) {
		mbc.rumblePack = 1;
	}

	if (mbcIsRTC()) {
		// give it at least 1 second
		rtc.rtcBase = rtcToSeconds() - 1;
	}

	// check if we support the ram bank type
	if (!supportedRAM(mbc.ramType)) {
		mbc.batteryBacked = 0;
		return false;
	}

	// zero out multi-bank SRAM storage in cachedBanks so uninitialized stack bytes never leak into saves
	for (unsigned int i = 0; i < firstRomCache; i++) {
		memset(cachedBanks[i]->bank, 0, sizeof(cachedBanks[i]->bank));
	}

	sramHash = curRAMHash(getRAMSize());

	return true;
}

// called when a write attempt occurs for rom
void mbcWrite(unsigned short address, unsigned char value) {
	unsigned char upperNibble = address >> 12;
	switch (mbc.type) {
		case ROM_PLAIN:
			// does nada
			return;
		case ROM_MBC1:
		case ROM_MBC1_RAM:
		case ROM_MBC1_RAM_BATT:
			// switch bank mode (0x6000-0x7FFF)
			if (upperNibble >= 0x06) {
				mbc.bankMode = value & 1;
				mbc1UpdateBanks();
			}
			// upper 2-bit bank / RAM bank select (0x4000-0x5FFF)
			else if (upperNibble >= 0x04) {
				mbc1High = value & 0x03;
				mbc1UpdateBanks();
			}
			// ROM bank select lower 5 bits (0x2000-0x3FFF)
			else if (upperNibble >= 0x02) {
				mbc1Low = value & 0x1F;
				mbc1UpdateBanks();
			}
			// RAM enable (0x0000-0x1FFF)
			else {
				if ((value & 0x0F) == 0x0A) {
					enableSRAM();
				}
				else {
					disableSRAM();
				}
			}
			return;
		case ROM_MBC2_BATTERY:
		case ROM_MBC2:
			// MBC2 decodes bit 8 of address across 0x0000-0x3FFF
			if (upperNibble <= 0x03) {
				if ((address & 0x0100) == 0) {
					if ((value & 0x0F) == 0x0A) {
						enableSRAM();
					}
					else {
						disableSRAM();
					}
				} else {
					unsigned char bank = value & 0x0F;
					selectRomBank(bank ? bank : 1);
				}
			}
			return;
		case ROM_MBC3:
		case ROM_MBC3_RAM:
		case ROM_MBC3_RAM_BATT:
		case ROM_MBC3_TIMER_BATT:
		case ROM_MBC3_TIMER_RAM_BATT:
			if (upperNibble <= 0x01) {
				rtcCheckDirty();

				if ((value & 0x0F) == 0x0A) {
					enableSRAM();
				} else {
					disableSRAM();
					rtc.rtcReg = 0;
				}
			}
			else if (upperNibble <= 0x03) {
				// lower 7 bits
				value &= 0x7F;
				if (value == 0) {
					selectRomBank(1);
				}
				else {
					selectRomBank(value);
				}
			}
			else if (upperNibble <= 0x05) {
				rtcCheckDirty();
				bool wasRTC = rtc.rtcReg != 0;
				rtc.rtcReg = 0;

				if (value < 0x08 || !mbcIsRTC()) {
					if (mbc.sramEnabled && wasRTC) {
						enableSRAM();
					}
					selectRamBank(value);
				} else {
					rtcSelectReg(value);
				}
			}
			else if (upperNibble <= 0x07 && mbcIsRTC()) {
				if (rtc.lastLatch == 0 && value == 1) {
					rtcLatch();
				}
				rtc.lastLatch = value;
			}
			return;
		case ROM_MBC5:
		case ROM_MBC5_RAM:
		case ROM_MBC5_RAM_BATT:
		case ROM_MBC5_RUMBLE:
		case ROM_MBC5_RUMBLE_SRAM:
		case ROM_MBC5_RUMBLE_SRAM_BATT:
			if (upperNibble <= 0x01) {
				if ((value & 0x0F) == 0x0A) {
					enableSRAM();
				} else {
					disableSRAM();
				}
			}
			else if (upperNibble <= 0x02) {
				selectRomBank(value);
			}
			else if (upperNibble <= 0x03) {
				// TODO : no support for ROM's greater > 256 banks
				// Some games set this to invalid #'s.. hardware will ignore
			}
			else if (upperNibble <= 0x05) {
				if (mbc.rumblePack) {
					selectRamBank(value & 0x7);
				} else {
					selectRamBank(value & 0xF);
				}
			}
			return;
	}
}

// SRAM saving / loading
static unsigned int curRAMHash(int size) {
	unsigned int ret = 0;

	if (size <= 8192) {
		for (int i = 0; i < size; i++) {
			ret = ((ret << 5) + (ret >> 27)) ^ sram[i];
		}
	} else {
		for (int b = 0; b < size / 0x1000; b++) {
			for (int i = 0; i < 0x1000; i++) {
				ret = ((ret << 5) + (ret >> 27)) ^ cachedBanks[b]->bank[i];
			}
		}
	}

	return ret;
}

static void SwapRTCState(rtc_state& r) {
	EndianSwap(r.rtcBase);
	EndianSwap(r.curRTC);
}

// attempts to load SRAM from the given file path, false on error
bool tryLoadSRAM(const char* filepath) {
	int sramSize = ramNibbleCount(mbc.ramType) * 256;
	int rtcSize = mbcIsRTC() ? sizeof(rtc_state) : 0;

	if (mbc.numRamBanks > 1) {
		sramSize = 8192 * mbc.numRamBanks;
	}

	if (sramSize + rtcSize) {
		unsigned short pFile[256];
		Bfile_StrToName_ncpy(pFile, (const char*)filepath, strlen(filepath) + 2);

		int hFile = Bfile_OpenFile_OS(pFile, READ, 0); // Get handle
		if (hFile < 0) {
			// not found
			return false;
		}

		bool readOk = true;
		if (sramSize <= 8192) {
			if (sramSize > 0 && Bfile_ReadFile_OS(hFile, sram, sramSize, 0) != sramSize) {
				readOk = false;
			}
		} else {
			// read each bank
			for (unsigned char i = 0; i < mbc.numRamBanks * 2; i++) {
				if (Bfile_ReadFile_OS(hFile, &cachedBanks[i]->bank[0], 0x1000, i * 0x1000) != 0x1000) {
					readOk = false;
				}
			}
		}

		if (readOk) {
			sramHash = curRAMHash(sramSize);

			// read rtc if we need it
			if (rtcSize) {
				rtc_state tmpRtc;
				if (Bfile_ReadFile_OS(hFile, &tmpRtc, rtcSize, sramSize) == rtcSize) {
					SwapRTCState(tmpRtc);
					rtc = tmpRtc;
				} else {
					readOk = false;
				}
			}
		}

		Bfile_CloseFile_OS(hFile);
		return readOk;
	} else {
		// no sram
		return false;
	}
}

// attempts to save SRAM for a game to the given file path
void trySaveSRAM(const char* filepath) {
	int sramSize = ramNibbleCount(mbc.ramType) * 256;
	int rtcSize = mbcIsRTC() ? sizeof(rtc_state) : 0;

	if (mbc.numRamBanks > 1) {
		sramSize = 8192 * mbc.numRamBanks;
	}
	if (sramSize + rtcSize <= 0) {
		return;
	}

	unsigned int curHash = curRAMHash(sramSize);

	if (curHash != sramHash || rtcSize) {
		if (isStorageLockedSafe()) {
			return;
		}

		unsigned short pFile[256];
		Bfile_StrToName_ncpy(pFile, (const char*)filepath, strlen(filepath) + 2);

		size_t wantedSize = (size_t)(sramSize + rtcSize);
		Bfile_DeleteEntry(pFile);
		if (Bfile_CreateEntry_OS(pFile, CREATEMODE_FILE, &wantedSize) < 0) {
			return;
		}

		int hFile = Bfile_OpenFile_OS(pFile, WRITE, 0); // Get handle
		if (hFile < 0) {
			Bfile_DeleteEntry(pFile);
			return;
		}

		bool writeOk = true;
		if (sramSize <= 8192) {
			if (sramSize > 0 && Bfile_WriteFile_OS(hFile, sram, sramSize) < 0) {
				writeOk = false;
			}
		} else {
			// write each bank
			for (unsigned char i = 0; i < mbc.numRamBanks * 2; i++) {
				if (Bfile_WriteFile_OS(hFile, &cachedBanks[i]->bank[0], 0x1000) < 0) {
					writeOk = false;
					break;
				}
			}
		}

		if (writeOk && rtcSize) {
			rtc_state tmpRtc = rtc;
			SwapRTCState(tmpRtc);
			if (Bfile_WriteFile_OS(hFile, &tmpRtc, rtcSize) < 0) {
				writeOk = false;
			}
		}

		Bfile_CloseFile_OS(hFile);

		if (writeOk) {
			// now in sync with file system
			sramHash = curHash;
		} else {
			Bfile_DeleteEntry(pFile);
		}
	}
}

// call after state save load for proper handling
void mbcOnStateLoad() {
	if (mbc.type == ROM_MBC1 || mbc.type == ROM_MBC1_RAM || mbc.type == ROM_MBC1_RAM_BATT) {
		mbc1Low = mbc.romBank & 0x1F;
		mbc1High = (mbc.bankMode == 1) ? (mbc.ramBank & 0x03) : ((mbc.romBank >> 5) & 0x03);
		selectMBC1Bank0(mbc.bankMode == 1 ? (unsigned char)(mbc1High << 5) : 0);
	} else {
		selectMBC1Bank0(0);
	}

	if (mbc.sramEnabled) {
		enableSRAM();
	} else {
		disableSRAM();
	}

	// invalidate memory areas for ROM bank
	for (int i = 0x40; i <= 0x7f; i++) {
		specialMap[i] |= 0x10;
	}
}