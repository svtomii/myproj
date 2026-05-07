/*
 * compiler.cpp — компилятор подмножества Python → Макроассемблер IBM PC
 *
 * Вариант:
 *   Язык источника : Python (подмножество)
 *   Подмножество   : целые выражения, =, while, read, write
 *   Промежуточный  : Обратная Польская Нотация (ОПН)
 *   Целевой код    : MASM/TASM 16-bit DOS
 *   Метод разбора  : нисходящий с возвратами (backtracking)
 *   Язык реализации: C++17
 *
 * Сборка:  g++ -std=c++17 compiler.cpp -o compiler
 * Запуск:  ./compiler input.py output.asm
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
using namespace std;

// ============================================================
// ТИПЫ ТОКЕНОВ
// ============================================================
enum TokenType {
    TOK_ID,      // идентификатор
    TOK_CONST,   // целая константа
    TOK_WHILE,   // while
    TOK_READ,    // read
    TOK_WRITE,   // write
    TOK_DIV,     // div
    TOK_MOD,     // mod
    TOK_ASSIGN,  // =
    TOK_PLUS,    // +
    TOK_MINUS,   // -
    TOK_MUL,     // *
    TOK_SLASH,   // /
    TOK_LPAREN,  // (
    TOK_RPAREN,  // )
    TOK_LBRACE,  // {
    TOK_RBRACE,  // }
    TOK_SEMICOL, // ;
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
        case TOK_ID:      return "идентификатор";
        case TOK_CONST:   return "константа";
        case TOK_WHILE:   return "'while'";
        case TOK_READ:    return "'read'";
        case TOK_WRITE:   return "'write'";
        case TOK_DIV:     return "'div'";
        case TOK_MOD:     return "'mod'";
        case TOK_ASSIGN:  return "'='";
        case TOK_PLUS:    return "'+'";
        case TOK_MINUS:   return "'-'";
        case TOK_MUL:     return "'*'";
        case TOK_SLASH:   return "'/'";
        case TOK_LPAREN:  return "'('";
        case TOK_RPAREN:  return "')'";
        case TOK_LBRACE:  return "'{'";
        case TOK_RBRACE:  return "'}'";
        case TOK_SEMICOL: return "';'";
        case TOK_LT:      return "'<'";
        case TOK_GT:      return "'>'";
        case TOK_LE:      return "'<='";
        case TOK_GE:      return "'>='";
        case TOK_EQ2:     return "'=='";
        case TOK_NE:      return "'!='";
        case TOK_EOF:     return "конец файла";
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

    void skipWS() {
        while (pos < (int)src.size()) {
            char c = src[pos];
            if (c == '\n')              { line++; col = 1; pos++; }
            else if (c == ' ' || c == '\t' || c == '\r') { col++; pos++; }
            else if (c == '#') {
                // Python-комментарий до конца строки
                while (pos < (int)src.size() && src[pos] != '\n') pos++;
            }
            else break;
        }
    }

    Token makeToken(TokenType t, const string& v, int sl, int sc) {
        return {t, v, sl, sc};
    }

    Token nextToken() {
        skipWS();
        if (pos >= (int)src.size())
            return {TOK_EOF, "", line, col};

        int sl = line, sc = col;
        char c = src[pos];

        // Идентификаторы и ключевые слова
        if (isalpha(c) || c == '_') {
            string w;
            while (pos < (int)src.size() && (isalnum(src[pos]) || src[pos] == '_'))
                w += src[pos++], col++;
            if (w == "while") return makeToken(TOK_WHILE, w, sl, sc);
            if (w == "read")  return makeToken(TOK_READ,  w, sl, sc);
            if (w == "write") return makeToken(TOK_WRITE, w, sl, sc);
            if (w == "div")   return makeToken(TOK_DIV,   w, sl, sc);
            if (w == "mod")   return makeToken(TOK_MOD,   w, sl, sc);
            return makeToken(TOK_ID, w, sl, sc);
        }

        // Целые константы
        if (isdigit(c)) {
            string n;
            while (pos < (int)src.size() && isdigit(src[pos]))
                n += src[pos++], col++;
            return makeToken(TOK_CONST, n, sl, sc);
        }

        // Операторы и разделители
        pos++; col++;
        switch (c) {
            case '+': return makeToken(TOK_PLUS,   "+", sl, sc);
            case '-': return makeToken(TOK_MINUS,  "-", sl, sc);
            case '*': return makeToken(TOK_MUL,    "*", sl, sc);
            case '/': return makeToken(TOK_SLASH,  "/", sl, sc);
            case '(': return makeToken(TOK_LPAREN, "(", sl, sc);
            case ')': return makeToken(TOK_RPAREN, ")", sl, sc);
            case '{': return makeToken(TOK_LBRACE, "{", sl, sc);
            case '}': return makeToken(TOK_RBRACE, "}", sl, sc);
            case ';': return makeToken(TOK_SEMICOL,";", sl, sc);
            case '=':
                if (pos < (int)src.size() && src[pos] == '=')
                    { pos++; col++; return makeToken(TOK_EQ2,   "==", sl, sc); }
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
                // fall through → лексическая ошибка
        }
        // Лексическая ошибка
        throw runtime_error("Строка " + to_string(sl) + ", позиция " + to_string(sc) +
                            ": неизвестный символ '" + string(1, c) + "'");
    }

public:
    Lexer(const string& source) : src(source), pos(0), line(1), col(1) {}

    // Токенизировать весь файл в вектор
    vector<Token> tokenize() {
        vector<Token> v;
        while (true) {
            Token t = nextToken();
            v.push_back(t);
            if (t.type == TOK_EOF) break;
        }
        return v;
    }
};

// ============================================================
// ПАРСЕР — Нисходящий с возвратами (Recursive Descent + Backtracking)
//
// Принцип работы (методичка стр. 9–11):
//   • Каждая parseX() возвращает bool.
//   • Перед альтернативой: saveAll() → сохраняем pos И размер postfix.
//   • Если попытка провалилась: restoreAll() → откатываем оба.
//   • Если попытка успешна: возвращаем true, не откатываем.
//
// Это позволяет безопасно пробовать несколько грамматических правил подряд.
// ============================================================
class Parser {
    vector<Token> tokens;
    int           pos;
    vector<string>& postfix;  // строится в процессе разбора
    int           labelCnt;

    // ── Структура для backtracking ──────────────────────────
    struct Checkpoint {
        int tokenPos;   // текущий индекс в массиве токенов
        int pfSize;     // размер вектора postfix
    };

    // Сохранить точку возврата
    Checkpoint saveAll() const {
        return { pos, (int)postfix.size() };
    }

    // Восстановить точку возврата (откатить и токены, и постфикс)
    void restoreAll(const Checkpoint& cp) {
        pos = cp.tokenPos;
        postfix.resize(cp.pfSize);
    }
    // ────────────────────────────────────────────────────────

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
        throw runtime_error("Строка " + to_string(t.line) + ", позиция " +
            to_string(t.col) + ": ожидалось " + expected +
            ", найдено " + tokenName(t.type) +
            (t.value.empty() ? "" : " ('" + t.value + "')"));
    }

    // ── Функции грамматики ──────────────────────────────────

    // factor → ( expression ) | - factor | const | id
    bool parseFactor() {
        // Попытка: ( expression )
        {
            auto cp = saveAll();
            if (match(TOK_LPAREN) && parseExpression() && match(TOK_RPAREN))
                return true;
            restoreAll(cp);   // ← backtracking
        }
        // Попытка: унарный минус
        {
            auto cp = saveAll();
            if (match(TOK_MINUS) && parseFactor()) {
                postfix.push_back("@");   // '@' = унарный минус в ОПН
                return true;
            }
            restoreAll(cp);   // ← backtracking
        }
        // Попытка: целая константа
        if (peek().type == TOK_CONST) {
            postfix.push_back(tokens[pos++].value);
            return true;
        }
        // Попытка: идентификатор
        if (peek().type == TOK_ID) {
            postfix.push_back(tokens[pos++].value);
            return true;
        }
        return false;
    }

    // term → factor { (* | / | div | mod) factor }
    bool parseTerm() {
        if (!parseFactor()) return false;
        while (true) {
            string op;
            if      (peek().type == TOK_MUL)   { op = "*";   pos++; }
            else if (peek().type == TOK_SLASH)  { op = "/";   pos++; }
            else if (peek().type == TOK_DIV)    { op = "div"; pos++; }
            else if (peek().type == TOK_MOD)    { op = "mod"; pos++; }
            else break;
            if (!parseFactor()) syntaxError("операнд после '" + op + "'");
            postfix.push_back(op);
        }
        return true;
    }

    // arith_expr → term { (+ | -) term }
    bool parseArithExpr() {
        if (!parseTerm()) return false;
        while (true) {
            string op;
            if      (peek().type == TOK_PLUS)  { op = "+"; pos++; }
            else if (peek().type == TOK_MINUS)  { op = "-"; pos++; }
            else break;
            if (!parseTerm()) syntaxError("операнд после '" + op + "'");
            postfix.push_back(op);
        }
        return true;
    }

    // expression → arith_expr { (< | > | <= | >= | == | !=) arith_expr }
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
            if (!parseArithExpr()) syntaxError("операнд после '" + op + "'");
            postfix.push_back(op);
        }
        return true;
    }

    // assignment → id = expression
    // ОПН: [expression] id =
    bool parseAssignment() {
        auto cp = saveAll();                         // ← сохранить позицию
        if (peek().type == TOK_ID) {
            string var = tokens[pos++].value;
            if (match(TOK_ASSIGN) && parseExpression()) {
                postfix.push_back(var);
                postfix.push_back("=");
                return true;
            }
        }
        restoreAll(cp);                              // ← откатить при неудаче
        return false;
    }

    // read_stmt → read ( id )
    // ОПН: id read
    bool parseReadStmt() {
        auto cp = saveAll();                         // ← сохранить позицию
        if (match(TOK_READ) && match(TOK_LPAREN) && peek().type == TOK_ID) {
            string var = tokens[pos++].value;
            if (match(TOK_RPAREN)) {
                postfix.push_back(var);
                postfix.push_back("read");
                return true;
            }
        }
        restoreAll(cp);                              // ← откатить при неудаче
        return false;
    }

    // write_stmt → write ( expression )
    // ОПН: [expression] write
    bool parseWriteStmt() {
        auto cp = saveAll();                         // ← сохранить позицию
        if (match(TOK_WRITE) && match(TOK_LPAREN) && parseExpression() && match(TOK_RPAREN)) {
            postfix.push_back("write");
            return true;
        }
        restoreAll(cp);                              // ← откатить при неудаче
        return false;
    }

    // while_loop → while ( expression ) { statement_list }
    // ОПН: Lstart: [cond] Lend jz [body] Lstart jmp Lend:
    bool parseWhileLoop() {
        auto cp = saveAll();                         // ← сохранить позицию
        if (match(TOK_WHILE) && match(TOK_LPAREN)) {
            string Ls = newLabel();   // метка начала цикла
            string Le = newLabel();   // метка выхода из цикла
            postfix.push_back(Ls + ":");             // Lstart:
            if (parseExpression() && match(TOK_RPAREN)) {
                postfix.push_back(Le);               // аргумент для jz
                postfix.push_back("jz");
                if (match(TOK_LBRACE) && parseStatementList() && match(TOK_RBRACE)) {
                    postfix.push_back(Ls);           // аргумент для jmp
                    postfix.push_back("jmp");
                    postfix.push_back(Le + ":");     // Lend:
                    return true;
                }
            }
        }
        restoreAll(cp);                              // ← откатить при неудаче
        return false;
    }

    // statement → while_loop | assignment | read_stmt | write_stmt
    // Backtracking: пробуем каждую альтернативу с откатом при неудаче
    bool parseStatement() {
        while (peek().type == TOK_SEMICOL) pos++;   // пропуск ';'

        // Попытка 1: while
        { auto cp = saveAll(); if (parseWhileLoop())  return true; restoreAll(cp); }
        // Попытка 2: присваивание
        { auto cp = saveAll(); if (parseAssignment()) return true; restoreAll(cp); }
        // Попытка 3: read
        { auto cp = saveAll(); if (parseReadStmt())   return true; restoreAll(cp); }
        // Попытка 4: write
        { auto cp = saveAll(); if (parseWriteStmt())  return true; restoreAll(cp); }

        return false;
    }

    // statement_list → statement { statement }
    bool parseStatementList() {
        if (!parseStatement()) return false;
        while (peek().type != TOK_EOF && peek().type != TOK_RBRACE) {
            while (peek().type == TOK_SEMICOL) pos++;
            if (peek().type == TOK_EOF || peek().type == TOK_RBRACE) break;
            auto cp = saveAll();
            if (!parseStatement()) { restoreAll(cp); break; }
        }
        return true;
    }

public:
    Parser(const vector<Token>& toks, vector<string>& pf)
        : tokens(toks), pos(0), postfix(pf), labelCnt(0) {}

    void parse() {
        if (!parseStatementList()) syntaxError("оператор");
        if (peek().type != TOK_EOF) syntaxError("конец файла");
    }
};

// ============================================================
// ГЕНЕРАТОР ОБЪЕКТНОГО КОДА — Макроассемблер IBM PC (MASM/TASM 16-bit)
//
// Алгоритм: обрабатываем ОПН слева направо,
//   поддерживаем стек имён (переменных, констант, временных).
//   Каждая бинарная операция порождает временную переменную _tN.
// ============================================================
class CodeGenerator {
    set<string> vars;       // переменные пользователя + временные → .data DW
    set<string> labels;     // метки (чтобы не объявлять их в .data)
    int         tempCnt;

    vector<string> code;      // строки сегмента .code (тело main)
    vector<string> valStack;  // стек имён значений

    void emit(const string& s) { code.push_back(s); }

    // Загрузить значение (переменную, константу или temp) в AX
    void loadAX(const string& v)   { emit("    mov ax, " + v); }
    // Сохранить AX в переменную
    void storeAX(const string& v)  { emit("    mov " + v + ", ax"); }

    void vpush(const string& v)    { valStack.push_back(v); }
    string vpop() {
        if (valStack.empty()) throw runtime_error("Внутренняя ошибка: пустой стек значений");
        string v = valStack.back(); valStack.pop_back(); return v;
    }

    // Выделить новую временную переменную и зарегистрировать в .data
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

    // Первый проход: собрать метки и переменные пользователя
    void collectSymbols(const vector<string>& pf) {
        // Собрать имена меток (токены вида "L1:")
        for (const string& tok : pf)
            if (!tok.empty() && tok.back() == ':')
                labels.insert(tok.substr(0, tok.size() - 1));

        static const set<string> ops = {
            "=","read","write","+","-","*","/","div","mod","@",
            "jz","jmp","<",">","<=",">=","==","!="
        };
        // Добавить в vars все токены, которые не являются:
        //   метками, операторами, ссылками на метки, числами
        for (const string& tok : pf) {
            if (tok.empty() || tok.back() == ':') continue;
            if (ops.count(tok))     continue;
            if (isNumber(tok))      continue;
            if (labels.count(tok))  continue;   // ссылка на метку
            vars.insert(tok);
        }
    }

    // Второй проход: генерация инструкций
    void generateCode(const vector<string>& pf) {
        for (int i = 0; i < (int)pf.size(); i++) {
            const string& tok = pf[i];

            // ── Определение метки ─────────────────────────
            if (!tok.empty() && tok.back() == ':') {
                emit(tok);
                continue;
            }

            // ── Операторы управления потоком ──────────────
            if (tok == "jmp") {
                string lbl = vpop();
                emit("    jmp " + lbl);
                continue;
            }
            if (tok == "jz") {
                // На стеке (сверху): метка выхода, ниже — результат условия
                string lbl  = vpop();
                string cond = vpop();
                loadAX(cond);
                emit("    cmp ax, 0");
                emit("    je  " + lbl);
                continue;
            }

            // ── Присваивание: [expr] var = ────────────────
            if (tok == "=") {
                string var = vpop();   // имя переменной-получателя
                string val = vpop();   // результат выражения
                loadAX(val);
                storeAX(var);
                continue;
            }

            // ── Ввод ──────────────────────────────────────
            if (tok == "read") {
                string var = vpop();
                emit("    ; --- read(" + var + ") ---");
                emit("    call _read_int");
                storeAX(var);
                continue;
            }

            // ── Вывод ─────────────────────────────────────
            if (tok == "write") {
                string val = vpop();
                emit("    ; --- write(" + val + ") ---");
                loadAX(val);
                emit("    call _write_int");
                emit("    call _write_nl");
                continue;
            }

            // ── Унарный минус ─────────────────────────────
            if (tok == "@") {
                string a = vpop();
                string t = newTemp();
                loadAX(a);
                emit("    neg ax");
                storeAX(t);
                vpush(t);
                continue;
            }

            // ── Бинарные арифметические операции ──────────
            if (tok=="+" || tok=="-" || tok=="*" || tok=="/" || tok=="div" || tok=="mod") {
                string b = vpop();
                string a = vpop();
                string t = newTemp();
                loadAX(a);
                emit("    mov bx, " + b);  // BX = правый операнд

                if (tok == "+") {
                    emit("    add ax, bx");
                } else if (tok == "-") {
                    emit("    sub ax, bx");
                } else if (tok == "*") {
                    // imul bx: DX:AX = AX * BX (знаковое, используем AX)
                    emit("    imul bx");
                } else if (tok == "/" || tok == "div") {
                    emit("    cwd");          // расширить знак AX → DX:AX
                    emit("    idiv bx");      // AX = частное
                } else {                      // mod
                    emit("    cwd");
                    emit("    idiv bx");
                    emit("    mov ax, dx");   // DX = остаток → AX
                }
                storeAX(t);
                vpush(t);
                continue;
            }

            // ── Операторы сравнения ────────────────────────
            // Результат: 0 (ложь) или 1 (истина) сохраняется во временную.
            // Метод: генерируем условный переход с двумя ветками.
            if (tok=="<" || tok==">" || tok=="<=" || tok==">=" || tok=="==" || tok=="!=") {
                string b = vpop();
                string a = vpop();
                string t = newTemp();
                int    id = tempCnt;  // уникальный id для локальных меток
                string ltrue = "_ct" + to_string(id) + "T";
                string ldone = "_ct" + to_string(id) + "D";

                loadAX(a);
                emit("    cmp ax, " + b);

                string jcc;
                if      (tok == "<")  jcc = "jl";
                else if (tok == ">")  jcc = "jg";
                else if (tok == "<=") jcc = "jle";
                else if (tok == ">=") jcc = "jge";
                else if (tok == "==") jcc = "je";
                else                  jcc = "jne";  // !=

                emit("    " + jcc + " " + ltrue);
                emit("    mov ax, 0");
                emit("    jmp " + ldone);
                emit(ltrue + ":");
                emit("    mov ax, 1");
                emit(ldone + ":");
                storeAX(t);
                vpush(t);
                continue;
            }

            // ── Операнд (переменная, константа, ссылка на метку) ──
            vpush(tok);
        }
    }

    // Запись .asm файла
    void writeASM(ostream& out) {
        out << "; === Сгенерировано компилятором Python-subset → IBM PC ASM ===\n"
            << ".model small\n"
            << ".stack 200h\n\n"
            << ".data\n";
        for (const string& v : vars)
            out << "    " << v << " DW 0\n";
        out << "    _nl  DB 13,10,'$'\n"
            << "    _buf DB 12 DUP('$')\n\n"
            << ".code\n\n";

        writeProc_ReadInt(out);
        writeProc_WriteInt(out);
        writeProc_WriteNL(out);

        out << "main PROC\n"
            << "    mov ax, @data\n"
            << "    mov ds, ax\n\n";
        for (const string& ln : code)
            out << ln << "\n";
        out << "\n"
            << "    mov ax, 4C00h\n"
            << "    int 21h\n"
            << "main ENDP\n\n"
            << "END main\n";
    }

    // Процедура чтения целого числа с клавиатуры → AX
    static void writeProc_ReadInt(ostream& out) {
        out << R"(; Читает строку с консоли, возвращает знаковое 16-bit целое в AX
_read_int PROC
    push bx
    push cx
    push dx
    xor  bx, bx          ; bx = накопитель результата
    xor  cx, cx          ; cx = флаг отрицательности
_ri_next:
    mov  ah, 01h
    int  21h              ; читаем символ с эхо → AL
    cmp  al, 13           ; Enter?
    je   _ri_done
    cmp  al, 10
    je   _ri_done
    cmp  al, '-'
    jne  _ri_trydigit
    mov  cx, 1
    jmp  _ri_next
_ri_trydigit:
    sub  al, '0'
    jb   _ri_next         ; символ < '0' → игнорировать
    cmp  al, 9
    ja   _ri_next         ; символ > '9' → игнорировать
    xor  ah, ah           ; AX = цифра 0..9
    push ax               ; сохранить цифру
    mov  ax, bx           ; AX = текущий накопитель
    mov  bx, 10
    imul bx               ; DX:AX = накопитель * 10
    pop  bx               ; BX = цифра
    add  ax, bx           ; AX = накопитель*10 + цифра
    mov  bx, ax           ; обновить накопитель
    jmp  _ri_next
_ri_done:
    test cx, cx
    jz   _ri_pos
    neg  bx               ; отрицательное число
_ri_pos:
    mov  ax, bx
    pop  dx
    pop  cx
    pop  bx
    ret
_read_int ENDP

)";
    }

    // Процедура вывода знакового 16-bit целого из AX
    static void writeProc_WriteInt(ostream& out) {
        out << R"(; Выводит знаковое 16-bit целое из AX на консоль
_write_int PROC
    push bx
    push cx
    push dx
    push si
    lea  si, _buf
    test ax, ax
    jns  _wi_pos
    mov  byte ptr [si], '-'
    inc  si
    neg  ax
_wi_pos:
    xor  cx, cx           ; счётчик цифр
_wi_extr:
    xor  dx, dx
    mov  bx, 10
    div  bx               ; AX = ax/10, DX = ax%10
    push dx               ; запомнить цифру
    inc  cx
    test ax, ax
    jnz  _wi_extr
_wi_fill:
    pop  dx
    add  dl, '0'
    mov  [si], dl
    inc  si
    loop _wi_fill
    mov  byte ptr [si], '$'
    lea  dx, _buf
    mov  ah, 09h
    int  21h              ; вывод строки DS:DX до '$'
    pop  si
    pop  dx
    pop  cx
    pop  bx
    ret
_write_int ENDP

)";
    }

    // Процедура вывода перевода строки
    static void writeProc_WriteNL(ostream& out) {
        out << R"(_write_nl PROC
    lea  dx, _nl
    mov  ah, 09h
    int  21h
    ret
_write_nl ENDP

)";
    }

public:
    CodeGenerator() : tempCnt(0) {}

    void generate(const vector<string>& postfix, const string& outfile) {
        collectSymbols(postfix);
        generateCode(postfix);

        ofstream out(outfile);
        if (!out)
            throw runtime_error("Не удалось открыть файл для записи: " + outfile);
        writeASM(out);
        out.close();
    }
};

// ============================================================
// MAIN
// ============================================================
int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Использование: " << argv[0] << " <input.py> <output.asm>\n";
        return 1;
    }

    ifstream fin(argv[1]);
    if (!fin) {
        cerr << "Ошибка: не удалось открыть файл '" << argv[1] << "'\n";
        return 1;
    }
    string src((istreambuf_iterator<char>(fin)), istreambuf_iterator<char>());
    fin.close();

    try {
        // ── Шаг 1: Лексический анализ ──────────────────────
        Lexer lexer(src);
        vector<Token> tokens = lexer.tokenize();

        // ── Шаг 2: Синтаксический анализ → ОПН ────────────
        vector<string> postfix;
        Parser parser(tokens, postfix);
        parser.parse();

        // Вывод ОПН (для отладки / протокол)
        cout << "ОПН: ";
        for (const string& s : postfix) cout << s << " ";
        cout << "\n";

        // ── Шаг 3: Генерация кода ──────────────────────────
        CodeGenerator gen;
        gen.generate(postfix, argv[2]);

        cout << "Успешно: " << argv[2] << "\n";
        return 0;

    } catch (const exception& e) {
        cerr << "Ошибка: " << e.what() << "\n";
        return 1;
    }
}
