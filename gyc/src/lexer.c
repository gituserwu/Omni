#include <inputs.h>
#include <lexer.h>
#include <error.h>
#include <assert.h>
#include <mm.h>

#include <stdio.h>

#define IsDigit(c)         ((c) >= '0' && (c) <= '9')
#define IsOctDigit(c)      ((c) >= '0' && (c) <= '7')
#define IsHexDigit(c)      (IsDigit(c) || ((c) >= 'A' && (c) <= 'F') || ((c) >= 'a' && (c) <= 'f'))
#define IsLetter(c)        (((c) >= 'a' && (c) <= 'z') || ((c) == '_') || ((c) >= 'A' && (c) <= 'Z'))
#define IsUtf8Header(c)          (IsUtf8_2ByteHeader(c) || IsUtf8_3ByteHeader(c) || IsUtf8_4ByteHeader(c) || IsUtf8_5ByteHeader(c) || IsUtf8_6ByteHeader(c))
#define IsLetterOrDigit(c) (IsLetter(c) || IsDigit(c))
//#define ToUpper(c)		   (c & ~0x20)

#define CURSOR      (lexer->inputs->cur)
#define COLUMN      (lexer->inputs->col)

static int EatWhiteSpace(struct Lexer *lexer) {
    int len = 0;
    uint8 c = InputsCurrentChar(lexer->inputs);
    while(c == '\t' || c == ' ' || c == '\n') {
        len++;
        c = InputsNextChar(lexer->inputs);
    }
    return len;
}

static uint8 EatEscapeSequence(struct Lexer *lexer) {
    uint8 c = InputsNextChar(lexer->inputs);

    switch (c) {
    case '\'':
    case '\"':
    case '?':
    case '\\':
    case 'a':
    case 'b':
    case 'f':
    case 'n':
    case 'r':
    case 't':
    case 'v':
        return InputsNextChar(lexer->inputs);
    default:
        break;
    }

    if (IsOctDigit(c)) {
        c = InputsNextChar(lexer->inputs);
        if (IsOctDigit(c)) {
            c = InputsNextChar(lexer->inputs);
            if (IsOctDigit(c)) {
                c = InputsNextChar(lexer->inputs);
            }
        }
        return c;
    }

    if ('x' == c) {
        c = InputsNextChar(lexer->inputs);
        assert(IsHexDigit(c));
        while (IsHexDigit(c)) {
            c = InputsNextChar(lexer->inputs);
        }
    }
    return c;
}

static eToken ScanIdentifier(struct Lexer *lexer) {
    uint8 c = InputsCurrentChar(lexer->inputs);
    while (IsLetterOrDigit(c) || '_' == c || IsUtf8Header(c)) {
        c = InputsNextChar(lexer->inputs);
        if (InputUtf8_Error == c)
            Fatal(lexer->inputs, "不能识别的字符");
    }
    return TK_Id;
}

static eToken ScanCharLiteral(struct Lexer *lexer) {
    uint8 c = InputsNextChar(lexer->inputs);

    while ('\'' != c) {
        if ('\\' == c)
            c = EatEscapeSequence(lexer);
        else
            c = InputsNextChar(lexer->inputs);
    }
    c = InputsNextChar(lexer->inputs);

    return TK_CharConstant;
}

static eToken ScanStringLiteral(struct Lexer *lexer) {
    uint8 c = InputsNextChar(lexer->inputs);

    while ('\"' != c) {
        if ('\\' == c)
            c = EatEscapeSequence(lexer);
        else
            c = InputsNextChar(lexer->inputs);
    }
    c = InputsNextChar(lexer->inputs);

    return TK_StringLiteral;
}

static eToken ScanFloatLiteral(struct Lexer *lexer) {
    uint8 c = InputsCurrentChar(lexer->inputs);

    if ('.' == c) {
        c = InputsNextChar(lexer->inputs);
        while (IsDigit(c))
            c = InputsNextChar(lexer->inputs);
    }

    if ('e' == c || 'E' == c) {
        c = InputsNextChar(lexer->inputs);
        if ('+' == c || '-' == c)
            c = InputsNextChar(lexer->inputs);
        while (IsDigit(c))
            c = InputsNextChar(lexer->inputs);
    }

    while ('f' == c || 'F' == c || 'l' == c || 'L' == c)
        c = InputsNextChar(lexer->inputs);

    return TK_FloatingConstant;
}

static eToken ScanNumber(struct Lexer *lexer) {
    uint8 c = InputsCurrentChar(lexer->inputs);

    if ('0' == c) {
        c = InputsNextChar(lexer->inputs);
        if ('x' == c || 'X' == c) {
            c = InputsNextChar(lexer->inputs);
            while (IsHexDigit(c))
                c = InputsNextChar(lexer->inputs);
        } else {
            while (IsOctDigit(c))
                c = InputsNextChar(lexer->inputs);
        }
    } else {
        while (IsDigit(c))
            c = InputsNextChar(lexer->inputs);
    }

    if ('.' == c || 'e' == c || 'E' == c)
        return ScanFloatLiteral(lexer);

    while ('u' == c || 'U' == c || 'l' == c || 'L' == c)
        c = InputsNextChar(lexer->inputs);
    return TK_IntegerConstant;
}

static eToken ScanBadChar(struct Lexer *lexer) {
    uint8 c = InputsCurrentChar(lexer->inputs);
    Fatal(lexer->inputs, "非法字符:0x%x", c);
    c = InputsNextChar(lexer->inputs);
    return TK_BadChar;
}

static eToken ScanEof(struct Lexer *lexer) {
    return TK_End;
}

static eToken ScanMinus(struct Lexer *lexer) {
    uint8 c = InputsNextChar(lexer->inputs);

    if (IsDigit(c))
        return ScanNumber(lexer);

    return TK_Begin;
}

static eToken ScanPlus(struct Lexer *lexer) {
    uint8 c = InputsNextChar(lexer->inputs);

    if (IsDigit(c))
        return ScanNumber(lexer);

    return TK_Begin;
}

struct Lexer *CreateLexer() {
    struct Lexer *lexer = (struct Lexer *)malloc(sizeof(struct Lexer));
    lexer->keyWordsDic = 0;

    uint8 i = 0;
    for (i = 0; i < END_OF_FILE; i++) {
        if (IsLetter(i) || IsUtf8Header(i))
            lexer->scanners[i] = ScanIdentifier;
        else if (IsDigit(i))
            lexer->scanners[i] = ScanNumber;
        else
            lexer->scanners[i] = ScanBadChar;
    }
    lexer->scanners['_'] = ScanIdentifier;
    lexer->scanners['\''] = ScanCharLiteral;
    lexer->scanners['\"'] = ScanStringLiteral;
    lexer->scanners['.'] = ScanFloatLiteral;
    lexer->scanners['-'] = ScanMinus;
    lexer->scanners['+'] = ScanPlus;
    lexer->scanners[END_OF_FILE] = ScanEof;

    return lexer;
}

/*
 * LexerLoadKeywords - 从指定文件中装载关键字
 *
 * @lexer: 目标词法器
 * @filename: 关键字文件名
 */
void LexerLoadKeywords(struct Lexer *lexer, char *filename) {
    struct Inputs *inputs = CreateInputs(filename);
    assert(0 != inputs);
    struct Inputs *bk = lexer->inputs;
    lexer->inputs = inputs;

    lexer->keyWordsDic = CreateDictionary();

    struct Token *tk = GetNextToken(lexer);
    while (TK_End != tk->token) {
        DicInsertPair(lexer->keyWordsDic, tk->str, 0);
        DestroyToken(tk);
        tk = GetNextToken(lexer);
    }
    DestroyToken(tk);

    DestroyInputs(inputs);
    lexer->inputs = bk;
}

void LexerSetInputs(struct Lexer *lexer, struct Inputs *inputs) {
    lexer->inputs = inputs;
}

void DestroyLexer(struct Lexer *lexer) {
    lexer->inputs = 0;
    if (0 != lexer->keyWordsDic)
        DestroyDictionary(lexer->keyWordsDic);
    free(lexer);
}

#define SCANNER(plexer) ((plexer)->scanners)

struct Token *GetNextToken(struct Lexer *lexer) {
    InputsUnMark(lexer->inputs);
    EatWhiteSpace(lexer);

    InputsMark(lexer->inputs);
    uint8 c = InputsCurrentChar(lexer->inputs);
    struct Token *re = Calloc(1, struct Token);
    re->line = lexer->inputs->line;
    re->col = lexer->inputs->col;

    eToken tk = SCANNER(lexer)[c](lexer);
    int len = InputsGetMarkedLen(lexer->inputs);
    if (len < 0) {
        Free(re);
        return 0;
    }

    re->token = tk;
    re->str = Calloc(len+1, uint8);
    memcpy(re->str, lexer->inputs->mark, len);
    re->str[len] = '\0';

    if (0 != lexer->keyWordsDic && TK_Id == tk) {
        DicPairPtr pair = DicGetPair(lexer->keyWordsDic, re->str);
        if (0 != pair)
            re->token = TK_Keyword;
    }

    return re;
}

void DestroyToken(struct Token *token) {
    Free(token->str);
    Free(token);
}

