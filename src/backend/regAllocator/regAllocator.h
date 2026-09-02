#include <stdint.h>
#include "../../frontend/parser/3opIrGen.h"

typedef struct{
	uint32_t i1, i2;
	const symbol* vreg;
}range;