#include "main.h"

#include "win32apiWrapper.h"
#include "mftAttributes.h"
#include "dataRunsParser.h"

#include <stdio.h>


extern const DWORD g_BYTES_PER_ATTRIBUTE_HEADER;
extern const DWORD g_BYTES_PER_ATTRIBUTE_RESIDENT_DATA;

extern const uint32_t g_STANDARD_INFORMATION_ATTRIBUTE_TYPECODE;
extern const uint32_t g_FILE_NAME_ATTRIBUTE_TYPECODE;
extern const uint32_t g_DATA_ATTRIBUTE_TYPECODE;


//move the file pointer of the C: volume to the location of the starting sector of the MFT.
//return 1 if everything is good, otherwise return 0
int MoveVolumePointerToMFT(HANDLE hVolume) {
	printf("[*] Reading the VBR ...\r\n");
	byte* lpBuffer = malloc(g_lBytesPerSector);
	if (ReadFileWrapper(hVolume, lpBuffer, g_lBytesPerSector, "Couldn't read the VBR") == 0) { //when reading from a disk drive, the maximum number of bytes to be read has to be a multiple of sector size
		return 0;
	}

	g_lBytesPerMFTEntry = 1 << (-(signed char)(lpBuffer[0x40])); //calculate the size in bytes of each MFT entry
	printf("\r\nAn MFT entry size is %ld bytes.\r\n\r\n", g_lBytesPerMFTEntry);

	printf("[*] Jumping to the location of the starting sector of the MFT"); 
	//calculate the first MFT cluster number, then multiply it by cluster size in bytes
	LONGLONG QuadPart = 0;
	memcpy(&QuadPart, lpBuffer + 0x30, 8);
	QuadPart *= g_lBytesPerCluster;
	free(lpBuffer);
	LARGE_INTEGER liDistanceToMove = { 0 };
	liDistanceToMove.QuadPart = QuadPart;
	printf(" in address 0x%016X ...\r\n", (int)liDistanceToMove.QuadPart);

	LARGE_INTEGER lNewFilePointer = { 0 };
	if (SetFilePointerExWrapper(hVolume, liDistanceToMove, (PLARGE_INTEGER) &lNewFilePointer, FILE_BEGIN, "Couldn't jump to the location of the starting sector of the MFT") == 0) {
		return 0;
	}
	if (lNewFilePointer.QuadPart != liDistanceToMove.QuadPart) {
		printf("[!] The new file pointer is of address 0x%016X, which is not the desired address.\r\n", (int)lNewFilePointer.QuadPart);
		return 0;
	}

	return 1;
}


//check the signature of the MFT entry to determine whether it is a valid entry or not (0x46494c45 = "FILE")
BOOL isValidMFTEntry(byte* mftEntryBuffer) {
	return mftEntryBuffer[0x0] == 0x46 && mftEntryBuffer[0x1] == 0x49 && mftEntryBuffer[0x2] == 0x4c && mftEntryBuffer[0x3] == 0x45; 
}


//return TRUE iff the MFT entry is of deleted file
BOOL isDeletedFile(byte* mftEntryBuffer) {
	return mftEntryBuffer[0x16] == 0x00; //check the entry flags. 0x00 means deleted file
}


BOOL isSystemFile(byte* mftEntryBuffer) {
	uint16_t standardInformationAttributeHeaderOffset = findAttributeHeaderOffset(mftEntryBuffer, g_STANDARD_INFORMATION_ATTRIBUTE_TYPECODE);
	if (standardInformationAttributeHeaderOffset == 0) { //if there is no $STANDARD_INFORMATION attribute in the MFT entry
		return FALSE;
	}

	if (isResident(mftEntryBuffer, standardInformationAttributeHeaderOffset)) { //if the actual attribute is resident
		uint16_t standardInformationAttributeOffset = standardInformationAttributeHeaderOffset + g_BYTES_PER_ATTRIBUTE_HEADER + g_BYTES_PER_ATTRIBUTE_RESIDENT_DATA;
		uint32_t fileAttributeFlags = 0;
		memcpy(&fileAttributeFlags, mftEntryBuffer + standardInformationAttributeOffset + 0x20, 4);
		return fileAttributeFlags & 0x00000004; //0x00000004 is the value for FILE_ATTRIBUTE_SYSTEM flag
	}
	else {
		//to be continued...
	}
	return FALSE;
}


//TODO: deal also with long names (a MFT entry with multiple $FILE_NAME attributes)
wchar_t* getName(byte* mftEntryBuffer) {
	uint16_t fileNameAttributeHeaderOffset = findAttributeHeaderOffset(mftEntryBuffer, g_FILE_NAME_ATTRIBUTE_TYPECODE);
	if (fileNameAttributeHeaderOffset == 0) { //if there is no $FILE_NAME attribute in the MFT entry
		return 0;
	}

	if (isResident(mftEntryBuffer, fileNameAttributeHeaderOffset)) { //if the actual attribute is resident
		uint16_t fileNameAttributeOffset = fileNameAttributeHeaderOffset + g_BYTES_PER_ATTRIBUTE_HEADER + g_BYTES_PER_ATTRIBUTE_RESIDENT_DATA;
		uint8_t nameStringSize = mftEntryBuffer[fileNameAttributeOffset + 0x40]; //retrieve the amount of wide characters in the name
		wchar_t* name = calloc(nameStringSize + 1, 2); //add 1 to nameStringSize for the null terminator, each wide character consists of two bytes
		memcpy(name, mftEntryBuffer + fileNameAttributeOffset + 0x42, nameStringSize * 2);
		return name;
	}
	else {
		//to be continued...
	}
	return 0;
}


uint64_t getFileSize(byte* mftEntryBuffer) {
	uint16_t dataAttributeHeaderOffset = findAttributeHeaderOffset(mftEntryBuffer, g_DATA_ATTRIBUTE_TYPECODE);
	if (dataAttributeHeaderOffset == 0) { //if there is no $DATA attribute in the MFT entry
		return 0; 
	}

	uint64_t fileSize = 0;
	if (isResident(mftEntryBuffer, dataAttributeHeaderOffset)) { //if the actual attribute is resident
		printf("[!] Error: The $MFT file has to be non-resident. Something is wrong!"); //the $DATA attribute of $MFT contains the content of the $MFT file, which is bigger than a single MFT entry
	}
	else { //if the actual attribute is non-resident flag is set (meaning NONRESIDENT_FORM)
		uint16_t dataAttributeNonResidentDataOffset = dataAttributeHeaderOffset + g_BYTES_PER_ATTRIBUTE_HEADER; //arrive to the attribute non-resident data
		memcpy(&fileSize, mftEntryBuffer + dataAttributeNonResidentDataOffset + 0x20, 8);
	}
	return fileSize;
}


void printAllDeletedFiles(HANDLE hVolume, BOOL includeSystemFiles) {
	byte* mftEntryBuffer = malloc(g_lBytesPerMFTEntry);
	if (ReadFileWrapper(hVolume, mftEntryBuffer, g_lBytesPerMFTEntry, "Couldn't read MFT entry") == 0) { //read the first entry of the MFT, which is the entry for $MFT
		return;
	}

	uint64_t mftFileSize = getFileSize(mftEntryBuffer); //get the size of the $MFT file
	uint64_t amountOfMFTEntries = mftFileSize / g_lBytesPerMFTEntry; //calculate the size of the $MFT file
	printf("The size of the $MFT file is %llu bytes, which means it has %llu entries.\r\n", mftFileSize, amountOfMFTEntries);

	//---------------------------
	printf("Data Run of $DATA attribute of %MFT:\r\n");
	DataRunsList* dataRunsListOfDataAttributOfMFT = parseDataRuns(mftEntryBuffer, findAttributeHeaderOffset(mftEntryBuffer, g_DATA_ATTRIBUTE_TYPECODE));
	DataRunListNode* dataRunListNode = dataRunsListOfDataAttributOfMFT->first;
	while (dataRunListNode != NULL) {
		printf("0x%X clusters start in cluster number 0x%016llX\r\n", dataRunListNode->numberOfClusters, dataRunListNode->numberOfStartingCluster);
		dataRunListNode = dataRunListNode->next;

	}
	//---------------------------

	int mftEntryNumber = 1; //MFT entry number zero is the entry of $MFT

	do {
		if (ReadFileWrapper(hVolume, mftEntryBuffer, g_lBytesPerMFTEntry, "Couldn't read MFT entry") == 0) {
			return;
		}
		
		if (isValidMFTEntry(mftEntryBuffer)) {
			//wchar_t* name = getName(mftEntryBuffer);
			//if (name != 0) {
			//	printf("%d: \"%ws\"\r\n", mftEntryNumber, name);
			//	}
			//	free(name);
			//}
		
			if (isDeletedFile(mftEntryBuffer)) {
				if (includeSystemFiles || !isSystemFile(mftEntryBuffer)) {
					wchar_t* deletedFileName = getName(mftEntryBuffer);
					if (deletedFileName != 0) { //if valid file name
						printf("MFT entry number %d (offset 0x%08x) of file \"%ws\" is marked as deleted.\r\n", mftEntryNumber, mftEntryNumber * g_lBytesPerMFTEntry, deletedFileName);
						free(deletedFileName);
					}
				}
			}
		}

		++mftEntryNumber;
	} while (mftEntryNumber < amountOfMFTEntries);

	free(mftEntryBuffer);
}


int main() {
	//get the size of sector and cluster in c: volume
	if (GetDiskFreeSpaceAWrapper("\\\\.\\C:\\", (LPDWORD)&g_lSectorsPerCluster, (LPDWORD)&g_lBytesPerSector, "Couldn't retrieve information about C: volume") == 0) {
		return 1;
	}

	g_lBytesPerCluster = g_lSectorsPerCluster * g_lBytesPerSector;
	printf("Sector size is %ld bytes.\r\n", g_lBytesPerSector);
	printf("Cluster is %ld sectors = %ld bytes.\r\n\r\n", g_lSectorsPerCluster, g_lBytesPerCluster);

	printf("[*] Opening C: volume ...\r\n");
	HANDLE hVolume = CreateFileWWrapper(L"\\\\.\\C:", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM, "Couldn't open C: volume");
	if (hVolume == INVALID_HANDLE_VALUE) {
		return 1;
	}

	if (MoveVolumePointerToMFT(hVolume) == 0) {
		return 1;
	}

	printAllDeletedFiles(hVolume, FALSE);

	printf("[*] Closing C: volume handle...\r\n");
	if (CloseHandleWrapper(hVolume, "Couldn't close the handle of C: volume") == 0) {
		return 1;
	}

	return 0;
}