
#include <Windows.h>
#include <ShObjIdl.h>
#include <atlbase.h>
#include <atlconv.h>
#include <Shlobj.h>
#include <cstdio>

#include "../utils/zx7/zx7.h"

bool FileOpen(char* intoPath)
{
	HRESULT hr;
	CComPtr<IFileOpenDialog> pDlg;
	COMDLG_FILTERSPEC aFileTypes[] = {
		{ L"Gameboy/ Gameboy Color ROM files", L"*.gb;*.gbc" },
	};

	// Create the file-open dialog COM object.
	hr = pDlg.CoCreateInstance ( __uuidof(FileOpenDialog) );

	if (FAILED(hr))
		return false;

	// Set the dialog's caption text and the available file types.
	// NOTE: Error handling omitted here for clarity.
	pDlg->SetFileTypes ( _countof(aFileTypes), aFileTypes );
	pDlg->SetTitle ( L"Select ROM file to compress" );

	// Show the dialog.
	hr = pDlg->Show ( NULL );
	// If the user chose a file, show a message box with the
	// full path to the file.
	if ( SUCCEEDED(hr) )
	{
		CComPtr<IShellItem> pItem;

		hr = pDlg->GetResult ( &pItem );

		if ( SUCCEEDED(hr) )
		{
			LPOLESTR pwsz = NULL;

			hr = pItem->GetDisplayName ( SIGDN_FILESYSPATH, &pwsz );

			if ( SUCCEEDED(hr) )
			{
				USES_CONVERSION;
				LPCSTR szText = OLE2CA(pwsz);
				strcpy_s(intoPath, 512, szText);
				CoTaskMemFree ( pwsz );
				return true;
			}
		}
	}

	return false;
}

unsigned int EndianSwapShort(unsigned int v) {
	return (v >> 8) | ((v & 0xFF) << 8);
}

int CALLBACK WinMain(
	_In_ HINSTANCE hInstance,
	_In_ HINSTANCE hPrevInstance,
	_In_ LPSTR     lpCmdLine,
	_In_ int       nCmdShow
	) {

	HRESULT coHr = CoInitialize(NULL);
	int exitCode = 0;

	char path[512];
	if (FileOpen(path)) {
		OutputDebugString(path);
		OutputDebugString("\n");

		// compressed rom format is thusly: 
		//  0x180 byte header
		//  2 byte word of num 4K pages (N) with 2 byte instruction overlap
		//  N 2 byte words of compressed page sizes (4098 if left uncompressed)
		//  N zx7 compressed pages
		FILE* f = NULL;
		if (fopen_s(&f, path, "rb") || !f) {
			OutputDebugString("Couldn't open for ROM read!\n");
			if (SUCCEEDED(coHr)) CoUninitialize();
			return -1;
		}
		fseek(f, 0, SEEK_END);

		long fileSize = ftell(f);
		if (fileSize < 0x180 || fileSize > 4 * 1024 * 1024 || (fileSize % 4096) != 0) {
			OutputDebugString("Invalid ROM file size!\n");
			fclose(f);
			if (SUCCEEDED(coHr)) CoUninitialize();
			return -1;
		}

		unsigned int size = (unsigned int)fileSize;
		int numPages = (int)(size / 4096);

		// read ROM into memory (plus 2 zeroed overlap bytes past end)
		unsigned char* rom = (unsigned char*) malloc(size + 2);
		if (!rom) {
			OutputDebugString("Out of memory reading ROM!\n");
			fclose(f);
			if (SUCCEEDED(coHr)) CoUninitialize();
			return -1;
		}
		fseek(f, 0, SEEK_SET);
		size_t readCount = fread(rom, 1, size, f);
		fclose(f);
		if (readCount != size) {
			OutputDebugString("Failed to read complete ROM file!\n");
			free(rom);
			if (SUCCEEDED(coHr)) CoUninitialize();
			return -1;
		}
		rom[size] = 0;
		rom[size + 1] = 0;

		// create compressed rom path (only strip '.' after the last path separator)
		const char* lastSlash = strrchr(path, '\\');
		const char* lastFwdSlash = strrchr(path, '/');
		const char* baseName = lastSlash > lastFwdSlash ? lastSlash : lastFwdSlash;
		const char* period = strrchr(path, '.');
		char newPath[512];
		if (period && (!baseName || period > baseName)) {
			size_t prefixLen = (size_t)(period - path);
			if (prefixLen >= sizeof(newPath) - 5) {
				prefixLen = sizeof(newPath) - 5;
			}
			strncpy_s(newPath, sizeof(newPath), path, prefixLen);
		} else {
			strncpy_s(newPath, sizeof(newPath), path, sizeof(newPath) - 5);
		}
		strcat_s(newPath, sizeof(newPath), ".gbz");

		if (fopen_s(&f, newPath, "wb") || !f) {
			OutputDebugString("Couldn't open for ROM write!\n");
			free(rom);
			if (SUCCEEDED(coHr)) CoUninitialize();
			return -1;
		}

		// progress bar stuff
		IProgressDialog *pd = NULL;
		CoCreateInstance(CLSID_ProgressDialog, NULL, CLSCTX_INPROC_SERVER, IID_IProgressDialog, (LPVOID *) (&pd));
		if (pd) {
			pd->StartProgressDialog(NULL, NULL, PROGDLG_NORMAL | PROGDLG_AUTOTIME | PROGDLG_NOMINIMIZE, NULL);
			pd->SetTitle(L"Compressing ROM File...");
			pd->SetProgress(0, numPages);
		}

		// write out header
		fwrite(rom, 1, 0x180, f);

		// num chunks (big endian format)
		unsigned short numChunks = (unsigned short)EndianSwapShort(numPages);
		fwrite(&numChunks, 1, 2, f);

		// write 0'd out sizes for now
		long tablePos = ftell(f);
		unsigned short zero = 0;
		for (int i = 0; i < numPages; i++) {
			fwrite(&zero, 1, 2, f);
		}

		// compress each 4k section and write it
		for (int i = 0; i < numPages; i++) {
			// include two bytes of instruction overlap
			unsigned char page[4098];
			memcpy(page, rom + i * 4096, 4098);

			unsigned char* compPage = 0;
			int compSize = ZX7Compress(page, 4098, &compPage);

			// ignore compression if not at least better than 10% reduction
			if (compSize > 0 && compSize < 3700 && compPage) {
				// write out compressed page
				fwrite(compPage, 1, compSize, f);
			} else {
				// decompressed is better for this page
				compSize = 4098;
				fwrite(page, 1, 4098, f);
			}

			if (compPage) {
				free(compPage);
			}

			// output result size to table
			long pos = ftell(f);
			fseek(f, tablePos, SEEK_SET);
			unsigned short chunkSize = (unsigned short)EndianSwapShort(compSize);
			fwrite(&chunkSize, 1, 2, f);
			tablePos += 2;
			fseek(f, pos, SEEK_SET);

			if (pd) {
				pd->SetProgress(i, numPages);
			}
		}

		if (pd) {
			pd->StopProgressDialog();
			pd->Release();
			pd = NULL;
		}

		fclose(f);
		free(rom);
	}

	if (SUCCEEDED(coHr)) {
		CoUninitialize();
	}

	return exitCode;
}