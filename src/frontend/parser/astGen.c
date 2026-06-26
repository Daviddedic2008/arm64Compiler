#include "astGen.h"
#include "../../helper/arenaAlloc.h"
#include <string.h>

/*
arena allocator for nodes, start with body node. child nodes will be allocated in linked list type of form.
allowing for sibling nodes to be children of the same parent and many children from one parent, no reallocation
*/

const char* nodeNames[] = {
    [bodyNode] = "bodyNode", [operatorNode] = "operatorNode", 
	[conditionalNode] = "conditionalNode", [literalNode] = "literalNode", 
	[identifierNode] = "identifierNode", [declarationNode] = "declarationNode", 
	[castNode] = "castNode", [funcDefNode] = "funcDefNode", [funcCallNode] = "funcCallNode",
	[statementNode] = "statementNode"
};

const char* tokenNames[] = {
    [opPlus] = "opPlus", [opMinus] = "opMinus", [opIncrement] = "opIncrement", [opNegate] = "opNegate",
	[opDecrement] = "opDecrement", [opEqual] = "opEqual", [keywordCharPtr] = "keywordCharPtr",
    [opMul] = "opMul", [opDiv] = "opDiv", [opLogicalOr] = "opLogicalOr",
    [opLogicalAnd] = "opLogicalAnd", [opLogicalNot] = "opLogicalNot",
    [opBitwiseNot] = "opBitwiseNot", [opBitwiseOr] = "opBitwiseOr", [opBitwiseXor] = "opBitwiseXor",
    [opShiftRight] = "opShiftRight", [opShiftLeft] = "opShiftLeft",
    [opBitwiseAnd] = "opBitwiseAnd", [opDereference] = "opDereference",
    [opReference] = "opReference", [opCmpEquals] = "opCmpEquals",
    [opCmpGreater] = "opCmpGreater", [opCmpLess] = "opCmpLess",
    [opCmpGrEq] = "opCmpGrEq", [opCmpLeEq] = "opCmpLeEq", [opCmpNe] = "opCmpNe",
    [curlyBraceR] = "curlyBraceR", [curlyBraceL] = "curlyBraceL",
    [parenthesesL] = "parenthesesL", [parenthesesR] = "parenthesesR",
    [keywordIf] = "keywordIf", [keywordElse] = "keywordElse",
    [keywordWhile] = "keywordWhile", [keywordInt] = "keywordInt",
    [keywordChar] = "keywordChar", [keywordIntPtr] = "keywordIntPtr", [keywordVoidPtr] = "keywordVoidPtr",
    [endStatement] = "endStatement", [identifier] = "identifier",
    [literal] = "literal", [keywordReturn] = "keywordReturn", [keywordBreak] = "keywordBreak",
	[keywordContinue] = "keywordContinue", [keywordVoid] = "keywordVoid", [nullToken] = "nullToken", [squareBraceL]  = "squareBraceL", [squareBraceR] = "squareBraceL"
};

const char* symbolNames[] = {
	[arg] = "arg", [local] = "local", [global] = "global", [invalidSymbol] = "INVALID"
};

#define nodeStep 2048
#define symbolStep 2048

arena nodePool; jmp_buf compRetEnv;

arena symbolPool; uint32_t stackDepth; uint32_t scopeDepth; uint32_t numVRegs;

uint32_t numFuncs;

uint32_t getNumFuncs(){return numFuncs;}

uint32_t getUsedVRegs(){return numVRegs;}
void deepenScope(){scopeDepth++;}

typedef struct{
	symbolType type; tokenType varType; int32_t vReg; uint32_t szArr;
	token name; uint32_t scopeDepth;
}symbolB;

void initPools(){nodePool = newArena(nodeStep * sizeof(node)); symbolPool = newArena(symbolStep * sizeof(symbolB));}

symbolB constructSymbol(const token name, const tokenType varType, const symbolType type){
	return (symbolB){.name = name, .type = type, .varType = varType, .scopeDepth = scopeDepth, .vReg = numVRegs++};
}

symbolB* addSymbol(const token name, const tokenType varType, const symbolType type){
	const symbolB tmp = constructSymbol(name, varType, type);
	return writeElement(&symbolPool, &tmp, sizeof(symbolB));
}

symbol getSymbol(const token t){
	for(int32_t si = symbolPool.used/sizeof(symbolB)-1; si >= 0; si--){
		const symbolB ts = ((symbolB*)symbolPool.pool)[si];
		if(t.len == ts.name.len && !strncmp(ts.name.str, t.str, t.len)){
			return (symbol){.varType = ts.varType, .type = ts.type, .vReg = ts.vReg, .szArr = ts.szArr};
		}
	}return (symbol){.type = invalidSymbol};
}

void returnScope(){ uint32_t rmi = 0;
	for(int32_t ti = symbolPool.used/sizeof(symbolB)-1; ti >= 0; rmi++, ti--){
		if(((symbolB*)symbolPool.pool)[ti].scopeDepth != scopeDepth) break;
	} scopeDepth--; symbolPool.used -= sizeof(symbolB) * rmi;
}

node constructNode(const nodeType type){
	return (node){.type = type, .val = (token){.type = nullToken}, .firstChild = NULL, .lastChild = NULL, .sibling = NULL};
}

node* addNode(const nodeType type){
	const node tmp = constructNode(type);
	return writeElement(&nodePool, &tmp, sizeof(node));
}

void addChildFromPtr(node* n, const node* add){
	if(n->lastChild == NULL){
		n->firstChild = add; n->lastChild = add;
		return;
	} n->lastChild->sibling = add; n->lastChild = add;
}

node* retrieveChild(node* n, uint32_t idx){
	n = n->firstChild; if(n == NULL) return NULL; while(idx--){
		if(n->sibling == NULL) return NULL;
		n = n->sibling;
	} return n;
}

void addChild(node* n, const node add){
	const node* ptr = writeElement(&nodePool, &add, sizeof(node));
	addChildFromPtr(n, ptr);
}

tokenArray srcArr; uint32_t tokensScanned;

token peekToken(){return tokensScanned >= srcArr.numTokens ? (token){.type = nullToken} : *srcArr.tokens;}
token peekAdvToken(const uint8_t i){return tokensScanned+i >= srcArr.numTokens ? (token){.type = nullToken} : *(srcArr.tokens+i);}
token eatToken(){return tokensScanned++ >= srcArr.numTokens ? (token){.type = nullToken} : *(srcArr.tokens++);}
token expect(const uint8_t type){if(peekToken().type == type){return eatToken();} else{longjmp(compRetEnv, 1);}}

node* parseBody(); node* parseExpression(const uint16_t minPrecedence); node* parseFuncCall(const token t);

bool withinFunctionDef;

token singleOpMap(token t){
	switch(t.type){
		case opMul:
		t.type = opDereference; break;
		case opBitwiseAnd:
		t.type = opReference; break;
		case opMinus:
		t.type = opNegate; break;
	} return t;
}

node* parseArgument(){
	token t = eatToken(); node* n;
	switch(t.type){
		case literal: n = addNode(literalNode); break;
		case identifier: switch(peekToken().type){
			case parenthesesL: n = parseFuncCall(t); eatToken(); return n;
			default: n = addNode(identifierNode); n->val = t; n->symbolData = getSymbol(t);
		} break;
		case keywordInt: case keywordChar:
		if(peekToken().type == opMul){uint8_t pd = 0; while(peekToken().type == opMul){eatToken(); pd++;}t.type = t.type == keywordInt ?  keywordIntPtr : keywordCharPtr; t.val = pd;}
		n = addNode(declarationNode);
		n->val = t; const uint8_t st = t.type; t = eatToken(); const uint8_t sz = st == keywordChar ? 1 : 4; 
		symbolB* s = addSymbol(t, st, withinFunctionDef ? arg : (scopeDepth ? local : global));
		t.val = sz; uint32_t as = 0; if(peekToken().type == squareBraceL){eatToken(); as = eatToken().val; s->szArr = as; eatToken();}
		addChild(n, (node){.type = identifierNode, .val = t, .symbolData = (symbol){.type = s->type, .varType = st, .vReg = s->vReg, .szArr = as}}); return n;
		case parenthesesL: if(const uint8_t tt = peekToken().type; (tt == keywordInt || tt == keywordChar)){token t1 = eatToken(); t1.val = 0; 
		while(peekToken().type == opMul){eatToken(); if(!t1.val)t1.type = t1.type == keywordInt ? keywordIntPtr : keywordCharPtr; t1.val++;}
		eatToken(); n = addNode(castNode); n->val = t1; addChildFromPtr(n, parseArgument()); return n;}
		n = parseExpression(0); expect(parenthesesR); return n;
		case opMinus: case opMul: case opBitwiseAnd: case opBitwiseNot: case opLogicalNot:
		n = addNode(operatorNode); t = singleOpMap(t);
		addChildFromPtr(n, parseExpression(110)); break;
		default:;
	}
	return n->val = t, n;
}

uint16_t getPrecedence(const uint8_t t) {
    switch(t) {
        case opMul: case opDiv: return 100;
        case opPlus: case opMinus: return 90;
        case opShiftLeft: case opShiftRight: return 80;
        case opCmpGreater: case opCmpLess: case opCmpGrEq: case opCmpLeEq: return 70;
        case opCmpEquals: return 60;
        case opBitwiseAnd: return 50;
        case opBitwiseOr: case opBitwiseXor: return 40;
        case opLogicalAnd: return 30;
        case opLogicalOr: return 20;
        case opEqual: case opIncrement: case opDecrement: return 10;
		case squareBraceL: return 110;
        default: return 0;
    }
}

node* parseFuncCall(const token t){
	node* fcNode = addNode(funcCallNode);
	fcNode->val = t; expect(parenthesesL);
	while(peekToken().type != parenthesesR){
		addChildFromPtr(fcNode, parseExpression(0)); if(peekToken().type == parenthesesR) break; expect(endStatement);
	} return fcNode;
}

token peekOperator(){
	token ret = peekToken(); const token tc = ret;
	switch(peekAdvToken(1).type){
		case opEqual: switch(ret.type){
			case opEqual: ret.type = opCmpEquals; break;
			case opPlus: ret.type = opIncrement; break;
			case opMinus: ret.type = opDecrement; break;
			case opCmpGreater: ret.type = opCmpGrEq; break;
			case opCmpLess: ret.type = opCmpLeEq; break;
			case opBitwiseNot: ret.type = opCmpNe; break;
		} break;
		case opBitwiseAnd:
		if(ret.type == opBitwiseAnd) ret.type = opLogicalAnd; break;
		case opBitwiseOr:
		if(ret.type == opBitwiseOr) ret.type = opLogicalOr; break;
	} if(ret.type != tc.type) eatToken(); *srcArr.tokens = ret; return ret;
}

node* parseExpression(const uint16_t minPrecedence){
	node* left = parseArgument();
	while(1){
		token op = peekOperator(); const uint16_t p = getPrecedence(op.type);
		if(op.type == squareBraceR){eatToken();continue;}
		if(p < minPrecedence || !p) break;
		eatToken();
		node* right = parseExpression(p+1);
		node* parent = addNode(operatorNode); parent->val = op;
		addChildFromPtr(parent, left); addChildFromPtr(parent, right); left = parent;
	} return left;
}

node* parseIf(){
	node* ifNode = addNode(conditionalNode);
	addChildFromPtr(ifNode, parseExpression(0)); expect(curlyBraceL); deepenScope(); // eat curly to prevent double nested body stuff
	addChildFromPtr(ifNode, parseBody());
	if(peekToken().type == keywordElse){eatToken(); expect(curlyBraceL); deepenScope();
		addChildFromPtr(ifNode, parseBody());
	} ifNode->val = (token){.type = keywordIf};
	return ifNode;
} // includes else parsing

node* parseWhile(){
	node* whileNode = addNode(conditionalNode);
	addChildFromPtr(whileNode, parseExpression(0)); expect(curlyBraceL); deepenScope();
	addChildFromPtr(whileNode, parseBody());
	whileNode->val = (token){.type = keywordWhile};
	return whileNode;
} 

node* parseFuncDef(){
	numFuncs++;
	node* fdNode = addNode(funcDefNode);
	node n = constructNode(literalNode);if(peekAdvToken(1).type == opMul){
		const uint8_t t = eatToken().type; 
		srcArr.tokens->type = t == keywordInt ? keywordIntPtr : (t == keywordChar ? keywordCharPtr : keywordVoidPtr);
	}
	n.val = eatToken();
	addChild(fdNode, n);
	fdNode->val = eatToken(); eatToken(); withinFunctionDef = true;
	while(peekToken().type != parenthesesR){addChildFromPtr(fdNode, parseArgument()); if(peekToken().type == endStatement) eatToken();}
	withinFunctionDef = false;
	eatToken(); eatToken(); deepenScope();
	addChildFromPtr(fdNode, parseBody());
	return fdNode;
}

node* parseBody(){
// parse until located } or EOF 
	node* ret = addNode(bodyNode); token ct;
	while(ct = peekToken(), !(ct.type == nullToken | ct.type == curlyBraceR)){
		switch(ct.type){
			case keywordContinue: case keywordBreak: case keywordReturn:
			node* statNode = addNode(statementNode); statNode->val = ct; eatToken();
			if(peekToken().type != endStatement) addChildFromPtr(statNode, parseExpression(0));
			addChildFromPtr(ret, statNode); break;
			case curlyBraceL: eatToken(); deepenScope(); addChildFromPtr(ret, parseBody()); continue;
			case keywordIf: eatToken(); addChildFromPtr(ret, parseIf()); continue;
			case keywordWhile: eatToken(); addChildFromPtr(ret, parseWhile()); continue;
			case keywordInt: case keywordChar: case keywordVoid: uint8_t pd = 0; if(peekAdvToken(1).type == opMul) pd = 1;
			while(peekAdvToken(pd+2).type == opMul){pd++;} if(peekAdvToken(2+pd).type == parenthesesL && peekAdvToken(1+pd).type == identifier){
				node* tmp = parseFuncDef();
				addChildFromPtr(ret, tmp); continue;
			}
			if(ct.type == keywordVoid) break;
			default: addChildFromPtr(ret, parseExpression(0));
		} eatToken();
	} if(ct.type == curlyBraceR) returnScope(); eatToken();
	return ret;
}

node constructTree(tokenArray arr){
	tokensScanned = 0; stackDepth = 0; scopeDepth = 0;
	withinFunctionDef = false; numVRegs = 0; numFuncs = 0;
	srcArr = arr; initPools();
	
	node* b = parseBody(); freeArena(symbolPool);
	return *b;
}

void printTree(node* n, int depth) {
    if (!n) return;
    for (int i = 0; i < depth; i++) printf("  ");
    const char* nName = (n->type <= statementNode) ? nodeNames[n->type] : "UNKNOWN_NODE";
    const char* tName = (n->val.type <= nullToken) ? tokenNames[n->val.type] : "UNKNOWN_TOKEN";

    printf("[%s | %s", nName, tName);
	if(n->type == identifierNode && n->symbolData.szArr != 0) printf(" | %d elements", n->symbolData.szArr);
	
	if(n->type == identifierNode) printf(" | %s", symbolNames[n->symbolData.type]); 

    printf("]\n");
    fflush(stdout);
    node* child = n->firstChild;
    while (child) {
        printTree(child, depth + 1);
        child = child->sibling;
    }
}

void setJmpBuf(jmp_buf f){memcpy(compRetEnv, f, sizeof(jmp_buf));}