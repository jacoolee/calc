// State Machine to parse Infix Arithmetic Expression
#ifndef INCLUDE_CALCULATOR_HPP
#define INCLUDE_CALCULATOR_HPP

#include <string>
#include "stack.hh"

enum State
{
    ST_BGN = 0,
    ST_OPR,                     // operator
    ST_OPD,                     // operand
    ST_LPS,
    ST_RPS,
    ST_FLG,
    ST_FNN,
    ST_ERR,
    ST_UPPER                    // upper bound
};

enum CharType
{
    CT_WHITESPACE = 0,
    CT_OP,
    CT_NUM,
    CT_LP,
    CT_RP,
    CT_FLG,
    CT_ALP,
    CT_COMMA,
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
    AT_UPPER
};

// TODO: convert +-*/^ into fnn too, so we can use state-like StateInfo to manage a->b/b->a conversion.
// TODO: to bind argument count for function, so can used paired up with m_fncmcnt_stack,
//       for more ux-friendly, eg. error out for case that wrong count of comma been
//       used for function.
enum FNN
{
    INT,   // 0
    FLOOR, // 1
    CEIL,  // 2
    ROUND, // 3
    FABS,  // 4
    SQRT,  // 5
    FMAX,  // 6
    FMIN,  // 7
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
    bool handleFlag();
    bool handleError();
    bool handleOperator();
    bool handleOperand();
    bool handleLeftParenthesis();
    bool handleRightParenthesis();
    bool handleAlpha(char c);
    bool handleSpace();
    bool handleComma();

    bool handleAction(ActionType type, char c);
    bool calculate(int opr);

    int fnn2fni(const std::string& fnn);
    std::string fni2fnn(const int fni);

    void dia(char* marker);
    void appendToRpnExpression(char c);

    bool isOnTerminalState() const {
        return (ST_OPD == m_state ||
                ST_RPS == m_state ||
                ST_BGN == m_state);
    }

    int getPriority(char c);
    CharType getCharType(int c);
    const char* getStateStr(State) const;
    const char* getCurStateStr() const;

private:
    Stack<double> m_operand_stack;
    Stack<int> m_operator_stack;

    State m_state;
    std::string m_arithmetic_expression;
    std::string m_fnn;
    int m_parse_pos;
    int m_lp_count;
    Stack<int> m_fncmcnt_stack;
    int m_flg;
    bool m_fnn_spc_occurred;

    std::string m_rpn_expression;

    typedef struct {
        State new_state;
        ActionType action_type;
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
    } FunctionMeta;
    static FunctionMeta m_function_meta[FNN_UPPER];
};

#endif /* INCLUDE_CALCULATOR_HPP */

