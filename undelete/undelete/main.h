#ifndef MAIN_H
#define MAIN_H

#include "dataRunsParser.h"

#include <Windows.h>
#include <stdint.h>


const uint64_t rootFileMftEntryIndex = 5;

DWORD g_lSectorsPerCluster = 0;
DWORD g_lBytesPerSector = 0;
extern DWORD g_lBytesPerCluster = 0;
DWORD g_lBytesPerMftEntry = 0;


typedef struct ParentDirectoryListNode {
	struct ParentDirectoryListNode* next;
	wchar_t* nameString;
	uint8_t nameStringLength;
} ParentDirectoryListNode;

typedef struct DirectoriesPathList {
	ParentDirectoryListNode* first;
} DirectoriesPathList;

typedef struct DeletedFileListNode {
	struct DeletedFileListNode* next;
	uint64_t mftEntryIndex;
	wchar_t* path;
} DeletedFileListNode;

typedef struct DeletedFilesList {
	DeletedFileListNode* first;
	DeletedFileListNode* last;

} DeletedFilesList;


int moveDrivePointerToMft(HANDLE hDrive);
BOOL isValidMftEntry(byte* mftEntryBuffer);
BOOL isDeletedFile(byte* mftEntryBuffer);
uint64_t getParentFileMftEntryIndex(byte* mftEntryBuffer);
byte* getMftEntryBufferOfIndex(HANDLE hDrive, DataRunsList* dataRunsListOfMftFile, uint64_t mftEntryIndex);
wchar_t* getFilePath(HANDLE hDrive, DataRunsList* dataRunsListOfMftFile, byte* mftEntryBuffer);
void insertDeletedFileToList(HANDLE hDrive, DataRunsList* dataRunsListOfMftFile, DeletedFilesList* deletedFilesList, byte* mftEntryBuffer, uint64_t mftEntryIndex);
DeletedFilesList* listAllDeletedFiles(HANDLE hDrive);


#endif