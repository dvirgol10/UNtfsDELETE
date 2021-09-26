#include "main.h"

#include "win32apiWrapper.h"
#include "mftAttributes.h"
#include "dataRunsParser.h"
#include "recover.h"

#include <stdio.h>


extern const DWORD g_BYTES_PER_ATTRIBUTE_HEADER;
extern const DWORD g_BYTES_PER_ATTRIBUTE_RESIDENT_DATA;

extern const uint32_t g_STANDARD_INFORMATION_ATTRIBUTE_TYPECODE;
extern const uint32_t g_FILE_NAME_ATTRIBUTE_TYPECODE;
extern const uint32_t g_DATA_ATTRIBUTE_TYPECODE;


//move the file pointer of the drive to the location of the starting sector of the MFT.
//return 1 if everything is good, otherwise return 0
int MoveDrivePointerToMFT(HANDLE hDrive) {
	printf("[*] Reading the VBR ...\r\n");
	byte* lpBuffer = malloc(g_lBytesPerSector);
	if (ReadFileWrapper(hDrive, lpBuffer, g_lBytesPerSector, "Couldn't read the VBR") == 0) { //when reading from a disk drive, the maximum number of bytes to be read has to be a multiple of sector size
		return 0;
	}

	g_lBytesPerMFTEntry = 1 << (-(signed char)(lpBuffer[0x40])); //calculate the size in bytes of each MFT entry
	printf("\r\nAn MFT entry size is %ld bytes.\r\n\r\n", g_lBytesPerMFTEntry);

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
BOOL isValidMFTEntry(byte* mftEntryBuffer) {
	return mftEntryBuffer[0x0] == 0x46 && mftEntryBuffer[0x1] == 0x49 && mftEntryBuffer[0x2] == 0x4c && mftEntryBuffer[0x3] == 0x45; 
}


//return TRUE iff the MFT entry is of deleted file
BOOL isDeletedFile(byte* mftEntryBuffer) {
	return mftEntryBuffer[0x16] == 0x00; //check the entry flags. 0x00 means deleted file
}


void printAllDeletedFiles(HANDLE hDrive) {
	byte* mftEntryBuffer = malloc(g_lBytesPerMFTEntry);
	if (ReadFileWrapper(hDrive, mftEntryBuffer, g_lBytesPerMFTEntry, "Couldn't read MFT entry") == 0) { //read the first entry of the MFT, which is the entry for $MFT
		return;
	}

	uint64_t mftFileSize = getFileValidDataSize(mftEntryBuffer); //get the size of the $MFT file
	uint64_t amountOfMFTEntries = mftFileSize / g_lBytesPerMFTEntry; //calculate the size of the $MFT file
	printf("The size of the $MFT file is %llu bytes, which means it has %llu entries.\r\n", mftFileSize, amountOfMFTEntries);

	int mftEntryNumber = 0; //MFT entry number zero is the entry of $MFT

	DataRunsList* dataRunsListOfDataAttributOfMFT = parseDataRuns(mftEntryBuffer, findAttributeHeaderOffset(mftEntryBuffer, g_DATA_ATTRIBUTE_TYPECODE));

	DataRunListNode* dataRunListNode = dataRunsListOfDataAttributOfMFT->first;
	//move through the data runs 
	while (dataRunListNode != NULL) {
		uint64_t startingAddressOfCurrentDataRun = dataRunListNode->numberOfStartingCluster * g_lBytesPerCluster; //calculate the starting address of the corresponding data run
		if (SetFilePointerExWrapper(hDrive, startingAddressOfCurrentDataRun, "Couldn't jump to the location of the data run of $MFT") == 0) {
			return 0;
		}

		for (int i = 0; i < dataRunListNode->numberOfClusters * (g_lBytesPerCluster / g_lBytesPerMFTEntry); i++) { //read all the MFT entries of the current data run

			if (mftEntryNumber < amountOfMFTEntries) {
				if (ReadFileWrapper(hDrive, mftEntryBuffer, g_lBytesPerMFTEntry, "Couldn't read MFT entry") == 0) {
					return;
				}

				if (isValidMFTEntry(mftEntryBuffer)) {
					if (isDeletedFile(mftEntryBuffer)) {
						wchar_t* deletedFileName = getName(mftEntryBuffer);
						if (deletedFileName != 0) { //if valid file name
							printf("MFT entry number %d (offset 0x%08x) of file \"%ws\" is marked as deleted.\r\n", mftEntryNumber, mftEntryNumber * g_lBytesPerMFTEntry, deletedFileName);
							free(deletedFileName);
						}
					}
				}

				++mftEntryNumber;
			}

		}

		dataRunListNode = dataRunListNode->next; //advance in the linked list
	}

	free(mftEntryBuffer);
}


int main() {
	//get the size of sector and cluster in the drive
	if (GetDiskFreeSpaceAWrapper("\\\\.\\D:\\", (LPDWORD)&g_lSectorsPerCluster, (LPDWORD)&g_lBytesPerSector, "Couldn't retrieve information about D: drive") == 0) {
		return 1;
	}

	g_lBytesPerCluster = g_lSectorsPerCluster * g_lBytesPerSector;
	printf("Sector size is %ld bytes.\r\n", g_lBytesPerSector);
	printf("Cluster is %ld sectors = %ld bytes.\r\n\r\n", g_lSectorsPerCluster, g_lBytesPerCluster);

	printf("[*] Opening D: drive ...\r\n");
	HANDLE hDrive = CreateFileWWrapper(L"\\\\.\\D:", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM, "Couldn't open D: drive");
	if (hDrive == INVALID_HANDLE_VALUE) {
		return 1;
	}

	if (MoveDrivePointerToMFT(hDrive) == 0) {
		return 1;
	}

	printAllDeletedFiles(hDrive);

	printf("[*] Closing D: drive handle...\r\n");
	if (CloseHandleWrapper(hDrive, "Couldn't close the handle of D: drive") == 0) {
		return 1;
	}

	return 0;
}