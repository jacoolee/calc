#include "arithmetic_expression.hh"

#include <string.h>
#include <math.h>

#define LOWEST_PRIO_OP '#'
#define FLG_NEGATIVE -1
#define FLG_ACTIVE 1

ArithmeticExpression::ArithmeticExpression(const std::string& infix_expression)
    : m_state(ST_NON),
      m_arithmetic_expression(infix_expression),
      m_parse_pos(0),
      m_lp_count(0),
      m_flg(FLG_ACTIVE)
{
    m_operator_stack.push(LOWEST_PRIO_OP);
}

ArithmeticExpression::StateInfo
ArithmeticExpression::m_state_info[] = {
    {ST_NON, "start"},
    {ST_OPR, "operator"},
    {ST_OPD, "operand"},
    {ST_LPS, "left parenthesis"},
    {ST_RPS, "right parenthesis"},
    {ST_FLG, "flag"},
    {ST_ERR, "error"},
    // to add
    {ST_UPPER, ""}
};

ArithmeticExpression::StateTable
ArithmeticExpression::m_state_table[ST_UPPER][CT_UPPER] = {
    // state table Description:
    ///////////////////////////////////////////////////////////////////////////////////////////////
    // // each Row stands for current state                                                      //
    // // each Column stands for the type of char will be processed                              //
    // // in each Table Entry,                                                                   //
    // //    the first item  is the next program state if the transition successfully processed, //
    // //    the second item is the type of process action of the transition                     //
    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Code:
    //           {CT_WHITESPACE},    {CT_OP},          {CT_NUM},         {CT_LP},          {CT_RP},          {CT_FLG}
    /* ST_NON */ {{ST_NON, AT_NON }, {ST_ERR, AT_ERR}, {ST_OPD, AT_OPD}, {ST_LPS, AT_LPS}, {ST_ERR, AT_ERR}, {ST_FLG, AT_FLG}},
    /* ST_OPR */ {{ST_OPR, AT_NON }, {ST_ERR, AT_ERR}, {ST_OPD, AT_OPD}, {ST_LPS, AT_LPS}, {ST_ERR, AT_ERR}, {ST_FLG, AT_FLG}},
    /* ST_OPD */ {{ST_OPD, AT_NON }, {ST_OPR, AT_OPR}, {ST_ERR, AT_ERR}, {ST_ERR, AT_ERR}, {ST_RPS, AT_RPS}, {ST_OPR, AT_OPR}},
    /* ST_LPS */ {{ST_LPS, AT_NON }, {ST_ERR, AT_ERR}, {ST_OPD, AT_OPD}, {ST_LPS, AT_LPS}, {ST_ERR, AT_ERR}, {ST_FLG, AT_FLG}},
    /* ST_RPS */ {{ST_RPS, AT_NON }, {ST_OPR, AT_OPR}, {ST_ERR, AT_ERR}, {ST_ERR, AT_ERR}, {ST_RPS, AT_RPS}, {ST_ERR, AT_ERR}},
    /* ST_FLG */ {{ST_ERR, AT_ERR }, {ST_ERR, AT_ERR}, {ST_OPD, AT_OPD}, {ST_ERR, AT_ERR}, {ST_ERR, AT_ERR}, {ST_ERR, AT_ERR}}
};

const char* ArithmeticExpression::getStateStr(State state) const
{
    if ( state >= ST_UPPER || state < ST_NON ) {
        printf("state is not valid, state=\"%d\"\n", (int)state);
        return m_state_info[ST_UPPER].state_str;
    }
    return m_state_info[state].state_str;
}

const char* ArithmeticExpression::getCurStateStr() const
{
    return getStateStr(m_state);
}


int ArithmeticExpression::getPriority(char c)
{
    /* priority order for high to low:
     * 4: '('
     * 3: '*' '/'
     * 2: '+' '-'
     * 1: ')'
     * 0: LOWEST_PRIO_OP
     */
    assert( c == '(' || c == ')' ||
            c == '^' ||
            c == '*' || c == '/' ||
            c == '+' || c == '-' ||
            c == LOWEST_PRIO_OP );
    int prio = 0;
    switch (c) {
    case '(': prio = 5; break;
    case '^': prio = 4; break;
    case '*':
    case '/': prio = 3; break;
    case '+':
    case '-': prio = 2; break;
    case ')': prio = 1; break;
    case LOWEST_PRIO_OP: prio = 0; break;
    default: prio = 0; break;
    }
    return prio;
}

bool ArithmeticExpression::handleAction(ActionType type)
{
    bool rc = false;
    switch (type) {
    case AT_NON: rc = handleNone(); break;
    case AT_FLG: rc = handleFlag(); break;
    case AT_OPR: rc = handleOperator();break;
    case AT_OPD: rc = handleOperand(); break;
    case AT_LPS: rc = handleLeftParenthesis(); break;
    case AT_RPS: rc = handleRightParenthesis(); break;
    case AT_ERR: rc = handleError(); break;
    default: rc = false;
    }
    return rc;
}

CharType ArithmeticExpression::getCharType(int c) {
    if ( ' ' == c || '\t' == c ) {
        return CT_WHITESPACE;
    }
    if ( (c <= '9' && c >= '0') || '.' == c ) {
        return CT_NUM;
    }
    if ( '(' == c ) {
        return CT_LP;
    }
    if ( ')' == c ) {
        return CT_RP;
    }
    if ( '*' == c || '/' == c || '^' == c) {
        return CT_OP;
    }
    // process CT_OP (+,-) and CT_FLG separately, make
    // sure return right char type
    if ( '+' == c || '-' == c) {
        switch (m_state) {
        case ST_NON:
        case ST_OPR:
        case ST_LPS:
        case ST_FLG: return CT_FLG;
        case ST_OPD:
        case ST_RPS: return CT_OP;
        default: break;
        }
    }
    return CT_UPPER;
}

bool ArithmeticExpression::handle(char c)
{
    CharType type = getCharType(c);
    if ( CT_UPPER == type) {
        printf("invalid char type, argument char='%c'\n",
                 m_arithmetic_expression[m_parse_pos]);
        return false;
    }
    // printf("current state=\"%s\", coming char = '%c', remaining=\"%s\"\n",
    //          getCurStateStr(), c, &m_arithmetic_expression[m_parse_pos+1]);

    StateTable* table_item = &m_state_table[m_state][type];
    bool rc = handleAction(table_item->action_type);
    if ( rc ) {
        // update state only when action runs successfully
        m_state = table_item->new_state;
    }
    return rc;
}

bool ArithmeticExpression::handleNone()
{
    return true;
}

bool ArithmeticExpression::handleFlag()
{
    //printf("handing flags ...\n");
    const char& c = m_arithmetic_expression[m_parse_pos];
    if ( '-' == c) {
        m_flg = FLG_NEGATIVE;
    } else {
        m_flg = FLG_ACTIVE;
    }
    return true;
}

bool ArithmeticExpression::handleOperator()
{
    const char& cur_opr = m_arithmetic_expression[m_parse_pos];
    char top = m_operator_stack.top();
    while ( getPriority(top) >= getPriority(cur_opr) && '(' != top ) {
        // update rpn
        m_rpn_expression.push_back(top);
        m_rpn_expression.push_back(' ');
        // calculate
        if (! calculate(top)) return false;
        // update new top
        m_operator_stack.pop();
        top = m_operator_stack.top();
    }
    // printf("pushing operator:'%c', rpn:\"%s\"\n",
    //          cur_opr, m_rpn_expression.c_str());
    m_operator_stack.push(cur_opr);
    return true;
}
bool ArithmeticExpression::handleError()
{
    printf("error occurs pos=\"%d\", tailing=\"%s\", raw=\"%s\"\n",
             m_parse_pos,
             &m_arithmetic_expression[m_parse_pos],
             m_arithmetic_expression.c_str());
    return false;               // always false
}

bool ArithmeticExpression::handleOperand()
{
    double int_val = 0;
    double dot_val = 0;
    double rst_val = 0;

    // sucks for fetching size each time for operand
    int size = m_arithmetic_expression.size();
    bool dot_occurred = false;
    int dot_part_length = 0;

    while ( m_parse_pos < size ) {
        char c = m_arithmetic_expression[m_parse_pos];
        if ( ! ('.' == c || (c >= '0' && c <= '9'))) {
            break;
        }
        if ( '.' == c && dot_occurred) {
            printf("dot occurs again\n");
            return false;
        }
        // now, three combination:
        // 1: '.' == c && ! dot_occurred
        if ( '.' == c ) {
            dot_occurred = true;
            c = m_arithmetic_expression[++m_parse_pos];
            continue;
        }
        // 2: '.' != c || dot_occurred
        if ( dot_occurred ) {
            ++dot_part_length;
            dot_val = dot_val * 10 + c - '0';
        }
        // 3: '.' != c || ! dot_occurred
        else {
            int_val = int_val * 10 + c - '0';
        }
        c = m_arithmetic_expression[++m_parse_pos];
    }
    if ( dot_occurred ) {
        dot_val *= pow(0.1, dot_part_length);
    }
    rst_val = m_flg * (int_val + dot_val);
    // NOTICE: important, reset m_flg to be +
    m_flg = FLG_ACTIVE;
    m_operand_stack.push(rst_val);

    // update m_rpn_expression
    static char val_str[20];
    memset(val_str, sizeof val_str, 0);
    snprintf(val_str, sizeof val_str, "%g ", rst_val);
    m_rpn_expression.append(val_str);

    // printf("rst part=\"%g\", int part=\"%g\", dot part=\"%g\", dot occurred=\"%s\"\n",
    //          rst_val, int_val, dot_val, dot_occurred ? "yes" : "no");
    return true;
}

bool ArithmeticExpression::handleLeftParenthesis()
{
    const char& cur_opr = m_arithmetic_expression[m_parse_pos];
    m_operator_stack.push(cur_opr);

    ++m_lp_count;
    return true;
}

bool ArithmeticExpression::handleRightParenthesis()
{
    --m_lp_count;
    if ( m_lp_count < 0 ) {
        printf("exists extra right parenthesis\n");
        return false;
    }

    char top;
    // pop till '('
    while ( ! m_operator_stack.empty()
            && '(' != (top = m_operator_stack.top())) {
        if ( LOWEST_PRIO_OP != top ) {
            m_rpn_expression.push_back(top);
            m_rpn_expression.push_back(' ');
        }
        if (! calculate(top)) return false;
        // always pop
        m_operator_stack.pop();
    }
    // just pop '(', do not append to RPN
    m_operator_stack.pop();

    // DO NOT save this operator:')' into stack
    return true;
}

bool ArithmeticExpression::calculate(char opr)
{
    if ( '(' == opr || ')' == opr || LOWEST_PRIO_OP == opr ) {
        // printf("do nothing for '%c'\n", opr);
        return true;
    }

    double r_opd = m_operand_stack.pop();
    double l_opd = m_operand_stack.pop();
    switch (opr) {
    case '^': m_operand_stack.push( pow(l_opd,r_opd) ); break;
    case '+': m_operand_stack.push( l_opd + r_opd ); break;
    case '-': m_operand_stack.push( l_opd - r_opd ); break;
    case '*': m_operand_stack.push( l_opd * r_opd ); break;
    case '/': {
        if ( 0 == r_opd ) {
            printf("divide 0\n");
            return false;
        }
        m_operand_stack.push( l_opd / r_opd);
        break;
    }
    default: printf("unknown operator='%c'\n", opr); return false;
    }

    // printf("opr='%c', l_opd=\"%g\", r_opd=\"%g\", rst=\"%g\"\n",
    //          opr, l_opd, r_opd, m_operand_stack.top());
    return true;
}

bool ArithmeticExpression::parse()
{
    int size = m_arithmetic_expression.size();
    if ( size == 0 ) {
        printf("empty arithmetic expression\n");
        return true;
    }
    while ( m_parse_pos < size ) {
        //printf("to handle \"%s\"\n", &m_arithmetic_expression[m_parse_pos]);
        const char& c = m_arithmetic_expression.at(m_parse_pos);
        if ( ! handle(c) ) {
            printf("failed to handle, "
                     "tailing=\"%s\", pos=%d, "
                     "char='%c', raw exp=\"%s\", "
                     "state=\"%s\"\n",
                     &m_arithmetic_expression[m_parse_pos],
                     m_parse_pos,
                     m_arithmetic_expression[m_parse_pos],
                     m_arithmetic_expression.c_str(),
                     getCurStateStr()
                );
            return false;
        }
        // only update m_parse_pos when char type is not CT_NUM,
        // Cause the handleOperand() already update m_parse_pos
        if ( CT_NUM != getCharType(c) ) {
            ++m_parse_pos;
        }
    }
    // check program terminal state
    if ( ! isOnTerminalState() ) {
        printf("program in not on terminal state, "
                 "current state=\"%s\"\n",
                 getCurStateStr());
        return false;
    }
    // check whether parenthesis matches
    if ( 0 != m_lp_count ) {
        printf("open left parenthesis exists, count=\"%d\"\n",
                 m_lp_count);
        return false;
    }
    // fulfill m_rpn_expression using operator stack
    while ( ! m_operator_stack.empty() ) {
        const char& top = m_operator_stack.top();
        if ( LOWEST_PRIO_OP != top ) {
            m_rpn_expression.push_back(top);
            m_rpn_expression.push_back(' ');
        }
        if (! calculate(top)) return false;
        // always pop
        m_operator_stack.pop();
    }

    // printf("arithmetic expression successfully parsed, "
    //          "rpn=\"%s\"\n", m_rpn_expression.c_str());
    if ( m_operand_stack.empty() ) {
        //printf("no value calculated\n");
        ;
    } else {
        // printf("value=\"%g\"\n", m_operand_stack.top());
        ;
    }
    return true;
}

bool ArithmeticExpression::getExpressionValue(double &val) const
{
    if ( !isOnTerminalState() ) {
        printf("program in not on terminal state, "
                 "current state=\"%s\"\n",
                 getCurStateStr());
        return false;
    }
    bool stack_empty = m_operand_stack.empty();
    if ( ! stack_empty || (stack_empty && ST_NON == m_state)) {
        val = stack_empty ? 0 : m_operand_stack.top();
        // printf("return value=\"%g\"\n", val);
        return true;
    }
    printf("the arithmetic expression is not successfully calculated\n");
    return false;
}
