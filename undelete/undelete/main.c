#include "main.h"

#include "win32apiWrapper.h"
#include "mftAttributes.h"
#include "recover.h"
#include "dataRunsParser.h"

#include <stdio.h>


extern const DWORD g_BYTES_PER_ATTRIBUTE_HEADER;
extern const DWORD g_BYTES_PER_ATTRIBUTE_RESIDENT_DATA;

extern const uint32_t g_STANDARD_INFORMATION_ATTRIBUTE_TYPECODE;
extern const uint32_t g_FILE_NAME_ATTRIBUTE_TYPECODE;
extern const uint32_t g_DATA_ATTRIBUTE_TYPECODE;


//move the file pointer of the drive to the location of the starting sector of the MFT.
//return 1 if everything is good, otherwise return 0
int moveDrivePointerToMft() {
	printf("[*] Reading the VBR ...\r\n");
	byte* lpBuffer = malloc(g_lBytesPerSector);
	if (ReadFileWrapper(hDrive, lpBuffer, g_lBytesPerSector, "Couldn't read the VBR") == 0) { //when reading from a disk drive, the maximum number of bytes to be read has to be a multiple of sector size
		return 0;
	}

	g_lBytesPerMftEntry = 1 << (-(signed char)(lpBuffer[0x40])); //calculate the size in bytes of each MFT entry
	//printf("\r\nAn MFT entry size is %ld bytes.\r\n\r\n", g_lBytesPerMftEntry);

	printf("[*] Jumping to the location of the starting sector of the MFT");
	//calculate the first MFT cluster number, then multiply it by cluster size in bytes
	LONGLONG lDistanceToMove = 0;
	memcpy(&lDistanceToMove, lpBuffer + 0x30, 8);
	lDistanceToMove *= g_lBytesPerCluster;
	free(lpBuffer);
	printf(" in address 0x%016X ...\r\n", (int)lDistanceToMove);

	if (SetFilePointerExWrapper(hDrive, lDistanceToMove, "Couldn't jump to the location of the starting sector of the MFT") == 0) {
		return 0;
	}

	return 1;
}


//check the signature of the MFT entry to determine whether it is a valid entry or not (0x46494c45 = "FILE")
BOOL isValidMftEntry(byte* mftEntryBuffer) {
	return mftEntryBuffer[0x0] == 0x46 && mftEntryBuffer[0x1] == 0x49 && mftEntryBuffer[0x2] == 0x4c && mftEntryBuffer[0x3] == 0x45;
}


//return TRUE iff the MFT entry is of deleted file
BOOL isDeletedFile(byte* mftEntryBuffer) {
	return mftEntryBuffer[0x16] == 0x00; //check the entry flags. 0x00 means deleted file
}


//return the offset of the mft entry which represents the parent file (parent directory)
uint64_t getParentFileMftEntryIndex(byte* mftEntryBuffer) {
	uint16_t fileNameAttributeHeaderOffset = findAttributeHeaderOffset(mftEntryBuffer, g_FILE_NAME_ATTRIBUTE_TYPECODE);
	if (fileNameAttributeHeaderOffset == 0) { //if there is no $FILE_NAME attribute in the MFT entry
		return 0;
	}

	uint64_t parentFileMftEntryIndex = 0;
	if (isResident(mftEntryBuffer, fileNameAttributeHeaderOffset)) { //if the actual attribute is resident
		uint16_t fileNameAttributeOffset = fileNameAttributeHeaderOffset + g_BYTES_PER_ATTRIBUTE_HEADER + g_BYTES_PER_ATTRIBUTE_RESIDENT_DATA;
		memcpy(&parentFileMftEntryIndex, mftEntryBuffer + fileNameAttributeOffset, 6); //retrieve the MFT entry index of the parent file
	}
	else {
		byte* firstClusterOfFileNameAttribute = getFirstClusterOfNonResidentAttribute(mftEntryBuffer, fileNameAttributeHeaderOffset);
		memcpy(&parentFileMftEntryIndex, firstClusterOfFileNameAttribute, 6);
		free(firstClusterOfFileNameAttribute);
	}
	return parentFileMftEntryIndex;
}


//return the mft entry which its index is mftEntryIndex
byte* getMftEntryBufferOfIndex(DataRunsList* dataRunsListOfMftFile, uint64_t mftEntryIndex) {
	uint32_t numberOfMftEntries = 0;
	DataRunListNode* dataRunListNode = dataRunsListOfMftFile->first;
	while (mftEntryIndex >= numberOfMftEntries + dataRunListNode->numberOfClusters * (g_lBytesPerCluster / g_lBytesPerMftEntry)) {
		numberOfMftEntries += (dataRunListNode->numberOfClusters * (g_lBytesPerCluster / g_lBytesPerMftEntry));
		dataRunListNode = dataRunListNode->next;
	}
	LONGLONG addressOfMftEntry = (dataRunListNode->numberOfStartingCluster * g_lBytesPerCluster) + (mftEntryIndex - numberOfMftEntries) * g_lBytesPerMftEntry;
	if (SetFilePointerExWrapper(hDrive, addressOfMftEntry, "Couldn't jump to the location of the data run of $MFT") == 0) {
		return 0;
	}
	byte* mftEntryBuffer = malloc(g_lBytesPerMftEntry);
	if (ReadFileWrapper(hDrive, mftEntryBuffer, g_lBytesPerMftEntry, "Couldn't read MFT entry") == 0) {
		return 0;
	}
	return mftEntryBuffer;
}


//return the path of the file that is represented in mftEntryBuffer
wchar_t* getFilePath(DataRunsList* dataRunsListOfMftFile, byte* mftEntryBuffer) {
	LONGLONG currentFilePointerLocation = getCurrentFilePointerLocation(hDrive, "Couldn't get the current file pointer location"); //we need the current file pointer location in order to set it back in the end of the path resolving

	//put each part of the file's path in a linked linked list
	DirectoriesPathList* directoriesPathList = (DirectoriesPathList*)malloc(sizeof(DirectoriesPathList));
	wchar_t* nameString = getName(mftEntryBuffer);
	if (nameString == 0) {
		return 0;
	}
	ParentDirectoryListNode* parentDirectoryListNode = (ParentDirectoryListNode*)malloc(sizeof(ParentDirectoryListNode));
	*parentDirectoryListNode = (ParentDirectoryListNode){ NULL, nameString, wcslen(nameString) + 1 }; //"wcslen(nameString)" doesn't include the null terminator bytes
	*directoriesPathList = (DirectoriesPathList){ parentDirectoryListNode };

	uint64_t parentFileMftEntryIndex = getParentFileMftEntryIndex(mftEntryBuffer);
	if (parentFileMftEntryIndex == 0) {
		return 0;
	}
	while (parentFileMftEntryIndex != ROOT_FILE_MFT_ENTRY_INDEX) { //while the parent directory isn't root
		byte* parentFileMftEntry = getMftEntryBufferOfIndex(dataRunsListOfMftFile, parentFileMftEntryIndex);

		nameString = getName(parentFileMftEntry);
		if (nameString == 0) {
			return 0;
		}
		parentDirectoryListNode = (ParentDirectoryListNode*)malloc(sizeof(ParentDirectoryListNode));
		//insert the node to the start of linked list, because the we are resolving the path backwards
		*parentDirectoryListNode = (ParentDirectoryListNode){ directoriesPathList->first, nameString, wcslen(nameString) + 1 }; //the "+ 1" in "(wcslen(nameString) + 1" is for the (wide) character L"\\"
		directoriesPathList->first = parentDirectoryListNode;

		parentFileMftEntryIndex = getParentFileMftEntryIndex(parentFileMftEntry);
		free(parentFileMftEntry);
		if (parentFileMftEntryIndex == 0) {
			return 0;
		}
	}

	//handle the root
	parentDirectoryListNode = (ParentDirectoryListNode*)malloc(sizeof(ParentDirectoryListNode));
	*parentDirectoryListNode = (ParentDirectoryListNode){ directoriesPathList->first, L"D:", wcslen(L"D:") + 1 }; //the "+ 1" in "(wcslen(nameString) + 1" is for the (wide) character L"\\"
	directoriesPathList->first = parentDirectoryListNode;

	SetFilePointerExWrapper(hDrive, currentFilePointerLocation, "Couldn't set back the file pointer location");

	//calculate the length of the path
	uint64_t pathLength = 0;
	while (parentDirectoryListNode != NULL) {
		pathLength += parentDirectoryListNode->nameStringLength;
		parentDirectoryListNode = parentDirectoryListNode->next;
	}

	//create a string of the file's path
	parentDirectoryListNode = directoriesPathList->first;
	wchar_t* path = malloc(pathLength * 2); //multiply by 2 because those are wide characters
	wcscpy_s(path, pathLength, parentDirectoryListNode->nameString); //root
	parentDirectoryListNode = parentDirectoryListNode->next;
	while (parentDirectoryListNode != NULL) {
		wcscat_s(path, pathLength, L"\\");
		wcscat_s(path, pathLength, parentDirectoryListNode->nameString);
		parentDirectoryListNode = parentDirectoryListNode->next;
	}

	freeDirectoriesPathList(directoriesPathList);

	return path;
}


//insert the deleted file to the end of linked list of the deleted files
void insertDeletedFileToList(DeletedFilesList* deletedFilesList, byte* mftEntryBuffer, uint64_t mftEntryIndex) {
	wchar_t* deletedFilePath = getFilePath(deletedFilesList->dataRunsListOfDataAttributOfMFT, mftEntryBuffer);
	if (deletedFilePath != 0) { //if valid file path
		DeletedFileListNode* deletedFileListNode = malloc(sizeof(DeletedFileListNode));
		*deletedFileListNode = (DeletedFileListNode){ NULL, mftEntryIndex, deletedFilePath };
		if (deletedFilesList->first == NULL && deletedFilesList->last == NULL) { //if this is the first deleted file
			deletedFilesList->first = deletedFileListNode;
		}
		else {
			deletedFilesList->last->next = deletedFileListNode;
		}
		deletedFilesList->last = deletedFileListNode;
	}

}


//create a linked list of all of the deleted files that are still in the $MFT file
DeletedFilesList* listAllDeletedFiles() {
	byte* mftEntryBuffer = malloc(g_lBytesPerMftEntry);
	if (ReadFileWrapper(hDrive, mftEntryBuffer, g_lBytesPerMftEntry, "Couldn't read MFT entry") == 0) { //read the first entry of the MFT, which is the entry for $MFT
		return;
	}

	uint64_t mftFileSize = getFileValidDataSize(mftEntryBuffer); //get the size of the $MFT file
	uint64_t amountOfMFTEntries = mftFileSize / g_lBytesPerMftEntry; //calculate the size of the $MFT file
	//printf("The size of the $MFT file is %llu bytes, which means it has %llu entries.\r\n", mftFileSize, amountOfMFTEntries);

	int mftEntryNumber = 0; //MFT entry number zero is the entry of $MFT

	DataRunsList* dataRunsListOfDataAttributOfMFT = parseDataRuns(mftEntryBuffer, findAttributeHeaderOffset(mftEntryBuffer, g_DATA_ATTRIBUTE_TYPECODE));

	DeletedFilesList* deletedFilesList = malloc(sizeof(DeletedFilesList));
	*deletedFilesList = (DeletedFilesList){ NULL, NULL, dataRunsListOfDataAttributOfMFT };

	DataRunListNode* dataRunListNode = dataRunsListOfDataAttributOfMFT->first;
	//move through the data runs 
	while (dataRunListNode != NULL) {
		uint64_t startingAddressOfCurrentDataRun = dataRunListNode->numberOfStartingCluster * g_lBytesPerCluster; //calculate the starting address of the corresponding data run
		if (SetFilePointerExWrapper(hDrive, startingAddressOfCurrentDataRun, "Couldn't jump to the location of the data run of $MFT") == 0) {
			return 0;
		}

		for (int i = 0; i < dataRunListNode->numberOfClusters * (g_lBytesPerCluster / g_lBytesPerMftEntry); i++) { //read all the MFT entries of the current data run

			if (mftEntryNumber < amountOfMFTEntries) {
				if (ReadFileWrapper(hDrive, mftEntryBuffer, g_lBytesPerMftEntry, "Couldn't read MFT entry") == 0) {
					return;
				}

				if (isValidMftEntry(mftEntryBuffer)) {
					if (isDeletedFile(mftEntryBuffer)) {
						wchar_t* deletedFileName = getName(mftEntryBuffer);
						if (deletedFileName != 0) { //if the deleted file has a name
							free(deletedFileName);
							insertDeletedFileToList(deletedFilesList, mftEntryBuffer, mftEntryNumber);
						}
					}
				}

				++mftEntryNumber;
			}

		}

		dataRunListNode = dataRunListNode->next; //advance in the linked list
	}

	free(mftEntryBuffer);

	return deletedFilesList;
}


void freeDirectoriesPathList(DirectoriesPathList* directoriesPathList) {
	ParentDirectoryListNode* parentDirectoryListNode = directoriesPathList->first;

	//this is not in the while loop because we don't have to "free(tmp->nameString)" for the first node
	ParentDirectoryListNode* tmp = parentDirectoryListNode;
	parentDirectoryListNode = parentDirectoryListNode->next;
	free(tmp);

	while (parentDirectoryListNode != NULL) {
		ParentDirectoryListNode* tmp = parentDirectoryListNode;
		parentDirectoryListNode = parentDirectoryListNode->next;
		free(tmp->nameString);
		free(tmp);
	}
	free(directoriesPathList);
}


void freeDeletedFilesList(DeletedFilesList* deletedFilesList) {
	DeletedFileListNode* deletedFileListNode = deletedFilesList->first;

	while (deletedFileListNode != NULL) {
		DeletedFileListNode* tmp = deletedFileListNode;
		deletedFileListNode = deletedFileListNode->next;
		free(tmp->path);
		free(tmp);
	}
	freeDataRunsList(deletedFilesList->dataRunsListOfDataAttributOfMFT);
	free(deletedFilesList);
}


int main() {
	//get the size of sector and cluster in the drive
	if (GetDiskFreeSpaceAWrapper("\\\\.\\D:\\", (LPDWORD)&g_lSectorsPerCluster, (LPDWORD)&g_lBytesPerSector, "Couldn't retrieve information about D: drive") == 0) {
		return 1;
	}

	g_lBytesPerCluster = g_lSectorsPerCluster * g_lBytesPerSector;
	//printf("Sector size is %ld bytes.\r\n", g_lBytesPerSector);
	//printf("Cluster is %ld sectors = %ld bytes.\r\n\r\n", g_lSectorsPerCluster, g_lBytesPerCluster);

	printf("[*] Opening D: drive ...\r\n");
	hDrive = CreateFileWWrapper(L"\\\\.\\D:", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM, "Couldn't open D: drive");
	if (hDrive == INVALID_HANDLE_VALUE) {
		return 1;
	}

	if (moveDrivePointerToMft() == 0) {
		return 1;
	}

	printf("[*] Looking for deleted files ...\r\n");
	DeletedFilesList* deletedFilesList = listAllDeletedFiles();

	wchar_t* absolutePathToDeletedFile = calloc(1024, 2);
	printf("Enter the absolute path of the deleted file to recover: ");
	_getws_s(absolutePathToDeletedFile, 1024);

	char* pathToFolderOfRecoveredFile = calloc(1024, 1);
	printf("Enter the path of the folder of the new recovered file: ");
	gets_s(pathToFolderOfRecoveredFile, 1024);


	printf("[*] Searching the file ...\r\n");
	BOOL found = FALSE;
	DeletedFileListNode* deletedFileListNode = deletedFilesList->first;
	while (deletedFileListNode != NULL) {
		if (wcscmp(absolutePathToDeletedFile, deletedFileListNode->path) == 0) {
			byte* mftEntryBuffer = getMftEntryBufferOfIndex(deletedFilesList->dataRunsListOfDataAttributOfMFT, deletedFileListNode->mftEntryIndex);
			recoverFileFromMftEntry(mftEntryBuffer, pathToFolderOfRecoveredFile);
			free(mftEntryBuffer);
			found = TRUE;
			break;
		}
		deletedFileListNode = deletedFileListNode->next;
	}

	if (found) {
		printf("[*] The file has been recovered successfully!\r\n");
	}
	else {
		printf("[!] The file isn't in the MFT\r\n");
	}

	free(absolutePathToDeletedFile);
	free(pathToFolderOfRecoveredFile);
	freeDeletedFilesList(deletedFilesList);

	printf("[*] Closing D: drive handle...\r\n");
	if (CloseHandleWrapper(hDrive, "Couldn't close the handle of D: drive") == 0) {
		return 1;
	}

	return 0;
}