// State Machine to parse Infix Arithmetic Expression
#ifndef INCLUDE_CALCULATOR_HPP
#define INCLUDE_CALCULATOR_HPP

#include <string>
#include <unordered_map>
#include "stack.hh"

enum State
{
    ST_BGN = 0,
    ST_OPR,//1
    ST_OPD,//2
    ST_LPS,//3
    ST_RPS,//4
    ST_FLG,//5
    ST_IDF,//6
    ST_UNC,                     // 7,uncertain, more needed to determine state
    ST_LCB,
    ST_RCB,
    ST_ERR,//8
    ST_STT,//9
    ST_UPPER//10 // upper bound
};

enum CharType
{
    CT_WHITESPACE = 0,
    CT_OP,                      // 1
    CT_NUM,                     // 2
    CT_LP,                      // 3
    CT_RP,                      // 4
    CT_FLG,                     // 5
    CT_ALP,                     // 6
    CT_COMMA,                   // 7
    CT_SEMICOLON,               // 8
    CT_SYMBOL_EQUAL,            // =
    CT_SYMBOL_AMPERSAND,        // &
    CT_SYMBOL_VBAR,             // |
    CT_EXCLAMATION,             // !
    CT_LCB,                     // {
    CT_RCB,                     // }
    CT_UPPER                    // upper bound
};

enum ActionType
{
    AT_NON = 0,
    AT_OPR, // 1
    AT_OPD,// 2
    AT_LPS,// 3
    AT_RPS,// 4
    AT_ERR,// 5
    AT_FLG,// 6
    AT_ALP,// 7
    AT_SPC,// 8
    AT_COMMA,// 9
    AT_SEMICOLON, // 10
    AT_SYMBOL_EQUAL, // 11
    AT_SYMBOL_AMPERSAND,
    AT_SYMBOL_VBAR,
    AT_EXCLAMATION,
    AT_LCB,
    AT_RCB,
    AT_UPPER
};

enum StmtType {
    STMT_NON, // 0
    STMT_ASSIGN,
    STMT_IF_COND,
    STMT_IF_BLOCK,
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

    PRINTID,
    RESULT,

    // add more
    FNN_UPPER
};

class ArithmeticExpression
{
public:
    ArithmeticExpression(const std::string& infix_expression, int print_enable);
    ~ArithmeticExpression() {}

    bool parse();
    const std::string& getRpnExpression() const {
        return m_rpn_expression;
    }
    const std::string& getArithmeticExpression() const {
        return m_arithmetic_expression;
    }
    bool getExpressionValue(double &val) const;

private:
    bool handle(char c);
    bool handleNone();
    bool handleFlag(char flag);
    bool handleError();
    bool handleOperator(char opr);
    bool handleOperand();
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

    bool pushIdfValueToOperandStack(std::string idf);

    bool handleAction(ActionType type, char c);
    bool calculate(int opr);

    int fnn2fni(const std::string& fnn);
    std::string fni2fnn(const int fni);

    void dia(const std::string& marker);
    void appendToRpnExpression(char c);

    bool isOnTerminalState() const {
        return (
            ST_BGN == m_state
            || ST_OPD == m_state
            || ST_IDF == m_state
            || ST_RPS == m_state
            || ST_RCB == m_state
            || ST_STT == m_state
            );
    }

    int getPriority(char c);
    CharType getCharType(int c);
    const char* getStateStr(State) const;

    bool findBalancedRightCurlyBraceAndSkipToIt();

private:
    Stack<double> m_operand_stack;
    Stack<int> m_operator_stack;

    State m_state;
    State m_state_next_certained;

    std::string m_arithmetic_expression;
    int m_parse_pos;
    int m_lp_count;
    int m_lcb_count;
    Stack<int> m_fncmcnt_stack;

    std::string m_id;
    std::unordered_map<std::string,double> m_id_map;
    bool m_id_spc_occurred;
    int m_stmt;
    std::string m_stmt_assign_key;

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
        const char* name;
        const int args_count;
        const int args_va;
    } FunctionMeta;
    static FunctionMeta m_function_meta[FNN_UPPER];

    double m_result;
};

#endif /* INCLUDE_CALCULATOR_HPP */

