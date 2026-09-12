#include "3opIrGen.h"
#include <stdint.h>
#include <stdlib.h>
#include "../../tester/testGen.h"

/*
descend to bottom left fo ast, parsing along the way.
for each node, attempt to transform into 3ac. if theres children linearize them first left to right
if a child is another operator or something and u need it for an argument, use most recent vReg.
*/

#define nullSymbol ((symbol){.type = invalidSymbol, .vReg = -1})

uint32_t numQuads, numLabels; arena quadPool;

const char* op_names[] = {
    [ADD] = "ADD", [SUB] = "SUB", [NEG] = "NEG", [MUL] = "MUL", [DIV] = "DIV", 
	[AND] = "AND", [NOT] = "NOT", [OR] = "OR", [XOR] = "XOR",
    [LOAD] = "LOAD", [STORE] = "STORE", [STACK] = "STACK", [GLOBAL] = "GLOBAL", [MOV] = "MOV", [LOADIMM] = "LOADIMM", [CMP] = "CMP", 
	[JMP] = "JMP", [JMPCND] = "JMPCND", [SETLABEL] = "SETLABEL", [READFLAGS] = "READFLAGS",
    [CALL] = "CALL", [ARG] = "ARG", [FNCDEF] = "FNCDEF", [RET] = "RET", [ALIGN] = "ALIGN", [REF] = "REF", [DEREF] = "DEREF", [REF_O]= "REF_OFF", [DEREF_O] = "DEREF_OFF", 
	[PUSH] = "PUSH", [POP] = "POP"
};

const char* flag_names[] = {
	[flagEq] = "EQ", [flagGt] = "GT", [flagGe] = "GE", [flagLt] = "LT", [flagLe] = "LE", [flagNe] = "NE"
};

const char* typeNames[] = {
	[keywordInt] = "INT", [keywordChar] = "CHAR", [keywordIntPtr] = "PTR_INT", [keywordCharPtr] = "PTR_CHAR"
};

void printType(const token t){
	printfD("%s", typeNames[t.type]);
	if(t.type == keywordIntPtr || t.type == keywordCharPtr) printfD("_depth{%d}", t.val);
}

void printSymbol(symbol s) {
    switch (s.type) {
        case physical:
            if (s.vReg == 31) printfD("sp");
            else if (s.vReg == 30) printfD("lr");
            else if (s.vReg == 29) printfD("fp");
            else printfD("x%d", s.vReg);
            break;
        case local: printfD("loc_id(%d)_", s.vReg); printType(s.varType); goto dop;
        case global: printfD("global_id(%d)_", s.vReg); printType(s.varType); goto dop;
        case arg: printfD("arg_%d_", s.vReg); printType(s.varType); goto dop;
		dop:
		if(s.szArr) printfD("[%d]", s.szArr);
		break;
        case literalSymbol: printfD("#%d", s.vReg); break;
        case strSymbol: printfD("%.*s", s.strLen, s.str); break;
		case label: printfD("label_%d", s.vReg); break;
		case flag: printfD("f_%s", flag_names[s.vReg]); break;
        case invalidSymbol: printfD("???"); break;
        default: printfD("v%d", s.vReg); break;
    }
}

void printQuad(quad q) {
	if(q.skippable) return;
    switch (q.op) {
        case ADD: case SUB: case MUL: case DIV: case AND: case OR: case XOR:
            printSymbol(q.o1); printfD(" = "); printSymbol(q.o2); printfD(" %s ", op_names[q.op]); printSymbol(q.o3);
            break;
        case MOV: case LOADIMM: case LOAD:
            printSymbol(q.o1); printfD(" = %s ", op_names[q.op]); printSymbol(q.o2);
            break;
        case STORE:
            printfD("STORE "); printSymbol(q.o2); printfD(" -> ["); printSymbol(q.o1); printfD("]");
            break;
        case JMPCND: printfD("IF "); printSymbol(q.o2); printfD(" JMP label_%d", q.o1.vReg); break;
        case CMP: printfD("CMP "); printSymbol(q.o1); printfD(", "); printSymbol(q.o2); break;
		case SETLABEL: printSymbol(q.o1); printfD(":"); break;
		case READFLAGS: printSymbol(q.o1); printfD(" = check "); printSymbol(q.o2); break;
        case FNCDEF: printfD("\nDEF "); printSymbol(q.o1); break;
        case ALIGN: printfD(op_names[q.op]); break;
        case RET: case PUSH: case POP: case CALL: case ARG: case JMP: case STACK: case GLOBAL: printfD("%s ", op_names[q.op]); printSymbol(q.o1); break;
        case NOT: case REF: case DEREF: case NEG: printSymbol(q.o1); printfD(" = %s ", op_names[q.op]); printSymbol(q.o2); break;
        default: printfD("UNKNOWN_OP(%d)", q.op); break;
    }
    printfD("\n");
}

void emitQuad(const quad q){
	writeElement(&quadPool, &q, sizeof(quad));
	numQuads++;
}

int32_t curTempVReg;

const operation operationMap[] = {
    [opPlus] = ADD, 
    [opMinus] = SUB,
	[opNegate] = NEG,
    [opMul] = MUL, 
    [opDiv] = DIV,	
    [opReference] = REF,
    [opDereference] = DEREF,
    [opBitwiseNot] = NOT,
	[opBitwiseAnd] = AND,
	[opBitwiseOr] = OR,
	[opBitwiseXor] = XOR,
	[opLogicalNot] = NOT
}; flagEnum flags[] = {flagNe, flagLe, flagGe, flagLt, flagGt, flagEq};

flagEnum flagsR[] = {
    [flagEq] = flagNe,
    [flagNe] = flagEq,
    [flagLt] = flagGe,
    [flagGe] = flagLt,
    [flagGt] = flagLe,
    [flagLe] = flagGt
};

const flagEnum cmpFlagMap[] = {
    [opCmpEquals]   = flagNe,
    [opCmpGreater]  = flagLe,
    [opCmpLess]     = flagGe,
    [opCmpGrEq]     = flagLt,
    [opCmpLeEq]     = flagGt
};

symbol reverseFlag(symbol s){
	s.vReg = flagsR[s.vReg]; return s;
}

uint32_t* fncJmpLabels; uint32_t fncsEncountered;

#define startingVregsFrame 2048
arena frameDescriptorVregs;

int printFrameDescHelper(void* data){
	printSymbol(*((symbol*)data));
	return sizeof(symbol);
}

void initFrameDescPool(){
	frameDescriptorVregs = newArena(sizeof(symbol) * 128);
}

typedef struct{
	uint32_t stackSize;
	symbol* vars; uint32_t numVars
}frameDescriptor;

arena frameDescriptors; frameDescriptor* curDescriptor = NULL;

frameDescriptor* newDescriptor(){
	const frameDescriptor wr = (frameDescriptor){.stackSize = 0, .numVars = 0 , .vars = frameDescriptorVregs.pool};
	return writeElement(&frameDescriptors, &wr, sizeof(frameDescriptor));
}

symbol newVReg(const tokenType varType){
	symbol nr; if(fncsEncountered)
		nr = (symbol){.type = local, .vReg = curTempVReg++, .varType = (token){.type = varType}};
	else nr = (symbol){.type = global, .vReg = curTempVReg++, .varType = (token){.type = varType}};
	writeElement(&frameDescriptorVregs, &nr, sizeof(symbol));
	if(curDescriptor != NULL) curDescriptor->numVars++;
	return nr;
}

void addVReg(const symbol s){
	if(curDescriptor == NULL) return; 
	curDescriptor->numVars++;
	writeElement(&frameDescriptorVregs, &s, sizeof(symbol));
}

uint32_t getTotalVRegs(){
	return curTempVReg;
}

int32_t curStartLbl, curEndLbl;

typedef enum{rvalue, lvalue}memType;

typedef struct{
	node* n; symbol targetReg;
	bool isConditional; memType valType;
}linData;

token combineTypes(const token t1, const token t2){
	if(t1.type == literal || t2.type == literal) return (token){.type = literal};
	if(t1.type == keywordIntPtr || t1.type == keywordCharPtr) return t1; if(t2.type == keywordIntPtr || t2.type == keywordCharPtr) return t2;
	if(t1.type == keywordInt || t2.type == keywordInt) return (token){.type = keywordInt};
	return (token){.type = keywordChar};
}

void printQuads(){
	for(uint32_t qi = 0; qi < quadPool.used/sizeof(quad); qi++){
		printQuad(((quad*)quadPool.pool)[qi]);
	}
}

int evalImm(const tokenType op, const symbol o1, const symbol o2){
	switch(op){
		case opPlus: return o1.vReg + o2.vReg;
		case opMinus: return o1.vReg - o2.vReg;
		case opMul: return o1.vReg * o2.vReg;
		case opDiv: return o1.vReg / o2.vReg;
	}
}

symbol linearizeNode(const linData dat){
	symbol secondaryTarget;
	node* n = dat.n; symbol targetReg = dat.targetReg; bool isConditional = dat.isConditional; memType valType = dat.valType;
	switch(n->type){
		case identifierNode:
		if(isConditional){
			emitQuad((quad){.op = CMP, .o1 = *(n->symbolData), .o2 = (symbol){.type = literalSymbol, .vReg = 0}});
			return (symbol){.type = flag, .vReg = flagEq};
		}
		else if(targetReg.vReg != -1 && targetReg.type != label){
			if(targetReg.isAddr){
				emitQuad((quad){.op = STORE, .o1 = targetReg, .o2 = *(n->symbolData)});
			} else{emitQuad((quad){.op = MOV, .o1 = targetReg, .o2 = *(n->symbolData)}); *(n->symbolData) = targetReg;}
		}
		return *(n->symbolData);
		case literalNode:{symbol tmpLit = (symbol){.type = literalSymbol, .vReg = n->val.val};
		if(isConditional){
			emitQuad((quad){.op = CMP, .o1 = tmpLit, .o2 = (symbol){.type = literalSymbol, .vReg = 0}});
			return (symbol){.type = flag, .vReg = flagEq};
		}
		else if(targetReg.vReg != -1 && targetReg.type != label){
			if(targetReg.isAddr){
					emitQuad((quad){.op = STORE, .o1 = targetReg, .o2 = tmpLit});
			} else {emitQuad((quad){.op = LOADIMM, .o1 = targetReg, .o2 = tmpLit}); tmpLit = targetReg;}
		}
		return tmpLit;
		}
		case operatorNode: {switch(n->val.type){
			case opEqual:
			symbol resultReg = linearizeNode((linData){n->firstChild, nullSymbol, 0, lvalue});
			linearizeNode((linData){n->firstChild->sibling, resultReg, 0, rvalue});
			if(isConditional){
				emitQuad((quad){.op = CMP, .o1 = resultReg, .o2 = (symbol){.type = literalSymbol, .vReg = 0}});
				return (symbol){.type = flag, .vReg = flagEq};
			}
			return resultReg;
			
			case opPlus: case opMinus: case opMul: case opDiv: case opBitwiseXor: case opBitwiseAnd: case opBitwiseOr:{
				symbol o1, o2;
				goto starto1o2;
				case opIncrement: case opDecrement: case opDPlus: case opDMinus:
				o1 = linearizeNode((linData){n->firstChild, nullSymbol, 0}); targetReg = o1;
				o2 = (n->val.type == opIncrement || n->val.type == opDecrement) ? linearizeNode((linData){n->firstChild->sibling, nullSymbol, 0}) : (symbol){.type = literalSymbol, .vReg = 1};
				n->val.type = (n->val.type == opIncrement || n->val.type == opDPlus) ? opPlus : opMinus;
				goto skipo1o2; starto1o2:;
				o1 = linearizeNode((linData){n->firstChild, nullSymbol, 0});
				o2 = linearizeNode((linData){n->firstChild->sibling, nullSymbol, 0});
				skipo1o2:;
				skpOtherOps:;
				if(targetReg.vReg == -1 && o1.type == literalSymbol && o2.type == literalSymbol) return (symbol){.type = literalSymbol, .vReg = evalImm(n->val.type, o1, o2)};
				if((targetReg.vReg == -1 || targetReg.isAddr) && targetReg.type != label){secondaryTarget = targetReg; targetReg = newVReg(targetReg.varType.type == keywordIntPtr ? keywordInt : keywordChar);}
				targetReg.varType = combineTypes(o1.varType, o2.varType);
				emitQuad((quad){.op = operationMap[n->val.type], .o1 = targetReg, .o2 = o1, .o3 = o2});
				if(isConditional){
					emitQuad((quad){.op = CMP, .o1 = targetReg, .o2 = (symbol){.type = literalSymbol, .vReg = 0}});
					return (symbol){.type = flag, .vReg = flagEq};
				} if(secondaryTarget.isAddr){
					emitQuad((quad){.op = STORE, .o1 = secondaryTarget, .o2 = targetReg});
				}
				return targetReg;
			}
			case opCmpEquals: case opCmpGreater: case opCmpLess: case opCmpGrEq: case opCmpLeEq:{
				const symbol o1 = linearizeNode((linData){n->firstChild, nullSymbol, 0});
				const symbol o2 = linearizeNode((linData){n->firstChild->sibling, nullSymbol, 0});
				emitQuad((quad){.op = CMP, .o1 = o1, .o2 = o2});
				const symbol flagSmbl = (symbol){.type = flag, .vReg = cmpFlagMap[n->val.type]};
				if(targetReg.type != label && !isConditional){
					if(targetReg.vReg == -1 || targetReg.isAddr){secondaryTarget = targetReg; targetReg = newVReg(keywordInt);}
					emitQuad((quad){.op = READFLAGS, .o1 = targetReg, .o2 = flagSmbl});
				} if(secondaryTarget.isAddr){
					emitQuad((quad){.op = STORE, .o1 = secondaryTarget, .o2 = targetReg});
				}
				return isConditional ? flagSmbl : targetReg;
			}
			case opReference: case opDereference: case opBitwiseNot: case opNegate: case opLogicalNot:{
				symbol o1 = linearizeNode((linData){n->firstChild, nullSymbol, n->val.type == opLogicalNot ? isConditional : 0});
				if(valType == lvalue && n->val.type == opDereference){
					o1.isAddr = true;
					return o1;
				}
				if(n->val.type == opLogicalNot && o1.type == flag) return reverseFlag(o1);
				if((targetReg.vReg == -1 || targetReg.isAddr) && targetReg.type != label){
					secondaryTarget = targetReg; targetReg = newVReg(keywordInt);
					if(n->val.type == opReference){
						switch(targetReg.varType.type){
							case keywordInt:
							targetReg.varType.type = keywordIntPtr; targetReg.varType.val = 1; break;
							case keywordChar:
							targetReg.varType.type = keywordCharPtr; targetReg.varType.val = 1; break;
							case keywordIntPtr: case keywordCharPtr:
							targetReg.varType.val += 1; break;
						}
					}	
				}				
				emitQuad((quad){.op = operationMap[n->val.type], .o1 = targetReg, .o2 = o1});
				if(n->val.type == opReference){
					emitQuad((quad){.op = STORE, .o1 = targetReg, .o2 = o1});
				}
				if(secondaryTarget.isAddr){
					emitQuad((quad){.op = STORE, .o1 = secondaryTarget, .o2 = targetReg});
				}
				return targetReg;
			}
			case squareBraceL:{
				symbol o1 = linearizeNode((linData){n->firstChild, nullSymbol, 0});
				const symbol o2 = linearizeNode((linData){n->lastChild, nullSymbol, 0});
				symbol tmp;
				if(o1.varType.type != keywordChar && (o1.szArr || o1.varType.type == keywordIntPtr || o1.varType.type == keywordCharPtr)){
					if(o2.type == literalSymbol) tmp = (symbol){.type = literalSymbol, .vReg = o2.vReg * 4};
					else{
						tmp = newVReg(keywordInt);
						emitQuad((quad){.op = MUL, .o1 = tmp, .o2 = o2, .o3 = (symbol){.type = literalSymbol, .vReg = 4}});
					}
				}
				else tmp = o2; const uint8_t isVarArr = o1.szArr > 0;
				symbol tmp2 = (symbol){.type = local, .vReg = curTempVReg++, .varType = (token){.type = (o1.varType.type == keywordChar || (!isVarArr && o1.varType.type == keywordCharPtr)) ? keywordCharPtr : keywordIntPtr}};
				tmp2.varType.val = o1.varType.val + isVarArr;
				emitQuad((quad){.op = ADD, .o1 = tmp2, .o2 = o1, .o3 = tmp});
				if(valType == lvalue){
					tmp2.isAddr = true; return tmp2;
				} if(targetReg.vReg == -1 && targetReg.type != label){
					targetReg = newVReg(keywordInt);
					if(o1.szArr){
						targetReg.varType = o1.varType;
					} else targetReg.varType = (o1.varType.val-1) ? (token){.type = o1.varType.type, .val = o1.varType.val - 1} : (token){.type = (o1.varType.type == keywordIntPtr ? keywordInt : keywordChar)};
				}
				emitQuad((quad){.op = LOAD, .o1 = targetReg, .o2 = tmp2});
				if(secondaryTarget.isAddr){
					emitQuad((quad){.op = STORE, .o1 = secondaryTarget, .o2 = targetReg});
					return secondaryTarget;
				}
				return targetReg;
			}
			case opLogicalAnd:{
				symbol resultReg; if(!isConditional){ secondaryTarget = targetReg;
					resultReg = ((targetReg.vReg == -1 || targetReg.isAddr) && targetReg.type != label) ? newVReg(keywordInt) : targetReg;
					emitQuad((quad){.op = LOADIMM, .o1 = resultReg, .o2 = (symbol){.type = literalSymbol, .vReg = 0}});
				}
				const symbol o1 = linearizeNode((linData){n->firstChild, targetReg, 1});
				const symbol labelSkip = targetReg.type == label ? targetReg : (o1.type == label ? o1 : (symbol){.type = label, .vReg = numLabels++});
				if(o1.type != label) emitQuad((quad){.op = JMPCND, .o1 = labelSkip, .o2 = o1});
				const symbol o2 = linearizeNode((linData){n->firstChild->sibling, targetReg, 1});
				resultReg.varType = combineTypes(o1.varType, o2.varType);
				if(o2.type != label) emitQuad((quad){.op = JMPCND, .o1 = labelSkip, .o2 = o2});
				if(!isConditional){
					emitQuad((quad){.op = LOADIMM, .o1 = resultReg, .o2 = (symbol){.type = literalSymbol, .vReg = 1}});
					emitQuad((quad){.op = SETLABEL, .o1 = labelSkip});
				} if(secondaryTarget.isAddr){
					emitQuad((quad){.op = STORE, .o1 = secondaryTarget, .o2 = resultReg});
				}
				return isConditional ? labelSkip : resultReg;
			}
			case opLogicalOr:{
				symbol resultReg; if(!isConditional){ secondaryTarget = targetReg;
					resultReg = ((targetReg.vReg == -1 || targetReg.isAddr) && targetReg.type != label) ? newVReg(keywordInt) : targetReg;
					emitQuad((quad){.op = LOADIMM, .o1 = resultReg, .o2 = (symbol){.type = literalSymbol, .vReg = 1}});
				}
				const symbol labelTrue = (symbol){.type = label, .vReg = numLabels++};
				const symbol o1 = linearizeNode((linData){n->firstChild, targetReg, 1});
				const symbol labelSkip = targetReg.type == label ? targetReg : (o1.type == label ? o1 : (symbol){.type = label, .vReg = numLabels++});
				if(o1.type != label) emitQuad((quad){.op = JMPCND, .o1 = labelTrue, reverseFlag(o1)});
				const symbol o2 = linearizeNode((linData){n->firstChild->sibling, targetReg, 1});
				if(o2.type != label) emitQuad((quad){.op = JMPCND, .o1 = labelTrue, reverseFlag(o2)});
				resultReg.varType = combineTypes(o1.varType, o2.varType);
				if(!isConditional){
					emitQuad((quad){.op = LOADIMM, .o1 = resultReg, .o2 = (symbol){.type = literalSymbol, .vReg = 0}});
				} else emitQuad((quad){.op = JMP, .o1 = labelSkip});
				emitQuad((quad){.op = SETLABEL, .o1 = labelTrue});
				if(secondaryTarget.isAddr){
					emitQuad((quad){.op = STORE, .o1 = secondaryTarget, .o2 = resultReg});
				}
				return isConditional ? labelSkip : resultReg;
			}
			case keywordReturn:{
				if(n->firstChild == NULL) return nullSymbol;
				symbol retV = linearizeNode((linData){n->firstChild, nullSymbol, 0, rvalue});
				retV.preferredReg = 1; 
				emitQuad((quad){.op = RET, .o1 = retV});
				return retV;
			}
		}
		break;
		}
		case funcDefNode:{
			emitQuad((quad){.op = FNCDEF, .o1 = (symbol){.type = strSymbol, .str = n->val.str, .strLen = n->val.len}});
			curDescriptor = newDescriptor();
			fncJmpLabels[fncsEncountered++] = numQuads;
			node* cn = n->firstChild->sibling; uint32_t numArgs = 0;
			while(cn != n->lastChild && cn != NULL){
				if(numArgs < 8){
					symbol a = linearizeNode((linData){cn, nullSymbol, 0}); a.preferredReg = numArgs+1;
					emitQuad((quad){.op = MOV, .o1 = a, .o2 = (symbol){.type = physical, .vReg = numArgs}});
				}
				else emitQuad((quad){.op = POP, .o1 = linearizeNode((linData){cn, nullSymbol, 0})});
				numArgs++; cn = cn->sibling;
			}
			linearizeNode((linData){n->lastChild, nullSymbol, 0});
			curDescriptor = NULL;
			return nullSymbol;
		}
		case funcCallNode:{
			node* cn = n->firstChild; uint32_t numArgs = 0;
			if(secondaryTarget.isAddr){secondaryTarget = targetReg; targetReg.type = fncsEncountered ? local : global; targetReg.vReg = curTempVReg++;}
			while(cn != NULL){
				symbol ln = linearizeNode((linData){cn, nullSymbol, 0});
				if(numArgs < 8){
					const symbol physicalSpace = (symbol){.type = physical, .vReg = numArgs};
					ln.preferredReg = numArgs + 1;
					emitQuad((quad){.op = MOV, .o1 = physicalSpace, .o2 = ln});
				} else{
					emitQuad((quad){.op = PUSH, .o1 = ln});
				}
				numArgs++; if(cn == n->lastChild) break;
				cn = cn->sibling;
			}
			emitQuad((quad){.op = ALIGN});
			emitQuad((quad){.op = CALL, .o1 = (symbol){.type = strSymbol, .str = n->val.str, .strLen = n->val.len}});
			const symbol r0s = (symbol){.type = physical, .vReg = 0};
			if(targetReg.vReg != -1 || targetReg.isAddr) emitQuad((quad){.op = MOV, .o1 = targetReg, .o2 = r0s});
			const symbol retReg = targetReg.vReg == -1 ? r0s : targetReg;
			if(isConditional){
				emitQuad((quad){.op = CMP, .o1 = retReg, .o2 = (symbol){.type = literal, .vReg = 0}});
				return (symbol){.type = flag, .vReg = flagEq};
			} if(secondaryTarget.isAddr){
				emitQuad((quad){.op = STORE, .o1 = secondaryTarget, .o2 = targetReg});
			}
			return retReg;
		}
		case declarationNode:{
			if(n->firstChild->symbolData->szArr != 0){
				if(n->firstChild->symbolData->type != global) curDescriptor->stackSize += n->firstChild->symbolData->szArr * ((n->firstChild->symbolData->varType.type != keywordChar) * 4);
				emitQuad((quad){.op = n->firstChild->symbolData->type == global ? GLOBAL : STACK, .o1 = *(n->firstChild->symbolData)});
			} addVReg(*(n->firstChild->symbolData));
			return *(n->firstChild->symbolData);
		}
		case castNode:{
			node* o1n = n->firstChild;
			symbol o1 = linearizeNode((linData){n->firstChild, nullSymbol, 0});
			const token castType = n->val;
			o1.varType = castType;
			if(targetReg.vReg != -1) emitQuad((quad){.op = MOV, .o1 = targetReg, .o2 = o1});
			if(isConditional){
				emitQuad((quad){.op = CMP, .o1 = o1, .o2 = (symbol){.type = literal, .vReg = 0}});
				return (symbol){.type = flag, .vReg = flagEq};
			}
			return o1;
		}
		case conditionalNode:{
			switch(n->val.type){
				case keywordIf:{
					const uint32_t lblUsed = numLabels++; const symbol sl = (symbol){.type = label, .vReg = lblUsed};
					const symbol o1 = linearizeNode((linData){n->firstChild, nullSymbol, 1});
					switch(o1.type){
						case flag: emitQuad((quad){.op = JMPCND, sl, o1}); break;
						case label: break;
					}
					linearizeNode((linData){n->firstChild->sibling, nullSymbol, 0});
					const bool isElse = n->firstChild->sibling != n->lastChild;
					uint32_t elseLbl;
					if(isElse){
						elseLbl = numLabels++;
						emitQuad((quad){.op = JMP, .o1 = (symbol){.type = label, .vReg = elseLbl}});
					}
					emitQuad((quad){.op = SETLABEL, .o1 = o1.type == label ? o1 : sl});
					if(isElse){
						linearizeNode((linData){n->firstChild->sibling->sibling, nullSymbol, 0});
						emitQuad((quad){.op = SETLABEL, .o1 = (symbol){.type = label, .vReg = elseLbl}});
					} break;
				}
				case keywordWhile:{
					const uint32_t lblLoop = numLabels++, lblInitial = numLabels++, lblBreak = numLabels++; 
					const uint32_t prevEndL = curEndLbl, prevStartL = curStartLbl; curEndLbl = lblBreak; curStartLbl = lblInitial;
					const symbol sl = (symbol){.type = label, .vReg = lblLoop};
					const symbol sf = (symbol){.type = label, .vReg = lblInitial};
					const symbol sb = (symbol){.type = label, .vReg = lblBreak};
					emitQuad((quad){.op = JMP, .o1 = sf});
					emitQuad((quad){.op = SETLABEL, .o1 = sl});
					linearizeNode((linData){n->firstChild->sibling, nullSymbol, 0});
					emitQuad((quad){.op = SETLABEL, .o1 = sf});
					const symbol o1 = reverseFlag(linearizeNode((linData){n->firstChild, sb, 1}));
					if(o1.type == flag) emitQuad((quad){.op = JMPCND, sl, o1});
					else emitQuad((quad){.op = JMP, .o1 = sl});
					emitQuad((quad){.op = SETLABEL, sb});
					curEndLbl = prevEndL; curStartLbl = prevStartL;
					break;
				}
			}
			return nullSymbol;
		}
		case statementNode:{
			switch(n->val.type){
				case keywordReturn:{
					symbol r1 = linearizeNode((linData){n->firstChild, nullSymbol, 0});
					r1.preferredReg = 1; 
					emitQuad((quad){.op = RET, .o1 = r1});
					break;
				}
				case keywordBreak:{
					emitQuad((quad){.op = JMP, .o1 = (symbol){.type = label, .vReg = curEndLbl}});
					break;
				}
				case keywordContinue:{
					emitQuad((quad){.op = JMP, .o1 = (symbol){.type = label, .vReg = curStartLbl}});
					break;
				}
			}
			return nullSymbol;
		}
		case bodyNode:{
			node* cn = n->firstChild;
			if(cn != NULL) do{
				linearizeNode((linData){cn, nullSymbol, 0});
				if(cn == n->lastChild) break;
				cn = cn->sibling;
			}while(true);
		}
		default: return nullSymbol;
	}
}

bool compareSymbols(const symbol s1, const symbol s2){
	return s1.type == s2.type && s1.vReg == s2.vReg;
}

void referenceOptimizationPass(){
	uint32_t qi = 0; const uint32_t lm = quadPool.used/sizeof(quad); for(quad* q = (quad*)quadPool.pool; qi < lm; qi++, q++){
		if(q->op == REF){ (q+1)->skippable = 1;
			for(uint32_t offset = 2; offset < lm - qi; offset++){
				const quad* q2 = q + offset; if(q2->op == LOAD && compareSymbols(q2->o2, q->o1)){
					(q+1)->skippable = 0;
				}
			}
		}
	}
}

void constantFoldingPass(){
	quad* prevOp;
	uint32_t qi = 0; const uint32_t lm = quadPool.used/sizeof(quad); for(quad* q = (quad*)quadPool.pool; qi < lm; qi++, q++){
		if(qi) switch(q->op){
			case ADD: case SUB:{
				const bool isChain = compareSymbols(q->o2, prevOp->o1) ? q->o3.type == literalSymbol : (compareSymbols(q->o3, prevOp->o1) ? q->o2.type == literalSymbol : 0);
				if((prevOp->op == ADD || prevOp->op == SUB) && isChain && (prevOp->o2.type == literalSymbol || prevOp->o3.type == literalSymbol)){
					prevOp->skippable = 1; symbol* newOpReg; curTempVReg--;
					symbol* const literalC = q->o2.type == literalSymbol ? (newOpReg = &q->o3, &q->o2) : (newOpReg = &q->o2, &q->o3);
					literalC->vReg += (q->op == SUB ? -1 : 1) * ((prevOp->o2.type == literalSymbol) ? (*newOpReg = prevOp->o3, prevOp->o2.vReg) : (*newOpReg = prevOp->o2, prevOp->o3.vReg));
				}
			}
			case MUL: case DIV:{
				const bool isChain = compareSymbols(q->o2, prevOp->o1) ? q->o3.type == literalSymbol : (compareSymbols(q->o3, prevOp->o1) ? q->o2.type == literalSymbol : 0);
				if((prevOp->op == DIV || prevOp->op == MUL) && isChain && (prevOp->o2.type == literalSymbol || prevOp->o3.type == literalSymbol)){
					prevOp->skippable = 1; symbol* newOpReg; curTempVReg--;
					symbol* const literalC = q->o2.type == literalSymbol ? (newOpReg = &q->o3, &q->o2) : (newOpReg = &q->o2, &q->o3);
					literalC->vReg += (q->op != prevOp->op) * ((prevOp->o2.type == literalSymbol) ? (*newOpReg = prevOp->o3, prevOp->o2.vReg) : (*newOpReg = prevOp->o2, prevOp->o3.vReg));
					if(literalC->vReg < 0){literalC->vReg = -1 * literalC->vReg; q->op = q->op == MUL ? DIV : MUL;}
				}
			}
			case NEG:{
			}
		} prevOp = q;
	}
}

#define maxQuads 4096

arena linearizeAST(const node* baseNode){
	fncJmpLabels = malloc(sizeof(uint32_t) * getNumFuncs()); numQuads = 0;
	quadPool = newArena(sizeof(quad) * maxQuads); numLabels = 0;
	initFrameDescPool(); frameDescriptors = newArena(sizeof(frameDescriptor) * 256);
	curTempVReg = getUsedVRegs(); fncsEncountered = 0;
	linearizeNode((linData){baseNode, nullSymbol, 0});
	referenceOptimizationPass();
	constantFoldingPass();
	printQuads();
	return quadPool;
}