#include "arithmetic_expression.hh"

#include <string.h>
#include <math.h>

#define LOWEST_PRIO_OP '#'
#define FLG_NEGATIVE -1
#define FLG_ACTIVE 1

#define log(...)                                \
    do {                                        \
        if (g_print_enable)                     \
            printf(__VA_ARGS__);                \
    } while (0)

#define error(...)                              \
    do {                                        \
        printf(__VA_ARGS__);                    \
    } while (0)

int g_print_enable = 0;

ArithmeticExpression::ArithmeticExpression(const std::string& infix_expression, int print_enable=0)
    : m_state(ST_BGN),
      m_arithmetic_expression(infix_expression),
      m_fnn(""),
      m_parse_pos(0),
      m_lp_count(0),
      m_fnn_spc_occurred(false),
      m_flg(FLG_ACTIVE)
{
    m_operator_stack.push(LOWEST_PRIO_OP);
    g_print_enable = print_enable;
}

ArithmeticExpression::StateInfo
ArithmeticExpression::m_state_info[] = {
    // NOTE: keep order same as that declared in enum State
    {ST_BGN, "ST_BGN"},
    {ST_OPR, "ST_OPR"},
    {ST_OPD, "ST_OPD"},
    {ST_LPS, "ST_LPS"},
    {ST_RPS, "ST_RPS"},
    {ST_FLG, "ST_FLG"},
    {ST_FNN, "ST_FNN"},
    {ST_ERR, "ST_ERR"},
    // to add
    {ST_UPPER, "ST_UPPER"}
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
    //           {CT_WHITESPACE},    {CT_OP},          {CT_NUM},         {CT_LP},          {CT_RP},          {CT_FLG},         {CT_ALP}          CT_COMMA
    /* ST_BGN */ {{ST_BGN, AT_NON }, {ST_ERR, AT_ERR}, {ST_OPD, AT_OPD}, {ST_LPS, AT_LPS}, {ST_ERR, AT_ERR}, {ST_FLG, AT_FLG}, {ST_FNN, AT_ALP}, {ST_ERR, AT_ERR}},
    /* ST_OPR */ {{ST_OPR, AT_NON }, {ST_ERR, AT_ERR}, {ST_OPD, AT_OPD}, {ST_LPS, AT_LPS}, {ST_ERR, AT_ERR}, {ST_FLG, AT_FLG}, {ST_FNN, AT_ALP}, {ST_ERR, AT_ERR}},
    /* ST_OPD */ {{ST_OPD, AT_NON }, {ST_OPR, AT_OPR}, {ST_ERR, AT_ERR}, {ST_ERR, AT_ERR}, {ST_RPS, AT_RPS}, {ST_OPR, AT_OPR}, {ST_ERR, AT_ERR}, {ST_BGN, AT_COMMA}},
    /* ST_LPS */ {{ST_LPS, AT_NON }, {ST_ERR, AT_ERR}, {ST_OPD, AT_OPD}, {ST_LPS, AT_LPS}, {ST_ERR, AT_ERR}, {ST_FLG, AT_FLG}, {ST_FNN, AT_ALP}, {ST_ERR, AT_ERR}},
    /* ST_RPS */ {{ST_RPS, AT_NON }, {ST_OPR, AT_OPR}, {ST_ERR, AT_ERR}, {ST_ERR, AT_ERR}, {ST_RPS, AT_RPS}, {ST_ERR, AT_ERR}, {ST_ERR, AT_ERR}, {ST_BGN, AT_COMMA}},
    /* ST_FLG */ {{ST_ERR, AT_ERR }, {ST_ERR, AT_ERR}, {ST_OPD, AT_OPD}, {ST_ERR, AT_ERR}, {ST_ERR, AT_ERR}, {ST_ERR, AT_ERR}, {ST_FNN, AT_ALP}, {ST_ERR, AT_ERR}},
    /* ST_FNN */ {{ST_FNN, AT_SPC }, {ST_ERR, AT_ERR}, {ST_FNN, AT_ALP}, {ST_BGN, AT_LPS}, {ST_ERR, AT_ERR}, {ST_ERR, AT_ERR}, {ST_FNN, AT_ALP}, {ST_ERR, AT_ERR}},
};

ArithmeticExpression::FunctionMeta
ArithmeticExpression::m_function_meta[FNN_UPPER] = {
    { "int"   , 1 },
    { "floor" , 1 },
    { "ceil"  , 1 },
    { "round" , 1 },
    { "fabs"  , 1 },
    { "sqrt"  , 1 },
    { "fmax"  , 2 },
    { "fmin"  , 2 },
};

const char* ArithmeticExpression::getStateStr(State state) const
{
    if ( state >= ST_UPPER || state < ST_BGN ) {
        log("state is not valid, state=\"%d\"\n", (int)state);
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

bool ArithmeticExpression::handleAction(ActionType type, char c)
{
    bool rc = false;
    switch (type) {
    case AT_NON: rc = handleNone(); break;
    case AT_FLG: rc = handleFlag(); break;
    case AT_OPR: rc = handleOperator();break;
    case AT_OPD: rc = handleOperand(); break;
    case AT_LPS: rc = handleLeftParenthesis(); break;
    case AT_RPS: rc = handleRightParenthesis(); break;
    case AT_ALP: rc = handleAlpha(c); break;
    case AT_SPC: rc = handleSpace(); break;
    case AT_COMMA: rc = handleComma(); break;
    case AT_ERR: rc = handleError(); break;
    default:
        log("no handler found for action type:%d\n", type);
        rc = false;
        break;
    }
    return rc;
}

CharType ArithmeticExpression::getCharType(int c) {
    if ( ',' == c ) return CT_COMMA;
    if ( ' ' == c || '\t' == c ) {
        return CT_WHITESPACE;
    }
    if ( (c <= '9' && c >= '0') || '.' == c ) {
        return CT_NUM;
    }
    if ( (c <= 'Z' && c >= 'A') || (c <= 'z' && c >= 'a') || '_' == c ) {
        return CT_ALP;
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
        case ST_BGN:
        case ST_OPR:
        case ST_LPS:
        case ST_FLG: return CT_FLG;
        case ST_OPD:
        case ST_RPS: return CT_OP;
        default:
            log("undefined charType: %c applied to state:%d\n", c, m_state);
            break;
        }
    }
    return CT_UPPER;
}

int ArithmeticExpression::fnn2fni(const std::string& fnn) {
    for (int i=0; i<FNN_UPPER; i++) {
        if (fnn == m_function_meta[i].name) {
            return i;
        }
    }
    log("unsupported function name: %s", m_fnn.c_str());
    return FNN_UPPER;
}

std::string ArithmeticExpression::fni2fnn(const int fni) {
    return m_function_meta[fni].name;
}

void ArithmeticExpression::dia(char* marker) {
    const char& c = m_arithmetic_expression[m_parse_pos];
    CharType type = getCharType(c);
    StateTable* table_item = &m_state_table[m_state][type];
    log("%s @%d %s '%c' CT=%d AT=%d",
        marker,
        m_parse_pos,
        getCurStateStr(),
        c,
        type,
        table_item->action_type
        );
    log(" opd_stack=");
    m_operand_stack.dia(g_print_enable);
    log(" opr_stack=");
    m_operator_stack.dia(g_print_enable);
    log(" cmc_stack=");
    m_fncmcnt_stack.dia(g_print_enable);
    log(" m_fnn=\"%s\" m_fnn_spc_occurred=%s m_flg='%d' m_lp_count=%d rpn=\"%s\"\n",
        m_fnn.c_str(),
        m_fnn_spc_occurred?"true":"false",
        m_flg,
        m_lp_count,
        m_rpn_expression.c_str()
        );
}

void ArithmeticExpression::appendToRpnExpression(char c) {
    if (c == 'f') {
        m_rpn_expression.append( fni2fnn(m_operator_stack.top(-1)) );
    } else {
        m_rpn_expression.push_back(c);
    }
}

bool ArithmeticExpression::handle(char c)
{
    CharType type = getCharType(c);
    if ( CT_UPPER == type) {
        error("invalid char type, argument char='%c'\n", c);
        return false;
    }

    StateTable* table_item = &m_state_table[m_state][type];
    bool rc = handleAction(table_item->action_type, c);
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
    log("handing flags ...\n");
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

        // NOTE: to support "right-associative ^", eg: 2^3^2, if both
        // cur_opr and top are '^', do not calculate immediately.
        if (cur_opr == top && cur_opr == '^') break;

        // update rpn
        appendToRpnExpression(top);
        appendToRpnExpression(' ');

        if (! calculate(top)) return false;
        // if calcute success, means cur opr consumed,
        // so pop, and go on to next opr.
        m_operator_stack.pop();
        top = m_operator_stack.top();
    }
    log("pushing operator:'%c', rpn:\"%s\"\n",
        cur_opr, m_rpn_expression.c_str());
    m_operator_stack.push(cur_opr);
    return true;
}
bool ArithmeticExpression::handleError()
{
    dia("E");
    error("error occurs pos=\"%d\", c='%c', raw=\"%s\"\n",
          m_parse_pos,
          m_arithmetic_expression[m_parse_pos],
          m_arithmetic_expression.c_str());
    return false;               // always false
}

bool ArithmeticExpression::handleOperand()
{
    double int_val = 0;
    double dot_val = 0;

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
            error("dot occurs again\n");
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

    // give chance to 'parse' to process m_parse_pos correctly
    --m_parse_pos;


    if ( dot_occurred ) {
        dot_val *= pow(0.1, dot_part_length);
    }

    double num = int_val + dot_val;

    if (m_flg == FLG_NEGATIVE) {
        // NOTE: to support conventional math precedence like "-2^2"
        // we expand "-2^2" as "-1 * 2 ^ 2", so to process "-2",
        // split into 3 steps:
        // 1. push -1 => opd_stack
        // 2. push  2 => opd_stack
        // 3. push  * => opr_stack
        m_operand_stack.push(-1);
        m_operand_stack.push(num);
        m_operator_stack.push('*');
        // NOTICE: important, reset m_flg to be +
        m_flg = FLG_ACTIVE;

        // m_rpn_expression
        // '*' will be filled by m_operator_stack
        m_rpn_expression.append(" -1 "+std::to_string(num)+" ");

    } else {
        m_operand_stack.push(num);

        // m_rpn_expression
        m_rpn_expression.append(" "+std::to_string(num)+" ");
    }

    log("m_flg=%d, int part=\"%g\", dot part=\"%g\", dot occurred=\"%s\"\n",
        m_flg, int_val, dot_val, dot_occurred ? "yes" : "no");
    return true;
}

bool ArithmeticExpression::handleLeftParenthesis()
{
    log("handleLeftParenthesis\n");
    if (m_fnn != "") {
        int fni = fnn2fni(m_fnn);
        if (fni == FNN_UPPER) {
            error("fni should never be equal to FNN_UPPER, fni=%d, m_fnn=%s\n", fni, m_fnn.c_str());
            return false;
        }
        m_operator_stack.push(fni);
        m_fnn = "";

        // NOTE: always record fncmcnt's comma count when fn gotten
        m_fncmcnt_stack.push(0);

        m_operator_stack.push('f'); // means function left parenthesis
        ++m_lp_count;
    } else {
        m_operator_stack.push('(');
        ++m_lp_count;
    }
    return true;
}

bool ArithmeticExpression::handleRightParenthesis()
{
    --m_lp_count;
    if ( m_lp_count < 0 ) {
        error("exists extra right parenthesis\n");
        return false;
    }

    // pop till '(' or 'f'
    while ( ! m_operator_stack.empty()) {
        dia("-");

        int top = m_operator_stack.top();
        if (top == '(') {
            // done for whole, (...), so pop '('
            int x = m_operator_stack.pop();
            log("pop <%d '%c'> in while loop\n", x, x);
            break;
        }

        if (top == 'f') {
            // NOTE: always calculate whole function when 'f' met.
            if (! calculate(top)) return false;
            // NOTE: 'f' already pop-ed in calculate(...), and due
            // to whole function: [fni, 'f', ..., ')'] have been processed,
            // so 1) pop 'f', fni, 2) break the loop immediately
            // that's meet the expection of handleRightParenthesis, coz
            // the right parenthesis handled is the closing parenthesis of
            // function if opr is 'f'.
            m_operator_stack.pop(); // 'f'
            m_operator_stack.pop(); // fni
            break;
        } else {
            if (! calculate(top)) return false;
            // NOTE: if calculate success, means the top opr been consumed,
            // so pop it. and go on to next opr
            int x = m_operator_stack.pop();
            log("pop opr:<%d '%c'> after calculate\n", x, x);
        }

        if ( LOWEST_PRIO_OP != top ) {
            appendToRpnExpression(top);
            appendToRpnExpression(' ');
        }

        dia("+");
    }

    // DO NOT save this operator:')' into stack
    return true;
}

bool ArithmeticExpression::handleAlpha(char c) {
    if (m_fnn_spc_occurred) {
        error("space not allow between alphas\n");
        return false;
    }
    m_fnn.push_back(c);
    m_fnn_spc_occurred = false;
    return true;
}

bool ArithmeticExpression::handleSpace() {
    if (ST_FNN == m_state) {
        m_fnn_spc_occurred = true;
    }
    return true;
}

bool ArithmeticExpression::handleComma() {
    int top;
    while (!m_operator_stack.empty() && (top = m_operator_stack.top()) != 'f') {
        dia("c");
        if(!calculate(top)) return false;
        dia("C");
        m_operator_stack.pop();
    }
    if (m_operator_stack.empty()) {
        error("invalid use of comma, comma can only be used as function argument separator\n");
        return false;
    }
    if (m_fncmcnt_stack.empty()) {
        error("comma can only be used as function argumet separator");
        return false;
    }
    m_fncmcnt_stack.push(m_fncmcnt_stack.pop() + 1);
    return true;
}

// NOTE: for easier maintainance, only modify m_operand_stack when
// calculating, keep m_operator_stack stay.
bool ArithmeticExpression::calculate(int opr)
{
    log("calculate: opr=<%c %d>\n", opr, opr);

    if ( '(' == opr || ')' == opr || LOWEST_PRIO_OP == opr ) {
        log("do nothing for opr='<%c %d>'\n", opr, opr);
        return true;
    }

    if ('f' == opr) {
        log("opr is 'f'\n");
        int fni = m_operator_stack.top(-1); // get fni
        log("fni=%d \n", opr, opr, fni);

        int fncmcnt = m_fncmcnt_stack.top();
        FunctionMeta fm = m_function_meta[fni];

        if (fncmcnt != fm.args_count-1) {
            error("function: '%s' expect %d arguments, while %d given. (fncmcnt=%d)\n", fm.name, fm.args_count, fncmcnt+1, fncmcnt);
            return false;
        }

        switch(fni) {
        case INT: {
            m_operand_stack.push((int)m_operand_stack.pop());
            break;
        }
        case FLOOR: {
            m_operand_stack.push(floor(m_operand_stack.pop()));
            break;
        }
        case CEIL: {
            m_operand_stack.push(ceil(m_operand_stack.pop()));
            break;
        }
        case ROUND: {
            m_operand_stack.push(round(m_operand_stack.pop()));
            break;
        }
        case FABS: {
            m_operand_stack.push(fabs(m_operand_stack.pop()));
            break;
        }
        case SQRT: {
            double opd = m_operand_stack.pop();
            if (opd < 0) {
                log("sqrt expects unsigned operand, while %f gotten\n", opd);
                return false;
            }
            m_operand_stack.push(sqrt(opd));
            break;
        }
        case FMAX: {
            double r_opd = m_operand_stack.pop();
            double l_opd = m_operand_stack.pop();
            m_operand_stack.push(fmax(l_opd, r_opd));
            break;
        }
        case FMIN: {
            double r_opd = m_operand_stack.pop();
            double l_opd = m_operand_stack.pop();
            m_operand_stack.push(fmin(l_opd, r_opd));
            break;
        }
        default:
            error("unsupported fni: %d", fni);
            return false;
        }

        // NOTE: always pop when fn has successfully been processed
        m_fncmcnt_stack.pop();
        return true;

    } else {
        // two-args opr
        double r_opd = m_operand_stack.pop();
        double l_opd = m_operand_stack.pop();
        switch (opr) {
        case '^': m_operand_stack.push( pow(l_opd,r_opd) ); break;
        case '+': m_operand_stack.push( l_opd + r_opd ); break;
        case '-': m_operand_stack.push( l_opd - r_opd ); break;
        case '*': m_operand_stack.push( l_opd * r_opd ); break;
        case '/': {
            if ( 0 == r_opd ) {
                error("divide 0\n");
                return false;
            }
            m_operand_stack.push( l_opd / r_opd);
            break;
        }
        default: log("unknown operator='<%c %d>'\n", opr, opr); return false;
        }
        log("opr='<%c %d>', l_opd=\"%g\", r_opd=\"%g\", rst=\"%g\"\n", opr, opr, l_opd, r_opd, m_operand_stack.top());

        return true;
    }
}

bool ArithmeticExpression::parse()
{
    int size = m_arithmetic_expression.size();
    if ( size == 0 ) {
        log("empty arithmetic expression\n");
        return true;
    }
    while ( m_parse_pos < size ) {
        log("> \"%s\"\n", &m_arithmetic_expression[m_parse_pos]);
        dia("-");
        const char& c = m_arithmetic_expression.at(m_parse_pos);
        if ( ! handle(c) ) {
            error("failed to handle, "
                  "m_fnn=\"%s\", "
                  "tailing=\"%s\", pos=%d, "
                  "char='%c', raw exp=\"%s\", "
                  "state=\"%s\"\n",
                  m_fnn.c_str(),
                  &m_arithmetic_expression[m_parse_pos],
                  m_parse_pos,
                  m_arithmetic_expression[m_parse_pos],
                  m_arithmetic_expression.c_str(),
                  getCurStateStr()
                );
            return false;
        }
        dia("+");
        ++m_parse_pos;
    }
    // check program terminal state
    if ( ! isOnTerminalState() ) {
        error("program in not on terminal state, current state=\"%s\"\n", getCurStateStr());
        return false;
    }
    // check whether parenthesis matches
    if ( 0 != m_lp_count ) {
        error("open left parenthesis exists, count=\"%d\"\n", m_lp_count);
        return false;
    }
    // fulfill m_rpn_expression using operator stack
    while ( ! m_operator_stack.empty() ) {
        dia("-");
        const int& top = m_operator_stack.top();
        if ( LOWEST_PRIO_OP != top ) {
            appendToRpnExpression(top);
            appendToRpnExpression(' ');
        }
        if (! calculate(top)) return false;
        // always pop to go on to next opr
        m_operator_stack.pop();
        dia("+");
    }
    dia("z");

    log("m_operand_stack.count() = %d\n", m_operand_stack.count());

    // now, m_operator_stack is empty, we expect m_operand_stack should be
    // only one value, aka. the result of math expression.
    if (m_operand_stack.count() != 1) {
        error("operand stack have %d oprand(s) left after process, should be 1 operand left\n", m_operand_stack.count());
        error("maybe too much argumets have been given to function(s), please check out the math expression\n");
        dia("C");
        return false;
    }

    log("arithmetic expression successfully parsed, rpn=\"%s\"\n", m_rpn_expression.c_str());
    log("value=\"%g\"\n", m_operand_stack.top());
    return true;
}

bool ArithmeticExpression::getExpressionValue(double &val) const
{
    if ( !isOnTerminalState() ) {
        error("program in not on terminal state, "
              "current state=\"%s\"\n",
              getCurStateStr());
        return false;
    }
    bool stack_empty = m_operand_stack.empty();
    if ( ! stack_empty || (stack_empty && ST_BGN == m_state)) {
        val = stack_empty ? 0 : m_operand_stack.top();
        log("return value=\"%g\"\n", val);
        return true;
    }
    error("the arithmetic expression is not successfully calculated\n");
    return false;
}
