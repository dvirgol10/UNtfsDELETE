#include "mftAttributes.h"


extern const DWORD g_BYTES_PER_ATTRIBUTE_HEADER = 16;
extern const DWORD g_BYTES_PER_ATTRIBUTE_RESIDENT_DATA = 8;

extern const uint32_t g_STANDARD_INFORMATION_ATTRIBUTE_TYPECODE = 0x00000010;
extern const uint32_t g_FILE_NAME_ATTRIBUTE_TYPECODE = 0x00000030;
extern const uint32_t g_DATA_ATTRIBUTE_TYPECODE = 0x00000080;
extern const uint32_t g_END_OF_ATTRIBUTE_MARKER = 0xFFFFFFFF;


//return the offset (relative from the start of the start of the MFT entry) of the starting of the first attribute header that has type code attributeTypeCode
//if the is no attribute with type code attributeTypeCode in the MFT entry, return 0
uint16_t findAttributeHeaderOffset(byte* mftEntryBuffer, uint32_t attributeTypeCode) {
	uint16_t attributeHeaderOffset = 0;
	memcpy(&attributeHeaderOffset, mftEntryBuffer + 0x14, 2); //retrieve the first attribute header offset from the MFT entry header

	uint32_t currentAttributeTypeCode = 0;
	memcpy(&currentAttributeTypeCode, mftEntryBuffer + attributeHeaderOffset, 4);
	uint32_t attributeSize;
	while (currentAttributeTypeCode != attributeTypeCode && currentAttributeTypeCode != g_END_OF_ATTRIBUTE_MARKER) { //while the current attribute type code isn't the one we are looking for and isn't the one that indicates the end of the attributes
		attributeSize = 0;
		memcpy(&attributeSize, mftEntryBuffer + attributeHeaderOffset + 4, 4);
		attributeHeaderOffset += attributeSize; //move to the next attribute
		currentAttributeTypeCode = 0;
		memcpy(&currentAttributeTypeCode, mftEntryBuffer + attributeHeaderOffset, 4); //retrieve the next attribute type code
	}

	if (currentAttributeTypeCode == g_END_OF_ATTRIBUTE_MARKER) { //if there is no attribute of attributeType
		return 0;
	}
	return attributeHeaderOffset;
}


//return TRUE iff the attribute corresponds to attributeHeaderOffset is resident
BOOL isResident(byte* mftEntryBuffer, uint16_t attributeHeaderOffset) {
	return mftEntryBuffer[attributeHeaderOffset + 0x8] == 0x00; //check if the actual attribute non-resident flag is not set (meaning RESIDENT_FORM)
}


//return the file attribute flags that are stored in $STANDARD_INFORMATION attribute
uint32_t getFileAttributeFlags(byte* mftEntryBuffer) {
	uint16_t standardInformationAttributeHeaderOffset = findAttributeHeaderOffset(mftEntryBuffer, g_STANDARD_INFORMATION_ATTRIBUTE_TYPECODE);
	if (standardInformationAttributeHeaderOffset == 0) { //if there is no $STANDARD_INFORMATION attribute in the MFT entry
		return 0;
	}

	if (isResident(mftEntryBuffer, standardInformationAttributeHeaderOffset)) { //if the actual attribute is resident
		uint16_t standardInformationAttributeOffset = standardInformationAttributeHeaderOffset + g_BYTES_PER_ATTRIBUTE_HEADER + g_BYTES_PER_ATTRIBUTE_RESIDENT_DATA;
		uint32_t fileAttributeFlags = 0;
		memcpy(&fileAttributeFlags, mftEntryBuffer + standardInformationAttributeOffset + 0x20, 4);
		return fileAttributeFlags;
	}
	else {
		//to be continued...
	}
	return 0;
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
		if (nameStringSize == 0) {
			return 0;
		}
		wchar_t* name = calloc(nameStringSize + 1, 2); //add 1 to nameStringSize for the null terminator, each wide character consists of two bytes
		memcpy(name, mftEntryBuffer + fileNameAttributeOffset + 0x42, nameStringSize * 2);
		return name;
	}
	else {
		//to be continued...
	}
	return 0;
}


//return the valid data size (in bytes) of the file
uint64_t getFileValidDataSize(byte* mftEntryBuffer) {
	uint16_t dataAttributeHeaderOffset = findAttributeHeaderOffset(mftEntryBuffer, g_DATA_ATTRIBUTE_TYPECODE);
	if (dataAttributeHeaderOffset == 0) { //if there is no $DATA attribute in the MFT entry
		return 0;
	}

	uint64_t fileSize = 0;
	if (isResident(mftEntryBuffer, dataAttributeHeaderOffset)) { //if the actual attribute is resident
		uint16_t dataAttributeNonResidentDataOffset = dataAttributeHeaderOffset + g_BYTES_PER_ATTRIBUTE_HEADER; //arrive to the attribute resident data
		memcpy(&fileSize, mftEntryBuffer + dataAttributeNonResidentDataOffset, 4); //note: this is the valid data size 
	}
	else { //if the actual attribute is non-resident flag is set (meaning NONRESIDENT_FORM)
		uint16_t dataAttributeNonResidentDataOffset = dataAttributeHeaderOffset + g_BYTES_PER_ATTRIBUTE_HEADER; //arrive to the attribute non-resident data
		memcpy(&fileSize, mftEntryBuffer + dataAttributeNonResidentDataOffset + 0x20, 8); //note: this is the valid data size 
	}
	return fileSize;
}
