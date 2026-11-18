

#include "platform.h"
#include "screen_faq.h"
#include "exam_mode.h"
#include "calctype/fonts/consolas_intl/consolas_intl.h"

void screen_faq::setup() {
	faqHandle = -1;
	textBuffer = 0;
	textOffset = 0;
}

static unsigned char hashString(const char* str) {
	int size = strlen(str);
	unsigned int ret = 0;
	for (int i = 0; i < size; i++) {
		ret = ((ret << 5) + (ret >> 27)) ^ str[i];
	}
	return (ret & 0xFF00) >> 8;
}

void screen_faq::select() {
	ResolveBG(bg_faq);

	textOffset = 0;

	if (isStorageLockedOrExamMode()) {
		Print(10, 10, "Access denied: Exam Mode / Storage locked.", false);
		return;
	}

	if (emulator.settings.selectedRom[0]) {
		unsigned char faqHash = hashString(emulator.settings.selectedRom);
		if (((emulator.settings.faqOffset & 0xFF000000) >> 24) == faqHash) {
			textOffset = emulator.settings.faqOffset & 0x00FFFFFF;
		}
		loadFAQ();
	}

	if (faqHandle < 0) {
		Print(10, 10, "No .txt file found for this rom.", false);
	}
}

void screen_faq::deselect() {
	if (textBuffer != 0) {
		free(textBuffer);
		textBuffer = 0;
	}

	if (faqHandle >= 0) {
		Bfile_CloseFile_OS(faqHandle);
		faqHandle = -1;

		// save the text offset
		textOffset += readOffset;
		unsigned char faqHash = hashString(emulator.settings.selectedRom);

		emulator.settings.faqOffset = (faqHash << 24) | (textOffset & 0x00FFFFFF);
		emulator.saveSettings();
	}
}

void screen_faq::loadFAQ() {
	char filename[64];
	unsigned short wFile[64];
	strcpy(filename, "\\\\fls0\\");
	strncat(filename, emulator.settings.selectedRom, sizeof(filename) - strlen(filename) - 5);
	char* dot = strrchr(filename, '.');
	if (dot && (size_t)(dot - filename) + 5 <= sizeof(filename)) {
		strcpy(dot, ".txt");
	} else {
		strcat(filename, ".txt");
	}

	Bfile_StrToName_ncpy(wFile, filename, 64);
	faqHandle = Bfile_OpenFile_OS(wFile, READ, 0);
	if (faqHandle < 0) {
		return;
	}

	faqSize = Bfile_GetFileSize_OS(faqHandle);
	if (faqSize <= 0) {
		Bfile_CloseFile_OS(faqHandle);
		faqHandle = -1;
		faqSize = 0;
		return;
	}

	if (textOffset > faqSize || textOffset < 0) {
		textOffset = 0;
	}
	readOffset = 0;
	x = 0;
	readText(textOffset);
	draw();
}

void screen_faq::draw() {
	ResolveBG(bg_faq);

	int curTextPos = readOffset;

	for (int y = 1; y + consolas_intl.height < 216 && curTextPos < textSize; y += consolas_intl.height) {
		const int lineSize = 384 / 5;
		char bufToDraw[lineSize + 1];
		bool skipLine = false;
		if (x) {
			for (int c = 0; c < x; c++) {
				if (curTextPos + c >= textSize || textBuffer[curTextPos + c] == '\n') {
					skipLine = true;
					break;
				}
			}
		}
		if (!skipLine && curTextPos + x < textSize) {
			int copyLen = min(lineSize, textSize - (curTextPos + x));
			memcpy(bufToDraw, textBuffer + curTextPos + x, copyLen);
			bufToDraw[copyLen] = '\0';
			for (int c = 0; c < copyLen; c++) {
				if (bufToDraw[c] == '\n') {
					bufToDraw[c] = 0;
					break;
				}
			}

			CalcType_Draw(&consolas_intl, bufToDraw, 2, y, COLOR_WHITE, 0, 0);
		}
		
		// go to next line
		do {
			curTextPos++;
		} while (curTextPos < textSize && textBuffer[curTextPos-1] != '\n');
	}

	// move down when we need to
	if (textOffset + textSize != faqSize && curTextPos >= textSize && readOffset > 2048) {
		readText(textOffset + 2048);
		readOffset -= 2048;
		draw();
	}
}

void screen_faq::readText(int bufferPos) {
	textSize = min(4096, faqSize - bufferPos);
	textOffset = bufferPos;
	if (textSize <= 0) {
		textSize = 0;
		return;
	}

	if (textBuffer == 0) {
		textBuffer = (char*) malloc(4096);
		if (!textBuffer) {
			textSize = 0;
			return;
		}
	}

	int bytesRead = Bfile_ReadFile_OS(faqHandle, textBuffer, textSize, bufferPos);
	if (bytesRead < 0) {
		textSize = 0;
	} else if (bytesRead < textSize) {
		textSize = bytesRead;
	}
}

void screen_faq::handleDown() {
	if (faqHandle < 0) {
		return;
	}

	x = 0;

	// go to next line (twice)
	for (int i = 0; i < 2 && readOffset < textSize; i++) {
		do {
			readOffset++;
		} while (readOffset < textSize && textBuffer[readOffset-1] != '\n');
	}

	draw();
}


void screen_faq::handleUp() {
	if (faqHandle < 0) {
		return;
	}

	x = 0;

	// go to previous line if possible (twice)
	for (int i = 0; i < 2; i++) {
		if (readOffset) {
			do {
				readOffset--;
			} while (readOffset > 0 && textBuffer[readOffset-1] != '\n');
		}
	}

	if (!readOffset) {
		if (textOffset > 0) {
			int newOffset = max(textOffset - 2048, 0);
			readOffset = textOffset - newOffset;
			readText(newOffset);
			handleUp();
			return;
		}
	}

	draw();
}

void screen_faq::handleRight() {
	if (faqHandle < 0) {
		return;
	}

	if (x + readOffset < textSize && textBuffer[readOffset + x] != '\n') {
		x++;
	}

	draw();
}

void screen_faq::handleLeft() {
	if (faqHandle < 0) {
		return;
	}

	if (x) {
		x--;
	}

	draw();
}