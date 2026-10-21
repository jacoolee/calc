// State Machine to parse Infix Arithmetic Expression
#ifndef INCLUDE_CALCULATOR_HPP
#define INCLUDE_CALCULATOR_HPP

#include <string>
#include <unordered_map>
#include "stack.hh"

enum State
{
    ST_BGN = 0,
    ST_OPR,
    ST_OPD,
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
    CT_OP,                      // 1
    CT_NUM,                     // 2
    CT_LP,                      // 3
    CT_RP,                      // 4
    CT_FLG,                     // 5
    CT_ALP,                     // 6
    CT_CMA,                   // 7
    CT_SMC,               // 8
    CT_SEQ,            // =
    CT_AMP,        // &
    CT_VBR,             // |
    CT_EXM,             // !
    CT_LCB,                     // {
    CT_RCB,                     // }
    CT_CLN,                     // :
    CT_DBQ,                     // "

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

enum CTXType {
    CTXT_NON, // 0
    STMT_DECLARE_ASSIGN,
    STMT_SCOPE_DECLARE_ASSIGN,//2
    STMT_IF_COND,
    STMT_ELIF_COND,
    STMT_IF_BLOCK,
    STMT_ELIF_BLOCK,
    STMT_ELSE_BLOCK,
    FUNCTION_DEFINITION,
    FUNCTION_ARGS,
    FUNCTION_BLOCK,
    FUNCTION_CALL,
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

class ArithmeticExpression
{
public:
    ArithmeticExpression(
        const std::string& infix_expression,
        int depth,
        int lcb_count,
        std::unordered_map<std::string,std::unordered_map<int,double>>, // copy
        std::unordered_map<std::string,double> pre_defined_idf_map, // copy
        int contextType,
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

    // bool getExpressionValue(double &val) const;

private:
    bool handle(char c);
    bool handleNone();
    bool handleFlag(char flag);
    void handleError();
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
    bool handleColon();
    bool handleDoubleQuote();

    bool handleAction(ActionType type, char c);
    bool calculate(int opr);
    bool call_old(const std::string);
    bool call(const std::string);

    double getIdfValue(const std::string& idf);
    bool pushIdfValueToOperandStack(const std::string& idf);
    void assignValueToIdf(const std::string& idf, double value, bool scoped);

    int fnn2fni(const std::string& fnn);
    std::string fni2fnn(const int fni);

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

    void pushNumberIntoOperandStack(double num);
    void pushStringIntoOperandStack(std::string str);

    bool _isn(const OperandStackItem& item);
    bool _iss(const OperandStackItem& item);
    double _2n(const OperandStackItem& item);
    std::string _2s(const OperandStackItem& item);

    std::unordered_map<std::string,std::array<int,2>>& getFunctionMap() {
        return m_function_map;
    }

    std::unordered_map<int,std::string>& getFunctionExecPosNameMap() {
        return m_function_exec_pos_name_map;
    }

    std::unordered_map<std::string,std::unordered_map<int,double>>& getIdfMap() {
        return m_idf_map;
    }

    std::unordered_map<int, std::unordered_map<std::string,int>>& getIdfScopeDeclared() {
        return m_idf_scope_declared;
    }

    void setMeta(
        std::unordered_map<std::string,std::array<int,2>>&,
        std::unordered_map<int,std::string>&,
        std::unordered_map<int, std::unordered_map<std::string,int>>&
        );

    int getDepth() {
        return m_depth;
    }

    int getLCBCount() {
        return m_lcb_count;
    }

private:
    Stack<OperandStackItem> m_operand_stack;
    Stack<int> m_operator_stack;

    int m_depth;                // use for function call

    State m_state_prev;
    State m_state;
    State m_state_next_certained;

    std::string m_arithmetic_expression;
    int m_parse_pos;

    int m_ln_count;
    int m_lp_count;
    int m_lcb_count_init;
    int m_lcb_count;            // aka. '{' block depth of current AE depth

    Stack<int> m_fncmcnt_stack;

    std::string m_function_name;
    std::unordered_map<std::string,std::array<int,2>> m_function_map;
    std::unordered_map<int,std::string> m_function_exec_pos_name_map;

    // FIXME: should merge m_function_args into m_function_map to support function in function
    std::vector<std::string> m_function_args;

    std::vector<CharType> m_expect_char_types;

    std::string m_id;
    // std::unordered_map<std::string,double> m_id_map;
    bool m_id_spc_occurred;

    // The main design purpose of {} in C is to group statements into a block,
    // so they can be treated as a single unit of code.

    std::unordered_map<std::string,double> m_pre_defined_idf_map;

    // record id's value
    // { idf: {block_depth: idf_value} }
    std::unordered_map<std::string,std::unordered_map<int,double>> m_idf_map;
    // record redeclared idf in block_depth, used for clear m_idf_map
    // { block_depth: { idf: 1 }}
    // NOTE: use {idf: 1} instead of [idf1, idf2] for simplicity and performance
    std::unordered_map<int, std::unordered_map<std::string,int>> m_idf_scope_declared;

    int m_ctxt;
    std::string m_stmt_assign_key;
    std::unordered_map<int,std::vector<int>> m_stmt_if_cond;

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

    typedef struct {
        const char* name;
        const int args_count;
        const int args_va;
    } FunctionMeta;
    static FunctionMeta m_function_meta[FNN_UPPER];
};

#endif /* INCLUDE_CALCULATOR_HPP */

