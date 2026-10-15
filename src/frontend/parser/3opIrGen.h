// 3 operand IR gen
#include <stdint.h>
#include "astGen.h"

typedef enum operation{
	INVALIDOP, ADD, SUB, NEG, MUL, DIV, AND, NOT, OR, XOR,
	MOV, LOADIMM, POP, REF, DEREF, REF_O, DEREF_O,
	LOAD, STACK, GLOBAL, STORE,
	CMP, JMP, JMPABS, JMPCND, SETLABEL, READFLAGS,
	CALL, ARG, FNCDEF, RET, ALIGN,
	PUSH, LABEL_MAP, WRITE_LABEL
}operation;

typedef enum flags{
	flagNe, flagLe, flagGe, flagLt, flagGt, flagEq
}flagEnum;

typedef enum refactorTypes{rfReplace, rfNew}refactorTypes;

typedef struct{
	operation op;
	symbol o1, o2, o3;
	uint8_t skippable; uint8_t refactorType;
}quad;

arena linearizeAST(const node* baseNode);

uint32_t getTotalVRegs();
arena* getQuadArena();

void printSymbol(symbol s);

void printQuads();

symbol newVReg(const tokenType varType);

typedef struct{
	uint32_t qId, label;
}lblInd;

lblInd* getLblInds();
uint32_t getNumLbls();
uint32_t getLabelQID(const uint32_t lbl);
uint32_t getCFGBreaks();

uint32_t getNumQuads();