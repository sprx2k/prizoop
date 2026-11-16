#pragma once

// Secondary in-app Exam Mode / locked-storage defense-in-depth guard.
// Note: On fx-CG100 / Graph Math+, MPM is the primary enforcement layer and
// prevents opening the add-in launcher (HOME -> TOOLS) during Exam Mode.
// On fx-CG50, CASIOWIN blocks .g3a add-ins directly in Exam Mode.
// This helper checks whether OS storage access (\\fls0\) is restricted
// (as occurs in Exam Mode) so Prizoop refuses to launch or run ROMs if
// invoked while storage is locked.

bool isStorageLockedOrExamMode();
void showExamModeBlockedScreen(bool waitForKey);
