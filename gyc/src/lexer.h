#ifndef LEXER_H
#define LEXER_H

#include <token.h>
#include <symboltable.h>
#include <inputs.h>

struct Lexer;
typedef eToken (*ScanFunc)(struct Lexer *lexer);

struct Lexer {
    struct Inputs *inputs;          // 输入流,由用户提供
    struct SymbolTable *symTable;   // 符号表,由用户提供
    ScanFunc scanners[256];
};

struct Lexer *CreateLexer();
void LexerLoadKeywords(struct Lexer *lexer, char *filename);
void LexerSetInputs(struct Lexer *lexer, struct Inputs *inputs);
void DestroyLexer(struct Lexer *lexer);

struct Token *GetNextToken(struct Lexer *lexer);

#endif // LEXER_H



