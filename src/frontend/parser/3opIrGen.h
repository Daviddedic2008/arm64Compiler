// 3 operand IR gen
#include <stdint.h>
#include "astGen.h"

typedef enum operation{
	ADD, SUB, NEG, MUL, DIV, AND, NOT, OR, XOR,
	LOAD, STORE, STACK, GLOBAL, MOV, LOADIMM,
	CMP, JMP, JMPCND, SETLABEL, READFLAGS,
	CALL, ARG, FNCDEF, RET,
	REF, DEREF, REF_O, DEREF_O,
	PUSH, POP
}operation;

typedef enum flags{
	flagNe, flagLe, flagGe, flagLt, flagGt, flagEq
}flagEnum;

typedef struct{
	operation op;
	symbol o1, o2, o3;
}quad;

quad* linearizeAST(const node* baseNode);