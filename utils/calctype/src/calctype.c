#include "calctype.h"

extern unsigned char* CalcType_DefaultVRAM();
extern unsigned int CalcType_DefaultPitch();
extern void CalcType_DrawGlyph(CalcTypeCharData* glyph, int x, int y, unsigned short color, unsigned char* vram, unsigned int pitch);

/*
	Returns the width in pixels of the given text using the given font
*/
unsigned int CalcType_Width(const CalcTypeFont* font, const char* text) {
	unsigned int subPixelWidth = 0;
	while (*text) {
		unsigned char ch = (unsigned char)*text;
		if (ch <= 32) {
			if (ch == 32) {
				subPixelWidth += font->space;
			} else if (ch == '\n') {
				// currently bad for multiline draws
			}
		} else {
			unsigned short dataOffset = font->charOffset[ch - 32];
			if (dataOffset != 0xFFFF) {
				subPixelWidth += ((CalcTypeCharData*)(font->charData + dataOffset))->xAdvance;
			}
		}
		text++;
	}

	return subPixelWidth / 3;
}

/*
	Draws the given font data to the given position. Use 0 for vram and pitch to use device defaults.
 */
void CalcType_Draw(const CalcTypeFont* font, const char* text, int x, int y, unsigned short color, unsigned char* vram, unsigned int pitch) {
	if (vram == 0) {
		vram = CalcType_DefaultVRAM();
	}

	if (pitch == 0) {
		pitch = CalcType_DefaultPitch();
	}
	int subX = x * 3;
	int startX = subX;
	int maxX = pitch * 3;

	while (*text) {
		unsigned char ch = (unsigned char)*text;
		if (ch <= 32) {
			if (ch == 32) {
				subX += font->space;
			}
			else if (ch == '\n') {
				subX = startX;
				y += font->height;
			}
		} else {
			unsigned short dataOffset = font->charOffset[ch - 32];
			if (dataOffset != 0xFFFF) {
				CalcTypeCharData* glyph = (CalcTypeCharData*)(font->charData + dataOffset);
				if (subX < maxX) {
					int gx = subX + glyph->xOffset;
					int px = (gx >= 0) ? (gx / 3) : ((gx - 2) / 3);
					CalcType_DrawGlyph(glyph, px, y + glyph->yOffset, color, vram, pitch);
				}
				subX += glyph->xAdvance;
			}
		}
		text++;
	}
}