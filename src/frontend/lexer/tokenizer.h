#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef enum tokenType : uint8_t{
	opPlus, opMinus, opDecrement, opIncrement, opDPlus, opDMinus, opEqual, opMul, opDiv, opNegate, opLogicalOr, opLogicalAnd, opLogicalNot, opComma, opBitwiseNot, opBitwiseOr, opBitwiseXor,
	opShiftRight, opShiftLeft, opBitwiseAnd, opDereference, opReference, opAccess, opIndirectAccess, opCmpEquals, opCmpGreater, opCmpLess, opCmpGrEq, opCmpLeEq, opCmpNe,
	curlyBraceR, curlyBraceL, parenthesesL, parenthesesR, squareBraceL, squareBraceR,
	keywordStruct, keywordStructPtr, keywordIf, keywordElse, keywordWhile, keywordFor, keywordSwitch, keywordCase, keywordDefault, keywordInt, keywordChar, keywordIntPtr, keywordCharPtr, keywordVoidPtr, keywordVoid, keywordReturn, keywordContinue, keywordBreak,
	endStatement, colon, identifier, literal, structTypeToken, nullToken
}tokenType;

typedef struct{
	tokenType type;
	int val; uint32_t len;
	char* str;
}token;

typedef struct{
	token* tokens;
	uint32_t numTokens;
}tokenArray;

tokenArray tokenizeSource(char* src);
void printToken(const token t);
void printTokens(const tokenArray arr);

void verifyAlphabeticalOrder();
