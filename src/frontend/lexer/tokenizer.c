#include "tokenizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* srcString;
typedef struct{
	char* str; uint16_t len;
}slice;

void setSrc(char* src){srcString = src;}

char eatChar(){return *(srcString++);}
char peekChar(){return *srcString;} char peekCharOffset(const uint32_t off){return *(srcString+off);}

bool isDelimiter(const char c){
	switch(c){
		case ' ': case '\n': case '\t': case '\v': case '\f': case '\r': return 1;
		default: return 0;
	}
}
int8_t isSingleCharToken(const char c){
	switch(c){
		case '+': return opPlus;
		case '*': return opMul;
		case '/': return opDiv;
		case '-': return opMinus;
		case '=': return opEqual;
		case '>': return opCmpGreater;
		case '<': return opCmpLess;
		case '&': return opBitwiseAnd;
		case '|': return opBitwiseOr;
		case '~': return opBitwiseNot;
		case '!': return opLogicalNot;
		case '(': return parenthesesL;
		case ')': return parenthesesR;
		case '{': return curlyBraceL;
		case '}': return curlyBraceR;
		case ';': case ',': return endStatement;
	} return -1;
}

typedef struct{char* str; uint8_t type;}keyData;

keyData keywords[] = {
	{"int", keywordInt}, {"char", keywordChar}, {"void", keywordVoid}, {"if", keywordIf}, {"else", keywordElse}, {"while", keywordWhile},
	{"return", keywordReturn}, {"continue", keywordContinue}, {"break", keywordBreak}
}; const uint8_t numKeywords = sizeof(keywords) / sizeof(keyData);

void verifyAlphabeticalOrder(){
	beginAlphaCheck:; bool inOrder = 1;
	for(uint8_t i1 = 0, i2 = 1; i2 < numKeywords; i1++, i2++){
		if(strcmp(keywords[i1].str, keywords[i2].str) >= 1){
			const keyData tmp = keywords[i1]; keywords[i1] = keywords[i2];
			keywords[i2] = tmp; inOrder = 0;
		}
	}if(!inOrder) goto beginAlphaCheck;
}

inline int8_t compareFixed(const char* nullTerm, char* nonTerm, const uint16_t len){
	const int8_t memDiff = strncmp(nullTerm, nonTerm, len);
	if(memDiff) return memDiff;
	return nullTerm[len] != '\0';
}

int8_t isKeyword(char* c, const uint16_t len){
	int8_t begin = 0, end = numKeywords-1;
	while(begin <= end){
		const int8_t splitter = (begin + end)/2;
		const int8_t cf = compareFixed(keywords[splitter].str, c, len);
		if(cf > 0) end = splitter-1;
		else if(cf < 0) begin = splitter+1;
		else return keywords[splitter].type;
	} return -1;
}

token getNextToken(){
	uint16_t length = 0;
	while(isDelimiter(peekChar())){eatChar();} int8_t isSC = -1; char curC; char* start = srcString;
	if(*start == '\0') return (token){.type = nullToken};
	while(curC = peekChar(), !(isDelimiter(curC) | ((isSC = isSingleCharToken(curC))+1) | curC == '\0')){
		length++; eatChar();
	}if(isSC != -1 && !length)return eatChar(), (token){.type = isSC};
	token ret = {.str = start};
	switch(const int8_t isk = isKeyword(start, length)){
		case -1: if(*start >= '0' && *start <= '9'){
			const char tmpc = start[length]; start[length] = '\0';
			ret.val = strtol(start, NULL, 0); start[length] = tmpc;
			return ret.type = literal, ret;
		}return ret.type = identifier, ret.len = length, ret;
		default: return ret.type = isk, ret.len = length, ret;
	}
}

#define minTokenAmount 128

tokenArray tokenizeSource(char* src){
	setSrc(src); token curT; uint32_t numAllocated = minTokenAmount; tokenArray ret = {.tokens = malloc(sizeof(token) * minTokenAmount), .numTokens = 0};
	while((curT = getNextToken()).type != nullToken){
		if(++ret.numTokens > numAllocated) ret.tokens = realloc(ret.tokens, numAllocated *= 2);
		ret.tokens[ret.numTokens-1] = curT;
	} return ret;
}