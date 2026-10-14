#include "arithmetic_expression.hh"

#include <string.h>
#include <math.h>
#include <float.h>

#define LOWEST_PRIO_OP '#'
#define FLG_NEGATIVE -1
#define FLG_ACTIVE 1

#define print(...)                                \
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
      m_state_next_certained(ST_UPPER),
      m_arithmetic_expression(infix_expression),
      m_id(""),
      m_stmt(STMT_NON),
      m_stmt_assign_key(""),
      m_parse_pos(0),
      m_lp_count(0),
      m_id_spc_occurred(false),
      m_result(-1)
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
    {ST_IDF, "ST_IDF"},
    {ST_UNC, "ST_UNC(ertain)"},
    {ST_ERR, "ST_ERR"},
    {ST_END, "ST_END"},
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
    //              CT_WHITESPACE     CT_OP             CT_NUM            CT_LP             CT_RP             CT_FLG            CT_ALP            CT_COMMA            CT_SEMICOLON            CT_SYMBOL_EQUAL           CT_EXCLAMATION
    /* ST_BGN */ {{AT_NON, ST_BGN}, {AT_ERR, ST_ERR}, {AT_OPD, ST_OPD}, {AT_LPS, ST_LPS}, {AT_ERR, ST_ERR}, {AT_FLG, ST_FLG}, {AT_ALP, ST_IDF}, {AT_ERR, ST_ERR},   {AT_SEMICOLON, ST_END}, {AT_ERR, ST_ERR},          {AT_EXCLAMATION, ST_OPR}},
    /* ST_OPR */ {{AT_NON, ST_OPR}, {AT_ERR, ST_ERR}, {AT_OPD, ST_OPD}, {AT_LPS, ST_LPS}, {AT_ERR, ST_ERR}, {AT_FLG, ST_FLG}, {AT_ALP, ST_IDF}, {AT_ERR, ST_ERR},   {AT_ERR, ST_ERR},       {AT_ERR, ST_ERR},          {AT_EXCLAMATION, ST_OPR}},
    /* ST_OPD */ {{AT_NON, ST_OPD}, {AT_OPR, ST_OPR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_RPS, ST_RPS}, {AT_OPR, ST_OPR}, {AT_ERR, ST_ERR}, {AT_COMMA, ST_BGN}, {AT_SEMICOLON, ST_END}, {AT_SYMBOL_EQUAL, ST_UNC}, {AT_ERR, ST_ERR}},
    /* ST_LPS */ {{AT_NON, ST_LPS}, {AT_ERR, ST_ERR}, {AT_OPD, ST_OPD}, {AT_LPS, ST_LPS}, {AT_ERR, ST_ERR}, {AT_FLG, ST_FLG}, {AT_ALP, ST_IDF}, {AT_ERR, ST_ERR},   {AT_ERR, ST_ERR},       {AT_ERR, ST_ERR},          {AT_EXCLAMATION, ST_OPR}},
    /* ST_RPS */ {{AT_NON, ST_RPS}, {AT_OPR, ST_OPR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_RPS, ST_RPS}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_COMMA, ST_BGN}, {AT_SEMICOLON, ST_END}, {AT_SYMBOL_EQUAL, ST_UNC}, {AT_ERR, ST_ERR}},
    /* ST_FLG */ {{AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_OPD, ST_OPD}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ALP, ST_IDF}, {AT_ERR, ST_ERR},   {AT_ERR, ST_ERR},       {AT_ERR, ST_ERR},          {AT_EXCLAMATION, ST_OPR}},
    /* ST_IDF */ {{AT_SPC, ST_IDF}, {AT_OPR, ST_OPR}, {AT_ALP, ST_IDF}, {AT_LPS, ST_BGN}, {AT_RPS, ST_RPS}, {AT_FLG, ST_FLG}, {AT_ALP, ST_IDF}, {AT_COMMA, ST_BGN}, {AT_SEMICOLON, ST_END}, {AT_SYMBOL_EQUAL, ST_UNC}, {AT_ERR, ST_ERR}},
};

ArithmeticExpression::FunctionMeta
ArithmeticExpression::m_function_meta[FNN_UPPER] = {

    { "fabs", 1, 0},
    { "fmod", 2, 0},
    { "remainder", 2, 0},
    { "ceil", 1, 0},
    { "floor", 1, 0},
    { "round", 1, 0},
    { "trunc", 1, 0},

    { "pow", 2, 0},
    { "sqrt", 1, 0},
    { "cbrt", 1, 0},
    { "hypot", 2, 0},
    { "exp", 1, 0},
    { "exp2", 1, 0},
    { "expm1", 1, 0},
    { "log", 1, 0},
    { "log10", 1, 0},
    { "log2", 1, 0},
    { "log1p", 1, 0},

    { "sin", 1, 0},
    { "cos", 1, 0},
    { "tan", 1, 0},
    { "asin", 1, 0},
    { "acos", 1, 0},
    { "atan", 1, 0},
    { "atan2", 2, 0},

    { "sinh", 1, 0},
    { "cosh", 1, 0},
    { "tanh", 1, 0},
    { "asinh", 1, 0},
    { "acosh", 1, 0},
    { "atanh", 1, 0},

    { "fmin", 2, 0},
    { "fmax", 2, 0},
    { "fdim", 2, 0},
    { "copysign", 2, 0},
    { "isfinite", 1, 0},
    { "isinf", 1, 0},
    { "isnan", 1, 0},
    { "isnormal", 1, 0},
    { "signbit", 1, 0},

    { "int", 1, 1 },
    { "max", 2, 1 },
    { "min", 2, 1 },
    { "cmp", 3, 0 },

    {"printid", 1, 0},
    { "result", 1, 0},
};

const char* ArithmeticExpression::getStateStr(State state) const
{
    if ( state > ST_UPPER || state < ST_BGN ) {
        print("state is not valid, state=\"%d\"\n", (int)state);
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
    /* priority order for high to low */
    switch (c) {
    case '(': return 66;
    case '!': return 56;
    case '^': return 55;
    case '*':
    case '/': return 44;
    case '+':
    case '-': return 33;
    case ')': return 22;
    case '>':
    case 'e':
    case '<': return 11;
    case LOWEST_PRIO_OP: return 0;
    default: return 0;
    }
}

bool ArithmeticExpression::handleAction(ActionType type, char c)
{
    bool rc = false;
    switch (type) {
    case AT_NON: rc = handleNone(); break;
    case AT_FLG: rc = handleFlag(c); break;
    case AT_OPR: rc = handleOperator(c); break;
    case AT_OPD: rc = handleOperand(); break;
    case AT_LPS: rc = handleLeftParenthesis(); break;
    case AT_RPS: rc = handleRightParenthesis(); break;
    case AT_ALP: rc = handleAlpha(c); break;
    case AT_SPC: rc = handleSpace(); break;
    case AT_COMMA: rc = handleComma(); break;
    case AT_SEMICOLON: rc = handleSemicolon(); break;
    case AT_SYMBOL_EQUAL: rc = handleSymbolEqual(); break;
    case AT_EXCLAMATION: rc = handleExclamation(); break;
    case AT_ERR: rc = handleError(); break;
    default:
        print("no handler found for action type:%d\n", type);
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
    if ( '*' == c || '/' == c || '^' == c || '>' == c || '<' == c) {
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
        case ST_IDF:
        case ST_RPS: return CT_OP;
        default:
            print("undefined charType: %c applied to state:%d\n", c, m_state);
            break;
        }
    }
    if ( ';' == c) return CT_SEMICOLON;
    if ( '=' == c) return CT_SYMBOL_EQUAL;
    if ( '!' == c) return CT_EXCLAMATION;
    return CT_UPPER;
}

int ArithmeticExpression::fnn2fni(const std::string& fnn) {
    for (int i=0; i<FNN_UPPER; i++) {
        if (fnn == m_function_meta[i].name) {
            return i;
        }
    }
    print("unsupported function name: %s", m_id.c_str());
    return FNN_UPPER;
}

std::string ArithmeticExpression::fni2fnn(const int fni) {
    return m_function_meta[fni].name;
}

void ArithmeticExpression::dia(char* marker) {
    const char& c = m_arithmetic_expression[m_parse_pos];
    CharType type = getCharType(c);
    StateTable* table_item = &m_state_table[m_state][type];
    print("%s @%d %s '%c' CT=%d AT=%d",
          marker,
          m_parse_pos,
          getCurStateStr(),
          c,
          type,
          table_item->action_type
        );
    print(" opd_stack=");
    m_operand_stack.dia(g_print_enable);
    print(" opr_stack=");
    m_operator_stack.dia(g_print_enable);
    print(" cmc_stack=");
    m_fncmcnt_stack.dia(g_print_enable);
    for (const auto& item : m_id_map) {
        print(" m_id_map[\"%s\"]=%f ", item.first.c_str(), item.second);
    }
    print(" m_id = \"%s\" m_id_spc_occurred = %s m_lp_count = %d m_stmt = %d m_result = %g m_state_next_certained = %s rpn = \"%s\"\n",
          m_id.c_str(),
          m_id_spc_occurred?"true":"false",
          m_lp_count,
          m_stmt,
          m_result,
          getStateStr(m_state_next_certained),
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
        error("error: invalid char type, argument char='%c'\n", c);
        return false;
    }

    StateTable* table_item = &m_state_table[m_state][type];
    bool rc = handleAction(table_item->action_type, c);
    if ( rc ) {
        // update state only when action runs successfully
        m_state = table_item->new_state;
        print("handle: m_state: %s, m_state_next_certained: %s, m_parse_pos:%d, m_arithmetic_expression.size():%d\n", getCurStateStr(), getStateStr(m_state_next_certained), m_parse_pos, m_arithmetic_expression.size());

        // certain-ty uncertained state
        if (m_state == ST_UNC) {
            if (m_state_next_certained != ST_UPPER) {
                m_state = m_state_next_certained;
            } else {
                error("error: uncertained state occurs\n");
                dia("handle[uncertained state]");
                return false;
            }
        }

        // more chars left to process
        if (m_state == ST_END && m_parse_pos < m_arithmetic_expression.size()-1) {
            m_state = ST_BGN;
            // extra: reset m_result;
            m_result = -1;
        }
    }
    return rc;
}

bool ArithmeticExpression::handleNone()
{
    return true;
}

bool ArithmeticExpression::handleFlag(char flag)
{
    print("handing flags ...\n");
    if ( '-' == flag) {
        m_operand_stack.push(-1);
        m_operator_stack.push('*');
    }
    return true;
}

bool ArithmeticExpression::pushIdfValueToOperandStack(std::string idf) {
    if (m_id_map.find(idf) == m_id_map.end()) {
        error("error: undefined id: \"%s\"\n", idf.c_str());
        return false;
    }
    m_operand_stack.push(m_id_map[idf]);
    return true;
}

bool ArithmeticExpression::handleOperator(char opr)
{
    dia("-handleOperator");

    if (m_state == ST_IDF) {
        if (m_id_spc_occurred) {
            m_id_spc_occurred = false;
        }
        if (!m_id.empty()) {
            if (!pushIdfValueToOperandStack(m_id)) return false;
        }
    }

    char top = m_operator_stack.top();
    while ( getPriority(top) >= getPriority(opr) && '(' != top ) {

        // NOTE: to support "right-associative ^", eg: 2^3^2, if both
        // opr and top are '^', do not calculate immediately.
        if (opr == top && opr == '^') break;

        // right-associative for !, do not calculate immediately, case: !!1
        if (opr == top && opr == '!') break;

        if (! calculate(top)) return false;

        // update rpn
        appendToRpnExpression(top);
        appendToRpnExpression(' ');

        // if calcute success, means cur opr consumed,
        // so pop, and go on to next opr.
        m_operator_stack.pop();
        top = m_operator_stack.top();
    }

    print("pushing operator:'%c', rpn:\"%s\"\n", opr, m_rpn_expression.c_str());
    m_operator_stack.push(opr);

    dia("handleOperator+");
    return true;
}

bool ArithmeticExpression::handleError()
{
    dia("-handleError");
    error("error: error occurs pos=\"%d\", c='%c', raw=\"%s\"\n",
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
            error("error: dot occurs again\n");
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
    m_operand_stack.push(num);

    print("int part=\"%g\", dot part=\"%g\", dot occurred=\"%s\"\n",
        int_val, dot_val, dot_occurred ? "yes" : "no");
    return true;
}

bool ArithmeticExpression::handleLeftParenthesis()
{
    if (m_state == ST_IDF) {
        if (m_id_spc_occurred) {
            m_id_spc_occurred = false;
        }
    }

    print("handleLeftParenthesis\n");
    if (m_id != "") {
        int fni = fnn2fni(m_id);
        if (fni == FNN_UPPER) {
            error("error: fni should never be equal to FNN_UPPER, fni=%d, m_id=%s\n", fni, m_id.c_str());
            return false;
        }
        m_operator_stack.push(fni);
        m_id = "";

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

    if (m_state == ST_IDF) {
        if (m_id_spc_occurred) {
            m_id_spc_occurred = false;
        }
        if (!m_id.empty()) {
            if (!pushIdfValueToOperandStack(m_id)) return false;
        }
    }

    --m_lp_count;
    if ( m_lp_count < 0 ) {
        error("error: exists extra right parenthesis\n");
        return false;
    }

    // pop till '(' or 'f'
    while ( ! m_operator_stack.empty()) {
        dia("-handleRightParenthesis");

        int top = m_operator_stack.top();
        if (top == '(') {
            // done for whole, (...), so pop '('
            int x = m_operator_stack.pop();
            print("pop <%d '%c'> in while loop\n", x, x);
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
            print("pop opr:<%d '%c'> after calculate\n", x, x);
        }

        if ( LOWEST_PRIO_OP != top ) {
            appendToRpnExpression(top);
            appendToRpnExpression(' ');
        }

        dia("handleRightParenthesis+");
    }

    // DO NOT save this operator:')' into stack
    return true;
}

bool ArithmeticExpression::handleAlpha(char _c) {
    // TODO: [2026-10-10] parse like handleOperand to ignore m_id_spc_occurred.

    // for case "id<SPC+>id"
    if (m_id_spc_occurred) {
        error("error: space not allow between alphas\n");
        return false;
    }

    m_id = "";

    char c;
    int size = m_arithmetic_expression.size();
    while ( m_parse_pos < size ) {
        c = m_arithmetic_expression[m_parse_pos];
        if (!(getCharType(c) == CT_ALP || (c >= '0' && c <= '9'))) {
            break;
        }
        m_id.push_back(c);
        print("m_id:%s c=%c\n", m_id.c_str(), c);
        ++m_parse_pos;
    }

    // make m_parse_pos points to last char of idf, so ++m_parse_pos in
    // parse function will get right char right on to current idf
    if (m_id != "") {
        --m_parse_pos;
    }

    m_id_spc_occurred = false;

    dia("handleAlpha+");

    return true;
}

bool ArithmeticExpression::handleSpace() {
    if (m_state == ST_IDF) {
        m_id_spc_occurred = true;
    }
    return true;
}

bool ArithmeticExpression::handleComma() {
    if (m_state == ST_IDF) {
        if (m_id_spc_occurred) {
            m_id_spc_occurred = false;
        }
        if (!m_id.empty()) {
            if (!pushIdfValueToOperandStack(m_id)) return false;
            m_id = "";          // reset
        }
    }

    int top;
    while (!m_operator_stack.empty() && (top = m_operator_stack.top()) != 'f') {
        dia("-handleComma");
        if(!calculate(top)) return false;
        dia("handleComma+");
        m_operator_stack.pop();
    }
    if (m_operator_stack.empty()) {
        error("error: invalid use of comma, comma can only be used as function argument separator\n");
        return false;
    }
    if (m_fncmcnt_stack.empty()) {
        error("error: comma can only be used as function argumet separator");
        return false;
    }
    m_fncmcnt_stack.push(m_fncmcnt_stack.pop() + 1);
    return true;
}

bool ArithmeticExpression::handleSemicolon() {
    print("TODO: handleSemicolon should consume everything.\n");
    dia("-handleSemicolon");

    if (m_id_spc_occurred) {
        m_id_spc_occurred = false;
    }

    if (m_state == ST_IDF) {
        if (m_id != "") {
            if (!pushIdfValueToOperandStack(m_id)) return false;
            m_id = "";          // reset
        }
    }

    dia("-handleSemicolon+1");

    int top;
    while (!m_operator_stack.empty() && (top = m_operator_stack.top()) != LOWEST_PRIO_OP) {
        if (!calculate(top)) return false;
        m_operator_stack.pop();
    }

    if (m_stmt == STMT_ASSIGN) {
        // TODO: check if condition fit
        m_id_map[m_stmt_assign_key] = m_operand_stack.pop();
    }

    // reset everything
    m_id = "";
    m_id_spc_occurred = false;
    m_lp_count = 0;
    m_stmt = STMT_NON;
    m_stmt_assign_key = "";
    m_result = -1;

    if (m_operand_stack.count() > 1) {
        error("error: more than 1 operand left\n");
        dia(".");
        return false;
    }

    if (m_operand_stack.count() == 1) {
        m_result = m_operand_stack.pop();
    }

    dia("handleSemicolon+");

    return true;
}

bool ArithmeticExpression::handleSymbolEqual() {
    dia("-handleSymbolEqual");

    // = or ==
    bool hasAtLeastTwoSymbolEqual = (m_parse_pos+1) < m_arithmetic_expression.size() && m_arithmetic_expression[m_parse_pos+1] == '=';

    if (hasAtLeastTwoSymbolEqual) {
        // treat it as '==' operator
        ++m_parse_pos;
        m_state_next_certained = ST_OPR;
        return handleOperator('e');
    } else {
        // treat it as assign statement if m_state is ST_IDF
        if (m_state == ST_IDF) {
            m_stmt = STMT_ASSIGN;
            m_stmt_assign_key = m_id;
            m_id = "";
            m_state_next_certained = ST_BGN;
        } else {
            error("error: single '=' can only be used as assign statement: 'id = ...;'\n");
            dia("handleSymbolEqual");
            return false;
        }
    }
    return true;
}

bool ArithmeticExpression::handleExclamation() {
    return handleOperator('!');
}

// NOTE: for easier maintainance, only modify m_operand_stack when
// calculating, keep m_operator_stack stay.
bool ArithmeticExpression::calculate(int opr)
{
    print("calculate: opr=<%c %d>\n", opr, opr);

    if ( '(' == opr || ')' == opr || LOWEST_PRIO_OP == opr ) {
        print("do nothing for opr='<%c %d>'\n", opr, opr);
        return true;
    }

    if ('f' == opr) {
        print("opr is 'f'\n");
        int fni = m_operator_stack.top(-1); // get fni
        print("fni=%d\n", opr, opr, fni);

        int fncmcnt = m_fncmcnt_stack.top();
        FunctionMeta fm = m_function_meta[fni];

        if (fm.args_va) {
            if (fncmcnt < fm.args_count-1) {
                error("error: function: '%s' expect at least %d arguments, while %d given. (fncmcnt=%d)\n", fm.name, fm.args_count, fncmcnt+1, fncmcnt);
                return false;
            }
        } else {
            if (fncmcnt != fm.args_count-1) {
                error("error: function: '%s' expect %d arguments, while %d given. (fncmcnt=%d)\n", fm.name, fm.args_count, fncmcnt+1, fncmcnt);
                return false;
            }
        }

        switch(fni) {

        case FABS: {
            m_operand_stack.push(fabs(m_operand_stack.pop()));
            break;
        }
        case FMOD: {
            double r_opd = m_operand_stack.pop();
            double l_opd = m_operand_stack.pop();
            m_operand_stack.push(fmod(l_opd, r_opd));
            break;
        }
        case REMAINDER: {
            double r_opd = m_operand_stack.pop();
            double l_opd = m_operand_stack.pop();
            m_operand_stack.push(remainder(l_opd, r_opd));
            break;
        }
        case CEIL: {
            m_operand_stack.push(ceil(m_operand_stack.pop()));
            break;
        }
        case FLOOR: {
            m_operand_stack.push(floor(m_operand_stack.pop()));
            break;
        }
        case ROUND: {
            m_operand_stack.push(round(m_operand_stack.pop()));
            break;
        }
        case TRUNC: {
            m_operand_stack.push(trunc(m_operand_stack.pop()));
            break;
        }

        case POW: {
            double r_opd = m_operand_stack.pop();
            double l_opd = m_operand_stack.pop();
            m_operand_stack.push(pow(l_opd, r_opd));
            break;
        }
        case SQRT: {
            double opd = m_operand_stack.pop();
            if (opd < 0) {
                print("sqrt expects unsigned operand, while %f gotten\n", opd);
                return false;
            }
            m_operand_stack.push(sqrt(opd));
            break;
        }
        case CBRT: {
            m_operand_stack.push(cbrt(m_operand_stack.pop()));
            break;
        }
        case HYPOT: {
            double r_opd = m_operand_stack.pop();
            double l_opd = m_operand_stack.pop();
            m_operand_stack.push(hypot(l_opd, r_opd));
            break;
        }
        case EXP: {
            m_operand_stack.push(exp(m_operand_stack.pop()));
            break;
        }
        case EXP2: {
            m_operand_stack.push(exp2(m_operand_stack.pop()));
            break;
        }
        case EXPM1: {
            m_operand_stack.push(expm1(m_operand_stack.pop()));
            break;
        }
        case LOG: {
            m_operand_stack.push(log(m_operand_stack.pop()));
            break;
        }
        case LOG10: {
            m_operand_stack.push(log10(m_operand_stack.pop()));
            break;
        }
        case LOG2: {
            m_operand_stack.push(log2(m_operand_stack.pop()));
            break;
        }
        case LOG1P: {
            m_operand_stack.push(log1p(m_operand_stack.pop()));
            break;
        }

        case SIN: {
            m_operand_stack.push(sin(m_operand_stack.pop()));
            break;
        }
        case COS: {
            m_operand_stack.push(cos(m_operand_stack.pop()));
            break;
        }
        case TAN: {
            m_operand_stack.push(tan(m_operand_stack.pop()));
            break;
        }
        case ASIN: {
            m_operand_stack.push(asin(m_operand_stack.pop()));
            break;
        }
        case ACOS: {
            m_operand_stack.push(acos(m_operand_stack.pop()));
            break;
        }
        case ATAN: {
            m_operand_stack.push(atan(m_operand_stack.pop()));
            break;
        }
        case ATAN2: {
            double r_opd = m_operand_stack.pop();
            double l_opd = m_operand_stack.pop();
            m_operand_stack.push(atan2(l_opd,r_opd));
            break;
        }

        case SINH: {
            m_operand_stack.push(sinh(m_operand_stack.pop()));
            break;
        }
        case COSH: {
            m_operand_stack.push(cosh(m_operand_stack.pop()));
            break;
        }
        case TANH: {
            m_operand_stack.push(tanh(m_operand_stack.pop()));
            break;
        }
        case ASINH: {
            m_operand_stack.push(asinh(m_operand_stack.pop()));
            break;
        }
        case ACOSH: {
            m_operand_stack.push(acosh(m_operand_stack.pop()));
            break;
        }
        case ATANH: {
            m_operand_stack.push(atanh(m_operand_stack.pop()));
            break;
        }

        case FMIN: {
            double x = m_operand_stack.pop();
            double y = m_operand_stack.pop();
            m_operand_stack.push(fmin(y,x));
            break;
        }
        case FMAX: {
            double x = m_operand_stack.pop();
            double y = m_operand_stack.pop();
            m_operand_stack.push(fmax(y,x));
            break;
        }
        case FDIM: {
            double x = m_operand_stack.pop();
            double y = m_operand_stack.pop();
            m_operand_stack.push(fdim(y,x));
            break;
        }
        case COPYSIGN: {
            double x = m_operand_stack.pop();
            double y = m_operand_stack.pop();
            m_operand_stack.push(copysign(y,x));
            break;
        }
        case ISFINITE: {
            m_operand_stack.push(isfinite(m_operand_stack.pop()));
            break;
        }
        case ISINF: {
            m_operand_stack.push(isinf(m_operand_stack.pop()));
            break;
        }
        case ISNAN: {
            m_operand_stack.push(isnan(m_operand_stack.pop()));
            break;
        }
        case ISNORMAL: {
            m_operand_stack.push(isnormal(m_operand_stack.pop()));
            break;
        }
        case SIGNBIT: {
            m_operand_stack.push(signbit(m_operand_stack.pop()));
            break;
        }

        case INT: {
            m_operand_stack.push((int)m_operand_stack.pop());
            break;
        }
        case MAX: {
            int args_count = fncmcnt + 1;
            double v = FLT_MIN;
            int opd;
            while (!m_operand_stack.empty() && args_count--) {
                opd = m_operand_stack.pop();
                if (opd > v) v = opd;
            }
            if (args_count != 0) {
                error("error: no args found in operand stack, should not happen\n");
                return false;
            }
            m_operand_stack.push(v);
            break;
        }
        case MIN: {
            int args_count = fncmcnt + 1;
            double v = FLT_MAX;
            int opd;
            while (!m_operand_stack.empty() && args_count--) {
                opd = m_operand_stack.pop();
                if (opd < v) v = opd;
            }
            if (args_count != 0) {
                error("error: no args found in operand stack, should not happen\n");
                return false;
            }
            m_operand_stack.push(v);
            break;
        }
        case CMP: {
            if (m_operand_stack.count() < 3) {
                error("cpm(v1,v2,v3) expects three operands, while %d left\n", m_operand_stack.count());
                return false;
            }
            double v3 = m_operand_stack.pop();
            double v2 = m_operand_stack.pop();
            double v1 = m_operand_stack.pop();
            double v = v1 != 0? v2: v3;
            m_operand_stack.push(v);
            break;
        }
        case PRINTID: {
            // TODO: check conditon fit
            printf("PRINTID: key=\"%s\", value=%g\n ", m_id.c_str(), m_id_map[m_id]);
            break;
        }
        case RESULT: {
            m_result = m_operand_stack.top();
            break;
        }
        default:
            error("error: unsupported fni: %d\n", fni);
            return false;
        }

        // NOTE: always pop when fn has successfully been processed
        m_fncmcnt_stack.pop();
        return true;

    } else {
        switch (opr) {
        case '^': {
            double r_opd = m_operand_stack.pop();
            double l_opd = m_operand_stack.pop();
            m_operand_stack.push( pow(l_opd,r_opd) ); break;
        }
        case '+': {
            double r_opd = m_operand_stack.pop();
            double l_opd = m_operand_stack.pop();
            m_operand_stack.push( l_opd + r_opd ); break;
        }
        case '-': {
            double r_opd = m_operand_stack.pop();
            double l_opd = m_operand_stack.pop();
            m_operand_stack.push( l_opd - r_opd ); break;
        }
        case '*': {
            double r_opd = m_operand_stack.pop();
            double l_opd = m_operand_stack.pop();
            m_operand_stack.push( l_opd * r_opd ); break;
        }
        case '/': {
            double r_opd = m_operand_stack.pop();
            if ( 0 == r_opd ) {
                error("error: divide 0\n");
                return false;
            }
            double l_opd = m_operand_stack.pop();
            m_operand_stack.push( l_opd / r_opd);
            break;
        }
        case '>': {
            double r_opd = m_operand_stack.pop();
            double l_opd = m_operand_stack.pop();
            m_operand_stack.push(l_opd > r_opd? 1: 0);
            break;
        }
        case '<': {
            double r_opd = m_operand_stack.pop();
            double l_opd = m_operand_stack.pop();
            m_operand_stack.push(l_opd < r_opd? 1: 0);
            break;
        }
        case 'e': {             // "=="
            double r_opd = m_operand_stack.pop();
            double l_opd = m_operand_stack.pop();
            m_operand_stack.push(l_opd == r_opd? 1: 0);
            break;
        }
        case '!': {
            double r_opd = m_operand_stack.pop();
            m_operand_stack.push(r_opd == 0? 1: 0);
            break;
        }
        default: {
            error("error: unsuppored operator='<%c %d>'\n", opr, opr); return false;
        }
        }
        return true;
    }
}

bool ArithmeticExpression::parse()
{
    int size = m_arithmetic_expression.size();
    if ( size == 0 ) {
        print("empty arithmetic expression\n");
        return true;
    }

    print("parse \"%s\"\n", m_arithmetic_expression.c_str());

    while ( m_parse_pos < size ) {
        print("\n> \"%s\"\n", &m_arithmetic_expression[m_parse_pos]);
        dia("-parse");
        const char& c = m_arithmetic_expression.at(m_parse_pos);
        if ( ! handle(c) ) {
            error("error: failed to handle, "
                  "m_id=\"%s\", "
                  "tailing=\"%s\", pos=%d, "
                  "char='%c', raw exp=\"%s\", "
                  "state=\"%s\"\n",
                  m_id.c_str(),
                  &m_arithmetic_expression[m_parse_pos],
                  m_parse_pos,
                  m_arithmetic_expression[m_parse_pos],
                  m_arithmetic_expression.c_str(),
                  getCurStateStr()
                );
            return false;
        }
        dia("parse+");
        ++m_parse_pos;
    }

    print("\n");

    // check program terminal state
    if ( ! isOnTerminalState() ) {
        error("error: program in not on terminal state, current state=\"%s\"\n", getCurStateStr());
        return false;
    }

    if (m_state == ST_IDF) {
        if (m_id_spc_occurred) {
            m_id_spc_occurred = false;
        }
        if (!m_id.empty()) {
            if (!pushIdfValueToOperandStack(m_id)) return false;
        }
    }

    // check whether parenthesis matches
    if ( 0 != m_lp_count ) {
        error("error: open left parenthesis exists, count=\"%d\"\n", m_lp_count);
        return false;
    }
    // fulfill m_rpn_expression using operator stack
    int top;
    while ( ! m_operator_stack.empty() && (top = m_operator_stack.top()) != LOWEST_PRIO_OP) {
        dia("-post-process");
        const int& top = m_operator_stack.top();
        if ( LOWEST_PRIO_OP != top ) {
            appendToRpnExpression(top);
            appendToRpnExpression(' ');
        }
        if (! calculate(top)) return false;
        // always pop to go on to next opr
        m_operator_stack.pop();
        dia("post-process+");
    }
    dia("z");

    print("m_operand_stack.count() = %d\n", m_operand_stack.count());

    if (m_stmt == STMT_NON) {
        // now, m_operator_stack is empty, we expect m_operand_stack should be
        // only one value, aka. the result of math expression.
        // AND m_result have not been set.
        if (m_result == -1) {   // not been set yet
            if (m_operand_stack.count() != 1) {
                if (m_result == -1) {
                    error("error: operand stack have %d oprand(s) left after process, should be 1 operand left\n", m_operand_stack.count());
                    dia("C");
                    return false;
                }
            } else {
                m_result = m_operand_stack.pop();
            }
        }
    }

    print("arithmetic expression successfully parsed, rpn=\"%s\"\n", m_rpn_expression.c_str());
    return true;
}

bool ArithmeticExpression::getExpressionValue(double &val) const
{
    val = m_result;
    return true;
}
