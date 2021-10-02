# UNtfsDELETE
UNtfsDELETE is a tool that allows undeleting NTFS files.

## How It Works
In NTFS, the MFT (Master File Table) consists of an array of MFT entries,
which store information about the file in the file system.
Each MFT entry has flags indicating the type of the file.
The value `0x0000` indicates that the file is deleted (permanently deleted, not stored in the Recycle Bin).
Thus, we can get a list of all the permanently deleted files that are still stored in the MFT
(the value `0x0000` of the MFT entry flag tells the file system that its corresponding entry isn't in use and is free for overwriting,
so the existence of an MFT entry of deleted file isn't guaranteed).  
The actual recovery is of the $DATA attribute of the deleted file, which stores the file data.

## How To Use It
First, we need to run [undelete.exe](undelete/x64/Debug/undelete.exe) as administrator in order to be able to open the desired volume.
After it finishes scanning the MFT and finds the deleted files that are still in the MFT, we need to type the absolute path of the deleted file
(for example, `D:\example_dir\example.txt`) and the desired path of the folder of the new recovered file (for example, `D:\example_recovery\`).
Then, if the deleted file (`example.txt`) is still in the MFT, a new file with its data will be created (`D:\example_recovery\UNtfsDELETE_recovered_example.txt`).

## Demonstration

In the screenshots below we can see that after permanently deleting the file `password.txt`, we recovered its data to a new file named `UNtfsDELETE_recovered_password.txt`.

### Original File:
![img](Demonstration/Original%20file.png "Original File")

### [undelete.exe](undelete/x64/Debug/undelete.exe)'s Run:
![img](Demonstration/undelete.exe's%20run.png "undelete.exe's Run")

### Recovered File:
![img](Demonstration/Recovered%20file.png "Recovered File")