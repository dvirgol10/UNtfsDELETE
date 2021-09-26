#ifndef MFT_ATTRIBUTES_H
#define MFT_ATTRIBUTES_H

#include <Windows.h>
#include <stdint.h>


uint16_t findAttributeHeaderOffset(byte* mftEntryBuffer, uint32_t attributeType);
BOOL isResident(byte* mftEntryBuffer, uint16_t attributeHeaderOffset);
uint32_t getFileAttributeFlags(byte* mftEntryBuffer);

#endif