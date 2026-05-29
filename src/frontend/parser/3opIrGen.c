#include "3opIrGen.h"
#include <stdint.h>
#include "../../helper/arenaAlloc.h"
#include <stdlib.h>

/*
descend to bottom left fo ast, parsing along the way.
for each node, attempt to transform into 3ac. if theres children linearize them first left to right
if a child is another operator or something and u need it for an argument, use most recent vReg.
*/

#define nullSymbol ((symbol){.type = invalidSymbol, .vReg = -1})

uint32_t numQuads, numLabels; arena quadPool;

const char* op_names[] = {
    "ADD", "SUB", "NEG", "MUL", "DIV", "AND", "NOT", "OR", "XOR",
    "LOAD", "STORE", "MOV", "LOADIMM", "CMP", "JMP", "JMPCND", "SETLABEL", "READFLAGS",
    "CALL", "ARG", "FNCDEF", "RET", "REF", "DEREF", "PUSH", "POP"
};

const char* flag_names[] = {
	[flagEq] = "EQ", [flagGt] = "GT", [flagGe] = "GE", [flagLt] = "LT", [flagLe] = "LE", [flagNe] = "NE"
};

void printSymbol(symbol s) {
    switch (s.type) {
        case physical:
            if (s.vReg == 31) printf("sp");
            else if (s.vReg == 30) printf("lr");
            else if (s.vReg == 29) printf("fp");
            else printf("x%d", s.vReg);
            break;
        case local: printf("loc_off(%d)", s.vReg); break;
        case global: printf("global_id(%d)", s.vReg); break;
        case arg: printf("arg_%d", s.vReg); break;
        case literalSymbol: printf("#%d", s.vReg); break;
        case strSymbol: printf("%.*s", s.strLen, s.str); break;
		case label: printf("label_%d", s.vReg); break;
		case flag: printf("f_%s", flag_names[s.vReg]); break;
        case invalidSymbol: printf("???"); break;
        default: printf("v%d", s.vReg); break;
    }
}

void printQuad(quad q) {
    switch (q.op) {
        case ADD: case SUB: case MUL: case DIV: case AND: case OR: case XOR:
            printSymbol(q.o1); printf(" = "); printSymbol(q.o2); printf(" %s ", op_names[q.op]); printSymbol(q.o3);
            break;
        case MOV: case LOADIMM: case LOAD:
            printSymbol(q.o1); printf(" = %s ", op_names[q.op]); printSymbol(q.o2);
            break;
        case STORE:
            printf("STORE "); printSymbol(q.o2); printf(" -> ["); printSymbol(q.o1); printf("]");
            break;
        case JMP: printf("JMP "); printSymbol(q.o1); break;
        case JMPCND: printf("IF "); printSymbol(q.o2); printf(" JMP label_%d", q.o1.vReg); break;
        case CMP: printf("CMP "); printSymbol(q.o1); printf(", "); printSymbol(q.o2); break;
		case SETLABEL: printSymbol(q.o1); printf(":"); break;
		case READFLAGS: printSymbol(q.o1); printf(" = check "); printSymbol(q.o2); break;
        case FNCDEF: printf("\nDEF "); printSymbol(q.o1); break;
        case RET: printf("RET "); break;
        case CALL: printf("CALL "); printSymbol(q.o1); break;
        case ARG: printf("PARAM "); printSymbol(q.o1); break;
        case PUSH: case POP: printf("%s ", op_names[q.op]); printSymbol(q.o1); break;
        case NOT: case REF: case DEREF: case NEG: printSymbol(q.o1); printf(" = %s ", op_names[q.op]); printSymbol(q.o2); break;
        default: printf("UNKNOWN_OP(%d)", q.op); break;
    }
    printf("\n");
}

void emitQuad(const quad q){
	printQuad(q);
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

int32_t curStartLbl, curEndLbl;

symbol linearizeNode(node* n, symbol targetReg, bool isConditional){
	switch(n->type){
		case identifierNode:
		if(isConditional){
			emitQuad((quad){.op = CMP, .o1 = n->symbolData, .o2 = (symbol){.type = literalSymbol, .vReg = 0}});
			return (symbol){.type = flag, .vReg = flagEq};
		}
		else if(targetReg.vReg != -1 && targetReg.type != label){emitQuad((quad){.op = MOV, .o1 = targetReg, .o2 = n->symbolData}); n->symbolData = targetReg;}
		return n->symbolData;
		case literalNode:{symbol tmpLit = (symbol){.type = literalSymbol, .vReg = n->val.val};
		if(isConditional){
			emitQuad((quad){.op = CMP, .o1 = tmpLit, .o2 = (symbol){.type = literalSymbol, .vReg = 0}});
			return (symbol){.type = flag, .vReg = flagEq};
		}
		else if(targetReg.vReg != -1 && targetReg.type != label){
		emitQuad((quad){.op = LOADIMM, .o1 = targetReg, .o2 = tmpLit}); tmpLit = targetReg;
		}
		return tmpLit;
		}
		case operatorNode:{switch(n->val.type){
			case opEqual:
			const symbol resultReg = linearizeNode(n->firstChild, nullSymbol, 0);
			linearizeNode(n->firstChild->sibling, resultReg, 0);
			if(isConditional){
				emitQuad((quad){.op = CMP, .o1 = resultReg, .o2 = (symbol){.type = literalSymbol, .vReg = 0}});
				return (symbol){.type = flag, .vReg = flagEq};
			}
			return resultReg;
			case opPlus: case opMinus: case opMul: case opDiv:{
				const symbol o1 = linearizeNode(n->firstChild, nullSymbol, 0);
				const symbol o2 = linearizeNode(n->firstChild->sibling, nullSymbol, 0);
				if(targetReg.vReg == -1 && targetReg.type != label){targetReg.vReg = curTempVReg++; targetReg.type = local;}
				emitQuad((quad){.op = operationMap[n->val.type], .o1 = targetReg, .o2 = o1, .o3 = o2});
				if(isConditional){
					emitQuad((quad){.op = CMP, .o1 = targetReg, .o2 = (symbol){.type = literalSymbol, .vReg = 0}});
					return (symbol){.type = flag, .vReg = flagEq};
				}
				return targetReg;
			}
			case opCmpEquals: case opCmpGreater: case opCmpLess: case opCmpGrEq: case opCmpLeEq:{
				const symbol o1 = linearizeNode(n->firstChild, nullSymbol, 0);
				const symbol o2 = linearizeNode(n->firstChild->sibling, nullSymbol, 0);
				emitQuad((quad){.op = CMP, .o1 = o1, .o2 = o2});
				const symbol flagSmbl = (symbol){.type = flag, .vReg = cmpFlagMap[n->val.type]};
				if(targetReg.type != label && !isConditional){
					if(targetReg.vReg == -1){targetReg.vReg = curTempVReg++; targetReg.type = local;}
					emitQuad((quad){.op = READFLAGS, .o1 = targetReg, .o2 = flagSmbl});
				}
				return isConditional ? flagSmbl : targetReg;
			}
			case opReference: case opDereference: case opBitwiseNot: case opNegate: case opLogicalNot:{
				const symbol o1 = linearizeNode(n->firstChild, nullSymbol, n->val.type == opLogicalNot ? isConditional : 0);
				if(n->val.type == opLogicalNot && o1.type == flag) return reverseFlag(o1);
				if(targetReg.vReg == -1 && targetReg.type != label){targetReg.vReg = curTempVReg++; targetReg.type = local;}
				emitQuad((quad){.op = operationMap[n->val.type], .o1 = targetReg, .o2 = o1});
				return targetReg;
			}
			case opLogicalAnd:{
				symbol resultReg; if(!isConditional){
					resultReg = (targetReg.vReg == -1 && targetReg.type != label) ? (symbol){.type = local, .vReg = curTempVReg++} : targetReg;
					emitQuad((quad){.op = LOADIMM, .o1 = resultReg, .o2 = (symbol){.type = literalSymbol, .vReg = 0}});
				}
				const symbol o1 = linearizeNode(n->firstChild, targetReg, 1);
				const symbol labelSkip = targetReg.type == label ? targetReg : (o1.type == label ? o1 : (symbol){.type = label, .vReg = numLabels++});
				if(o1.type != label) emitQuad((quad){.op = JMPCND, .o1 = labelSkip, .o2 = o1});
				const symbol o2 = linearizeNode(n->firstChild->sibling, targetReg, 1);
				if(o2.type != label) emitQuad((quad){.op = JMPCND, .o1 = labelSkip, .o2 = o2});
				if(!isConditional){
					emitQuad((quad){.op = LOADIMM, .o1 = resultReg, .o2 = (symbol){.type = literalSymbol, .vReg = 1}});
					emitQuad((quad){.op = SETLABEL, .o1 = labelSkip});
				}
				return isConditional ? labelSkip : resultReg;
			}
			case opLogicalOr:{
				symbol resultReg; if(!isConditional){
					resultReg = (targetReg.vReg == -1 && targetReg.type != label) ? (symbol){.type = local, .vReg = curTempVReg++} : targetReg;
					emitQuad((quad){.op = LOADIMM, .o1 = resultReg, .o2 = (symbol){.type = literalSymbol, .vReg = 1}});
				}
				const symbol labelTrue = (symbol){.type = label, .vReg = numLabels++};
				const symbol o1 = linearizeNode(n->firstChild, targetReg, 1);
				const symbol labelSkip = targetReg.type == label ? targetReg : (o1.type == label ? o1 : (symbol){.type = label, .vReg = numLabels++});
				if(o1.type != label) emitQuad((quad){.op = JMPCND, .o1 = labelTrue, reverseFlag(o1)});
				const symbol o2 = linearizeNode(n->firstChild->sibling, targetReg, 1);
				if(o2.type != label) emitQuad((quad){.op = JMPCND, .o1 = labelTrue, reverseFlag(o2)});
				if(!isConditional){
					emitQuad((quad){.op = LOADIMM, .o1 = resultReg, .o2 = (symbol){.type = literalSymbol, .vReg = 0}});
				} else emitQuad((quad){.op = JMP, .o1 = labelSkip});
				emitQuad((quad){.op = SETLABEL, .o1 = labelTrue});
				return isConditional ? labelSkip : resultReg;
			}
			case keywordReturn:{
				if(n->firstChild == NULL) return nullSymbol;
				const symbol retV = linearizeNode(n->firstChild, (symbol){.type = physical, .vReg = 0}, 0);
				emitQuad((quad){.op = RET});
				return retV;
			}
		}
		break;
		}
		case funcDefNode:{
			emitQuad((quad){.op = FNCDEF, .o1 = (symbol){.type = strSymbol, .str = n->val.str, .strLen = n->val.len}});
			
			fncJmpLabels[fncsEncountered++] = numQuads;
			node* cn = n->firstChild->sibling; uint32_t numArgs = 0;
			while(cn != n->lastChild && cn != NULL){
				if(numArgs < 8) emitQuad((quad){.op = MOV, .o1 = linearizeNode(cn, nullSymbol, 0), .o2 = (symbol){.type = physical, .vReg = numArgs}});
				else emitQuad((quad){.op = POP, .o1 = linearizeNode(cn, nullSymbol, 0)});
				numArgs++; cn = cn->sibling;
			}
			linearizeNode(n->lastChild, nullSymbol, 0);
			return nullSymbol;
		}
		case funcCallNode:{
			node* cn = n->firstChild; uint32_t numArgs = 0;
			while(cn != NULL){
				if(cn->type != literalNode && cn->type != identifierNode) cn->symbolData = linearizeNode(cn, (symbol){.type = arg, .vReg = curTempVReg++}, 0);
				else{
					cn->symbolData = linearizeNode(cn, nullSymbol, 0);
				}
				numArgs++; if(cn == n->lastChild) break;
				cn = cn->sibling;
			}cn = n->firstChild; numArgs = 0;
			while(cn != NULL){
				if(numArgs < 8) emitQuad((quad){.op = MOV, .o1 = (symbol){.type = physical, .vReg = numArgs}, cn->symbolData});
				else emitQuad((quad){.op = PUSH, .o1 = cn->symbolData});
				numArgs++; if(cn == n->lastChild) break;
				cn = cn->sibling;
			}
			emitQuad((quad){.op = CALL, .o1 = (symbol){.type = strSymbol, .str = n->val.str, .strLen = n->val.len}});
			const symbol r0s = (symbol){.type = physical, .vReg = 0};
			if(targetReg.vReg != -1) emitQuad((quad){.op = MOV, .o1 = targetReg, .o2 = r0s});
			const symbol retReg = targetReg.vReg == -1 ? r0s : targetReg;
			if(isConditional){
				emitQuad((quad){.op = CMP, .o1 = retReg, .o2 = (symbol){.type = literal, .vReg = 0}});
				return (symbol){.type = flag, .vReg = flagEq};
			}
			return retReg;
		}
		case declarationNode:{
			return n->firstChild->symbolData;
		}
		case castNode:{
			node* o1 = n->firstChild;
			const token castType = n->val;
			o1->val = castType;
			o1->symbolData.varType = castType.type;
			if(targetReg.vReg != -1) emitQuad((quad){.op = MOV, .o1 = targetReg, .o2 = o1->symbolData});
			if(isConditional){
				emitQuad((quad){.op = CMP, .o1 = o1->symbolData, .o2 = (symbol){.type = literal, .vReg = 0}});
				return (symbol){.type = flag, .vReg = flagEq};
			}
			return o1->symbolData;
		}
		case conditionalNode:{
			switch(n->val.type){
				case keywordIf:{
					const uint32_t lblUsed = numLabels++; const symbol sl = (symbol){.type = label, .vReg = lblUsed};
					const symbol o1 = linearizeNode(n->firstChild, nullSymbol, 1);
					switch(o1.type){
						case flag: emitQuad((quad){.op = JMPCND, sl, o1}); break;
						case label: break;
					}
					linearizeNode(n->firstChild->sibling, nullSymbol, 0);
					const bool isElse = n->firstChild->sibling != n->lastChild;
					uint32_t elseLbl;
					if(isElse){
						elseLbl = numLabels++;
						emitQuad((quad){.op = JMP, .o1 = (symbol){.type = label, .vReg = elseLbl}});
					}
					emitQuad((quad){.op = SETLABEL, .o1 = o1.type == label ? o1 : sl});
					if(isElse){
						linearizeNode(n->firstChild->sibling->sibling, nullSymbol, 0);
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
					linearizeNode(n->firstChild->sibling, nullSymbol, 0);
					emitQuad((quad){.op = SETLABEL, .o1 = sf});
					const symbol o1 = reverseFlag(linearizeNode(n->firstChild, sb, 1));
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
					if(n->firstChild != NULL){
						const symbol r1 = linearizeNode(n->firstChild, nullSymbol, 0);
						emitQuad((quad){.op = MOV, .o1 = (symbol){.type = physical, .vReg = 0}, r1});
					} emitQuad((quad){.op = RET});
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
				if(cn == NULL) break;
				linearizeNode(cn, nullSymbol, 0);
				if(cn == n->lastChild) break;
				cn = cn->sibling;
			}while(true);
		}
		default: return nullSymbol;
	}
}

#define maxQuads 4096

quad* linearizeAST(const node* baseNode){
	fncJmpLabels = malloc(sizeof(uint32_t) * getNumFuncs()); numQuads = 0;
	quadPool = newArena(sizeof(quad) * maxQuads); numLabels = 0;
	curTempVReg = getUsedVRegs(); fncsEncountered = 0;
	linearizeNode(baseNode, nullSymbol, 0);
	return quadPool.pool;
}