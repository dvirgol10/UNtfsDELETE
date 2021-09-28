#include "dataRunsParser.h"


extern const DWORD g_BYTES_PER_ATTRIBUTE_HEADER;


//return the offset (from the start of the MFT entry) of the first data run element
uint16_t getOffsetOfFirstDataRunElement(byte* mftEntryBuffer, uint16_t attributeHeaderOffset) {
	uint16_t attributeNonResidentDataOffset = attributeHeaderOffset + g_BYTES_PER_ATTRIBUTE_HEADER; //arrive to the attribute non-resident data
	uint16_t dataRunsOffset = 0;
	memcpy(&dataRunsOffset, mftEntryBuffer + attributeNonResidentDataOffset + 0x10, 2);
	return attributeHeaderOffset + dataRunsOffset;
}


//retrieve the metadata (sizes of the values) of the current data run element
//note: "relative" relates to the previous data run element, or to zero if it is the first data run element  
void getMetadataOfDataRunElement(byte* mftEntryBuffer, uint16_t offsetOfDataRunElement, uint8_t* pNumberOfClustersValueSize, uint8_t* pRelativeNumberOfStartingClusterValueSize) {
	*pNumberOfClustersValueSize = mftEntryBuffer[offsetOfDataRunElement] & 0x0F; //retrieve the lower nibble of the value size tuple
	*pRelativeNumberOfStartingClusterValueSize = (mftEntryBuffer[offsetOfDataRunElement] & 0xF0) >> 4; //retrieve the upper nibble of the value size tuple
}


//retrieve the actual information of the current data run element
//note: "relative" relates to the previous data run element, or to zero if it is the first data run element  
void getInformationOfDataRunElement(byte* mftEntryBuffer, uint16_t offsetOfDataRunElement, uint8_t numberOfClustersValueSize, uint64_t* pNumberOfClusters, uint8_t relativeNumberOfStartingClusterValueSize, int64_t* pRelativeNumberOfStartingCluster) {
	*pNumberOfClusters = 0;
	memcpy(pNumberOfClusters, mftEntryBuffer + offsetOfDataRunElement, numberOfClustersValueSize);
	*pRelativeNumberOfStartingCluster = 0;
	memcpy(pRelativeNumberOfStartingCluster, mftEntryBuffer + offsetOfDataRunElement + numberOfClustersValueSize, relativeNumberOfStartingClusterValueSize);
}


BOOL isSigned(int64_t number, uint64_t numberSizeInBytes) {
	int64_t mostSignificantBitPlace = ((int64_t)1) << (numberSizeInBytes * 8 - 1); //this is a 64-bit integer with all bits not set except the MSB bit of number, that is set
	return number & mostSignificantBitPlace;
}


void convertTo64BitSignedInteger(int64_t* number, uint64_t numberSizeInBytes) {
	if (isSigned(*number, numberSizeInBytes)) { //check if its most significant bit (MSB) is set
		for (int64_t i = numberSizeInBytes; i < 8; i++) { //convert the remaining bits to FFF..., thus it will be a proper 64-bit signed integer
			*number |= (((int64_t)0xFF) << (i * 8));
		}
	}
}


//add a new data run list node to the end of dataRunList
void addToDataRunList(DataRunsList* dataRunList, uint64_t numberOfClusters, int64_t relativeNumberOfStartingCluster) {
	DataRunListNode* dataRunListNode = (DataRunListNode*)malloc(sizeof(DataRunListNode));
	*dataRunListNode = (DataRunListNode){ NULL, numberOfClusters, dataRunList->last->numberOfStartingCluster + relativeNumberOfStartingCluster }; //the cluster number in the data run is an offset to that of the previous data run
	//maintain the members of the list
	dataRunList->last->next = dataRunListNode;
	dataRunList->last = dataRunListNode;
}


//return 0 if the attribute of the MFT entry which starts at offset attributeHeaderOffset is non-resident,
//otherwise return a linked list of the attribute's data runs. Each linked list element contains the number of clusters and the real number of starting cluster (the LCN, not the relative one) of its corresponding data run 
DataRunsList* parseDataRuns(byte* mftEntryBuffer, uint16_t attributeHeaderOffset) {
	if (isResident(mftEntryBuffer, attributeHeaderOffset)) { //make sure that the attribute is non-resident
		return 0;
	}

	uint16_t offsetOfDataRunElement = getOffsetOfFirstDataRunElement(mftEntryBuffer, attributeHeaderOffset); //the offset is from the start of the MFT entry

	uint8_t numberOfClustersValueSize = 0;
	uint8_t numberOfStartingClusterValueSize = 0;
	getMetadataOfDataRunElement(mftEntryBuffer, offsetOfDataRunElement, &numberOfClustersValueSize, &numberOfStartingClusterValueSize);
	offsetOfDataRunElement += 1;

	//retrieve the actual information of the (first) data run element
	uint64_t numberOfClusters = 0;
	int64_t numberOfStartingCluster = 0;
	getInformationOfDataRunElement(mftEntryBuffer, offsetOfDataRunElement, numberOfClustersValueSize, &numberOfClusters, numberOfStartingClusterValueSize, &numberOfStartingCluster);

	offsetOfDataRunElement += numberOfClustersValueSize + numberOfStartingClusterValueSize;

	//create the data run list and its first node
	DataRunListNode* firstDataRunListNode = (DataRunListNode*)malloc(sizeof(DataRunListNode));
	*firstDataRunListNode = (DataRunListNode){ 0, numberOfClusters, numberOfStartingCluster };
	DataRunsList* dataRunList = (DataRunsList*)malloc(sizeof(DataRunsList));
	*dataRunList = (DataRunsList){ firstDataRunListNode, firstDataRunListNode };

	uint8_t relativeNumberOfStartingClusterValueSize = 0;
	int64_t relativeNumberOfStartingCluster = 0;
	while (mftEntryBuffer[offsetOfDataRunElement] != 0) { //the last element in the runlist is a 0 byte
		getMetadataOfDataRunElement(mftEntryBuffer, offsetOfDataRunElement, &numberOfClustersValueSize, &relativeNumberOfStartingClusterValueSize);
		offsetOfDataRunElement += 1;

		//retrieve the actual information of the current data run element
		getInformationOfDataRunElement(mftEntryBuffer, offsetOfDataRunElement, numberOfClustersValueSize, &numberOfClusters, relativeNumberOfStartingClusterValueSize, &relativeNumberOfStartingCluster);
		offsetOfDataRunElement += numberOfClustersValueSize + relativeNumberOfStartingClusterValueSize;

		//the relative number of starting cluster in the data run is a signed value, so we have to check if its most significant bit (MSB) is set,
		//	and then convert this number to a proper 64-bit signed integer (int64_t)
		convertTo64BitSignedInteger(&relativeNumberOfStartingCluster, relativeNumberOfStartingClusterValueSize);

		//add a new data run list node (for the current data run element) to the end of dataRunList
		addToDataRunList(dataRunList, numberOfClusters, relativeNumberOfStartingCluster);
	}

	return dataRunList;
}