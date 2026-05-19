#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef enum tokenType : uint8_t{
	opPlus, opMinus, opDecrement, opIncrement, opEqual, opMul, opDiv, opNegate, opLogicalOr, opLogicalAnd, opLogicalNot, opBitwiseNot, opBitwiseOr, opShiftRight, opShiftLeft, opBitwiseAnd, opDereference, opReference, opCmpEquals, opCmpGreater, opCmpLess, opCmpGrEq, opCmpLeEq,
	curlyBraceR, curlyBraceL, parenthesesL, parenthesesR,
	keywordIf, keywordElse, keywordWhile, keywordInt, keywordChar, keywordIntPtr, keywordCharPtr, keywordVoidPtr, keywordVoid, keywordReturn, keywordContinue, keywordBreak,
	endStatement, identifier, literal, nullToken
}tokenType;

typedef struct{
	tokenType type;
	union{
		int val; uint32_t len;
	}; char* str;
}token;

typedef struct{
	token* tokens;
	uint32_t numTokens;
}tokenArray;

tokenArray tokenizeSource(char* src);

void verifyAlphabeticalOrder();
