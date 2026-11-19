
// displays a frame to the a pause preview window (which is just the palette entries 0-11 at half rez) and then returns to the menu on drawFrame 

#include "debug.h"
#include "emulator.h"

#include "gpu.h"
#include "memory.h"
#include "keys.h"

#include "tilerow.inl"
#include "dmg_scanline.inl"
#include "cgb_scanline.inl"

static void renderPreviewLine() {
	if ((cpu.memory.LY_lcdline & 1) == 0) {
		if (cgb.isCGB) {
			RenderCGBScanline();
			if (cgb.dirtyPalette) {
				cgbResolvePalette();
			}

			unsigned char* src = &lineBuffer[8 + lineBuffer[0]];
			unsigned char* previewLine = &emulator.pausePreview[80 * cpu.memory.LY_lcdline / 2];
			// every other pixel goes into the preview line, packed at 8 bpp
			for (int i = 0; i < 160; i += 2) {
				*(previewLine++) = src[i] >> 2;
			}
		} else {
			RenderDMGScanline();

			unsigned char* src = &lineBuffer[8 + lineBuffer[0]];
			unsigned char* previewLine = &emulator.pausePreview[80 * cpu.memory.LY_lcdline / 2];
			// every other pixel goes into the preview line, packed at 4 bpp
			for (int i = 0; i < 160; i += 2) {
				*(previewLine++) = (src[i + 1] << 2) | (src[i] >> 2);
			}
		}

	}
}

static void renderPreviewBlankLine() {
	if ((cpu.memory.LY_lcdline & 1) == 0) {
		unsigned char* previewLine = &emulator.pausePreview[80 * cpu.memory.LY_lcdline / 2];
		memset(previewLine, 0, 80);
	}
}

static void renderPreviewScreen() {
	// set the preview to valid and exit
	emulator.pausePreviewValid = true;
	keys.exit = true;
}

void enablePausePreview() {
	if (renderScanline != renderPreviewLine) {
		renderScanline = renderPreviewLine;
		renderBlankScanline = renderPreviewBlankLine;
		drawFramebuffer = renderPreviewScreen;
	}
}

bool isPausePreviewPending() {
	return renderScanline == renderPreviewLine;
}