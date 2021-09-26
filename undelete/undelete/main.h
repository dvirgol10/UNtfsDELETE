#ifndef MAIN_H
#define MAIN_H

#include <Windows.h>
#include <stdint.h>

DWORD g_lSectorsPerCluster = 0;
DWORD g_lBytesPerSector = 0;
extern DWORD g_lBytesPerCluster = 0;
DWORD g_lBytesPerMFTEntry = 0;


int MoveDrivePointerToMFT(HANDLE hDrive);
BOOL isValidMFTEntry(byte* mftEntryBuffer);
BOOL isDeletedFile(byte* mftEntryBuffer);
void printAllDeletedFiles(HANDLE hDrive);

#endif