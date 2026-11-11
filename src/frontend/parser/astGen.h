#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#include "../../helper/algos.h"
#include "../lexer/tokenizer.h"

arena* getStructDataFrames();

typedef enum nodeType : uint8_t{
	bodyNode, operatorNode, conditionalNode, literalNode, funcDefNode, funcCallNode, identifierNode, castNode, declarationNode, structNode, structIdNode, statementNode
}nodeType;

typedef enum symbolType : uint8_t {invalidSymbol, global, label, member, flag, local, refSymbol, temp, arg, physical, literalSymbol, strSymbol, frameSymbol, defaultSymbol}symbolType;

typedef enum dirtyType : bool {dirty, clean} dirtyType;

typedef enum vregType : uint8_t {vregInvalid, vregInt, vregChar, vregVoid, vregStruct, vregLiteral} vregType;

typedef struct symbol symbol;

typedef struct structData{
	uint32_t structId, totalSize;
	char* structName; uint32_t nameSize;
	uint32_t numMembers; uint32_t memAllocated;
	struct symbol* members[];
}structData;

typedef struct varType{
	vregType type;
	uint32_t ptrDepth, szArr, size, parentStructIn, structIn;
	bool isAddr, isMember;
	token name; 
	structData* sData;
}varType;

struct symbol{
	symbolType type; varType vType; uint32_t localOff; union{int64_t vReg; void* frame;};
	bool isDirty; uint32_t scopeDepth; uint16_t preferredReg; uint16_t spillCost; uint32_t physicalReg;
};

typedef struct node{
	nodeType type; token val; symbol* symbolData;
	struct node* firstChild; struct node* lastChild; struct node* sibling;
}node;

node constructTree(tokenArray arr);

void printTree(node* n, int depth);

void setJmpBuf(jmp_buf f);

uint32_t getUsedVRegs();

arena* getSymbolPool();

uint32_t getNumFuncs();

uint32_t getSize(const varType v);

symbol* getInstanceMember(const uint32_t parentStructId, const token nameM);

vregType getVregType(const token t);

symbol* addSymbolPre(const symbol tmp);