// State Machine to parse Infix Arithmetic Expression
#ifndef INCLUDE_CALCULATOR_HPP
#define INCLUDE_CALCULATOR_HPP

#include <string>
#include <unordered_map>
#include <vector>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

enum OpdType {
    OPD_STACK_BOTTOM_BASE=-1,
    OPD_NUM,
    OPD_STR,
    OPD_IDF,
};

typedef struct {
    int token_idx;
    int stmt_count;
    OpdType type; // keep it simple, use 0 - number, 1 - string, 2 - idf
    // TODO: maybe use std::variant, for now, keep simple even with more memory cost
    double num;
    std::string str;
} Opd;

typedef struct {
    int token_idx;
    int stmt_count;
    int v;
} Opr;

enum ContextType {
    CTXT_STACK_BOTTOM_BASE=-1,
    CTXT_BGN,
    CTXT_EXP,

    STMT_DECLARE_ASSIGN,
    STMT_DECLARE_ASSIGN_VALUE,

    STMT_SCOPE_DECLARE_ASSIGN,  // TODO

    STMT_IF,                    // keyword
    STMT_IF_COND,
    STMT_IF_BLOCK,

    STMT_ELIF,
    STMT_ELIF_COND,
    STMT_ELIF_BLOCK,

    STMT_ELSE,
    STMT_ELSE_BLOCK,

    FUNCTION_DEFINITION,
    FUNCTION_ARGS,
    FUNCTION_BLOCK,
    FUNCTION_CALL,

    FUNCTION_RETURN,
    FUNCTION_RETURN_VALUE,

};

inline const char* getOpdTypeStr(OpdType t) {
    switch(t) {
    case OPD_STACK_BOTTOM_BASE: return "*BASE*";
    case OPD_NUM: return "OPD_NUM";
    case OPD_STR: return "OPD_STR";
    case OPD_IDF: return "OPD_IDF";
    default: {
        fprintf(stderr, "error: getOpdTypeStr unsupported OpdType: %d\n", (int)t);
        exit(-1);
    }
    }
}

inline const char* getContextTypeStr(ContextType t)
{
    switch(t) {
    case CTXT_STACK_BOTTOM_BASE: return "*BASE*";

    case CTXT_BGN: return "CTXT_BGN";
    case CTXT_EXP: return "CTXT_EXP";

    case STMT_DECLARE_ASSIGN: return "STMT_DECLARE_ASSIGN";
    case STMT_DECLARE_ASSIGN_VALUE: return "STMT_DECLARE_ASSIGN_VALUE";
    case STMT_SCOPE_DECLARE_ASSIGN: return "STMT_SCOPE_DECLARE_ASSIGN";

    case STMT_IF: return "STMT_IF";
    case STMT_IF_COND: return "STMT_IF_COND";
    case STMT_IF_BLOCK: return "STMT_IF_BLOCK";

    case STMT_ELIF: return "STMT_ELIF";
    case STMT_ELIF_COND: return "STMT_ELIF_COND";
    case STMT_ELIF_BLOCK: return "STMT_ELIF_BLOCK";

    case STMT_ELSE: return "STMT_ELSE";
    case STMT_ELSE_BLOCK: return "STMT_ELSE_BLOCK";

    case FUNCTION_DEFINITION: return "FUNCTION_DEFINITION";
    case FUNCTION_ARGS: return "FUNCTION_ARGS";
    case FUNCTION_BLOCK: return "FUNCTION_BLOCK";
    case FUNCTION_CALL: return "FUNCTION_CALL";

    case FUNCTION_RETURN: return "FUNCTION_RETURN";
    case FUNCTION_RETURN_VALUE: return "FUNCTION_RETURN_VALUE";

    default: {
        fprintf(stderr, "error: getContextTypeStr unsupported contextType: %d\n", (int)t);
        exit(-1);
    }
    }
}

inline void printStackItem(const Opd& item) {
    fprintf(stderr, "(%d,%d,%s,%g,\"%s\"), ", item.token_idx, item.stmt_count, getOpdTypeStr(item.type), item.num, item.str.c_str());
}

inline void printStackItem(const Opr& item) {
    int v = item.v;
    fprintf(stderr, "(%d,v:%f,%d,'%c'), ", item.token_idx, (float)v,(int)v,(char)v);
}

inline void printStackItem(const int& item) {
    fprintf(stderr, "(%f,%d,'%c'), ", (float)item,(int)item,(char)item);
}

inline void printStackItem(const ContextType& item) {
    fprintf(stderr, "%s, ", getContextTypeStr(item));
}

#define STACK_SP_INIT -1
template <class T>
class Stack {
public:
    Stack(int size = 1) { init(size); }
    ~Stack() { destory(); }

    T& top(int step=0) {
        if (empty()) {
            fprintf(stderr, "*** %s.top: no top item found, because current stack is empty", m_name.c_str());
            exit(-1);
        };

        return m_tunk.at(m_sp+step);
    }
    const T& top(int step=0) const { return m_tunk.at(m_sp+step); }
    void push(T val);
    T pop() {
        if (empty()) {
            fprintf(stderr, "*** %s.pop: failed because current stack is already empty\n", m_name.c_str());
            exit(-1);
        }

        if (m_print_enable) {
            fprintf(stderr, "*** %s.pop ...", m_name.c_str());
            printStackItem(m_tunk[m_sp]);
            fprintf(stderr, "\n");
        }

        return m_tunk[m_sp--];
    }
    bool empty() const { return m_sp == STACK_SP_INIT; }
    int count() const { return m_sp+1; }

    void dia(int enable=0){
        if (!enable) return;
        fprintf(stderr, "[");
        for (int i=0; i< m_sp+1; i++) {
            T& x = m_tunk[i];
            printStackItem(x);
        }
        fprintf(stderr, m_sp>=0? "]": "]");
    }

    void setName(std::string name) {
        m_name = name;
    }

    void setPrintEnable(bool enable) {
        m_print_enable = enable;
    }

protected:
    virtual T& get(int idx);
    virtual void init(int size);

private:
    bool stack_storage_full() { return m_sp == (int)(m_tunk.size() - 1); }
    void destory() {}
private:
    std::vector<T> m_tunk;
    int m_sp;
    std::string m_name="";
    bool m_print_enable = false;
};

template<class T>
void Stack<T>::init(int size)
{
    m_tunk.resize(size);
    m_sp = STACK_SP_INIT;
}

template<class T>
void Stack<T>::push(T val)
{
    //fprintf(stderr, "Stack %s, m_sp:%d\n", stack_storage_full()?"full":"not full", m_sp);
    if ( stack_storage_full() ) {
        m_tunk.push_back(val);
        ++m_sp;
    } else {
        m_tunk[++m_sp] = val;
    }
}

template<class T>
T &Stack<T>::get(int idx)
{
    assert(idx > STACK_SP_INIT && idx <= m_sp);
    return m_tunk.at(idx);
}

enum State
{
    ST_BGN = 0,
    ST_OPR,
    ST_NUM,
    ST_LPS,
    ST_RPS,
    ST_FLG,
    ST_IDF,
    ST_LCB,
    ST_RCB,
    ST_CLN,

    ST_UNC,                     // 7,uncertain, more needed to determine state
    ST_ERR,
    ST_STT,
    ST_UPPER // aka. NON
};

enum CharType
{
    CT_SPC = 0,

    CT_OP,                      // (fundamental) see .cc

    CT_NUM,                     // 0-9

    CT_LP,                      // ( group
    CT_RP,                      // ) group

    CT_FLG,                     // +/-
    CT_ALP,                     // A-Za-z alpha

    CT_CMA,                     // ,
    CT_SMC,                     // ;
    CT_SEQ,                     // =
    CT_AMP,                     // &
    CT_VBR,                     // |
    CT_EXM,                     // !
    CT_LCB,                     // {
    CT_RCB,                     // }
    CT_CLN,                     // :
    CT_DBQ,                     // " mainly for string (ipd)

    CT_UPPER                    // upper bound
};

enum ActionType
{
    AT_NON = 0,
    AT_OPR, // 1
    AT_NUM,// 2
    AT_LPS,// 3
    AT_RPS,// 4
    AT_ERR,// 5
    AT_FLG,// 6
    AT_ALP,// 7
    AT_SPC,// 8
    AT_CMA,// 9
    AT_SMC, // 10
    AT_SEQ, // 11
    AT_AMP,
    AT_VBR,
    AT_EXM,
    AT_LCB,
    AT_RCB,
    AT_CLN,
    AT_DBQ,

    AT_UPPER
};

// TODO: convert +-*/^ into fnn too, so we can use state-like StateInfo to manage a->b/b->a conversion.
// TODO: to bind argument count for function, so can used paired up with m_fncmcnt_stack,
//       for more ux-friendly, eg. error out for case that wrong count of comma been
//       used for function.
enum FNN
{

    // Basic / rounding

    FABS,
    FMOD,
    REMAINDER,
    CEIL,
    FLOOR,
    ROUND,
    TRUNC,

    // Powers / roots / logarithms

    POW,
    SQRT,
    CBRT,
    HYPOT,
    EXP,
    EXP2,
    EXPM1,
    LOG,
    LOG10,
    LOG2,
    LOG1P,

    // Trigonometric

    SIN,
    COS,
    TAN,
    ASIN,
    ACOS,
    ATAN,
    ATAN2,

    // Hyperbolic

    SINH,
    COSH,
    TANH,
    ASINH,
    ACOSH,
    ATANH,

    // Floating-point utilities

    FMIN,
    FMAX,
    FDIM,
    COPYSIGN,
    ISFINITE,
    ISINF,
    ISNAN,
    ISNORMAL,
    SIGNBIT,

    // Shortcuts

    INT,
    MAX,
    MIN,

    CMP,

    PRINT,
    PRINTF,

    // add more
    FNN_UPPER
};

#define IF_COND_TYPE_IF 0
#define IF_COND_TYPE_ELIF 1
#define IF_COND_TYPE_ELSE 2

typedef struct {
    int exec_begin;
    int exec_end;
    std::vector<std::string> args;
} FunctionDeclare;

typedef struct {
    int type;                      // 0-if, 1-elif, 2-else
    int value;                      // 0,1
} IfCond;

class ArithmeticExpression
{
public:
    ArithmeticExpression(
        const std::string& infix_expression,
        int depth,
        int lcb_count,
        std::unordered_map<std::string,std::unordered_map<int,double>>, // copy
        std::unordered_map<std::string,double> pre_defined_idf_map, // copy
        ContextType contextType,
        int print_enable
        );
    ~ArithmeticExpression() {}

    bool parse(int begin_pos=0, int end_pos=-1);
    const std::string& getRpnExpression() const {
        return m_rpn_expression;
    }
    const std::string& getArithmeticExpression() const {
        return m_arithmetic_expression;
    }

    bool returnStatementExists() {
        return m_return_stmt_exists;
    }

    Opd* getReturnValuePtr() const {
        return m_return_value_ptr;
    }

    // bool getExpressionValue(double &val) const;

private:
    bool handle(char c);
    bool handleNone();
    bool handleFlag(char flag);
    void handleError();
    bool handleOperator(char opr);
    bool handleNum();
    bool handleLeftParenthesis();
    bool handleRightParenthesis();
    bool handleAlpha(char c);
    bool handleSpace();
    bool handleComma();
    bool handleSemicolon();
    bool handleSymbolEqual();
    bool handleSymbolAmpersand();
    bool handleSymbolVbar();
    bool handleExclamation();
    bool handleLeftCurlyBrace();
    bool handleRightCurlyBrace();
    bool handleColon();
    bool handleDoubleQuote();

    bool handleAction(ActionType type, char c);
    bool handleInputAllParsed();

    bool calculate(int opr);
    bool call_old(const std::string);
    bool call(const std::string);

    double getIdfValue(const std::string& idf);
    void assignValueToIdf(const std::string& idf, double value, bool scoped);

    void checkFollowingIforElif();

    int fnn2fni(const std::string& fnn);
    std::string fni2fnn(const int fni);

    void syntax(ContextType);
    void dia(const std::string& marker);
    void appendToRpnExpression(char c);

    bool isOnTerminalState() const;
    bool isCharTypeExpected(CharType type);

    int getPriority(char c);
    CharType getCharType(int c);

    const char* getStateStr(State) const;
    const char* getCharTypeStr(int) const;

    bool findBalancedRightCurlyBraceAndSkipToIt(bool skipRigthCurlyBraceAlong=false);
    void printErrorLocation();

    char getNextNonSpcChar();
    std::string getNextToken();

    void pushOpr(int);

    void pushOpdIdf(std::string idf);
    void pushOpdIdfValue(const std::string& idf);
    void pushOpdNum(double num);
    void pushOpdStr(std::string str);

    // TODO: marked following funs as inline
    bool _isbase(const Opd& item);
    bool _isidf(const Opd& item);
    bool _isnum(const Opd& item);
    bool _isstr(const Opd& item);

    // TODO: marked following funs as inline
    std::string _2idf(const Opd& item);
    double _2num(const Opd& item);
    std::string _2str(const Opd& item);

    std::unordered_map<std::string,FunctionDeclare>& getFunctionDeclareMap() {
        return m_function_declare_map;
    }

    std::unordered_map<int,std::string>& getFunctionDeclareExecBeginNameMap() {
        return m_function_declare_exec_begin_name_map;
    }

    std::unordered_map<std::string,std::unordered_map<int,double>>& getIdfMap() {
        return m_idf_map;
    }

    std::unordered_map<std::string,double>& getIdfOuterScopeReassignedMap() {
        return m_idf_outer_scope_reassigned_map;
    }

    std::unordered_map<int, std::unordered_map<std::string,int>>& getIdfScopeDeclared() {
        return m_idf_scope_declared;
    }

    void setMeta(
        std::unordered_map<std::string,FunctionDeclare>&,
        std::unordered_map<int,std::string>&,
        std::unordered_map<int, std::unordered_map<std::string,int>>&
        );

    int getDepth() {
        return m_depth;
    }

    int getLCBCount() {
        return m_lcb_count;
    }

    void pushContextBGN();

private:
    Stack<Opd> m_opd_stack;
    Stack<Opr> m_opr_stack;

    int m_token_idx;
    int m_depth;                // use for function call

    State m_state_prev;
    State m_state;
    State m_state_next_certained;

    std::string m_arithmetic_expression;
    int m_parse_pos;

    int m_stmt_count;
    int m_ln_count;
    int m_lp_count;
    int m_lcb_count_init;
    int m_lcb_count;            // aka. '{' block depth of current AE depth

    Stack<int> m_fncmcnt_stack;

    std::string m_function_declare_cur_name; // temporary for faster accessing
    std::vector<std::string> m_function_declare_cur_argidfs; // temporary for faster accessing
    std::unordered_map<std::string,FunctionDeclare> m_function_declare_map;
    std::unordered_map<int,std::string> m_function_declare_exec_begin_name_map;

    std::vector<CharType> m_expect_char_types;

    std::string m_id;           // TODO: already replace with scope-related operand stack item.
    // std::unordered_map<std::string,double> m_id_map;
    bool m_id_spc_occurred;

    // The main design purpose of {} in C is to group statements into a block,
    // so they can be treated as a single unit of code.

    // NOTE: instead of using global var to record idf stuff, self-maintain class member is more
    // scope-friedly, without global state management headache, esp. for `function call`, `eval`
    // and other cases, this is prefered even with the cost of using more memory.

    // mainly used for passing function arguments when do function call
    std::unordered_map<std::string,double> m_idf_pre_defined_scoped_map;

    // record id's value
    // { idf: {block_depth: idf_value} }
    std::unordered_map<std::string,std::unordered_map<int,double>> m_idf_map;
    // record redeclared idf in block_depth, used for clear m_idf_map
    // { block_depth: { idf: 1 }}
    // NOTE: use {idf: 1} instead of [idf1, idf2] for simplicity and performance
    // value is marker: 1 - yes, 0 - no
    std::unordered_map<int, std::unordered_map<std::string,int>> m_idf_scope_declared;
    // used for record value of reassigned outer scope idf, to merge into outer m_idf_map,
    // mainly used for function call
    std::unordered_map<std::string,double> m_idf_outer_scope_reassigned_map;

    bool m_return_stmt_exists = true;
    Opd *m_return_value_ptr = NULL;

    // FIXME: [2026-10-22] m_ctxt should be a stack for nested contexts,
    // so better grammar checking can be supported
    Stack<ContextType> m_ctxt_stack;

    std::string m_stmt_assign_key;

    // {lcb_count: [if_cond_value, elif_cond_value, ...]}
    std::unordered_map<int,std::vector<IfCond>> m_stmt_if_cond;

    std::string m_rpn_expression;

    typedef struct {
        ActionType action_type;
        State new_state;
    } StateTable;
    static StateTable m_state_table[ST_UPPER][CT_UPPER];

    typedef struct {
        State state;
        const char* state_str;
    } StateInfo;
    static StateInfo m_state_info[ST_UPPER+1];

    typedef struct {
        CharType char_type;
        const char* str;
    } CharTypeInfo;
    static CharTypeInfo m_char_type_info[CT_UPPER+1];

    // main for built-in function
    typedef struct {
        const char* name;
        const int args_count;
        const int args_va;
    } FunctionMeta;
    static FunctionMeta m_function_meta[FNN_UPPER];
};

#endif /* INCLUDE_CALCULATOR_HPP */
