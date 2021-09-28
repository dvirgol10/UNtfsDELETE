#include "recover.h"

extern const DWORD g_BYTES_PER_ATTRIBUTE_HEADER;

extern const uint32_t g_DATA_ATTRIBUTE_TYPECODE;

extern DWORD g_lBytesPerCluster;
extern HANDLE hDrive;

//convert an ASCII string to wide character string
wchar_t* strToWcs(char* str) {
	wchar_t* wideCharacterString = (wchar_t*) calloc(strlen(str) + 1, 2);
	for (int i = 0; i < strlen(str); i++) {
		((byte*)wideCharacterString)[i * 2] = str[i];
		((byte*)wideCharacterString)[i * 2 + 1] = '\0';
	}
	return wideCharacterString;
}


//check if the path specifies a directory
BOOL isDirectory(char* path) {
	DWORD fileAttributes = GetFileAttributesA(path);
	return (fileAttributes != INVALID_FILE_ATTRIBUTES) && (fileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}


//return the path of the new recovered file
wchar_t* getNewRecoveredFilePath(byte* mftEntryBuffer, char* pathToDirectoryOfNewRecoveredFile) {
	wchar_t* pathToDirectoryOfNewRecoveredFileWCS = strToWcs(pathToDirectoryOfNewRecoveredFile);
	wchar_t newRecoveredFilePrefix[] = L"UNtfsDELETE_recovered_";
	wchar_t* fileName = getName(mftEntryBuffer);
	

	size_t newRecoveredFilePathLength = wcslen(pathToDirectoryOfNewRecoveredFileWCS) + wcslen(newRecoveredFilePrefix) + wcslen(fileName) + 1;
	wchar_t* newRecoveredFilePath = malloc(newRecoveredFilePathLength * 2); //multiply by 2 because those are wide characters
	
	wcscpy_s(newRecoveredFilePath, newRecoveredFilePathLength, pathToDirectoryOfNewRecoveredFileWCS);
	wcscat_s(newRecoveredFilePath, newRecoveredFilePathLength, newRecoveredFilePrefix);
	wcscat_s(newRecoveredFilePath, newRecoveredFilePathLength, fileName);
	
	free(pathToDirectoryOfNewRecoveredFileWCS);
	free(fileName);

	return newRecoveredFilePath;
}


LONGLONG getCurrentFilePointerLocation(HANDLE hFile, char* errorMessage) {
	LARGE_INTEGER liDistanceToMove = { 0 };
	liDistanceToMove.QuadPart = 0;
	LARGE_INTEGER lNewFilePointer = { 0 };
	if (SetFilePointerEx(hFile, liDistanceToMove, (PLARGE_INTEGER)&lNewFilePointer, FILE_CURRENT) == 0) {
		printErrorMessage(errorMessage, "SetFilePointerEx");
		return -1;
	}
	return lNewFilePointer.QuadPart;
}


void recoverFileFromMftEntry(byte* mftEntryBuffer, char* pathToDirectoryOfNewRecoveredFile) {
	if (!isDirectory(pathToDirectoryOfNewRecoveredFile)) {
		printf("[!] \"%s\" is not a valid directory!\r\n", pathToDirectoryOfNewRecoveredFile);
		return;
	}

	wchar_t* newRecoveredFilePath = getNewRecoveredFilePath(mftEntryBuffer, pathToDirectoryOfNewRecoveredFile); //the path of the new recovered file

	HANDLE hNewRecoverdFile = CreateFileWWrapper(newRecoveredFilePath, FILE_GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, "Couldn't create the recovered file");
	free(newRecoveredFilePath);
	if (hNewRecoverdFile == INVALID_HANDLE_VALUE) {
		return;
	}

	uint16_t dataAttributeHeaderOffset = findAttributeHeaderOffset(mftEntryBuffer, g_DATA_ATTRIBUTE_TYPECODE);
	if (dataAttributeHeaderOffset == 0) { //if there is no $DATA attribute in the MFT entry
		return;
	}

	if (isResident(mftEntryBuffer, dataAttributeHeaderOffset)) {
		uint16_t dataAttributResidentDataOffset = dataAttributeHeaderOffset + g_BYTES_PER_ATTRIBUTE_HEADER; //arrive to the attribute non-resident data
		
		uint32_t dataSize = 0;
		memcpy(&dataSize, mftEntryBuffer + dataAttributResidentDataOffset, 4);
		uint16_t dataOffset = 0;
		memcpy(&dataOffset, mftEntryBuffer + dataAttributResidentDataOffset + 4, 2);

		WriteFileWrapper(hNewRecoverdFile, mftEntryBuffer + dataAttributeHeaderOffset + dataOffset, dataSize, "Couldn't write the data of the deleted file");
	}
	else {
		LONGLONG currentFilePointerLocation = getCurrentFilePointerLocation(hDrive, "Couldn't get the current file pointer location"); //we need the current file pointer location in order to set it back in the end of the recovering
		
		uint64_t validDataSize = getFileValidDataSize(mftEntryBuffer);
		DWORD numberOfBytesToWrite = g_lBytesPerCluster;
		byte* dataBuffer = malloc(g_lBytesPerCluster);

		DataRunsList* dataRunsListOfDataAttributOfMFT = parseDataRuns(mftEntryBuffer, dataAttributeHeaderOffset);
		
		uint64_t numberOfWrittenBytes = 0;
		DataRunListNode* dataRunListNode = dataRunsListOfDataAttributOfMFT->first;
		//move through the data runs 
		while (dataRunListNode != NULL) {
			uint64_t startingAddressOfCurrentDataRun = dataRunListNode->numberOfStartingCluster * g_lBytesPerCluster; //calculate the starting address of the corresponding data run
			if (SetFilePointerExWrapper(hDrive, startingAddressOfCurrentDataRun, "Couldn't jump to the location of the data run of the deleted file") == 0) { //set the file pointer at the start of the current data "block"
				return 0;
			}

			for (int i = 0; i < dataRunListNode->numberOfClusters; i++) {
				if (numberOfWrittenBytes + numberOfBytesToWrite > validDataSize) { //there is a padding with null bytes at the end of the valid data, so we need to avoid it
					numberOfBytesToWrite = validDataSize - numberOfWrittenBytes;
				}
				ReadFileWrapper(hDrive, dataBuffer, g_lBytesPerCluster, "Couldn't read MFT entry");  //read from deleted file (we have to read a multiple of the amount of bytes per sector, and g_lBytesPerCluster is such a multiple)
				WriteFileWrapper(hNewRecoverdFile, dataBuffer, numberOfBytesToWrite, "Couldn't write to the new recovred file"); //write to the new recovered file
				numberOfWrittenBytes += numberOfBytesToWrite;
			}

			dataRunListNode = dataRunListNode->next; //advance in the linked list
		}
		free(dataBuffer);
		freeDataRunsList(dataRunsListOfDataAttributOfMFT);
		SetFilePointerExWrapper(hDrive, currentFilePointerLocation, "Couldn't set back the file pointer location");
	}

	CloseHandleWrapper(hNewRecoverdFile, "Couldn't close the handle of the new recovered file");
}