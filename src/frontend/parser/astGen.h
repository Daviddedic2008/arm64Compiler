#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#include "../lexer/tokenizer.h"

typedef enum nodeType : uint8_t{
	bodyNode, operatorNode, conditionalNode, literalNode, funcDefNode, funcCallNode, identifierNode, castNode, declarationNode, statementNode
}nodeType;

typedef enum symbolType : uint8_t {global, label, flag, local, arg, physical, literalSymbol, strSymbol, invalidSymbol}symbolType;

typedef struct{
	symbolType type; tokenType varType; uint32_t szArr; union{int64_t vReg; char* str;};
	uint8_t strLen;
}symbol;

typedef struct node{
	nodeType type; token val; symbol symbolData;
	struct node* firstChild; struct node* lastChild; struct node* sibling;
}node;

node constructTree(tokenArray arr);

void printTree(node* n, int depth);

void setJmpBuf(jmp_buf f);

uint32_t getUsedVRegs();

uint32_t getNumFuncs();