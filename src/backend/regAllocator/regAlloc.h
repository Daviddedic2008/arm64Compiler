#include "../../frontend/parser/3opIrGen.h"

typedef struct{
	int32_t i1, i2;
	const symbol* vReg;
	uint8_t edgesFound, numEdges;
	uint8_t offGraph;
}range;

void printRanges();
void printEdges();
void chaitinPass();
void cfgPassTest();

void printQuadsPhysical();

bool isWriteQuad(uint32_t op);
bool validType(const symbol s);

#define numGPRegs 30