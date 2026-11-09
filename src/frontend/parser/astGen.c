#include "astGen.h"
#include "../../tester/testGen.h"
#include <string.h>
#include <stdarg.h>

/*
arena allocator for nodes, start with body node. child nodes will be allocated in linked list type of form.
allowing for sibling nodes to be children of the same parent and many children from one parent, no reallocation
*/

const char* nodeNames[] = {
    [bodyNode] = "bodyNode", [operatorNode] = "operatorNode", 
	[conditionalNode] = "conditionalNode", [literalNode] = "literalNode", 
	[identifierNode] = "identifierNode", [declarationNode] = "declarationNode", 
	[castNode] = "castNode", [funcDefNode] = "funcDefNode", [funcCallNode] = "funcCallNode",
	[statementNode] = "statementNode", [structNode] = "structNode", [structIdNode] = "structIdNode"
};

const char* tokenNames[] = {
    [opPlus] = "opPlus", [opMinus] = "opMinus", [opIncrement] = "opIncrement", [opNegate] = "opNegate",
	[opDecrement] = "opDecrement", [opDPlus] = "opDPlus", [opDMinus] = "opDMinus", [opEqual] = "opEqual", [keywordCharPtr] = "keywordCharPtr",
    [opMul] = "opMul", [opDiv] = "opDiv", [opLogicalOr] = "opLogicalOr",
    [opLogicalAnd] = "opLogicalAnd", [opLogicalNot] = "opLogicalNot",
	[keywordStruct] = "keywordStruct", [keywordStructPtr] = "keywordStructPtr", [structTypeToken] = "structTypeToken",
    [opBitwiseNot] = "opBitwiseNot", [opBitwiseOr] = "opBitwiseOr", [opBitwiseXor] = "opBitwiseXor",
    [opShiftRight] = "opShiftRight", [opShiftLeft] = "opShiftLeft",
    [opBitwiseAnd] = "opBitwiseAnd", [opDereference] = "opDereference",
    [opReference] = "opReference", [opCmpEquals] = "opCmpEquals",
    [opCmpGreater] = "opCmpGreater", [opCmpLess] = "opCmpLess",
    [opCmpGrEq] = "opCmpGrEq", [opCmpLeEq] = "opCmpLeEq", [opCmpNe] = "opCmpNe",
	[opAccess] = "opAccess", [opIndirectAccess] = "opIndirectAccess",
    [curlyBraceR] = "curlyBraceR", [curlyBraceL] = "curlyBraceL",
    [parenthesesL] = "parenthesesL", [parenthesesR] = "parenthesesR",
    [keywordIf] = "keywordIf", [keywordElse] = "keywordElse",
	[keywordCase] = "keywordCase", [keywordSwitch] = "keywordSwitch",
    [keywordWhile] = "keywordWhile", [keywordInt] = "keywordInt",
    [keywordChar] = "keywordChar", [keywordIntPtr] = "keywordIntPtr", [keywordVoidPtr] = "keywordVoidPtr",
    [endStatement] = "endStatement", [identifier] = "identifier",
    [literal] = "literal", [keywordReturn] = "keywordReturn", [keywordBreak] = "keywordBreak",
	[keywordContinue] = "keywordContinue", [keywordVoid] = "keywordVoid", [nullToken] = "nullToken", [squareBraceL]  = "squareBraceL", [squareBraceR] = "squareBraceL"
};

const char* symbolNames[] = {
	[arg] = "arg", [local] = "local", [global] = "global", [member] = "member", [invalidSymbol] = "INVALID"
};

#define nodeStep 2048
#define symbolStep 2048

arena nodePool; jmp_buf compRetEnv;

uint32_t stackDepth; uint32_t scopeDepth; uint32_t numVRegs, maxVRegs = 0;
arena symbolPool;
arena* getSymbolPool(){return &symbolPool;}

uint32_t numFuncs;

uint16_t loopDepth;

uint32_t getNumFuncs(){return numFuncs;}

uint32_t getUsedVRegs(){return maxVRegs;}
void deepenScope(){scopeDepth++;}

void initPools(){nodePool = newArena(nodeStep * sizeof(node)); symbolPool = newArena(symbolStep * sizeof(symbol));}

symbol constructSymbol(const token name, const varType vt, const symbolType type){
	symbol s = {.type = type, .vType = vt, .scopeDepth = scopeDepth, .vReg = numVRegs++}; s.vType.name = name; return s;
}

symbol* addSymbol(const token name, const varType vt, const symbolType type){
	const symbol tmp = constructSymbol(name, vt, type);
	return writeElement(&symbolPool, &tmp, sizeof(symbol));
}
symbol* addSymbolPre(const symbol tmp){
	return writeElement(&symbolPool, &tmp, sizeof(symbol));
}

symbol* getSymbol(const token t){
	for(int32_t si = symbolPool.used/sizeof(symbol)-1; si >= 0; si--){
		const symbol* ts = ((symbol*)symbolPool.pool) + si;
		if(t.len == ts->vType.name.len && !strncmp(ts->vType.name.str, t.str, t.len)){
			return ts;
		}
	}return NULL;
}

void returnScope(){ uint32_t rmi = 0;
	for(int32_t ti = symbolPool.used/sizeof(symbol)-1; ti >= 0; rmi++, ti--){
		if(((symbol*)symbolPool.pool)[ti].scopeDepth != scopeDepth) break;
	} scopeDepth--;
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

node* parseBody(); node* parseUntil(const uint32_t numTokens, ...); node* parseExpression(const uint16_t minPrecedence); node* parseFuncCall(const token t);

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

arena structDataFrames; uint32_t numStructs = 0;

structData* getStructFrame(const token nameT){
	uint32_t* nameSz = &(((structData*)(structDataFrames.pool))->nameSize); char** name = &(((structData*)(structDataFrames.pool))->structName);
	structData* sFrame = (structData*)(structDataFrames.pool);
	for(uint32_t sid = 0; sid < numStructs; sid++){
		char* nameP = *name; const uint32_t stepSize = sFrame->memAllocated/sizeof(structData) + 1;
		if(nameT.len == *nameSz && !strncmp(nameT.str, sFrame->structName, nameT.len)) return sFrame;
		sFrame += stepSize; name += stepSize * sizeof(structData) / sizeof(char*);
		nameSz += stepSize * sizeof(structData) / sizeof(uint32_t);
	}
	return NULL;
}

symbol* getStructMemberSymbol(const structData* sData, const token nameM){
	symbol** s = sData->members; for(uint32_t mi = 0; mi < sData->numMembers; mi++, s++){
		if((*s)->vType.name.len == nameM.len && !strncmp((*s)->vType.name.str, nameM.str, nameM.len)) return *s;
	} return NULL;
}

vregType getVregType(const token t){
	switch(t.type){
		case keywordInt: return vregInt;
		case keywordChar: return vregChar;
		case keywordStruct: return vregStruct;
		case keywordVoid: return vregVoid;
	} return vregInvalid;
}

uint32_t getSize(const varType v){
	if(v.ptrDepth) return 8; switch(v.type){
		case vregInt: case vregLiteral: return 4;
		case vregChar: return 1;
		case vregVoid: case vregInvalid: return 0;
	}const structData* const sid = getStructFrame(v.name);
	// sum up all member sizes
	return sid->totalSize;
}

node* parseArgument(){
	token t = eatToken(); node* n; bool isStruct = 0;
	switch(t.type){
		case literal: n = addNode(literalNode); n->symbolData = addSymbol(t, (varType){.type = vregLiteral}, literalSymbol); numVRegs--; break;
		case identifier: switch(peekToken().type){
			case parenthesesL: n = parseFuncCall(t); eatToken(); return n;
			default: n = addNode(identifierNode); n->val = t; n->symbolData = getSymbol(t); n->symbolData->spillCost += (1 << (3 * loopDepth));
		} break;
		case keywordStruct:{	
			t = eatToken(); const structData* const sFrame = getStructFrame(t); isStruct = 1; t.type = keywordStruct; case keywordInt: case keywordChar:
			varType typeCntxt = {.type = getVregType(t), .name = t};
			if(peekToken().type == opMul){while(peekToken().type == opMul){eatToken(); typeCntxt.ptrDepth++;}}
			n = addNode(declarationNode);
			n->val = t; const token st = t; t = eatToken(); typeCntxt.size = getSize(typeCntxt);
			const uint8_t glt = (scopeDepth ? local : global);
			symbol* s = addSymbol(t, typeCntxt, withinFunctionDef ? arg : glt); if(isStruct) s->vType.parentStructId = sFrame->structId; s->vType.isAddr = s->type == global;
			s->spillCost = (1 << (3 * loopDepth));
			uint32_t as = 0; if(peekToken().type == squareBraceL){eatToken(); as = eatToken().val; s->vType.szArr = as; eatToken();}
			s->vType.type = typeCntxt.type; s->vType.ptrDepth = typeCntxt.ptrDepth; addChild(n, (node){.type = identifierNode, .val = t, .symbolData = s}); return n;
		}
		case parenthesesL: if(const uint8_t tt = peekToken().type; (tt == keywordInt || tt == keywordChar || tt == keywordVoid || tt == keywordStruct)){
			token t1 = eatToken(); if(tt == keywordStruct) t1 = eatToken();
			n = addNode(castNode); n->val = t1; n->val.val = 0;
			while(peekToken().type == opMul){eatToken(); n->val.val++;}
			addChildFromPtr(n, parseArgument()); return n;
		} n = parseExpression(0); expect(parenthesesR); return n;
		case opMinus: case opMul: case opBitwiseAnd: case opBitwiseNot: case opLogicalNot:
		n = addNode(operatorNode); t = singleOpMap(t);
		addChildFromPtr(n, parseExpression(110)); break;
		default:;
	}
	return n->val = t, n;
}

uint16_t getPrecedence(const uint8_t t) {
    switch(t) {
		case opAccess: case opIndirectAccess: return 110;
        case opMul: case opDiv: return 100;
        case opPlus: case opMinus: return 90;
        case opShiftLeft: case opShiftRight: return 80;
        case opCmpGreater: case opCmpLess: case opCmpGrEq: case opCmpLeEq: return 70;
        case opCmpEquals: return 60;
        case opBitwiseAnd: return 50;
        case opBitwiseOr: case opBitwiseXor: return 40;
        case opLogicalAnd: return 30;
        case opLogicalOr: return 20;
        case opEqual: case opIncrement: case opDecrement: case opDPlus: case opDMinus: return 10;
		case squareBraceL: return 110;
        default: return 0;
    }
}

node* parseFuncCall(const token t){
	node* fcNode = addNode(funcCallNode);
	fcNode->val = t; expect(parenthesesL);
	while(peekToken().type != parenthesesR){
		addChildFromPtr(fcNode, parseExpression(0)); if(peekToken().type == parenthesesR) break; expect(opComma);
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
		case opCmpGreater: if(ret.type == opMinus) ret.type = opIndirectAccess; break;
		case opPlus: if(ret.type == opPlus) ret.type = opDPlus; break;
		case opMinus: if(ret.type == opMinus) ret.type = opDMinus; break;
		case opBitwiseAnd:
		if(ret.type == opBitwiseAnd) ret.type = opLogicalAnd; break;
		case opBitwiseOr:
		if(ret.type == opBitwiseOr) ret.type = opLogicalOr; break;
	} if(ret.type != tc.type) eatToken(); *srcArr.tokens = ret; return ret;
}

uint32_t getSz(int type){
	return 4;
}

#define baseNumStructs 64
node* parseStructDef(){ numStructs++;
	eatToken();
	static uint32_t r; const uint32_t numStructs = structDataFrames.used/sizeof(structData);
	if(!numStructs) structDataFrames = newArena(baseNumStructs * sizeof(structData));
	node* defNode = addNode(structNode); uint32_t numMembers = 0; symbol* memberSz[128];
	token name = eatToken(); uint32_t totalSz = 0; expect(curlyBraceL);
	do{
		node* cNode = parseExpression(0); node* const idNode = cNode->firstChild;
		idNode->symbolData->type = member;
		expect(endStatement); numVRegs--;
		addChildFromPtr(defNode, cNode);
		idNode->symbolData->localOff = totalSz;
		memberSz[numMembers++] = idNode->symbolData;
		const uint32_t cs = getSize(idNode->symbolData->vType); totalSz += cs; if(totalSz % cs) totalSz += cs - (totalSz % cs);
	}while(peekToken().type != curlyBraceR); eatToken();
	defNode->val.val = numStructs; structData newStructData = {.totalSize = totalSz}; newStructData.structId = numStructs+1;
	newStructData.numMembers = numMembers;
	newStructData.structName = name.str; newStructData.nameSize = name.len;
	uint32_t szMemberAlloc = sizeof(symbol*) * numMembers; if(szMemberAlloc % sizeof(structData)) szMemberAlloc += sizeof(structData) - szMemberAlloc % sizeof(structData);
	newStructData.memAllocated = szMemberAlloc; writeElement(&structDataFrames, &newStructData, sizeof(structData));
	void* const mcd = blankElement(&structDataFrames, szMemberAlloc);
	memcpy(mcd, memberSz, sizeof(symbol*) * numMembers);
	expect(endStatement);
	return defNode;
}

node* parseExpression(const uint16_t minPrecedence){
	node* left = parseArgument();
	while(1){
		token op = peekOperator(); const uint16_t p = getPrecedence(op.type);
		if(op.type == squareBraceR){eatToken();continue;}
		if(p < minPrecedence || !p) break;
		eatToken();
		node* parent = addNode(operatorNode); parent->val = op;
		addChildFromPtr(parent, left); 
		if(op.type != opDPlus && op.type != opDMinus){
			node* right = parseExpression(p+1);
			addChildFromPtr(parent, right); 
		}
		left = parent;
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
	loopDepth++;
	addChildFromPtr(whileNode, parseExpression(0)); expect(curlyBraceL); deepenScope();
	addChildFromPtr(whileNode, parseBody());
	loopDepth--;
	whileNode->val = (token){.type = keywordWhile};
	return whileNode;
} 

node* parseCase(){
	node* caseNode = addNode(statementNode);
	caseNode->val = (token){.type = eatToken().type};
	
	if(caseNode->val.type == keywordCase) addChildFromPtr(caseNode, parseArgument());
	expect(colon); addChildFromPtr(caseNode, parseUntil(2, keywordCase, keywordDefault));
	return caseNode;
}

node* parseSwitch(){
	node* switchNode = addNode(conditionalNode);
	addChildFromPtr(switchNode, parseExpression(0)); expect(curlyBraceL); deepenScope();
	switchNode->val = (token){.type = keywordSwitch};
	tokenType t; while(t = peekToken().type, (t == keywordCase || t == keywordDefault)){
		addChildFromPtr(switchNode, parseCase());
	}
	return switchNode;
}

node* parseFuncDef(){
	node* fdNode = addNode(funcDefNode);
	node n = constructNode(literalNode);if(peekAdvToken(1).type == opMul){
		const uint8_t t = eatToken().type; 
		srcArr.tokens->type = t == keywordInt ? keywordIntPtr : (t == keywordChar ? keywordCharPtr : keywordVoidPtr);
	}
	n.val = eatToken();
	addChild(fdNode, n);
	fdNode->val = eatToken(); eatToken(); withinFunctionDef = true;
	while(peekToken().type != parenthesesR){addChildFromPtr(fdNode, parseArgument()); if(peekToken().type == opComma) eatToken();}
	withinFunctionDef = false;
	eatToken(); eatToken(); deepenScope();
	addChildFromPtr(fdNode, parseBody());
	numFuncs++; maxVRegs = (numVRegs > maxVRegs) ? numVRegs : maxVRegs; numVRegs = 0;
	return fdNode;
}

node* parseBody(){
// parse until located } or EOF 
	node* ret = addNode(bodyNode); token ct;
	while(ct = peekToken(), !(ct.type == nullToken | ct.type == curlyBraceR)){
		switch(ct.type){
			case endStatement: break;
			case keywordSwitch: eatToken(); addChildFromPtr(ret, parseSwitch()); continue;
			case keywordStruct: if(peekAdvToken(2).type == curlyBraceL) addChildFromPtr(ret, parseStructDef()); else addChildFromPtr(ret, parseExpression(0)); continue;
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

node* parseUntil(const uint32_t numTokens, ...){
	// used for case statements
	va_list args;
	va_start(args, numTokens);
	int tokensToCheck[numTokens];
	for(int i = 0; i < numTokens; i++) tokensToCheck[i] = va_arg(args, int);
	va_end(args);
	node* ret = addNode(bodyNode); token ct;
	while(ct = peekToken(), !(ct.type == nullToken || ct.type == curlyBraceR)){
		for(int i = 0; i < numTokens; i++) if(tokensToCheck[i] == ct.type) goto endWhileUntil;
		switch(ct.type){
			case endStatement: break;
			case keywordSwitch: eatToken(); addChildFromPtr(ret, parseSwitch()); continue;
			case keywordStruct: if(peekAdvToken(2).type == curlyBraceL) addChildFromPtr(ret, parseStructDef()); else addChildFromPtr(ret, parseExpression(0)); continue;
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
	} endWhileUntil:; if(ct.type == curlyBraceR) returnScope(); for(int i = 0; i < numTokens; i++) if(tokensToCheck[i] == ct.type) goto returnUntil; eatToken();
	returnUntil:; return ret;
}

node constructTree(tokenArray arr){
	tokensScanned = 0; stackDepth = 0; scopeDepth = 0;
	withinFunctionDef = false; numVRegs = 0; numFuncs = 0; loopDepth = 0;
	srcArr = arr; initPools();
	node* b = parseBody();
	return *b;
}

void printTree(node* n, int depth) {
    if (!n) return;
    for (int i = 0; i < depth; i++) printfD("  ");
    const char* nName = (n->type <= statementNode) ? nodeNames[n->type] : "UNKNOWN_NODE";
    const char* tName = (n->val.type <= nullToken) ? tokenNames[n->val.type] : "UNKNOWN_TOKEN";

	bool printC = 1;
	if(n->type != structIdNode || 1) printfD("[%s | %s", nName, tName);
	else{printfD("[%s | sID:%d",  nName, n->firstChild->val.val); printC = 0;}
	if(n->type == identifierNode && n->symbolData->vType.szArr != 0) printfD(" | %d elements", n->symbolData->vType.szArr);
	
	if(n->type == identifierNode) printfD(" | %s", symbolNames[n->symbolData->type]); 

    printfD("]\n");
    fflush(stdout);
    node* child = n->firstChild;
    if(printC) while(child){
        printTree(child, depth + 1);
        child = child->sibling;
    }
}

void setJmpBuf(jmp_buf f){memcpy(compRetEnv, f, sizeof(jmp_buf));}