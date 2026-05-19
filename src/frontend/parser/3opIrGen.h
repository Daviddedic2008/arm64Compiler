// 3 operand IR gen
#include <stdint.h>
#include "astGen.h"

typedef enum operation{
	ADD, SUB, MUL, DIV, AND, NOT, OR, XOR,
	LOAD, STORE, MOV, LOADIMM,
	CMP, JMP, JMPCND,
	CALL, ARG, FNCDEF, RET,
	REF, DEREF,
	PUSH, POP
}operation;

typedef struct{
	operation op;
	symbol o1, o2, o3;
}quad;

quad* linearizeAST(const node* baseNode);