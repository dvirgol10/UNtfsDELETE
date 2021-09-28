#ifndef MAIN_H
#define MAIN_H


#include "dataRunsParser.h"

#include <Windows.h>
#include <stdint.h>


const uint64_t ROOT_FILE_MFT_ENTRY_INDEX = 5;

extern HANDLE hDrive = 0;

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
	DataRunsList* dataRunsListOfDataAttributOfMFT;

} DeletedFilesList;


int moveDrivePointerToMft();
BOOL isValidMftEntry(byte* mftEntryBuffer);
BOOL isDeletedFile(byte* mftEntryBuffer);
uint64_t getParentFileMftEntryIndex(byte* mftEntryBuffer);
byte* getMftEntryBufferOfIndex(DataRunsList* dataRunsListOfMftFile, uint64_t mftEntryIndex);
wchar_t* getFilePath(DataRunsList* dataRunsListOfMftFile, byte* mftEntryBuffer);
void insertDeletedFileToList(DeletedFilesList* deletedFilesList, byte* mftEntryBuffer, uint64_t mftEntryIndex);
DeletedFilesList* listAllDeletedFiles();
void freeDirectoriesPathList(DirectoriesPathList* directoriesPathList);
void freeDeletedFilesList(DeletedFilesList* deletedFilesList);

#endif