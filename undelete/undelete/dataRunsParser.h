#ifndef DATA_RUNS_PARSER_H
#define DATA_RUNS_PARSER_H


#include "mftAttributes.h"

#include <Windows.h>
#include <stdint.h>


typedef struct DataRunListNode {
	struct DataRunListNode* next;
	uint64_t numberOfClusters;
	uint64_t numberOfStartingCluster;
} DataRunListNode;

typedef struct DataRunsList {
	DataRunListNode* first;
	DataRunListNode* last;
} DataRunsList;


uint16_t getOffsetOfFirstDataRunElement(byte* mftEntryBuffer, uint16_t attributeHeaderOffset);
void getMetadataOfDataRunElement(byte* mftEntryBuffer, uint16_t offsetOfDataRunElement, uint8_t* pNumberOfClustersValueSize, uint8_t* pRelativeNumberOfStartingClusterValueSize);
void getInformationOfDataRunElement(byte* mftEntryBuffer, uint16_t offsetOfDataRunElement, uint8_t numberOfClustersValueSize, uint64_t* pNumberOfClusters, uint8_t relativeNumberOfStartingClusterValueSize, int64_t* pRelativeNumberOfStartingCluster);
BOOL isSigned(int64_t number, uint64_t numberSizeInBytes);
void convertTo64BitSignedInteger(int64_t* number, uint64_t numberSizeInBytes);
void addToDataRunList(DataRunsList* dataRunList, uint64_t numberOfClusters, int64_t relativeNumberOfStartingCluster);
DataRunsList* parseDataRuns(byte* mftEntryBuffer, uint16_t attributeHeaderOffset);

#endif