#include "win32apiWrapper.h"


void printErrorMessage(char* errorMessage, char* functionName) {
	printf("[!] %s (%s): 0x%x\r\n", errorMessage, functionName, GetLastError());
}


//return 1 if everything is good, otherwise return 0
BOOL GetDiskFreeSpaceAWrapper(LPCSTR lpRootPathName, LPDWORD lpSectorsPerCluster, LPDWORD lpBytesPerSector, char* errorMessage) {
	DWORD junk = 1337;
	if (GetDiskFreeSpaceA(lpRootPathName, lpSectorsPerCluster, lpBytesPerSector, (LPDWORD) &junk, (LPDWORD) &junk) == 0) {
		printErrorMessage(errorMessage, "GetDiskFreeSpaceA");
		return 0;
	}
	return 1;
}


//return a valid handle if everything is good, otherwise return INVALID_HANDLE_VALUE (like CreateFileW itself)
HANDLE CreateFileWWrapper(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, char* errorMessage) {
	HANDLE handle = CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, NULL);
	if (handle == INVALID_HANDLE_VALUE) {
		printErrorMessage(errorMessage, "CreateFileW");
	}
	return handle;
}


//return 1 if everything is good, otherwise return 0
BOOL ReadFileWrapper(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, char* errorMessage) {
	if (ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, NULL, NULL) == 0) {
		printErrorMessage(errorMessage, "ReadFile");
		return 0;
	}
	return 1;
}


//return 1 if everything is good, otherwise return 0
BOOL SetFilePointerExWrapper(HANDLE hFile, LARGE_INTEGER liDistanceToMove, PLARGE_INTEGER lpNewFilePointer, DWORD dwMoveMethod, char* errorMessage) {
	if (SetFilePointerEx(hFile, liDistanceToMove, lpNewFilePointer, dwMoveMethod) == 0) {
		printErrorMessage(errorMessage, "SetFilePointerEx");
		return 0;
	}
	return 1;
}


//return 1 if everything is good, otherwise return 0
BOOL CloseHandleWrapper(HANDLE hObject, char* errorMessage) {
	if (CloseHandle(hObject) == 0) {
		printErrorMessage(errorMessage, "CloseHandle");
	}
	return 1;
}