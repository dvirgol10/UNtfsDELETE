#include <Windows.h>
#include <stdio.h>
#include <fileapi.h>
#include <errhandlingapi.h>
#include <stdint.h>

const DWORD g_lBytesPerMFTAttributeHeader = 16;
const DWORD g_lBytesPerAttributeResidentData = 8;
const DWORD g_lBytesPerAttributeNonResidentData = 48; //or 56, we need to find out individully for each attribute

DWORD g_lSectorsPerCluster = 0;
DWORD g_lBytesPerSector = 0;
DWORD g_lBytesPerCluster = 0;
DWORD g_lBytesPerMFTEntry = 0;



//move the file pointer of the C: volume to the location of the starting sector of the MFT.
//return 0 if everything is good, otherwise return 1 (for compatability with main)
int MoveVolumePointerToMFT(HANDLE hVolume) {
	printf("[*] Reading the VBR ...\r\n");
	byte* lpBuffer = malloc(g_lBytesPerSector);
	if (ReadFile(hVolume, lpBuffer, g_lBytesPerSector, NULL, NULL) == 0) { //when reading from a disk drive, the maximum number of bytes to be read has to be a multiple of sector size
		printf("[!] Couldn't read the VBR (ReadFile): 0x%x\r\n", GetLastError());
		return 1;
	}

	g_lBytesPerMFTEntry = 1 << (-(signed char)(lpBuffer[0x40])); //calculate the size in bytes of each MFT entry
	printf("\r\n\An MFT entry size is %ld bytes.\r\n\r\n", g_lBytesPerMFTEntry);

	printf("[*] Jumping to the location of the starting sector of the MFT"); 
	//calculate the first MFT cluster, then multiply it by cluster size in bytes
	LONGLONG QuadPart = 0;
	memcpy(&QuadPart, lpBuffer + 0x30, 8);
	QuadPart *= g_lBytesPerCluster;
	free(lpBuffer);
	LARGE_INTEGER liDistanceToMove;
	liDistanceToMove.QuadPart = QuadPart;
	printf(" in address 0x%016X ...\r\n", (int)liDistanceToMove.QuadPart);

	LARGE_INTEGER lNewFilePointer;
	if (SetFilePointerEx(hVolume, liDistanceToMove, &lNewFilePointer, FILE_BEGIN) == 0) {
		printf("[!] Couldn't jump to the location of the starting sector of the MFT (SetFilePointerEx): 0x%x\r\n", GetLastError());
		return 1;
	}
	if (lNewFilePointer.QuadPart != liDistanceToMove.QuadPart) {
		printf("[!] The new file pointer is of address 0x%016X, which is not the desired address.\r\n", (int)lNewFilePointer.QuadPart);
		return 1;
	}

	return 0;
}


uint16_t findAttributeHeaderOffset(byte* mftEntryBuffer, uint32_t attributeType) {
	uint16_t attributeHeaderOffset;
	memcpy(&attributeHeaderOffset, mftEntryBuffer + 0x14, 2); //retrieve the first attribute header offset from the MFT entry header

	uint32_t currentAttributeType;
	memcpy(&currentAttributeType, mftEntryBuffer + attributeHeaderOffset, 4);
	
	uint32_t attributeSize;
	while (currentAttributeType != attributeType && currentAttributeType != 0xFFFFFFFF) { //0xFFFFFFFF is end-of-attributes marker
		memcpy(&attributeSize, mftEntryBuffer + attributeHeaderOffset + 4, 4);
		attributeHeaderOffset += attributeSize; //move to the next attribute
		memcpy(&currentAttributeType, mftEntryBuffer + attributeHeaderOffset, 4);
	}
	if (currentAttributeType == 0xFFFFFFFF) { //if there is no attribute of attributeType
		return 0;
	}
	return attributeHeaderOffset;
}


uint16_t findFileNameAttribute(byte* mftEntryBuffer) {
	return findAttributeHeaderOffset(mftEntryBuffer, 0x00000030);  //0x00000030 is the attribute type value of $FILE_NAME attribute
}


uint16_t findDataAttribute(byte* mftEntryBuffer) {
	return findAttributeHeaderOffset(mftEntryBuffer, 0x00000080); //0x00000080 is the attribute type value of $DATA attribute
}


uint16_t findStandardInformationAttribute(byte* mftEntryBuffer) {
	return findAttributeHeaderOffset(mftEntryBuffer, 0x00000010); //0x00000010 is the attribute type value of $STANDARD_INFORMATION attribute
}


//return TRUE iff the attribute corresponds to attributeHeaderOffset is resident
BOOL isResident(byte* mftEntryBuffer, uint16_t attributeHeaderOffset) {
	return mftEntryBuffer[attributeHeaderOffset + 0x8] == 0x00; //check if the actual attribute non-resident flag is not set (meaning RESIDENT_FORM)
}


//return TRUE iff the MFT entry is of deleted file
BOOL isDeletedFile(byte* mftEntryBuffer) {
	return mftEntryBuffer[0x16] == 0x00; //check the entry flags. 0x00 means deleted file
}


BOOL isSystemFile(byte* mftEntryBuffer) {
	uint16_t standardInformationAttributeHeaderOffset = findStandardInformationAttribute(mftEntryBuffer);
	if (standardInformationAttributeHeaderOffset == 0) { //if there is no $STANDARD_INFORMATION attribute in the MFT entry
		return FALSE;
	}


	if (isResident(mftEntryBuffer, standardInformationAttributeHeaderOffset)) { //if the actual attribute is resident
		uint16_t standardInformationAttributeOffset = standardInformationAttributeHeaderOffset + g_lBytesPerMFTAttributeHeader + g_lBytesPerAttributeResidentData;
		uint32_t fileAttributeFlags;
		memcpy(&fileAttributeFlags, mftEntryBuffer + standardInformationAttributeOffset + 0x20, 4);
		return fileAttributeFlags & 0x00000004; //0x00000004 is the value for FILE_ATTRIBUTE_SYSTEM flag
	}
	else {
		//to be continued...
	}
	return FALSE;
}

wchar_t* getName(byte* mftEntryBuffer) {
	uint16_t fileNameAttributeHeaderOffset = findFileNameAttribute(mftEntryBuffer);
	if (fileNameAttributeHeaderOffset == 0) { //if there is no $FILE_NAME attribute in the MFT entry
		return 0;
	}

	if (isResident(mftEntryBuffer, fileNameAttributeHeaderOffset)) { //if the actual attribute is resident
		uint16_t fileNameAttributeOffset = fileNameAttributeHeaderOffset + g_lBytesPerMFTAttributeHeader + g_lBytesPerAttributeResidentData;
		uint64_t nameStringSize = (mftEntryBuffer[fileNameAttributeOffset + 0x40]); //retrieve the amount of wide characters in the name
		wchar_t* name = calloc(nameStringSize + 1, 2); //add 1 to nameStringSize for the null terminator
		memcpy(name, mftEntryBuffer + fileNameAttributeOffset + 0x42, nameStringSize * 2);
		return name;
	}
	else {
		//to be continued...
	}
	return 0;
}

uint64_t getFileSize(byte* mftEntryBuffer) {
	uint16_t dataAttributeHeaderOffset = findDataAttribute(mftEntryBuffer);
	if (dataAttributeHeaderOffset == -1) { //if there is no $DATA attribute in the MFT entry
		return 0; 
	}

	uint64_t fileSize = 0;
	if (isResident(mftEntryBuffer, dataAttributeHeaderOffset)) { //if the actual attribute is resident
		printf("[!] Error: The $MFT file has to be non-resident. Something is wrong!"); //the $DATA attribute of $MFT contains the content of the $MFT file, which is bigger than a single MFT entry
	}
	else { //if the actual attribute is non-resident flag is set (meaning NONRESIDENT_FORM)
		uint16_t dataAttributeNonResidentDataOffset = dataAttributeHeaderOffset + g_lBytesPerMFTAttributeHeader; //arrive to the attribute non-resident data
		memcpy(&fileSize, mftEntryBuffer + dataAttributeNonResidentDataOffset + 0x20, 8);
	}
	return fileSize;
}


void printAllDeletedFiles(HANDLE hVolume, BOOL includeSystemFiles) {
	byte* mftEntryBuffer = malloc(g_lBytesPerMFTEntry);
	if (ReadFile(hVolume, mftEntryBuffer, g_lBytesPerMFTEntry, NULL, NULL) == 0) { //read the first entry of the MFT, which is the entry for $MFT
		printf("[!] Couldn't read MFT entry (ReadFile): 0x%x\r\n", GetLastError());
	}

	uint64_t mftFileSize = getFileSize(mftEntryBuffer); //get the size of the $MFT file
	uint64_t amountOfMFTEntries = mftFileSize / g_lBytesPerMFTEntry; //calculate the size of the $MFT file
	printf("The size of the $MFT file is %d bytes, which means it has %d entries.\r\n", mftFileSize, amountOfMFTEntries);

	int mftEntryNumber = 1; //MFT entry number zero is the entry of $MFT

	do {
		if (ReadFile(hVolume, mftEntryBuffer, g_lBytesPerMFTEntry, NULL, NULL) == 0) { //read the next MFT entry
		printf("[!] Couldn't read MFT entry (ReadFile): 0x%x\r\n", GetLastError());
		}

		if (mftEntryBuffer[0x0] == 0x46 && mftEntryBuffer[0x1] == 0x49 && mftEntryBuffer[0x2] == 0x4c && mftEntryBuffer[0x3] == 0x45) { //check the signature of the MFT entry to determine whether it is a valid entry or not (0x46494c45 = "FILE")
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
	DWORD junk = 0;
	if (GetDiskFreeSpaceA("\\\\.\\C:\\", (LPDWORD)&g_lSectorsPerCluster, (LPDWORD)&g_lBytesPerSector, (LPDWORD)&junk, (LPDWORD)&junk) == 0) {
		printf("[!] Couldn't retrieve information about C: volume (GetDiskFreeSpaceA): 0x%x\r\n", GetLastError());
	}

	g_lBytesPerCluster = g_lSectorsPerCluster * g_lBytesPerSector;
	printf("Sector size is %ld bytes.\r\n", g_lBytesPerSector);
	printf("Cluster is %ld sectors = %ld bytes.\r\n\r\n", g_lSectorsPerCluster, g_lBytesPerCluster);



	printf("[*] Opening C: volume ...\r\n");
	HANDLE hVolume = CreateFileW(L"\\\\.\\C:", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM, NULL);
	if (hVolume == INVALID_HANDLE_VALUE) {
		printf("[!] Couldn't open C: volume (CreateFileW): 0x%x\r\n", GetLastError());
		return 1;
	}


	if (MoveVolumePointerToMFT(hVolume) != 0) {
		return 1;
	}


	printAllDeletedFiles(hVolume, FALSE);



	printf("[*] Closing C: volume handle...\r\n");
	if (CloseHandle(hVolume) == 0) {
		printf("[!] Couldn't close the handle of C: volume (CloseHandle): 0x%x\r\n", GetLastError());
		return 1;
	}

	return 0;
}