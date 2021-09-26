#ifndef MAIN_H
#define MAIN_H

#include <Windows.h>
#include <stdint.h>

DWORD g_lSectorsPerCluster = 0;
DWORD g_lBytesPerSector = 0;
DWORD g_lBytesPerCluster = 0;
DWORD g_lBytesPerMFTEntry = 0;


int MoveVolumePointerToMFT(HANDLE hVolume);
BOOL isValidMFTEntry(byte* mftEntryBuffer);
BOOL isDeletedFile(byte* mftEntryBuffer);
wchar_t* getName(byte* mftEntryBuffer);
uint64_t getFileSize(byte* mftEntryBuffer);
void printAllDeletedFiles(HANDLE hVolume);

#endif