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
BOOL WriteFileWrapper(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, char* errorMessage) {
	DWORD junk = 1337;
	if (WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, (LPDWORD)&junk, NULL) == 0) {
		printErrorMessage(errorMessage, "WriteFile");
		return 0;
	}
	return 1;
}


//return 1 if everything is good, otherwise return 0
BOOL SetFilePointerExWrapper(HANDLE hFile, LONGLONG lDistanceToMove, char* errorMessage) {
	LARGE_INTEGER liDistanceToMove = { 0 };
	liDistanceToMove.QuadPart = lDistanceToMove;
	LARGE_INTEGER lNewFilePointer = { 0 };
	if (SetFilePointerEx(hFile, liDistanceToMove, (PLARGE_INTEGER)&lNewFilePointer, FILE_BEGIN) == 0) {
		printErrorMessage(errorMessage, "SetFilePointerEx");
		return 0;
	}
	if (lNewFilePointer.QuadPart != liDistanceToMove.QuadPart) {
		printf("[!] The new file pointer is of address 0x%016X, which is not the desired address (0x%016X).\r\n", (int)lNewFilePointer.QuadPart, (int)liDistanceToMove.QuadPart);
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