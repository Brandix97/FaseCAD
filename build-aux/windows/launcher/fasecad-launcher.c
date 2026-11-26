/*
	Copyright 2026 The FaseCAD Team
	This file is part of FaseCAD.

	FaseCAD is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	FaseCAD is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with FaseCAD.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
	FaseCAD.exe at the root of the portable folder: it starts
	bin\fasecad.exe with the symbols, title blocks and translations that
	come in the folder, as FaseCAD.bat did, without a console window.
	What it is given (the files to open) is handed on.

	Built by the packaging (MSYS2 UCRT64):
		windres fasecad-launcher.rc -O coff -o fasecad-launcher.res
		gcc -municode -mwindows -O2 -s fasecad-launcher.c fasecad-launcher.res -o FaseCAD.exe
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>

#define LAUNCHER_PATH 4096

static const wchar_t data_arguments[] =
	L" --common-elements-dir=elements/ --common-tbt-dir=titleblocks/ --lang-dir=lang/";

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR arguments, int show)
{
	(void)instance;
	(void)previous;
	(void)show;

		/* The folder of this launcher, with its last backslash */
	wchar_t folder[LAUNCHER_PATH];
	DWORD length = GetModuleFileNameW(NULL, folder, LAUNCHER_PATH);
	if (length == 0 || length >= LAUNCHER_PATH) return 1;
	wchar_t *slash = wcsrchr(folder, L'\\');
	if (!slash) return 1;
	slash[1] = L'\0';

	wchar_t program[LAUNCHER_PATH];
	if (swprintf(program, LAUNCHER_PATH, L"%lsbin\\fasecad.exe", folder) < 0) return 1;

	if (GetFileAttributesW(program) == INVALID_FILE_ATTRIBUTES) {
		MessageBoxW(NULL,
					L"bin\\fasecad.exe was not found beside FaseCAD.exe.\n\n"
					L"Extract the whole ZIP file and start FaseCAD.exe from the "
					L"folder it makes.",
					L"FaseCAD", MB_OK | MB_ICONERROR);
		return 1;
	}

		/* The command line: the program, the data of the folder (relative
		   to it: it is the working folder), then what this launcher got */
	static wchar_t command[LAUNCHER_PATH * 4];
	if (swprintf(command, sizeof(command) / sizeof(command[0]), L"\"%ls\"%ls%ls%ls",
				 program, data_arguments,
				 (arguments && *arguments) ? L" " : L"",
				 arguments ? arguments : L"") < 0) {
		return 1;
	}

	STARTUPINFOW startup;
	PROCESS_INFORMATION process;
	ZeroMemory(&startup, sizeof(startup));
	startup.cb = sizeof(startup);
	ZeroMemory(&process, sizeof(process));
	if (!CreateProcessW(program, command, NULL, NULL, FALSE, 0, NULL,
						folder, &startup, &process)) {
		wchar_t message[512];
		swprintf(message, 512, L"FaseCAD could not be started (error %lu).",
				 GetLastError());
		MessageBoxW(NULL, message, L"FaseCAD", MB_OK | MB_ICONERROR);
		return 1;
	}
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	return 0;
}
