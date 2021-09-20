#include <Windows.h>
#include <stdio.h>
#include <fileapi.h>
#include <errhandlingapi.h>

DWORD glSectorsPerCluster = 0;
DWORD glBytesPerSector = 0;
DWORD glBytesPerCluster = 0;


//move the file pointer of the C: volume to the location of the starting sector of the MFT.
//return 0 if everything is good, otherwise return 1 (for compatability with main)
int MoveVolumePointerToMFT(HANDLE hVolume) {
	printf("[*] Reading the VBR ...\r\n");
	byte* lpBuffer = malloc(glBytesPerSector);
	if (ReadFile(hVolume, lpBuffer, glBytesPerSector, NULL, NULL) == 0) { //when reading from a disk drive, the maximum number of bytes to be read has to be a multiple of sector size
		printf("[!] Couldn't read the VBR (ReadFile): 0x%x\r\n", GetLastError());
		return 2;
	}

	printf("[*] Jumping to the location of the starting sector of the MFT");
	//calculate the first MFT cluster, then multiply it by cluster size in bytes
	LONGLONG QuadPart = 0;
	QuadPart += lpBuffer[0x30];
	QuadPart += (lpBuffer[0x31] << 8);
	QuadPart += (lpBuffer[0x32] << 16);
	QuadPart += (lpBuffer[0x33] << 24);
	QuadPart += (lpBuffer[0x34] << 32);
	QuadPart += (lpBuffer[0x35] << 40);
	QuadPart += (lpBuffer[0x36] << 48);
	QuadPart += (lpBuffer[0x37] << 56);
	QuadPart *= glBytesPerCluster;
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


int main() {

	//get the size of sector and cluster in c: volume
	DWORD junk = 0;
	if (GetDiskFreeSpaceA("\\\\.\\C:\\", (LPDWORD) &glSectorsPerCluster, (LPDWORD) &glBytesPerSector, (LPDWORD) &junk, (LPDWORD) &junk) == 0) {
		printf("[!] Couldn't retrieve information about C: volume (GetDiskFreeSpaceA): 0x%x\r\n", GetLastError());
	}

	glBytesPerCluster = glSectorsPerCluster * glBytesPerSector;
	printf("Sector size is %ld bytes.\r\n", glBytesPerSector);
	printf("Cluster is %ld sectors = %ld bytes.\r\n\r\n", glSectorsPerCluster, glBytesPerCluster);



	printf("[*] Opening C: volume ...\r\n");
	HANDLE hVolume = CreateFileW(L"\\\\.\\C:", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM, NULL);
	if (hVolume == INVALID_HANDLE_VALUE) {
		printf("[!] Couldn't open C: volume (CreateFileW): 0x%x\r\n", GetLastError());
		return 1;
	}


	if (MoveVolumePointerToMFT(hVolume) != 0) {
		return 1;
	}


	printf("[*] Closing C: volume handle...\r\n");
	if (CloseHandle(hVolume) == 0) {
		printf("[!] Couldn't close the handle of C: volume (CloseHandle): 0x%x\r\n", GetLastError());
		return 1;
	}

	return 0;
}
