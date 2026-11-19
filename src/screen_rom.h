#pragma once

#include "emulator.h"

struct foundFile {
	char path[32];
};

struct screen_rom : public emulator_screen {
	foundFile* files;
	int numFiles;
	int selectedFile;
	int loadedFile;
	int curScroll;
	bool blockedByExamMode;

	virtual void setup() override;
	virtual void select() override;
	virtual void deselect() override;

	// key press handles
	virtual void handleUp() override;
	virtual void handleDown() override;

private:
	void discoverFiles();
	void drawFiles();
	bool checkScroll();
};