#ifndef WIN32API_WRAPPER_H
#define WIN32API_WRAPPER_H


#include <Windows.h>
#include <stdio.h>


void printErrorMessage(char* errorMessage, char* functionName);
BOOL GetDiskFreeSpaceAWrapper(LPCSTR lpRootPathName, LPDWORD lpSectorsPerCluster, LPDWORD lpBytesPerSector, char* errorMessage);
HANDLE CreateFileWWrapper(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, char* errorMessage);
BOOL ReadFileWrapper(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, char* errorMessage);
BOOL SetFilePointerExWrapper(HANDLE hFile, LARGE_INTEGER liDistanceToMove, PLARGE_INTEGER lpNewFilePointer, DWORD dwMoveMethod, char* errorMessage);
BOOL CloseHandleWrapper(HANDLE hObject, char* errorMessage);

#endif