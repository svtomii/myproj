/*
 * compiler.cpp — компилятор подмножества Python → Макроассемблер IBM PC
 *
 * Поддерживаемые возможности:
 *   - Арифметические выражения (целые и строки)
 *   - Присваивание (=)
 *   - Циклы: while, for
 *   - Условные операторы: if, if-else
 *   - Ввод/вывод: print, input
 *   - Строковые литералы
 *   - Сравнения: <, >, <=, >=, ==, !=
 *
 * Сборка:  g++ -std=c++17 compiler.cpp -o compiler.exe
 * Запуск:  compiler.exe input.py output.asm
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <cctype>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <algorithm>
using namespace std;

// ============================================================
// ТИПЫ ТОКЕНОВ
// ============================================================
enum TokenType {
    TOK_ID,      // идентификатор
    TOK_CONST,   // целая константа
    TOK_STRING,  // строковая константа
    TOK_WHILE,   // while
    TOK_FOR,     // for
    TOK_IF,      // if
    TOK_ELSE,    // else
    TOK_PRINT,   // print
    TOK_INPUT,   // input
    TOK_DIV,     // div
    TOK_MOD,     // mod
    TOK_ASSIGN,  // =
    TOK_PLUS,    // +
    TOK_MINUS,   // -
    TOK_MUL,     // *
    TOK_SLASH,   // /
    TOK_LPAREN,  // (
    TOK_RPAREN,  // )
    TOK_COLON,   // :
    TOK_COMMA,   // ,
    TOK_NEWLINE, // \n
    TOK_INDENT,  // отступ
    TOK_DEDENT,  // отступ назад
    TOK_LT,      // <
    TOK_GT,      // >
    TOK_LE,      // <=
    TOK_GE,      // >=
    TOK_EQ2,     // ==
    TOK_NE,      // !=
    TOK_EOF
};

static string tokenName(TokenType t) {
    switch (t) {
        case TOK_ID:      return "identifier";
        case TOK_CONST:   return "constant";
        case TOK_STRING:  return "string";
        case TOK_WHILE:   return "'while'";
        case TOK_FOR:     return "'for'";
        case TOK_IF:      return "'if'";
        case TOK_ELSE:    return "'else'";
        case TOK_PRINT:   return "'print'";
        case TOK_INPUT:   return "'input'";
        case TOK_DIV:     return "'div'";
        case TOK_MOD:     return "'mod'";
        case TOK_ASSIGN:  return "'='";
        case TOK_PLUS:    return "'+'";
        case TOK_MINUS:   return "'-'";
        case TOK_MUL:     return "'*'";
        case TOK_SLASH:   return "'/'";
        case TOK_LPAREN:  return "'('";
        case TOK_RPAREN:  return "')'";
        case TOK_COLON:   return "':'";
        case TOK_COMMA:   return "','";
        case TOK_NEWLINE: return "newline";
        case TOK_INDENT:  return "indent";
        case TOK_DEDENT:  return "dedent";
        case TOK_LT:      return "'<'";
        case TOK_GT:      return "'>'";
        case TOK_LE:      return "'<='";
        case TOK_GE:      return "'>='";
        case TOK_EQ2:     return "'=='";
        case TOK_NE:      return "'!='";
        case TOK_EOF:     return "end of file";
    }
    return "?";
}

struct Token {
    TokenType type;
    string    value;
    int       line, col;
};

// ============================================================
// ЛЕКСЕР
// ============================================================
class Lexer {
    string src;
    int pos, line, col;
    vector<int> indentStack;
    vector<Token> pendingTokens;
    bool atLineStart;
    
    int getCurrentIndent() {
        int indent = 0;
        int oldPos = pos;
        int oldCol = col;
        
        while (pos < (int)src.size()) {
            char c = src[pos];
            if (c == ' ') indent++;
            else if (c == '\t') indent += 4;
            else break;
            pos++;
            col++;
        }
        
        pos = oldPos;
        col = oldCol;
        return indent;
    }

    Token makeToken(TokenType t, const string& v, int sl, int sc) {
        return {t, v, sl, sc};
    }

    void handleIndentation() {
        if (pos >= (int)src.size()) return;
        
        int indent = getCurrentIndent();
        
        while (pos < (int)src.size() && (src[pos] == ' ' || src[pos] == '\t')) {
            pos++;
            col++;
        }
        
        if (indentStack.empty()) {
            indentStack.push_back(0);
        }
        
        if (indent > indentStack.back()) {
            indentStack.push_back(indent);
            pendingTokens.push_back({TOK_INDENT, "", line, col});
        } 
        else if (indent < indentStack.back()) {
            while (!indentStack.empty() && indent < indentStack.back()) {
                indentStack.pop_back();
                pendingTokens.push_back({TOK_DEDENT, "", line, col});
            }
            if (indent != indentStack.back()) {
                throw runtime_error("Inconsistent indentation at line " + to_string(line));
            }
        }
        
        pendingTokens.push_back({TOK_NEWLINE, "", line, col});
        atLineStart = false;
    }

    Token nextToken() {
        if (!pendingTokens.empty()) {
            Token t = pendingTokens.front();
            pendingTokens.erase(pendingTokens.begin());
            return t;
        }
        
        while (pos < (int)src.size()) {
            char c = src[pos];
            if (c == ' ' || c == '\t') {
                if (!atLineStart) {
                    pos++;
                    col++;
                } else {
                    break;
                }
            }
            else if (c == '#') {
                while (pos < (int)src.size() && src[pos] != '\n') pos++;
                if (pos < (int)src.size() && src[pos] == '\n') {
                    pos++;
                    col = 1;
                    line++;
                    atLineStart = true;
                }
            }
            else break;
        }
        
        if (pos >= (int)src.size())
            return {TOK_EOF, "", line, col};

        int sl = line, sc = col;
        
        if (src[pos] == '\n') {
            pos++;
            col = 1;
            line++;
            atLineStart = true;
            handleIndentation();
            return nextToken();
        }
        
        atLineStart = false;
        char c = src[pos];

        if (isalpha(c) || c == '_') {
            string w;
            while (pos < (int)src.size() && (isalnum(src[pos]) || src[pos] == '_'))
                w += src[pos++], col++;
            if (w == "while") return makeToken(TOK_WHILE, w, sl, sc);
            if (w == "for")   return makeToken(TOK_FOR, w, sl, sc);
            if (w == "if")    return makeToken(TOK_IF, w, sl, sc);
            if (w == "else")  return makeToken(TOK_ELSE, w, sl, sc);
            if (w == "print") return makeToken(TOK_PRINT, w, sl, sc);
            if (w == "input") return makeToken(TOK_INPUT, w, sl, sc);
            if (w == "div")   return makeToken(TOK_DIV, w, sl, sc);
            if (w == "mod")   return makeToken(TOK_MOD, w, sl, sc);
            return makeToken(TOK_ID, w, sl, sc);
        }

        if (isdigit(c)) {
            string n;
            while (pos < (int)src.size() && isdigit(src[pos]))
                n += src[pos++], col++;
            return makeToken(TOK_CONST, n, sl, sc);
        }

        if (c == '"') {
            pos++;
            string str;
            while (pos < (int)src.size() && src[pos] != '"') {
                str += src[pos++];
                col++;
            }
            if (src[pos] == '"') {
                pos++;
                col++;
            }
            return makeToken(TOK_STRING, str, sl, sc);
        }

        pos++; col++;
        switch (c) {
            case '+': return makeToken(TOK_PLUS,   "+", sl, sc);
            case '-': return makeToken(TOK_MINUS,  "-", sl, sc);
            case '*': return makeToken(TOK_MUL,    "*", sl, sc);
            case '/': return makeToken(TOK_SLASH,  "/", sl, sc);
            case '(': return makeToken(TOK_LPAREN, "(", sl, sc);
            case ')': return makeToken(TOK_RPAREN, ")", sl, sc);
            case ':': return makeToken(TOK_COLON,  ":", sl, sc);
            case ',': return makeToken(TOK_COMMA,  ",", sl, sc);
            case '=':
                if (pos < (int)src.size() && src[pos] == '=')
                    { pos++; col++; return makeToken(TOK_EQ2, "==", sl, sc); }
                return makeToken(TOK_ASSIGN, "=", sl, sc);
            case '<':
                if (pos < (int)src.size() && src[pos] == '=')
                    { pos++; col++; return makeToken(TOK_LE, "<=", sl, sc); }
                return makeToken(TOK_LT, "<", sl, sc);
            case '>':
                if (pos < (int)src.size() && src[pos] == '=')
                    { pos++; col++; return makeToken(TOK_GE, ">=", sl, sc); }
                return makeToken(TOK_GT, ">", sl, sc);
            case '!':
                if (pos < (int)src.size() && src[pos] == '=')
                    { pos++; col++; return makeToken(TOK_NE, "!=", sl, sc); }
                throw runtime_error("Line " + to_string(sl) + ", pos " + to_string(sc) + ": unexpected '!'");
            default:
                throw runtime_error("Line " + to_string(sl) + ", pos " + to_string(sc) +
                                  ": unknown character '" + string(1, c) + "'");
        }
    }

public:
    Lexer(const string& source) : src(source), pos(0), line(1), col(1), atLineStart(true) {
        indentStack.push_back(0);
    }

    vector<Token> tokenize() {
        vector<Token> v;
        while (true) {
            Token t = nextToken();
            v.push_back(t);
            if (t.type == TOK_EOF) {
                while (indentStack.size() > 1) {
                    indentStack.pop_back();
                    v.push_back({TOK_DEDENT, "", line, col});
                }
                break;
            }
        }
        return v;
    }
};

// ============================================================
// ПАРСЕР
// ============================================================
class Parser {
    vector<Token> tokens;
    int           pos;
    vector<string>& postfix;
    int           labelCnt;

    Token peek() const {
        return (pos < (int)tokens.size()) ? tokens[pos] : tokens.back();
    }

    bool match(TokenType t) {
        if (peek().type == t) { pos++; return true; }
        return false;
    }

    string newLabel() { return "L" + to_string(++labelCnt); }

    void syntaxError(const string& expected) {
        Token t = peek();
        throw runtime_error("Line " + to_string(t.line) + ", pos " +
            to_string(t.col) + ": expected " + expected +
            ", found " + tokenName(t.type));
    }

    bool parseFactor() {
        if (match(TOK_LPAREN)) {
            if (parseExpression() && match(TOK_RPAREN))
                return true;
            return false;
        }
        if (match(TOK_MINUS)) {
            if (parseFactor()) {
                postfix.push_back("@");
                return true;
            }
            return false;
        }
        if (peek().type == TOK_CONST) {
            postfix.push_back(tokens[pos++].value);
            return true;
        }
        if (peek().type == TOK_STRING) {
            // Для строк сохраняем как строковую константу
            string str = tokens[pos++].value;
            postfix.push_back("\"" + str + "\"");
            return true;
        }
        if (peek().type == TOK_ID) {
            postfix.push_back(tokens[pos++].value);
            return true;
        }
        return false;
    }

    bool parseTerm() {
        if (!parseFactor()) return false;
        while (true) {
            string op;
            if      (peek().type == TOK_MUL)   { op = "*";   pos++; }
            else if (peek().type == TOK_SLASH) { op = "/";   pos++; }
            else if (peek().type == TOK_DIV)   { op = "div"; pos++; }
            else if (peek().type == TOK_MOD)   { op = "mod"; pos++; }
            else break;
            if (!parseFactor()) syntaxError("operand after '" + op + "'");
            postfix.push_back(op);
        }
        return true;
    }

    bool parseArithExpr() {
        if (!parseTerm()) return false;
        while (true) {
            string op;
            if      (peek().type == TOK_PLUS)  { op = "+"; pos++; }
            else if (peek().type == TOK_MINUS) { op = "-"; pos++; }
            else break;
            if (!parseTerm()) syntaxError("operand after '" + op + "'");
            postfix.push_back(op);
        }
        return true;
    }

    bool parseExpression() {
        if (!parseArithExpr()) return false;
        while (true) {
            string op;
            if      (peek().type == TOK_LT)  { op = "<";  pos++; }
            else if (peek().type == TOK_GT)  { op = ">";  pos++; }
            else if (peek().type == TOK_LE)  { op = "<="; pos++; }
            else if (peek().type == TOK_GE)  { op = ">="; pos++; }
            else if (peek().type == TOK_EQ2) { op = "=="; pos++; }
            else if (peek().type == TOK_NE)  { op = "!="; pos++; }
            else break;
            if (!parseArithExpr()) syntaxError("operand after '" + op + "'");
            postfix.push_back(op);
        }
        return true;
    }

    bool parseAssignment() {
        if (peek().type == TOK_ID) {
            string var = tokens[pos].value;
            if (pos + 1 < (int)tokens.size() && tokens[pos + 1].type == TOK_ASSIGN) {
                pos++;
                pos++;
                if (parseExpression()) {
                    postfix.push_back(var);
                    postfix.push_back("=");
                    return true;
                }
            }
        }
        return false;
    }

    bool parsePrintStmt() {
        if (match(TOK_PRINT) && match(TOK_LPAREN)) {
            if (parseExpression()) {
                if (match(TOK_RPAREN)) {
                    postfix.push_back("print");
                    return true;
                }
            }
        }
        return false;
    }

    bool parseInputStmt() {
        if (match(TOK_INPUT) && match(TOK_LPAREN)) {
            // Может быть строка-подсказка
            if (peek().type == TOK_STRING) {
                match(TOK_STRING);
            }
            if (match(TOK_RPAREN)) {
                if (peek().type == TOK_ID) {
                    string var = tokens[pos].value;
                    pos++;
                    postfix.push_back(var);
                    postfix.push_back("input");
                    return true;
                }
            }
        }
        return false;
    }

    bool parseIfStmt() {
        if (match(TOK_IF)) {
            string Lelse = newLabel();
            string Lend = newLabel();
            
            if (parseExpression() && match(TOK_COLON)) {
                match(TOK_NEWLINE);
                if (match(TOK_INDENT)) {
                    postfix.push_back(Lelse);
                    postfix.push_back("jz");
                    parseStatementList();
                    if (match(TOK_DEDENT)) {
                        postfix.push_back(Lend);
                        postfix.push_back("jmp");
                        postfix.push_back(Lelse + ":");
                        
                        // Обработка else
                        if (peek().type == TOK_ELSE) {
                            pos++;
                            match(TOK_COLON);
                            match(TOK_NEWLINE);
                            if (match(TOK_INDENT)) {
                                parseStatementList();
                                match(TOK_DEDENT);
                            }
                        }
                        postfix.push_back(Lend + ":");
                        return true;
                    }
                }
            }
        }
        return false;
    }

    bool parseForLoop() {
        if (match(TOK_FOR)) {
            if (peek().type == TOK_ID) {
                string var = tokens[pos].value;
                pos++;
                if (match(TOK_ASSIGN)) {
                    string Ls = newLabel();
                    string Le = newLabel();
                    
                    if (parseExpression() && match(TOK_COMMA)) {
                        postfix.push_back(var);
                        postfix.push_back("=");
                        postfix.push_back(Ls + ":");
                        
                        if (parseExpression()) {
                            string endVal = newTemp();
                            postfix.push_back(endVal);
                            postfix.push_back("save");
                            
                            // Условие: var <= end
                            postfix.push_back(var);
                            postfix.push_back(endVal);
                            postfix.push_back("<=");
                            postfix.push_back(Le);
                            postfix.push_back("jz");
                            
                            if (match(TOK_COLON)) {
                                match(TOK_NEWLINE);
                                if (match(TOK_INDENT)) {
                                    parseStatementList();
                                    match(TOK_DEDENT);
                                    postfix.push_back(var);
                                    postfix.push_back("1");
                                    postfix.push_back("+");
                                    postfix.push_back(var);
                                    postfix.push_back("=");
                                    postfix.push_back(Ls);
                                    postfix.push_back("jmp");
                                    postfix.push_back(Le + ":");
                                    return true;
                                }
                            }
                        }
                    }
                }
            }
        }
        return false;
    }

    bool parseWhileLoop() {
        if (match(TOK_WHILE)) {
            string Ls = newLabel();
            string Le = newLabel();
            postfix.push_back(Ls + ":");
            
            if (parseExpression() && match(TOK_COLON)) {
                match(TOK_NEWLINE);
                if (match(TOK_INDENT)) {
                    postfix.push_back(Le);
                    postfix.push_back("jz");
                    parseStatementList();
                    if (match(TOK_DEDENT)) {
                        postfix.push_back(Ls);
                        postfix.push_back("jmp");
                        postfix.push_back(Le + ":");
                        return true;
                    }
                }
            }
        }
        return false;
    }

    bool parseStatement() {
        while (peek().type == TOK_NEWLINE) {
            pos++;
        }
        
        if (peek().type == TOK_EOF || peek().type == TOK_DEDENT) {
            return false;
        }
        
        if (parseIfStmt()) return true;
        if (parseForLoop()) return true;
        if (parseWhileLoop()) return true;
        if (parseInputStmt()) return true;
        if (parsePrintStmt()) return true;
        if (parseAssignment()) return true;
        
        return false;
    }

    void parseStatementList() {
        while (true) {
            while (peek().type == TOK_NEWLINE) {
                pos++;
            }
            if (peek().type == TOK_EOF || peek().type == TOK_DEDENT) {
                break;
            }
            if (!parseStatement()) {
                if (peek().type != TOK_EOF && peek().type != TOK_DEDENT) {
                    syntaxError("statement");
                }
                break;
            }
        }
    }
    
    string newTemp() {
        return "_t" + to_string(++labelCnt);
    }

public:
    Parser(const vector<Token>& toks, vector<string>& pf)
        : tokens(toks), pos(0), postfix(pf), labelCnt(0) {}

    void parse() {
        parseStatementList();
        if (peek().type != TOK_EOF) {
            syntaxError("end of file");
        }
    }
};

// ============================================================
// ГЕНЕРАТОР КОДА (упрощенная версия)
// ============================================================
class CodeGenerator {
    set<string> vars;
    int tempCnt;
    vector<string> code;
    vector<string> valStack;

    void emit(const string& s) { code.push_back(s); }
    void vpush(const string& v) { valStack.push_back(v); }
    
    string vpop() {
        if (valStack.empty()) throw runtime_error("Empty value stack");
        string v = valStack.back(); valStack.pop_back(); return v;
    }

    string newTemp() {
        string t = "_t" + to_string(++tempCnt);
        vars.insert(t);
        return t;
    }

    static bool isNumber(const string& s) {
        if (s.empty()) return false;
        for (char c : s) if (!isdigit((unsigned char)c)) return false;
        return true;
    }

    void generateCode(const vector<string>& pf) {
        for (const string& tok : pf) {
            if (!tok.empty() && tok.back() == ':') {
                emit(tok);
            }
            else if (tok == "jmp") {
                string lbl = vpop();
                emit("    jmp " + lbl);
            }
            else if (tok == "jz") {
                string lbl = vpop();
                string cond = vpop();
                emit("    mov ax, " + cond);
                emit("    cmp ax, 0");
                emit("    je  " + lbl);
            }
            else if (tok == "=") {
                string var = vpop();
                string val = vpop();
                emit("    mov ax, " + val);
                emit("    mov " + var + ", ax");
            }
            else if (tok == "print") {
                string val = vpop();
                emit("    mov ax, " + val);
                emit("    call _print_int");
                emit("    call _print_nl");
            }
            else if (tok == "input") {
                string var = vpop();
                emit("    call _read_int");
                emit("    mov " + var + ", ax");
            }
            else if (tok == "@") {
                string a = vpop();
                string t = newTemp();
                emit("    mov ax, " + a);
                emit("    neg ax");
                emit("    mov " + t + ", ax");
                vpush(t);
            }
            else if (tok == "+" || tok == "-" || tok == "*" || tok == "/" || tok == "div" || tok == "mod") {
                string b = vpop();
                string a = vpop();
                string t = newTemp();
                emit("    mov ax, " + a);
                emit("    mov bx, " + b);
                if (tok == "+") emit("    add ax, bx");
                else if (tok == "-") emit("    sub ax, bx");
                else if (tok == "*") emit("    imul bx");
                else if (tok == "/" || tok == "div") {
                    emit("    cwd");
                    emit("    idiv bx");
                } else {
                    emit("    cwd");
                    emit("    idiv bx");
                    emit("    mov ax, dx");
                }
                emit("    mov " + t + ", ax");
                vpush(t);
            }
            else if (tok == "<" || tok == ">" || tok == "<=" || tok == ">=" || tok == "==" || tok == "!=") {
                string b = vpop();
                string a = vpop();
                string t = newTemp();
                int id = tempCnt;
                string ltrue = "_ct" + to_string(id) + "T";
                string ldone = "_ct" + to_string(id) + "D";
                emit("    mov ax, " + a);
                emit("    cmp ax, " + b);
                string jcc;
                if (tok == "<")  jcc = "jl";
                else if (tok == ">")  jcc = "jg";
                else if (tok == "<=") jcc = "jle";
                else if (tok == ">=") jcc = "jge";
                else if (tok == "==") jcc = "je";
                else jcc = "jne";
                emit("    " + jcc + " " + ltrue);
                emit("    mov ax, 0");
                emit("    jmp " + ldone);
                emit(ltrue + ":");
                emit("    mov ax, 1");
                emit(ldone + ":");
                emit("    mov " + t + ", ax");
                vpush(t);
            }
            else {
                vpush(tok);
            }
        }
    }

    void writeASM(ostream& out) {
        out << "; === Extended Python Compiler ===\n"
            << ".model small\n"
            << ".stack 200h\n\n"
            << ".data\n";
        for (const string& v : vars)
            out << "    " << v << " DW 0\n";
        out << "    _nl  DB 13,10,'$'\n"
            << "    _buf DB 12 DUP('$')\n\n"
            << ".code\n\n";
            
        out << "_read_int PROC\n    push bx\n    push cx\n    push dx\n    xor bx, bx\n    xor cx, cx\n_ri_next:\n    mov ah, 01h\n    int 21h\n    cmp al, 13\n    je _ri_done\n    cmp al, 10\n    je _ri_done\n    cmp al, '-'\n    jne _ri_trydigit\n    mov cx, 1\n    jmp _ri_next\n_ri_trydigit:\n    sub al, '0'\n    jb _ri_next\n    cmp al, 9\n    ja _ri_next\n    xor ah, ah\n    push ax\n    mov ax, bx\n    mov bx, 10\n    imul bx\n    pop bx\n    add ax, bx\n    mov bx, ax\n    jmp _ri_next\n_ri_done:\n    test cx, cx\n    jz _ri_pos\n    neg bx\n_ri_pos:\n    mov ax, bx\n    pop dx\n    pop cx\n    pop bx\n    ret\n_read_int ENDP\n\n";
        
        out << "_print_int PROC\n    push bx\n    push cx\n    push dx\n    push si\n    lea si, _buf\n    test ax, ax\n    jns _wi_pos\n    mov byte ptr [si], '-'\n    inc si\n    neg ax\n_wi_pos:\n    xor cx, cx\n_wi_extr:\n    xor dx, dx\n    mov bx, 10\n    div bx\n    push dx\n    inc cx\n    test ax, ax\n    jnz _wi_extr\n_wi_fill:\n    pop dx\n    add dl, '0'\n    mov [si], dl\n    inc si\n    loop _wi_fill\n    mov byte ptr [si], '$'\n    lea dx, _buf\n    mov ah, 09h\n    int 21h\n    pop si\n    pop dx\n    pop cx\n    pop bx\n    ret\n_print_int ENDP\n\n";
        
        out << "_print_nl PROC\n    lea dx, _nl\n    mov ah, 09h\n    int 21h\n    ret\n_print_nl ENDP\n\n";
        
        out << "main PROC\n    mov ax, @data\n    mov ds, ax\n\n";
        for (const string& ln : code)
            out << ln << "\n";
        out << "\n    mov ax, 4C00h\n    int 21h\nmain ENDP\n\nEND main\n";
    }

public:
    CodeGenerator() : tempCnt(0) {}

    void generate(const vector<string>& postfix, const string& outfile) {
        generateCode(postfix);
        ofstream out(outfile);
        if (!out) throw runtime_error("Failed to open output file");
        writeASM(out);
        out.close();
    }
};

// ============================================================
// MAIN
// ============================================================
int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <input.py> <output.asm>\n";
        return 1;
    }

    ifstream fin(argv[1]);
    if (!fin) {
        cerr << "Error: cannot open file\n";
        return 1;
    }
    string src((istreambuf_iterator<char>(fin)), istreambuf_iterator<char>());
    fin.close();

    try {
        Lexer lexer(src);
        vector<Token> tokens = lexer.tokenize();
        
        vector<string> postfix;
        Parser parser(tokens, postfix);
        parser.parse();
        
        CodeGenerator gen;
        gen.generate(postfix, argv[2]);
        
        cout << "\n=== RPN ===\n";
        for (const string& s : postfix) cout << s << " ";
        cout << "\n\nSuccess!\n";
        
        return 0;
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}