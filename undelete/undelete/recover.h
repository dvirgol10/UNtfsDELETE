#ifndef RECOVER_H
#define RECOVER_H

#include "win32apiWrapper.h"
#include "mftAttributes.h"
#include "dataRunsParser.h"

#include <Windows.h>
#include <stdio.h>

wchar_t* strToWcs(char* str);
BOOL isDirectory(char* path);
wchar_t* getNewRecoveredFilePath(byte* mftEntryBuffer, char* pathToDirectoryOfNewRecoveredFile);
LONGLONG getCurrentFilePointerLocation(HANDLE hFile, char* errorMessage);
void recoverFileFromMftEntry(byte* mftEntryBuffer, char* pathToDirectoryOfNewRecoveredFile);


#endif
