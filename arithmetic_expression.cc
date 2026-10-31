#include "arithmetic_expression.hh"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#define LOWEST_PRIO_OP '#'
#define FLG_NEGATIVE -1
#define FLG_ACTIVE 1

#define E_DEV_ERROR 1       // ERROR happened during development, *must* to be fixed by coding
#define E_INPUT_ERROR 10    // *fundamental* parse error
#define E_SYNTAX 20         // parse error
#define E_GRAMMAR 30        // language rule error
#define E_EVALUATE 50       // evaluating error
#define E_RUN 89
#define E_SHOULD_NOT_HAPPEN 99

#define print(...)                                                      \
    do {                                                                \
        if (g_print_enable) {                                           \
            fprintf(stderr,"%s:%04d:%d: note: %d:%s:%d ", __FILE__, __LINE__, 1, g_depth, scope_name.c_str(), g_ln_count); \
            fprintf(stderr,__VA_ARGS__);                                \
            fprintf(stderr," :%s\n", __func__);                        \
        }                                                               \
    } while (0)

#define printnoln(...)                                                  \
    do {                                                                \
        if (g_print_enable) {                                           \
            fprintf(stderr,"%s:%04d:%d: note: %d:%s:%d ", __FILE__, __LINE__, 1, g_depth, scope_name.c_str(), g_ln_count); \
            fprintf(stderr,__VA_ARGS__);                                \
        }                                                               \
    } while (0)

#define __print(...)                            \
    do {                                        \
        if (g_print_enable) {                   \
            fprintf(stderr,__VA_ARGS__);        \
        }                                       \
    } while (0)

#define error(...)                                                      \
    do {                                                                \
        fprintf(stderr,"%s:%04d:%d: error: %d:%s:%d ", __FILE__, __LINE__, 1, g_depth, scope_name.c_str(), g_ln_count); \
        fprintf(stderr,__VA_ARGS__);                                    \
        fprintf(stderr," :%s\n", __func__);                            \
    } while (0)

#define errornoln(...)                                                  \
    do {                                                                \
        fprintf(stderr,"%s:%04d:%d: error: %d:%s:%d ", __FILE__, __LINE__, 1, g_depth, scope_name.c_str(), g_ln_count); \
        fprintf(stderr,__VA_ARGS__);                                    \
    } while (0)

#define __error(...)                            \
    do {                                        \
        fprintf(stderr,__VA_ARGS__);            \
    } while (0)

#define warn(...)                                                       \
    do {                                                                \
        if (g_print_enable) {                                           \
            fprintf(stderr,"%s:%04d:%d: warning: %d:%s:%d ", __FILE__, __LINE__, 1, g_depth, scope_name.c_str(), g_ln_count); \
            fprintf(stderr,__VA_ARGS__);                                \
            fprintf(stderr," :%s\n", __func__);                        \
        }                                                               \
    } while (0)                                                         \

int g_print_enable = 0;
int g_depth = 0;
int g_ln_count = 1;
std::string scope_name = "global";

ArithmeticExpression::ArithmeticExpression(
    const std::string& infix_expression,
    int depth,
    int lcb_count,
    std::unordered_map<std::string,std::unordered_map<int,double>> idf_map, // FIXME: [2026-10-21] not memory-saving friendly
    std::unordered_map<std::string,double> pre_defined_idf_map,
    ContextType contextType,
    int print_enable)
    : m_depth(depth),
      m_state_prev(ST_UPPER),
      m_state(ST_BGN),
      m_state_next_certained(ST_UPPER),

      m_arithmetic_expression(infix_expression),
      m_parse_pos(0),

      m_stmt_count(0),
      m_ln_count(1),
      m_lp_count(0),
      m_lcb_count_init(lcb_count),
      m_lcb_count(lcb_count),

      m_function_declare_cur_name(""),
      m_function_declare_cur_argidfs({}),
      m_function_declare_map({}),
      m_function_declare_exec_begin_name_map({}),

      m_expect_char_types({}),

      m_id(""),
      m_id_spc_occurred(false),

      m_stmt_assign_key("")
{
    m_token_idx = 0;

    m_opr_stack.setName("m_opr_stack");
    pushOpr(LOWEST_PRIO_OP);

    m_opd_stack.setName("m_opd_stack");
    m_opd_stack.push({m_token_idx, -9, OPD_STACK_BOTTOM_BASE, 0, ""}); // to ensure top() always have stuff to avoid break

    m_ctxt_stack.setName("m_ctxt_stack");
    m_ctxt_stack.push(CTXT_STACK_BOTTOM_BASE); // to ensure top() always have stuff to avoid break
    m_ctxt_stack.push(contextType);

    m_fncmcnt_stack.setName("m_fncmcnt_stack");

    // FIXME: [2026-10-27] idf store should be global, so nested code can re-assign value to global var
    // one way it to can use setMeta to use referer, or use global var directly.
    m_idf_map = idf_map;
    m_idf_pre_defined_scoped_map = pre_defined_idf_map;
    m_idf_outer_scope_reassigned_map = {};

    g_print_enable = print_enable;
    print("g_print_enable: %d", g_print_enable);

    dia("ArithmeticExpression:+");
}

ArithmeticExpression::StateInfo
ArithmeticExpression::m_state_info[] = {
    // NOTE: keep order same as that declared in enum State
    {ST_BGN, "ST_BGN"},
    {ST_OPR, "ST_OPR"},
    {ST_NUM, "ST_NUM"},
    {ST_LPS, "ST_LPS"},
    {ST_RPS, "ST_RPS"},
    {ST_FLG, "ST_FLG"},
    {ST_IDF, "ST_IDF"},
    {ST_LCB, "ST_LCB"},
    {ST_RCB, "ST_RCB"},
    {ST_CLN, "ST_CLN"},

    {ST_UNC, "ST_UNC"}, // uncertained
    {ST_ERR, "ST_ERR"},
    {ST_STT, "ST_STT"},         // statement terminal
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
    //             CT_SPC            CT_OP             CT_NUM 0-9        CT_LP (           CT_RP )           CT_FLG -+         CT_ALP            CT_CMA ,          CT_SMC ;          CT_SEQ =          CT_AMP &          CT_VBR |          CT_EXM !          CT_LCB {          CT_RCB }          CT_CLN :          CT_DBQ "
    //
    /* ST_BGN */ {{AT_SPC, ST_BGN}, {AT_ERR, ST_ERR}, {AT_NUM, ST_NUM}, {AT_LPS, ST_LPS}, {AT_RPS, ST_UNC}, {AT_FLG, ST_FLG}, {AT_ALP, ST_IDF}, {AT_ERR, ST_ERR}, {AT_SMC, ST_STT}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_EXM, ST_OPR}, {AT_LCB, ST_LCB}, {AT_RCB, ST_RCB}, {AT_ERR, ST_ERR}, {AT_DBQ, ST_NUM}},
    /* ST_OPR */ {{AT_SPC, ST_OPR}, {AT_ERR, ST_ERR}, {AT_NUM, ST_NUM}, {AT_LPS, ST_LPS}, {AT_ERR, ST_ERR}, {AT_FLG, ST_FLG}, {AT_ALP, ST_IDF}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_EXM, ST_OPR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_DBQ, ST_NUM}},
    /* ST_NUM */ {{AT_SPC, ST_NUM}, {AT_OPR, ST_OPR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_RPS, ST_RPS}, {AT_OPR, ST_OPR}, {AT_ERR, ST_ERR}, {AT_CMA, ST_BGN}, {AT_SMC, ST_STT}, {AT_SEQ, ST_UNC}, {AT_AMP, ST_UNC}, {AT_VBR, ST_UNC}, {AT_ERR, ST_ERR}, {AT_LCB, ST_LCB}, {AT_RCB, ST_RCB}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}},
    /* ST_LPS */ {{AT_SPC, ST_LPS}, {AT_ERR, ST_ERR}, {AT_NUM, ST_NUM}, {AT_LPS, ST_LPS}, {AT_ERR, ST_ERR}, {AT_FLG, ST_FLG}, {AT_ALP, ST_IDF}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_EXM, ST_OPR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_DBQ, ST_NUM}},
    /* ST_RPS */ {{AT_SPC, ST_RPS}, {AT_OPR, ST_OPR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_RPS, ST_RPS}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_CMA, ST_BGN}, {AT_SMC, ST_STT}, {AT_SEQ, ST_UNC}, {AT_AMP, ST_UNC}, {AT_VBR, ST_UNC}, {AT_ERR, ST_ERR}, {AT_LCB, ST_LCB}, {AT_RCB, ST_RCB}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}},
    /* ST_FLG */ {{AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_NUM, ST_NUM}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ALP, ST_IDF}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_EXM, ST_OPR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}},
    /* ST_IDF */ {{AT_SPC, ST_UNC}, {AT_OPR, ST_OPR}, {AT_ALP, ST_IDF}, {AT_LPS, ST_BGN}, {AT_RPS, ST_RPS}, {AT_FLG, ST_FLG}, {AT_ALP, ST_IDF}, {AT_CMA, ST_BGN}, {AT_SMC, ST_STT}, {AT_SEQ, ST_UNC}, {AT_AMP, ST_UNC}, {AT_VBR, ST_UNC}, {AT_ERR, ST_ERR}, {AT_LCB, ST_LCB}, {AT_RCB, ST_RCB}, {AT_CLN, ST_CLN}, {AT_CLN, ST_CLN}},
    /* ST_LCB */ {{AT_SPC, ST_LCB}, {AT_ERR, ST_ERR}, {AT_NUM, ST_NUM}, {AT_LPS, ST_BGN}, {AT_ERR, ST_ERR}, {AT_FLG, ST_FLG}, {AT_ALP, ST_IDF}, {AT_ERR, ST_ERR}, {AT_SMC, ST_STT}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_LCB, ST_LCB}, {AT_NON, ST_UNC}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}},
    /* ST_RCB */ {{AT_SPC, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}},
    /* ST_RCB */ {{AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_SEQ, ST_UNC}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}},
};

ArithmeticExpression::CharTypeInfo
ArithmeticExpression::m_char_type_info[] = {
    {CT_SPC, " "},
    {CT_OP,  "+-*/^>< && ||"},
    {CT_NUM, "0-9"},
    {CT_LP,  "("},
    {CT_RP,  ")"},
    {CT_FLG, "flag +-"},
    {CT_ALP, "A-Za-z"},
    {CT_CMA, ","},
    {CT_SMC, ";"},
    {CT_SEQ, "="},
    {CT_AMP, "&"},
    {CT_VBR, "|"},
    {CT_EXM, "!"},
    {CT_LCB, "{"},
    {CT_RCB, "}"},
    {CT_CLN, ":"},
    {CT_DBQ, "\""},

    {CT_UPPER, "CT_UPPER"}
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

    { "int", 1, 0 },
    { "max", 2, 1 },
    { "min", 2, 1 },
    { "cmp", 3, 0 },

    // TODO: { "fnn", min, max }, max = -1 means variable arguments, so for {"print", 0, 1}
    { "print", 1, 0},
    { "printf", 1, 1},
};

const char* ArithmeticExpression::getStateStr(State state) const
{
    if ( state > ST_UPPER || state < ST_BGN ) {
        print("state is not valid, state=\"%d\"", (int)state);
        return m_state_info[ST_UPPER].state_str;
    }
    return m_state_info[state].state_str;
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
    case '<': return 11;
    case 'e': return 9;
    case 'a':
    case 'o': return 5;
    case LOWEST_PRIO_OP: return 0;
    default: return 0;
    }
}

bool ArithmeticExpression::handleAction(ActionType type, char c)
{
    switch (type) {
    case AT_NON: return handleNone();
    case AT_FLG: return handleFlag(c);
    case AT_OPR: return handleOperator(c);
    case AT_NUM: return handleNum();
    case AT_LPS: return handleLeftParenthesis();
    case AT_RPS: return handleRightParenthesis();
    case AT_ALP: return handleAlpha(c);
    case AT_SPC: return handleSpace();
    case AT_CMA: return handleComma();
    case AT_SMC: return handleSemicolon();
    case AT_SEQ: return handleSymbolEqual();
    case AT_AMP: return handleSymbolAmpersand();
    case AT_VBR: return handleSymbolVbar();
    case AT_EXM: return handleExclamation();
    case AT_LCB: return handleLeftCurlyBrace();
    case AT_RCB: return handleRightCurlyBrace();
    case AT_CLN: return handleColon();
    case AT_DBQ: return handleDoubleQuote();
    // more here
    case AT_ERR: handleError(); return false;
    default:
        print("no handler found for action type:%d", type);
        return false;
    }
    return false;
}

const char* ArithmeticExpression::getCharTypeStr(int ct) const {
    const CharTypeInfo& item = m_char_type_info[ct];
    if (item.char_type != ct) {
        error("*DEV ERROR* unmatched chartype in m_char_type_info, ct=%d, item.char_type=%d", ct, item.char_type);
        exit(E_DEV_ERROR);
    }
    return item.str;
}

CharType ArithmeticExpression::getCharType(int c) {
    if ( ',' == c ) return CT_CMA;
    if ( ' ' == c || '\t' == c || '\n' == c ) {
        return CT_SPC;
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
        case ST_NUM:
        case ST_IDF:
        case ST_RPS: return CT_OP;
        default:
            error("undefined charType: %c applied to state:%d", c, m_state);
            break;
        }
    }

    if ( ';' == c) return CT_SMC;
    if ( '=' == c) return CT_SEQ;
    if ( '&' == c) return CT_AMP;
    if ( '|' == c) return CT_VBR;
    if ( '!' == c) return CT_EXM;
    if ( '{' == c) return CT_LCB;
    if ( '}' == c) return CT_RCB;
    if ( ':' == c) return CT_CLN;
    if ( '"' == c) return CT_DBQ;

    return CT_UPPER;
}

int ArithmeticExpression::fnn2fni(const std::string& fnn) {
    for (int i=0; i<FNN_UPPER; i++) {
        if (fnn == m_function_meta[i].name) {
            return i;
        }
    }
    error("unsupported function name: %s", m_id.c_str());
    error("\"function\" keyword missed, suggest: \"function fn(...)\"");
    exit(E_GRAMMAR);
}

std::string ArithmeticExpression::fni2fnn(const int fni) {
    return m_function_meta[fni].name;
}

void ArithmeticExpression::syntax(ContextType t) {
    error("syntax:");
    switch(t) {
    case STMT_IF:
    case STMT_IF_COND:
    case STMT_IF_BLOCK: {
        errornoln("\t`if condition {}`\n");
        errornoln("\u200B\n");
        errornoln("\tif `condition` starts with '(', the space after `if` is optional\n");
        break;
    }
    case STMT_ELIF:
    case STMT_ELIF_COND:
    case STMT_ELIF_BLOCK: {
        errornoln("\t`if condition {} elif condtion {}`\n");
        errornoln("\t`if condition {} elif condition {} else {}`\n");
        errornoln("\u200B\n");
        errornoln("\tif `condition` of `elif` starts with '(', the space after `elif` is optional\n");
        break;
    }
    case STMT_ELSE:
    case STMT_ELSE_BLOCK: {
        errornoln("\t`if condition {} else {}`\n");
        errornoln("\t`if condition {} elif condition {} else {}`\n");
        errornoln("\u200B\n");
        errornoln("\tspace between `else` and `{` is optional\n");
        break;
    }
    case FUNCTION_DEFINITION:
    case FUNCTION_ARGS:
    case FUNCTION_BLOCK: {
        errornoln("\t`function function_name() {} `\n");
        errornoln("\t`function function_name(arg, ...) {} `\n");
        errornoln("\u200B\n");
        break;
    }
    case FUNCTION_RETURN:
    case FUNCTION_RETURN_VALUE: {
        errornoln("\t`return;`\n");
        errornoln("\t`return exp;`\n");
        errornoln("\u200B\n");
        break;
    }
    default: {
        // warn("TODO add synatx for contextType: %s", getContextTypeStr(t));
        break;
    }
    }
}

void ArithmeticExpression::dia(const std::string& marker="") {
    if (g_print_enable== 0) return;

    const char& c = m_arithmetic_expression[m_parse_pos];
    CharType type = getCharType(c);
    StateTable* table_item = &m_state_table[m_state][type];

    printnoln("%s @%d", marker.c_str(), m_parse_pos);

    if (g_print_enable & 1) {   // st
        __print(" #1");
        __print(" [%s:%s + '%c'(CT=%d,AT=%d) => %s]",
                getStateStr(m_state_prev),
                getStateStr(m_state),
                c,
                type,
                table_item->action_type,
                getStateStr(table_item->new_state)
            );
        if (g_print_enable <1*2) __print(" :dia\n");
    }

    if (g_print_enable & 2) {   // opr, opd
        __print(" #2");
        __print(" opd_stack="); m_opd_stack.dia(g_print_enable);
        __print(" opr_stack="); m_opr_stack.dia(g_print_enable);
        if (g_print_enable <2*2) __print(" :dia\n");
    }
    if (g_print_enable & 4) {
        __print(" #4");
        __print(" cmc_stack="); m_fncmcnt_stack.dia(g_print_enable);
    }

    if (g_print_enable & 8) {
        __print(" #8");
        __print(" m_ctxt_stack="); m_ctxt_stack.dia(g_print_enable);
        __print(" m_expect_char_types=[");
        for (const auto& item : m_expect_char_types) {
            __print("'%s,' ", getCharTypeStr(item));
        }
        __print("]");
        if (g_print_enable <8*2) __print(" :dia\n");
    }

    if (g_print_enable & 16) {  // idf
        __print(" #16");
        __print(" m_idf_map");
        for (const auto& item : m_idf_map) {
            __print("[\"%s\"]={", item.first.c_str());
            for (const auto& item : item.second) {
                __print("%d:%g,", item.first, item.second);
            }
            __print("");
            __print("},");
        }
        if (!m_idf_map.empty()) {
            __print("");
        }

        __print(" m_idf_pre_defined_scoped_map={");
        for (const auto& item : m_idf_pre_defined_scoped_map) {
            __print("%s=%g,", item.first.c_str(), item.second);
        }
        __print("}");

        __print(" m_idf_outer_scope_reassigned_map={");
        for (const auto& item : m_idf_outer_scope_reassigned_map) {
            __print("%s=%g,", item.first.c_str(), item.second);
        }
        __print("}");

        if (g_print_enable <16*2) __print(" :dia\n");
    }

    if (g_print_enable & 32) {  // idf (scope_declared)
        __print(" #32");
        __print(" m_idf_scope_declared");
        for (const auto& item : m_idf_scope_declared) {
            __print("[\"%d\"]={", item.first);
            for (const auto& item : item.second) {
                __print("%s:%d,", item.first.c_str(), item.second);
            }
            __print("");
            __print("},");
        }

        if (!m_idf_scope_declared.empty()) {
            __print("");
        }

        if (g_print_enable <32*2) __print(" :dia\n");
    }

    if (g_print_enable & 64) {  // if
        __print(" #64");
        __print(" m_stmt_if_cond");
        for (const auto& item : m_stmt_if_cond) {
            __print("[\"%d\"]=[", item.first);
            for (IfCond x : item.second) {
                __print("(%d,%d), ", x.type,x.value);
            }
            __print("]");
        }

        if (g_print_enable <64*2) __print(" :dia\n");
    }

    if (g_print_enable & 128) { // fn
        __print(" #128");
        __print(" m_function_declare_cur_name = %s", m_function_declare_cur_name.c_str());

        __print(" m_function_declare_map");
        for (const auto& item : m_function_declare_map) {
            __print("[\"%s\"]=", item.first.c_str());
            __print("(%d,%d,(", item.second.exec_begin, item.second.exec_end);
            for (const auto& item : item.second.args) {
                __print("%s,", item.c_str());
            }
            __print(")");
        }

        __print(" m_function_declare_pos2name_name");
        for (const auto& item : m_function_declare_exec_begin_name_map) {
            __print("[%d]=%s ", item.first, item.second.c_str());
        }

        __print(" m_function_declare_cur_argidfs=[");
        for (const auto& item : m_function_declare_cur_argidfs) {
            __print("\"%s\",", item.c_str());
        }
        __print("]");

        if (g_print_enable <128*2) __print(" @\n");
    }

    if (g_print_enable & 256) {
        __print(" #256");
        __print(" m_depth = %d", m_depth);
        __print(" m_stmt_count = %d", m_stmt_count);
        __print(" m_token_idx = %d", m_token_idx);

        __print(" m_id = \"%s\" m_id_spc_occurred = %s m_lp_count = %d m_lcb_count = %d m_state_next_certained = %s",
                m_id.c_str(),
                m_id_spc_occurred?"true":"false",
                m_lp_count,
                m_lcb_count,
                getStateStr(m_state_next_certained)
            );

        if (m_return_value_ptr != NULL) {
            __print(" *m_return_value_ptr = (%d,%d,%s,%g,\"%s\")",
                    m_return_value_ptr->token_idx,
                    m_return_value_ptr->stmt_count,
                    getOpdTypeStr(m_return_value_ptr->type),
                    m_return_value_ptr->num,
                    m_return_value_ptr->str.c_str()
                );
        } else {
            __print(" m_return_value_ptr = NULL");
        }
    }

    __print("\n");
}

void ArithmeticExpression::appendToRpnExpression(char c) {
    // if (c == 'f') {
    //     m_rpn_expression.append( fni2fnn(m_opr_stack.top(-1)) );
    // } else {
    //     m_rpn_expression.push_back(c);
    // }
}

bool ArithmeticExpression::isOnTerminalState() const {
    return (
        ST_BGN == m_state
        || ST_NUM == m_state
        || ST_IDF == m_state
        || ST_RPS == m_state
        || ST_RCB == m_state
        || ST_STT == m_state
        );
}

bool ArithmeticExpression::isCharTypeExpected(CharType type) {
    // TODO: [2026-10-26] performance trick:
    // with 1) int "m_expect_char_type" instead of a vector "m_expect_char_types"
    // and with 2) each CT is equal to 2^N, and 3) make this function inline,
    // checking could be faster:
    // (type & m_expect_char_type) == 1

    std::vector<CharType>& v = m_expect_char_types;
    return std::find(v.begin(), v.end(), type) != v.end();
}

bool ArithmeticExpression::handle(char c)
{
    CharType type = getCharType(c);
    if ( CT_UPPER == type) {
        error("invalid char type, argument char='%c'", c);
        printErrorLocation();
        exit(E_INPUT_ERROR);
    }

    if (!m_expect_char_types.empty()) {
        if (!isCharTypeExpected(type)) {
            errornoln("%s expects [", getContextTypeStr(m_ctxt_stack.top()));
            for (const auto& item : m_expect_char_types) {
                __error("'%s', ", getCharTypeStr(item));
            }
            __error("\b ], while '%s' gotten\n", getCharTypeStr(type));
            printErrorLocation();
            syntax(m_ctxt_stack.top());
            exit(E_GRAMMAR);
        }
    }

    StateTable* table_item = &m_state_table[m_state][type];

    if (table_item->action_type != AT_NON) {
        dia("-:handle");
    }

    bool rc = handleAction(table_item->action_type, c);
    if ( rc ) {
        // update state only when action runs successfully
        m_state_prev = m_state;
        m_state = table_item->new_state;

        // certain-ty uncertained state
        if (m_state == ST_UNC) {
            if (m_state_next_certained != ST_UPPER) {
                m_state = m_state_next_certained;
                m_state_next_certained = ST_UPPER;
            } else {
                dia("handle[uncertained state]");
                error("uncertained state occurs");
                printErrorLocation();
                exit(E_DEV_ERROR);
            }
        }

        // lcb manual-switch
        else if (m_state == ST_LCB) {
            if (m_state_next_certained != ST_UPPER) {
                m_state = m_state_next_certained;
                m_state_next_certained = ST_UPPER;
            }
        }

        // rcb manual-switch
        else if (m_state == ST_RCB) {
            if (m_state_next_certained != ST_UPPER) {
                m_state = m_state_next_certained;
                m_state_next_certained = ST_UPPER;
            }
        }

        // st_stt
        else if (m_state == ST_STT) {
            // more chars left to process
            if (m_parse_pos < m_arithmetic_expression.size()-1) {
                m_state = ST_BGN;
            }
        }

        else {
            // nothing
        }
    }

    if (table_item->new_state != m_state) {
        print("after action_type:%d, state manual-switched %s -> %s", table_item->action_type, getStateStr(table_item->new_state), getStateStr(m_state));
    }

    return rc;
}

bool ArithmeticExpression::handleNone()
{
    return true;
}

void ArithmeticExpression::pushOpr(int v) {
    m_opr_stack.push({m_token_idx, m_stmt_count, v});
}

bool ArithmeticExpression::handleFlag(char flag)
{
    print("handing flags ...");
    if ( '-' == flag) {
        pushOpdNum(-1);
        pushOpr('*');
    }

    ContextType ctxt = m_ctxt_stack.top();
    print("ctxt: %s", getContextTypeStr(ctxt));

    if (ctxt == STMT_DECLARE_ASSIGN) {
        m_ctxt_stack.push(STMT_DECLARE_ASSIGN_VALUE);
        m_expect_char_types = {};
        return true;

    } else if (ctxt == STMT_IF_COND) {
        return true;

    } else if (ctxt == STMT_ELIF_COND) {
        return true;

    } else {
        print("un-specially-processed contextType: %s", getContextTypeStr(ctxt));
        return true;
    }

    return true;
}

double ArithmeticExpression::getIdfValue(const std::string& idf) {
    if (m_idf_map.find(idf) == m_idf_map.end()) {
        error("undefined id: \"%s\"", idf.c_str());
        printErrorLocation();
        exit(E_EVALUATE);
    }
    double v = NAN;
    std::unordered_map<int,double>& dv = m_idf_map[idf];
    int idf_key = m_depth + m_lcb_count;
    for (int i=idf_key; i>=0; --i) {
        if (dv.find(i) != dv.end()) { // found
            v = dv[i];
            print("found, idf:%s, i:%d, value:%g", idf.c_str(), i, v);
            return v;
        }
    }
    if (isnan(v)) {
        error("no value found for idf:%s, m_depth:%d, m_lcb_count:%d, idf_key:%d, check out m_idf_map please", idf.c_str(), m_depth, m_lcb_count, idf_key);
        printErrorLocation();
        exit(E_SHOULD_NOT_HAPPEN);
    }
    exit(E_SHOULD_NOT_HAPPEN);
}

void ArithmeticExpression::assignValueToIdf(const std::string& idf, double value, bool is_scope_declared) {
    if (idf.empty()) {
        error("assign statement lv: idf should not be empty string");
        printErrorLocation();
        exit(E_SYNTAX);
    }

    print("idf=%s, m_depth=%d, m_lcb_count=%d, value=%g, is_scope_declared=%d", idf.c_str(), m_depth, m_lcb_count, value, is_scope_declared);

    if (m_idf_map.find(idf) == m_idf_map.end()) {
        // not found, to init
        std::unordered_map<int,double> dv;
        dv[m_depth + m_lcb_count] = value;
        m_idf_map[idf] = dv;
    } else {
        bool assign_success = false;
        std::unordered_map<int,double>& dv = m_idf_map[idf];
        if (is_scope_declared) {           // for current and inner-scope
            dv[m_depth + m_lcb_count] = value;
            assign_success = true;

        } else {                // overwrite/re-use ancient scope if exists
            int scope = m_depth + m_lcb_count;
            for (int i=scope; i>=0; --i) {
                if (dv.find(i) != dv.end()) { // found
                    dv[i] = value;
                    assign_success = true;
                    if (i < scope) { // is outer scope
                        print("reassign: %s = %g, scope:%d", idf.c_str(), value, scope);
                        m_idf_outer_scope_reassigned_map[idf] = value; // always update
                    }
                    break;
                }
            }
        }

        if (!assign_success) {
            error("*DEV ERROR* no assign slot found to assign idf:%s=value:%g, should never happen", idf.c_str(), value);
            exit(E_DEV_ERROR);
        }
    }

    if (is_scope_declared) {           // for current and inner-scope(s)
        if (m_idf_scope_declared.find(m_depth + m_lcb_count) == m_idf_scope_declared.end()) { // scope has no idf scope-declared
            std::unordered_map<std::string,int> idf1map;
            idf1map[idf] = 1;
            m_idf_scope_declared[m_depth + m_lcb_count] = idf1map;

        } else {            // been scope-declared
            std::unordered_map<std::string,int>& idf1map = m_idf_scope_declared[m_depth + m_lcb_count];
            if (idf1map.find(idf) != idf1map.end()) {
                // for better debugging to separate functions arguments from scope declare
                if (m_idf_pre_defined_scoped_map.find(idf) != m_idf_pre_defined_scoped_map.end()) {
                    bool is_in_function_call = false;
                    int step = 0;
                    while (-1*step < m_ctxt_stack.count()) {
                        ContextType t = m_ctxt_stack.top(step);
                        if (t == FUNCTION_CALL) {
                            is_in_function_call = true;
                            break;
                        }
                        --step;
                    }
                    if (is_in_function_call) {
                        error("\"%s\" is a function argument marker, can not be re-scope-declared, m_depth:%d lcb:%d", idf.c_str(), m_depth, m_lcb_count);
                        printErrorLocation();
                        exit(E_GRAMMAR);
                    }
                }
                error("idf:\"%s\" has already been scope-declared in current m_depth:%d scope:%d", idf.c_str(), m_depth, m_lcb_count);
                printErrorLocation();
                error("in same scope, same-named idf can only be scope-declared once");
                exit(E_GRAMMAR);
            }
            idf1map[idf] = 1;
        }
    }
}

bool ArithmeticExpression::handleOperator(char opr)
{
    dia("-:handleOperator");

    ++m_token_idx;

    char top = m_opr_stack.top().v;
    while ( getPriority(top) >= getPriority(opr) && '(' != top ) {

        // NOTE: to support "right-associative ^", eg: 2^3^2, if both
        // opr and top are '^', do not calculate immediately.
        if (opr == top && opr == '^') break;

        // right-associative for !, do not calculate immediately, case: !!1
        if (opr == top && opr == '!') break;

        if (! calculate(top)) return false;

        // if calcute success, means cur opr consumed,
        // so pop, and go on to next opr.
        m_opr_stack.pop();
        top = m_opr_stack.top().v;
    }

    print("pushing operator:'%c', rpn:\"%s\"", opr, m_rpn_expression.c_str());
    pushOpr(opr);

    // STMT_DECLARE_ASSIGN_VALUE is a special case of CTXT_EXP
    ContextType ctxt = m_ctxt_stack.top();
    print("current ctxt:%s", getContextTypeStr(ctxt));

    if (ctxt == STMT_DECLARE_ASSIGN) {
        m_ctxt_stack.push(STMT_DECLARE_ASSIGN_VALUE);
        return true;

    } else if (ctxt == STMT_DECLARE_ASSIGN_VALUE) {
        return true;

    } else if (ctxt == FUNCTION_RETURN_VALUE) {
        return true;

    } else {
        print("un-specially-processed contextType: %s", getContextTypeStr(ctxt));

        bool is_exp_context = (ctxt == CTXT_EXP || ctxt == STMT_DECLARE_ASSIGN_VALUE || ctxt == STMT_IF_COND);
        if (!is_exp_context) {

            m_ctxt_stack.push(CTXT_EXP);

            // // if current context expect speicial exp context, do not push too
            // bool is_cur_context_expect_special_exp = (ctxt == STMT_IF);
            // if (! is_cur_context_expect_special_exp) {
            //     m_ctxt_stack.push(CTXT_EXP);
            // }

        }

        return true;
    }

    return true;
}

void ArithmeticExpression::printErrorLocation() {
    errornoln("\u200B\n");
    errornoln("`%s`\n", m_arithmetic_expression.c_str());
    errornoln("%*s^\n", m_parse_pos+1, ""); // for head added "'"
    errornoln("%*s%d\n", m_parse_pos+1, "", m_parse_pos);
    errornoln("\u200B\n");
}

void ArithmeticExpression::handleError()
{
    dia("-:handleError");
    error("invalid syntax: pos=\"%d\", c='%c', raw=\"%s\"",
          m_parse_pos,
          m_arithmetic_expression[m_parse_pos],
          m_arithmetic_expression.c_str());

    // TODO: [2026-10-17] support char: '\n'.
    printErrorLocation();

    // better readable error message
    errornoln("%s expects [", getStateStr(m_state));

    StateTable* l = m_state_table[m_state];
    for (int i=0; i<CT_UPPER; i++) {
        if (l[i].action_type != AT_ERR) {
            __error(" '%s',", getCharTypeStr(i));
        }
    }
    __error("\b ], while '%c' gotten\n", m_arithmetic_expression[m_parse_pos]);

    exit(E_SYNTAX);
}

bool ArithmeticExpression::handleNum()
{
    ++m_token_idx;

    double int_val = 0;
    double dot_val = 0;

    // sucks for fetching size each time for operand
    int size = m_arithmetic_expression.size();
    bool dot_occurred = false;
    int dot_part_length = 0;

    // compose a number
    while ( m_parse_pos < size ) {
        char c = m_arithmetic_expression[m_parse_pos];
        if ( ! ('.' == c || (c >= '0' && c <= '9'))) {
            break;
        }
        if ( '.' == c && dot_occurred) {
            error("dot occurs again");
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

    pushOpdNum(num);

    print("int part=\"%g\", dot part=\"%g\", dot occurred=\"%s\"",
          int_val, dot_val, dot_occurred ? "yes" : "no");

    ContextType ctxt = m_ctxt_stack.top();
    print("in context: %s", getContextTypeStr(ctxt));

    if (ctxt == STMT_DECLARE_ASSIGN) {
        m_ctxt_stack.push(STMT_DECLARE_ASSIGN_VALUE);
        m_expect_char_types = {};
        return true;

    } else if (ctxt == STMT_IF_COND) {
        // num is sufficient for if cond, so expect '{'
        m_expect_char_types = {CT_ALP, CT_FLG, CT_LCB, CT_LP, CT_NUM, CT_RP, CT_OP, CT_SEQ, CT_AMP, CT_VBR, CT_CMA, CT_EXM, CT_SPC };
        return true;

    } else if (ctxt == STMT_ELIF_COND) {
        // num is sufficient for if cond, so expect '{'
        m_expect_char_types = {CT_ALP, CT_FLG, CT_LCB, CT_LP, CT_NUM, CT_RP, CT_OP, CT_SEQ, CT_AMP, CT_VBR, CT_CMA, CT_EXM, CT_SPC };
        return true;

    } else if (ctxt == FUNCTION_RETURN_VALUE) {
        // num is sufficient for return cond, so expect ';', exclude '{'
        m_expect_char_types = {CT_ALP, CT_FLG, CT_LP, CT_NUM, CT_RP, CT_OP, CT_SEQ, CT_AMP, CT_VBR, CT_EXM, CT_SPC, CT_SMC, CT_CMA, CT_RCB};
        return true;

    } else {
        print("un-specially-processed contextType: %s", getContextTypeStr(ctxt));
        return true;
    }

    error("should not come here");
    printErrorLocation();
    exit(E_DEV_ERROR);
    return false;
}

bool ArithmeticExpression::handleLeftParenthesis()
{
    ++m_token_idx;
    ++m_lp_count;               // *always*

    if (m_id_spc_occurred) {
        m_id_spc_occurred = false;
    }

    // check context first
    ContextType ctxt = m_ctxt_stack.top();
    print("in context: %s", getContextTypeStr(ctxt));

    if (ctxt == CTXT_BGN ||
        ctxt == CTXT_EXP ||
        ctxt == STMT_DECLARE_ASSIGN_VALUE ||
        ctxt == STMT_IF_COND ||
        ctxt == STMT_ELIF_COND ||
        ctxt == FUNCTION_RETURN_VALUE
        ) {

        if (ctxt == STMT_IF_COND || ctxt == STMT_ELIF_COND) {
            m_expect_char_types = {CT_ALP, CT_FLG, CT_LCB, CT_LP, CT_NUM, CT_RP, CT_OP, CT_SEQ, CT_AMP, CT_VBR, CT_CMA, CT_EXM, CT_SPC };
        } else if (ctxt == FUNCTION_RETURN_VALUE) {
            m_expect_char_types = {CT_ALP, CT_FLG, CT_LP, CT_NUM, CT_RP, CT_OP, CT_SEQ, CT_AMP, CT_VBR, CT_CMA, CT_EXM, CT_SPC, CT_SMC};
        } else {
        }

        // NOTE: We need check wether current '(' is a function call openning parentheses
        // or expr grouping parentheses but now, can not tell "a+(...)" from
        // "+a(...)" coz we have not record the sequence/order info across
        // opd and opr.
        //
        // we need an token index bind to opd and opr, assume str begin
        // index is 4, we get:
        //
        // str                opd stack opr stack
        // s1: "... a+(...)" [<4,a>]    [<5,+>, <6,(>]
        // s2: "... +a(...)" [<5,a>]    [<4,+>, <6,(>]
        //
        // now, opr is <6,(>, we can check where opd_stack.top().index == opr.index-1 or not,
        // if yes, opr is a function call opening parentheses, else, is exp grouping parentheses

        // to check role of current '(', grouping or function call opening parentheses, we have to rely on top opd.
        Opd opd = m_opd_stack.top();

        if (_isbase(opd)) {
            pushOpr('(');
            return true;

        } else if (_isidf(opd)) { // 'idf?('

            bool is_fnc_olp = (opd.token_idx == m_token_idx-1); // prev-token
            print("is_fnc_olp = %d", is_fnc_olp);

            if (is_fnc_olp) { // 'idf(' => treat as 'fnn('
                std::string idf = opd.str;

                if (m_function_declare_map.find(idf) != m_function_declare_map.end()) {
                    int exec_begin = m_function_declare_map[idf].exec_begin;
                    pushOpr(exec_begin); // uniq
                    pushOpr('F'); // self-defined function

                    // TODO, use m_function_declare_call_args_count_stack directly, comma can't tell 0 argument and 1 argument
                    m_fncmcnt_stack.push(0);

                    m_opd_stack.pop(); // pop idf opd after been registered in to opr stack

                    dia("F");
                    return true;

                } else {// built-in function
                    int fni = fnn2fni(idf);
                    if (fni == FNN_UPPER) {
                        error("fni should never be equal to FNN_UPPER, fni=%d, idf=%s", fni, idf.c_str());
                        return false;
                    }

                    pushOpr(fni);
                    pushOpr('f'); // built-in function, means function left parenthesis

                    // NOTE: always record fncmcnt's comma count when fn gotten
                    m_fncmcnt_stack.push(0);

                    m_opd_stack.pop(); // pop idf opd after been registered in to opr stack

                    return true;
                }
            } else {                // 'idf other_token_between (', just treat '(' as normal opr
                pushOpr('(');
                return true;
            }

        } else if (_isnum(opd)) {
            // treat '(' as grouping, has nothing to with '_isn' // or not,
            // '_isn' here is fit "else if" control flow
            pushOpr('(');
            return true;

        }  else if (_isstr(opd)) {
            print("TODO");
            exit(0);

        } else {
            print("TODO");
            exit(0);
        }

    } else if (ctxt == STMT_DECLARE_ASSIGN) {
        pushOpr('(');
        m_ctxt_stack.push(STMT_DECLARE_ASSIGN_VALUE);
        m_expect_char_types = {};
        return true;

    } else if (ctxt == STMT_IF) { // 'if('
        m_ctxt_stack.push(STMT_IF_COND); // STMT_IF_COND is a special case of CTXT_EXP
        m_expect_char_types = {CT_ALP, CT_FLG, CT_LP, CT_NUM, CT_EXM, CT_SPC}; // 'if(COND'
        m_state_next_certained = ST_BGN; // can get any chars to compose exp
        return true;

    } else if (ctxt == STMT_ELIF) { // 'elif('
        checkFollowingIforElif();

        m_ctxt_stack.push(STMT_ELIF_COND); // STMT_ELIF_COND is a special case of CTXT_EXP
        m_expect_char_types = {CT_ALP, CT_FLG, CT_LP, CT_NUM, CT_EXM, CT_SPC}; // 'elif(COND'
        m_state_next_certained = ST_BGN; // can get any chars to compose exp
        return true;

    } else if (ctxt == FUNCTION_DEFINITION) {
        if (m_function_declare_cur_name == "") {
            error("no function name found after keyword \"function\"");
            printErrorLocation();
            exit(E_GRAMMAR);
        }
        m_ctxt_stack.push(FUNCTION_ARGS);
        m_expect_char_types = { CT_ALP, CT_RP, CT_SPC };
        return true;

    } else if (ctxt == FUNCTION_RETURN) { // 'return('
        m_ctxt_stack.push(FUNCTION_RETURN_VALUE); // FUNCTION_RETURN_VALUE is a special case of CTXT_EXP
        m_expect_char_types = {CT_ALP, CT_FLG, CT_LP, CT_NUM, CT_EXM, CT_SPC}; // 'return(...'
        m_state_next_certained = ST_BGN; // can get any chars to compose exp
        return true;

    } else {
        print("un-specially-processed contextType: %s", getContextTypeStr(ctxt));
        return true;
    }

    error("should not come here");
    printErrorLocation();
    exit(E_DEV_ERROR);
    return false;
}

bool ArithmeticExpression::handleRightParenthesis()
{
    ++m_token_idx;
    --m_lp_count;               // always

    if ( m_lp_count < 0 ) {
        error("exists extra right parenthesis");
        return false;
    }

    if (m_ctxt_stack.top() == FUNCTION_ARGS) {
        // // TODO: use opd
        // if (m_id != "") {
        //     m_function_declare_cur_argidfs.push_back(m_id);
        // }
        // m_id = "";
        m_expect_char_types = { CT_LCB, CT_SPC };
        m_state_next_certained = ST_RPS;
        return true;

    } else {
        m_state_next_certained = ST_BGN;
    }

    if (m_state == ST_IDF) {
        if (m_id_spc_occurred) m_id_spc_occurred = false;

        // already replace by opd stack
        // if (!m_id.empty()) {
        //     if (!pushOpdIdfValue(m_id)) return false;
        //     m_id = ""; // reset after consumed
        // }
    }

    // pop till '(' or 'f'
    int top;
    while ( ! m_opr_stack.empty() && (top = m_opr_stack.top().v) != LOWEST_PRIO_OP) {
        dia("-:handleRightParenthesis loop");
        if (top == '(') {
            // done for whole, (...), so pop '('
            Opr x = m_opr_stack.pop();
            print("pop <%d '%c'> in while loop", x.v, x.v);
            break;
        }

        if (top == 'f' || top == 'F') {
            // NOTE: always calculate whole function when 'f' met.
            if (! calculate(top)) return false;
            // NOTE: 'f' already pop-ed in calculate(...), and due
            // to whole function: [fni, 'f', ..., ')'] have been processed,
            // so 1) pop 'f', fni, 2) break the loop immediately
            // that's meet the expection of handleRightParenthesis, coz
            // the right parenthesis handled is the closing parenthesis of
            // function if opr is 'f'.
            m_opr_stack.pop(); // 'f', 'F'
            m_opr_stack.pop(); // fni, exec_begin
            break;
        } else {
            if (! calculate(top)) return false;
            // NOTE: if calculate success, means the top opr been consumed,
            // so pop it. and go on to next opr
            Opr x = m_opr_stack.pop();
            print("pop opr:<%d '%c'> after calculate", x.v, x.v);
        }

        dia("handleRightParenthesis:+ loop");
    }

    // DO NOT save this operator:')' into stack
    return true;
}

std::string ArithmeticExpression::getNextToken() {
    int i = m_parse_pos;
    int size = m_arithmetic_expression.size();
    while (++i<size) { if (m_arithmetic_expression[i] != ' ') break; } // skip space

    if (i==size) {
        return "";
    }

    char c = m_arithmetic_expression[i];
    switch(getCharType(c)) {
    case CT_ALP: {
        std::string token;
        while (i<size && getCharType(c) == CT_ALP) {
            token.push_back(c);
            c = m_arithmetic_expression[++i];
        }
        return token;
    }
    default:
        print("TODO to support char type:%s", getCharTypeStr(c));
        return "";
    }
}

char ArithmeticExpression::nextChar(bool skipWhitespace) {
    int i = m_parse_pos;
    while(++i < m_arithmetic_expression.size()) {
        if (!skipWhitespace) return m_arithmetic_expression[i];
        if (m_arithmetic_expression[i] != ' ')
            return m_arithmetic_expression[i];
    }
    return '\0';
}

bool ArithmeticExpression::handleAlpha(char _c)
{
    ++m_token_idx;

    // TODO: [2026-10-10] parse like handleNum to ignore m_id_spc_occurred.

    // process m_id_spc_occurred firstly
    do {
        if (m_id_spc_occurred) {
            // avoid `idf idf`, but allow following cases:
            ContextType ctxt = m_ctxt_stack.top();
            if (ctxt == FUNCTION_DEFINITION) { // `function fnn`
                break;
            } else if (ctxt == STMT_IF || ctxt == STMT_IF_COND) { // `if var`
                break;
            } else if (ctxt == STMT_ELIF || ctxt == STMT_ELIF_COND) { // `elif var`
                break;
            } else if (ctxt == FUNCTION_RETURN || ctxt == FUNCTION_RETURN_VALUE) { // `return var`
                break;
            } else {
            }

            Opd opd = m_opd_stack.top();
            if (opd.type == OPD_IDF && opd.token_idx == m_token_idx-1) {
                error("space not allowed between alphas, current ctxt:%s, m_id:%s, m_id_spc_occurred: %d", getContextTypeStr(ctxt), m_id.c_str(), m_id_spc_occurred);
                printErrorLocation();
                exit(E_SYNTAX);
            }
        }
    } while (0);

    if (m_id_spc_occurred)
        m_id_spc_occurred = false;

    // TODO: use idf instead of m_id
    m_id = "";

    ////////////////////////////////////////////////////////////////

    // get token: "idf"
    // TODO: maybe we will remove m_id, coz idf now saved in Opd, can access anyway.

    char c;
    int size = m_arithmetic_expression.size();
    while ( m_parse_pos < size ) {
        c = m_arithmetic_expression[m_parse_pos];
        // TODO: to support '_'
        if (!(getCharType(c) == CT_ALP || (c >= '0' && c <= '9'))) {
            break;
        }
        m_id.push_back(c);
        print("m_id:%s c=%c", m_id.c_str(), c);
        ++m_parse_pos;
    }

    // make m_parse_pos points to last char of idf, so ++m_parse_pos in
    // parse function will get right char right on to current idf
    if (m_id != "") {
        --m_parse_pos;
    }

    // PATTERN: context + expects (token aka. chars) -> context
    // context could be stack
    // example:
    //
    // if a {           // push : [if cond]
    //     if b {       // push : [if cond, if block]
    //     }            // pop  : [if cond]
    // }                // pop  : []

    dia("-:handleAlpha:.h1");

    ContextType ctxt = m_ctxt_stack.top();
    print("ctxt: %s", getContextTypeStr(ctxt));

    if (ctxt == CTXT_BGN) {
        if (m_id == "if") {     // keyword
            // ensure "if" is start of current statement
            Opd top_opd = m_opd_stack.top();
            Opr top_opr = m_opr_stack.top();

            if ((top_opd.stmt_count == m_stmt_count && top_opd.type != OPD_STACK_BOTTOM_BASE) ||
                (top_opr.stmt_count == m_stmt_count && top_opr.v != LOWEST_PRIO_OP)) {
                error("`if` supposed to be at very begin of one statement");
                printErrorLocation();
                exit(E_GRAMMAR);
            }

            m_ctxt_stack.push(STMT_IF);
            m_expect_char_types = {CT_LP, CT_SPC}; // 'if(' or 'if '
            m_state_next_certained = ST_BGN; // can get any chars to compose exp
            return true;

        } else if (m_id == "elif") { // keyword
            // ensure "elif" is start of current statement
            Opd top_opd = m_opd_stack.top();
            Opr top_opr = m_opr_stack.top();

            if ((top_opd.stmt_count == m_stmt_count && top_opd.type != OPD_STACK_BOTTOM_BASE) ||
                (top_opr.stmt_count == m_stmt_count && top_opr.v != LOWEST_PRIO_OP)) {
                error("`else` supposed to be at very begin of one statement");
                printErrorLocation();
                syntax(STMT_ELSE);
                exit(E_GRAMMAR);
            }

            m_ctxt_stack.push(STMT_ELIF);
            m_expect_char_types = {CT_LP, CT_SPC}; // 'elif(' or 'elif '
            m_state_next_certained = ST_BGN; // can get any chars to compose exp
            return true;

        } else if (m_id == "else") { // keyword
            // NOTE: following-"if"-or-"elif"-checking is done in handleRightParenthesis
            Opd top_opd = m_opd_stack.top();
            Opr top_opr = m_opr_stack.top();

            if ((top_opd.stmt_count == m_stmt_count && top_opd.type != OPD_STACK_BOTTOM_BASE) ||
                (top_opr.stmt_count == m_stmt_count && top_opr.v != LOWEST_PRIO_OP)) {
                error("`else` supposed to be at very begin of one statement");
                printErrorLocation();
                syntax(STMT_ELSE);
                exit(E_GRAMMAR);
            }

            m_ctxt_stack.push(STMT_ELSE);
            m_expect_char_types = {CT_LCB, CT_SPC};
            return true;

        } else if (m_id == "function") { // keyword
            Opd top_opd = m_opd_stack.top();
            Opr top_opr = m_opr_stack.top();

            if ((top_opd.stmt_count == m_stmt_count && top_opd.type != OPD_STACK_BOTTOM_BASE) ||
                (top_opr.stmt_count == m_stmt_count && top_opr.v != LOWEST_PRIO_OP)) {
                error("`function` supposed to be at very begin of one statement");
                printErrorLocation();
                syntax(STMT_ELSE);
                exit(E_GRAMMAR);
            }

            m_ctxt_stack.push(FUNCTION_DEFINITION);
            // because we alreay parse whole following chars to compose token,
            // so now next char can be anything except alpha. parser will process
            // 2 cases:
            // 1) ' ' => handleSpace,
            // 2) other char => handler
            // so for next handler, they can expects CT_ALP and go on process.
            // TODO: support _fnn002 '_' and '0-9'
            m_expect_char_types = {CT_ALP, CT_SPC};
            m_state_next_certained = ST_BGN;
            return true;

        } else if (m_id == "return") { // keyword
            // ensure is start of current statement
            Opd top_opd = m_opd_stack.top();
            Opr top_opr = m_opr_stack.top();

            if ((top_opd.stmt_count == m_stmt_count && top_opd.type != OPD_STACK_BOTTOM_BASE) ||
                (top_opr.stmt_count == m_stmt_count && top_opr.v != LOWEST_PRIO_OP)) {
                error("`return` supposed to be at very begin of one statement");
                printErrorLocation();
                syntax(STMT_ELSE);
                exit(E_GRAMMAR);
            }

            m_return_stmt_exists = true;
            m_ctxt_stack.push(FUNCTION_RETURN);
            // TODO: how about `return}`, maybe against st_state transition
            m_expect_char_types = {CT_LP, CT_SPC, CT_SMC, CT_RCB}; // 'return(' or 'return ' or 'return;' or '{return}'
            m_state_next_certained = ST_BGN; // can get any chars to compose exp
            return true;

        } else { // fnn or var or assign-left-var, undetermined, just push idf for later use
            pushOpdIdf(m_id);
            return true;

        }

    } else if (ctxt == CTXT_EXP) { // fnn or var
        // TODO: firstly, to check whether m_id is keywords or what
        if (m_id == "function") {
            error("keyword `function` only be at very begin of statement");
            printErrorLocation();
            syntax(FUNCTION_DEFINITION);
            exit(E_GRAMMAR);
        }

        if (nextChar() == '(') {
            pushOpdIdf(m_id);
        } else {
            pushOpdIdfValue(m_id);
        }
        return true;

    } else if (ctxt == STMT_IF_COND) { // STMT_IF_COND is a special case of CTXT_EXP

        // TODO: due to now we are in exp context, use pushOpdIdfValue(m_id) instead of
        // pushOpdIdf(m_id) directly for faster error-checking and performance
        // pushOpdIdfValue(m_id);
        //
        // could not process case y=x(3);
        // `x` can not be treat as var directly, coz `x` is a function since it follows '('
        // so we need a function to look ahead to determine the next token is '(' or not.
        // if is '(', then treat as fnn (and we can *push into opr stack* immediately),
        // else treat x as var (opd) and use pushOpdIdfValue.

        // TODO: later, if next non-spc char is '(', treat as opr directly and in '('-handler,
        // check top opr's token_idx to determine '(' is fnc_olp or grouping

        // TODO: m_id here can not be following keywords:
        // if elif else function

        print("'%c'", nextChar());
        if (nextChar() == '(') {
            pushOpdIdf(m_id);
        } else {
            pushOpdIdfValue(m_id);
        }

        // alpha is sufficient for if cond, so expect '{'
        m_expect_char_types = {CT_ALP, CT_FLG, CT_LCB, CT_LP, CT_NUM, CT_RP, CT_OP, CT_SEQ, CT_AMP, CT_VBR, CT_CMA, CT_EXM, CT_SPC };
        return true;

    } else if (ctxt == STMT_ELIF_COND) { // STMT_ELIF_COND is a special case of CTXT_EXP
        print("'%c'", nextChar());
        if (nextChar() == '(') {
            pushOpdIdf(m_id);
        } else {
            pushOpdIdfValue(m_id);
        }

        // alpha is sufficient for if cond, so expect '{'
        m_expect_char_types = {CT_ALP, CT_FLG, CT_LCB, CT_LP, CT_NUM, CT_RP, CT_OP, CT_SEQ, CT_AMP, CT_VBR, CT_EXM, CT_CMA, CT_SPC };
        return true;

    } else if (ctxt == FUNCTION_DEFINITION) { // function FNN
        m_function_declare_cur_name = m_id;
        m_expect_char_types = { CT_LP, CT_SPC };
        return true;

    } else if (ctxt == FUNCTION_ARGS) {                // function name(ARG or function name(arg, ARG
        m_expect_char_types = { CT_CMA, CT_RP, CT_SPC };
        // TODO: replace m_id with current idf
        m_function_declare_cur_argidfs.push_back(m_id);
        return true;

    } else if (ctxt == STMT_DECLARE_ASSIGN) {
        if (nextChar() == '(') {
            pushOpdIdf(m_id);
        } else {
            pushOpdIdfValue(m_id);
        }
        m_ctxt_stack.push(STMT_DECLARE_ASSIGN_VALUE);
        m_expect_char_types = {};
        return true;

    } else if (ctxt == STMT_DECLARE_ASSIGN_VALUE) {
        if (nextChar() == '(') {
            pushOpdIdf(m_id);
        } else {
            pushOpdIdfValue(m_id);
        }
        return true;

    } else if (ctxt == FUNCTION_RETURN_VALUE) { // exclude '{'
        if (nextChar() == '(') {
            pushOpdIdf(m_id);
        } else {
            pushOpdIdfValue(m_id);
        }
        m_expect_char_types = {CT_ALP, CT_FLG, CT_LP, CT_NUM, CT_RP, CT_OP, CT_SEQ, CT_AMP, CT_VBR, CT_EXM, CT_SPC, CT_SMC, CT_CMA, CT_RCB};
        return true;

    } else {
        print("un-specially-processed contextType: %s", getContextTypeStr(ctxt));
        return true;
        // PATTERN: context + expects

        // if (m_id == "if") {
        //     m_ctxt_stack.push(STMT_IF_COND);
        //     m_expect_char_types = { CT_SPC, CT_LP, CT_FLG };
        // }
    }

    error("should not come here");
    printErrorLocation();
    exit(E_DEV_ERROR);
    return false;
}

bool ArithmeticExpression::handleSpace() {
    // make next state certained on pre-state as ST_IDF
    if (m_state == ST_IDF) {
        Opd top = m_opd_stack.top();
        if (_isidf(top) && (top.str == "if" || top.str == "elif" || top.str == "else" || top.str == "function") ) {
            m_state_next_certained = ST_BGN;
        } else {
            m_id_spc_occurred = true; // TODO: bind to idf
            m_state_next_certained = ST_IDF;
        }
    }

    ContextType ctxt = m_ctxt_stack.top();
    print("ctxt: %s", getContextTypeStr(ctxt));

    if (ctxt == STMT_DECLARE_ASSIGN) {
        m_ctxt_stack.push(STMT_DECLARE_ASSIGN_VALUE);
        m_expect_char_types = {};
        goto handleSpace_common;

    } else if (ctxt == STMT_IF) {
        m_ctxt_stack.push(STMT_IF_COND); // STMT_IF_COND is a special case of CTXT_EXP
        m_expect_char_types = {CT_ALP, CT_FLG, CT_LP, CT_NUM, CT_EXM, CT_SPC}; // 'if COND'
        m_state_next_certained = ST_BGN; // can get any chars to compose exp
        goto handleSpace_common;

    } else if (ctxt == STMT_ELIF) {
        checkFollowingIforElif();

        m_ctxt_stack.push(STMT_ELIF_COND); // STMT_ELIF_COND is a special case of CTXT_EXP
        m_expect_char_types = {CT_ALP, CT_FLG, CT_LP, CT_NUM, CT_EXM, CT_SPC}; // 'elif COND'
        m_state_next_certained = ST_BGN; // can get any chars to compose exp
        goto handleSpace_common;

    } else if (ctxt == STMT_IF_COND) {
        // always treat '{' as candiate. worst case is 'if {' which will be catched and treated as error
        // by st transition.
        // always treat ')' as candiate, to closing openning '('. worst case is 'if whatever_no_lp )'
        // which will be guranteed by st transition.
        m_expect_char_types = {CT_ALP, CT_FLG, CT_LCB, CT_LP, CT_NUM, CT_RP, CT_OP, CT_SEQ, CT_AMP, CT_VBR, CT_CMA, CT_EXM, CT_SPC };
        goto handleSpace_common;

    } else if (ctxt == STMT_ELIF_COND) {
        // always treat '{' as candiate. worst case is 'if {' which will be catched and treated as error
        // by st transition.
        // always treat ')' as candiate, to closing openning '('. worst case is 'if whatever_no_lp )'
        // which will be guranteed by st transition.
        m_expect_char_types = {CT_ALP, CT_FLG, CT_LCB, CT_LP, CT_NUM, CT_RP, CT_OP, CT_SEQ, CT_AMP, CT_VBR, CT_CMA, CT_EXM, CT_SPC };
        goto handleSpace_common;

    } else if (ctxt == FUNCTION_RETURN) {
        m_ctxt_stack.push(FUNCTION_RETURN_VALUE); // FUNCTION_RETURN_VALUE is a special case of CTXT_EXP
        // 'return exp' or 'return ;', exclude '{'
        m_expect_char_types = {CT_ALP, CT_FLG, CT_LP, CT_NUM, CT_RP, CT_OP, CT_SEQ, CT_AMP, CT_VBR, CT_EXM, CT_SPC, CT_SMC, CT_CMA, CT_RCB};
        m_state_next_certained = ST_BGN; // can get any chars to compose exp
        goto handleSpace_common;

    } else if (ctxt == FUNCTION_RETURN_VALUE) {
        goto handleSpace_common;

    } else {
        print("un-specially-processed contextType: %s", getContextTypeStr(ctxt));
        goto handleSpace_common;
    }

handleSpace_common: {
        int size = m_arithmetic_expression.size();
        while (m_parse_pos<size) {
            if (m_arithmetic_expression[++m_parse_pos] == ' ');
            else break;
        }
        if (m_parse_pos != size) {
            --m_parse_pos;      // leave non-space char untouched
        }
        return true;
    }

    error("should not come here");
    printErrorLocation();
    exit(E_DEV_ERROR);
    return false;
}

bool ArithmeticExpression::handleComma() {
    ++m_token_idx;

    if (m_id_spc_occurred) m_id_spc_occurred = false;

    ContextType ctxt = m_ctxt_stack.top();

    if (ctxt == FUNCTION_ARGS) {
        m_expect_char_types = { CT_ALP, CT_SPC };
        return true;
    }

    int top;
    while (!m_opr_stack.empty() && (top = m_opr_stack.top().v) != LOWEST_PRIO_OP) {
        if (top == 'f' || top == 'F') {
            break;
        }
        dia("-:handleComma");
        if(!calculate(top)) return false;
        dia("handleComma:+");
        m_opr_stack.pop();
    }
    if (m_opr_stack.empty()) {
        error("invalid use of comma, comma can only be used as function argument separator");
        handleError();
        exit(E_SYNTAX);
    }

    if (m_fncmcnt_stack.empty()) {
        error("comma can only be used as function argumet separator");
        return false;
    }
    m_fncmcnt_stack.push(m_fncmcnt_stack.pop() + 1);

    return true;
}

bool ArithmeticExpression::handleSemicolon() {
    ++m_stmt_count;

    print("TODO: should consume everything.");
    dia("-:handleSemicolon");

    if (m_lp_count > 0) {
        error("';' not allowed inside ()");
        handleError();
        exit(E_SYNTAX);
    }

    dia("-:handleSemicolon+1");

    ContextType ctxt = m_ctxt_stack.top();

    // we can check very firstly, for cases that should not happend
    switch(ctxt) {
    case STMT_DECLARE_ASSIGN:
        error("invalid assign statement. syntax: \"idf = ...;\"");
        printErrorLocation();
        exit(E_GRAMMAR);

    case STMT_IF_COND:
        error("invalid syntax: \"if exp ;\", should never come here, ';' should not be in m_expect_char_types under context: STMT_IF_COND");
        printErrorLocation();
        exit(E_SYNTAX);

    default:
        print("ignore first-checking context type: %s", getContextTypeStr(ctxt));
        break;
    }

    // prepare opd

    int top;
    while (!m_opr_stack.empty() && (top = m_opr_stack.top().v) != LOWEST_PRIO_OP) {
        if (!calculate(top)) return false;
        m_opr_stack.pop();
        if (m_ctxt_stack.top() == CTXT_EXP) {
            m_ctxt_stack.pop();
        }
    }

    // clean extra CTXT_EXP if exits
    while (!m_ctxt_stack.empty()) {
        ContextType& _t = m_ctxt_stack.top();
        if (_t == CTXT_EXP) {
            m_ctxt_stack.pop();
        } else {
            break;
        }
    }

    assert(m_ctxt_stack.top() != CTXT_EXP);

    dia("handleSemicolon: after opr ops");

    ctxt = m_ctxt_stack.top();

    if (ctxt == STMT_DECLARE_ASSIGN) {
        error("STMT_DECLARE_ASSIGN can not be terminal context type for a statement (marked';')");
        printErrorLocation();
        exit(E_GRAMMAR);

    } else if (ctxt == STMT_DECLARE_ASSIGN_VALUE) {
        // at least two opd need
        if (m_opd_stack.count() < 3) {
            error("two opds need to form a assign statement, while \"%d\" gotten", m_opd_stack.count());
            printErrorLocation();
            exit(E_GRAMMAR);
        } else {
            m_expect_char_types = {};
        }

        Opd opdv = m_opd_stack.pop();
        double assign_v = _isidf(opdv)? getIdfValue(opdv.str): opdv.num;
        std::string assign_idf = _2idf(m_opd_stack.pop());
        assignValueToIdf(assign_idf, assign_v, false);

    } else if (ctxt == STMT_SCOPE_DECLARE_ASSIGN) {
        if (m_opd_stack.count() < 3) { // base + 2
            error("two opds need to form a assign statement, while \"%d\" gotten", m_opd_stack.count());
            printErrorLocation();
            exit(E_GRAMMAR);
        } else {
            m_expect_char_types = {};
        }

        Opd opdv = m_opd_stack.pop();
        double assign_v = _isidf(opdv)? getIdfValue(opdv.str): opdv.num;
        std::string assign_idf = _2idf(m_opd_stack.pop());
        assignValueToIdf(assign_idf, assign_v, true);

    } else if (ctxt == FUNCTION_RETURN) { // 'return;' or 'return ;'
        m_parse_pos = m_arithmetic_expression.size();
        m_expect_char_types = {};
        m_return_value_ptr = NULL;
        return true;

    } else if (ctxt == FUNCTION_RETURN_VALUE) { // 'return xxx;'
        m_parse_pos = m_arithmetic_expression.size();
        m_expect_char_types = {};
        // WE have no clue function need return a value or not, so
        // no error here, checking should be done at caller side
        if (m_opd_stack.count() >= 2) {
            m_return_value_ptr = &(m_opd_stack.top());
            // FIXME: assert failed for recursively call (eg. fib.al)
            // assert(m_return_value_ptr->stmt_count == m_stmt_count-1);
        }
        return true;

    } else {
        print("un-specially-processed contextType: %s", getContextTypeStr(ctxt));
    }

    print("clean context stack ...");
    while(!m_ctxt_stack.empty() && (ctxt = m_ctxt_stack.pop()) != CTXT_BGN) {
        print("m_ctxt_stack.pop %s", getContextTypeStr(ctxt));
    }

    // reset everything
    m_id = "";
    m_id_spc_occurred = false;
    m_lp_count = 0;
    m_expect_char_types = {};

    pushContextBGN();

    m_stmt_assign_key = "";

    while (!m_opd_stack.empty() && m_opd_stack.top().stmt_count == m_stmt_count) {
        m_opd_stack.pop();
    }

    print("m_opd_stack.count() = %d", m_opd_stack.count());
    dia("-:handleSemicolon:1");

    dia("handleSemicolon:+");

    return true;
}

void ArithmeticExpression::pushContextBGN() {
    print("pushing ...");
    bool top_already_ctxt_bgn = !m_ctxt_stack.empty() && m_ctxt_stack.top() == CTXT_BGN;
    if (!top_already_ctxt_bgn) {
        m_ctxt_stack.push(CTXT_BGN);
    }
}

bool ArithmeticExpression::handleSymbolEqual() {
    dia("-:handleSymbolEqual");

    ++m_token_idx;

    // :=
    if (m_state == ST_CLN) {
        m_ctxt_stack.push(STMT_SCOPE_DECLARE_ASSIGN);
        m_stmt_assign_key = m_id;
        m_id = "";
        if (m_id_spc_occurred) m_id_spc_occurred = false;
        m_state_next_certained = ST_BGN;
        return true;
    }

    // others
    // = or ==
    bool hasAtLeastTwoSymbolEqual = (m_parse_pos+1) < m_arithmetic_expression.size() && m_arithmetic_expression[m_parse_pos+1] == '=';

    if (hasAtLeastTwoSymbolEqual) {
        // treat it as '==' operator
        ++m_parse_pos;
        m_state_next_certained = ST_OPR;
        return handleOperator('e');
    } else {
        ContextType ctxt = m_ctxt_stack.top();

        if (ctxt == CTXT_BGN || ctxt == STMT_IF_BLOCK || ctxt == STMT_ELIF_BLOCK) {
            if (m_state == ST_IDF && (m_state_prev == ST_BGN || m_state_prev == ST_IDF)) { // more restrict grammer
                // treat it as assign statement
                m_ctxt_stack.push(STMT_DECLARE_ASSIGN);

                // TODO: [2026-10-27] remove m_stmt_assign_key.
                m_stmt_assign_key = m_id;
                m_id = "";

                if (m_id_spc_occurred) m_id_spc_occurred = false;
                m_state_next_certained = ST_BGN;
                m_expect_char_types = {CT_ALP, CT_FLG, CT_LP, CT_NUM, CT_SPC};
                return true;
            }
        } else {
            dia("handleSymbolEqual");
            error("single '=' can only be used as assign statement: 'id = ...;', should not be part of contextType:%s", getContextTypeStr(ctxt));
            printErrorLocation();
            exit(E_GRAMMAR);
        }

        dia("handleSymbolEqual");
        error("single '=' can only be used as assign statement: 'id = ...;'");
        printErrorLocation();
        exit(E_GRAMMAR);
    }

    return true;
}

bool ArithmeticExpression::handleSymbolAmpersand() {
    dia("-:handleSymbolAmpersand");

    ++m_token_idx;

    // &&
    bool hasAtLeastTwoSymbolAmpersand = (m_parse_pos+1) < m_arithmetic_expression.size() && m_arithmetic_expression[m_parse_pos+1] == '&';

    if (hasAtLeastTwoSymbolAmpersand) {
        // treat it as '&&' operator
        ++m_parse_pos;
        m_state_next_certained = ST_OPR;
        return handleOperator('a'); // and
    } else {
        error("single '&' not supported yet");
        dia("handleSymbolAmpersand:+");
        return false;
    }
    dia("handleSymbolAmpersand:+");
    return true;
}

bool ArithmeticExpression::handleSymbolVbar() {
    dia("-:handleSymbolVbar");

    ++m_token_idx;

    // ||
    bool hasAtLeastTwoSymbolVbar = (m_parse_pos+1) < m_arithmetic_expression.size() && m_arithmetic_expression[m_parse_pos+1] == '|';

    if (hasAtLeastTwoSymbolVbar) {
        // treat it as '||' operator
        ++m_parse_pos;
        m_state_next_certained = ST_OPR;
        return handleOperator('o'); // or
    } else {
        error("single '|' not supported yet");
        dia("handleSymbolVbar:+");
        return false;
    }
    dia("handleSymbolVbar:+");
    return true;
}

bool ArithmeticExpression::handleExclamation() {
    ++m_token_idx;

    // TODO: handleExclamation is useless, use handleOperator directly
    return handleOperator('!');
}

void ArithmeticExpression::checkFollowingIforElif() {
    if (m_stmt_if_cond.find(m_lcb_count) == m_stmt_if_cond.end()) {
        m_stmt_if_cond[m_lcb_count] = {};
    }
    std::vector<IfCond>& if_elif_conds = m_stmt_if_cond[m_lcb_count];
    if (if_elif_conds.size() == 0) {
        error("`elif` statement must follow `if` statement");
        printErrorLocation();
        exit(E_GRAMMAR);
    }
}

bool ArithmeticExpression::handleLeftCurlyBrace() {
    dia("-:handleLeftCurlyBrace");

    ++m_token_idx;
    ++m_stmt_count;

    if (m_lp_count > 0) {
        error("'{' not allowed inside ()");
        printErrorLocation();
        exit(E_SYNTAX);
    }

    if (m_id_spc_occurred) {
        m_id_spc_occurred = false;
    }

    ContextType ctxt = m_ctxt_stack.top();
    print("ctxt=%d", ctxt);

    if (ctxt == STMT_ELSE) {
        // NOTE: no exp before this context pos needed to evaluate, so process immediately

        const int if_cond_key = m_lcb_count;
        if (m_stmt_if_cond.find(if_cond_key) == m_stmt_if_cond.end()) {
            error("else: no if/elif found before this else");
            printErrorLocation();
            syntax(ctxt);
            exit(E_GRAMMAR);
        }

        std::vector<IfCond>& if_elif_conds = m_stmt_if_cond[if_cond_key];
        if (if_elif_conds.empty()) {
            error("else: if/elif found, but cond value list is empty, should not happen");
            printErrorLocation();
            syntax(ctxt);
            exit(E_SHOULD_NOT_HAPPEN);
        }

        // calculate
        bool pre_cond_all_zero = true;
        for (int i = 0; i < if_elif_conds.size(); i++) {
            if (if_elif_conds[i].value != 0) {
                pre_cond_all_zero = false;
                break;
            }
        }
        if_elif_conds.push_back({IF_COND_TYPE_ELSE, pre_cond_all_zero?1:0});

        if (pre_cond_all_zero == true) {
            m_ctxt_stack.push(STMT_ELSE_BLOCK);
            m_ctxt_stack.push(CTXT_BGN);
            m_expect_char_types = {};

        } else {
            m_ctxt_stack.push(STMT_ELSE_BLOCK);
            m_ctxt_stack.push(CTXT_BGN);
            m_expect_char_types = {};

            print("TODO: to ignore else-block, just skip to balanced '}'");
            if (!findBalancedRightCurlyBraceAndSkipToIt()) {
                error("else: no balanced } found to close else block");
                printErrorLocation();
                syntax(ctxt);
                exit(E_SYNTAX);
            }
        }

        m_state_next_certained = ST_BGN;
        ++m_lcb_count;
        return true;

    } else if (ctxt == FUNCTION_CALL) {
        // NOTE: no exp before this context pos needed to evaluate, so process immediately
        dia("FUNCTION_CALL");
        print("now, we are in block of called function, merge m_idf_pre_defined_scoped_map into m_idf_map");

        // NOTE: m_idf_pre_defined_scoped_map will be treated as local variables, mainly for
        // passing function arguments which need treated as local vars
        //
        // `function f(a,b) {}`
        //     with first arguemt value as 1, second argument value as 2
        // `f(1,2) { /* code */ }`
        //     are intepreted as:
        // `f() {
        //     a:=1; b:=2;
        //     /* code */
        // }`
        //
        // for now, we do not refer function argument, so barely,
        // it's just a marker of arguments index.

        // count beginning '{', and now, with m_dpeth and m_lcb_count properly set, we are in right
        // scope so assignValueToIdf works as expected.
        ++m_lcb_count;

        for (const auto& item : m_idf_pre_defined_scoped_map) {
            assignValueToIdf(item.first, item.second, true); // :=
        }
        dia("after idf pre defined scoped map merge");

        pushContextBGN();
        m_state_next_certained = ST_BGN;

        return true;

    } else if (ctxt == FUNCTION_ARGS) {
        // NOTE: no exp before this context pos needed to evaluate, so process immediately

        m_ctxt_stack.push(FUNCTION_BLOCK);
        m_expect_char_types = {};

        // TODO register function definition meta
        // record from definition's '{', so we can run from here
        int exec_begin = m_parse_pos;
        m_function_declare_map[m_function_declare_cur_name].exec_begin = exec_begin;
        m_function_declare_map[m_function_declare_cur_name].args = m_function_declare_cur_argidfs;
        m_function_declare_exec_begin_name_map[exec_begin] = m_function_declare_cur_name;

        if (!findBalancedRightCurlyBraceAndSkipToIt()) {
            error("function: no balanced ending-block symbol '}' found");
            printErrorLocation();
            syntax(ctxt);
            exit(E_GRAMMAR);
        }

        // ' } '   =>   ' } '
        //  ^              ^
        //  m_parse_pos    end_pos
        m_function_declare_map[m_function_declare_cur_name].exec_end = m_parse_pos + 2;

        // reset temp vars after consumed
        m_function_declare_cur_name = "";
        m_function_declare_cur_argidfs = {};

        pushContextBGN();
        m_state_next_certained = ST_BGN;
        ++m_lcb_count;
        return true;

    } else {
        print("un-specially-processed contextType: %s", getContextTypeStr(ctxt));
    }

    print("process '{' self");

    ////////////////////////////////////////////////////////////////
    // do not rely on context-free m_id, it can not be used to construct grammar

    // // NOTE: process case "if|elif idf {}" specifically
    // // get opd stack well-prepared before calculate with opr stack.
    // if (m_id == "if" || m_id == "elif") {
    //     if (m_opd_stack.empty()) {
    //         error("no condtion found for \"%s\"", m_id.c_str());
    //         printErrorLocation();
    //         exit(E_GRAMMAR);
    //     }
    // }

    // if (m_opd_stack.empty() && !m_id.empty() && ( m_id != "else" )) {
    //     pushOpdIdfValue(m_id);
    //     m_id = ""; // reset after consumed
    // }

    ////////////////////////////////////////////////////////////////

    print("calculate all oprs");
    int top;
    while (!m_opr_stack.empty() && (top = m_opr_stack.top().v) != LOWEST_PRIO_OP) {
        if (!calculate(top)) return false;
        m_opr_stack.pop();
    }

    dia("-:handleLeftCurlyBrace:after-calculate");


    // NOTE: clear m_stmt_if_cond
    // for those key larger than m_lcb_count, clear the value
    // TODO: maybe just clean vector value is good enough, for later reuse.

    print("lcb count:%d", m_lcb_count);

    for (auto it = m_stmt_if_cond.begin(); it != m_stmt_if_cond.end();) {
        if (it->first > m_lcb_count) {
            print("NOTE: erase m_stmt_if_cond[%d], coz m_lcb_count=%d", it->first, m_lcb_count);
            it = m_stmt_if_cond.erase(it);
        } else {
            ++it;
        }
    }

    // post-process

    ctxt = m_ctxt_stack.top();

    if (ctxt == CTXT_BGN) {
        ++m_lcb_count;
        m_state_next_certained = ST_BGN;
        return true;

    } else if (ctxt == STMT_IF_COND) {
        // condtion
        // why always process top opd, coz opr stack can be empty, so idf opd will
        // never be calculated. esp. for case:
        // `x=1; if x {}`

        Opd top = m_opd_stack.top();
        if (_isbase(top)) {
            error("no condition found for context: %s", getContextTypeStr(ctxt));
            printErrorLocation();
            exit(E_GRAMMAR);
        } else if (_isidf(top)) {
            std::string idf = _2str(m_opd_stack.pop());
            pushOpdIdfValue(idf);
        } else if (_isnum(top)) {
            // ok, do nothing
        } else if (_isstr(top)) {
            print("TODO");
            exit(0);
        }

        if (m_stmt_if_cond.find(m_lcb_count) == m_stmt_if_cond.end()) {
            m_stmt_if_cond[m_lcb_count] = {};
        }
        std::vector<IfCond>& if_elif_conds = m_stmt_if_cond[m_lcb_count];

        int cond = _2num(m_opd_stack.pop()) != 0? 1: 0;
        if_elif_conds.push_back({IF_COND_TYPE_IF, cond});

        if (cond != 0) {
            m_ctxt_stack.push(STMT_IF_BLOCK);
            m_ctxt_stack.push(CTXT_BGN);
            m_expect_char_types = {}; // clear

        } else {
            m_ctxt_stack.push(STMT_IF_BLOCK);
            m_expect_char_types = {}; // clear

            print("TODO: to ignore if-block, just skip to balanced '}', no syntax check for skipped content");
            if (!findBalancedRightCurlyBraceAndSkipToIt()) {
                error("if: no balanced } found");
                printErrorLocation();
                syntax(ctxt);
                exit(E_GRAMMAR);
            }
        }

        m_state_next_certained = ST_BGN;
        ++m_lcb_count;
        return true;

    } else if (ctxt == STMT_ELIF_COND) {
        // condtion
        // why always process top opd, coz opr stack can be empty, so idf opd will
        // never be calculated. esp. for case:
        // `x=1; if x {}`

        Opd top = m_opd_stack.top();
        if (_isbase(top)) {
            error("no condition found for context: %s", getContextTypeStr(ctxt));
            printErrorLocation();
            exit(E_GRAMMAR);
        } else if (_isidf(top)) {
            std::string idf = _2str(m_opd_stack.pop());
            pushOpdIdfValue(idf);
        } else if (_isnum(top)) {
            // ok, do nothing
        } else if (_isstr(top)) {
            print("TODO");
            exit(0);
        }

        if (m_stmt_if_cond.find(m_lcb_count) == m_stmt_if_cond.end()) {
            m_stmt_if_cond[m_lcb_count] = {};
        }
        std::vector<IfCond>& if_elif_conds = m_stmt_if_cond[m_lcb_count];

        if (if_elif_conds.size() == 0) {
            error("`elif` statement must follow `if` statement");
            printErrorLocation();
            exit(E_GRAMMAR);
        }

        // pre if/elif condtions
        bool pre_cond_all_zero = true;
        for (int i = 0; i < if_elif_conds.size(); i++) {
            if (if_elif_conds[i].value != 0) {
                pre_cond_all_zero = false;
                break;
            }
        }

        // self condtion
        int elif_cond = _2num(m_opd_stack.pop()) !=0? 1: 0;
        if_elif_conds.push_back({IF_COND_TYPE_ELIF, elif_cond});

        int cond = (pre_cond_all_zero == true && elif_cond != 0)? 1: 0;

        if (cond != 0) {
            m_ctxt_stack.push(STMT_ELIF_BLOCK);
            m_ctxt_stack.push(CTXT_BGN);
            m_expect_char_types = {}; // clear

        } else {
            m_ctxt_stack.push(STMT_ELIF_BLOCK);
            m_expect_char_types = {}; // clear

            print("TODO: to ignore elif-block, just skip to balanced '}', no syntax check for skipped content");
            if (!findBalancedRightCurlyBraceAndSkipToIt()) {
                error("elif: no balanced } found");
                printErrorLocation();
                exit(E_SYNTAX);
            }
        }

        m_state_next_certained = ST_BGN;
        ++m_lcb_count;
        return true;

    } else {                    // other context
        print("un-specially-processed contextType: %s", getContextTypeStr(ctxt));

        m_state_next_certained = ST_BGN;
        ++m_lcb_count;
        return true;
    }

    error("should not come here");
    printErrorLocation();
    exit(E_DEV_ERROR);
    return false;
}

bool ArithmeticExpression::findBalancedRightCurlyBraceAndSkipToIt(bool skipRigthCurlyBraceAlong) {
    int size = m_arithmetic_expression.size();
    int n_skiped_lcb = 0;
    bool found = false;

    // starting from next char right after current '{'
    //
    // TODO: to process '}' inside string
    int __log_b = m_parse_pos, __log_e = m_parse_pos;

    while (++m_parse_pos < size) {
        const char& c = m_arithmetic_expression.at(m_parse_pos);
        print("c='%c'", c);

        if (c == '\n') {
            ++m_ln_count;
            g_ln_count = m_ln_count;
        }

        switch(c) {
        case '{': ++n_skiped_lcb; break;
        case '}': --n_skiped_lcb; break;
        default: break;
        }
        if (n_skiped_lcb == -1) { // balanced } for starting '{' found
            print("balanced } found");
            dia("balanced-}-found");
            found = true;
            break;
        }
    }

    if (!found) {
        error("findBalancedRightCurlyBraceAndSkipToIt: balanced '}' not found");
        dia("findBalancedRightCurlyBraceAndSkipToIt");
        printErrorLocation();
        exit(E_INPUT_ERROR);
    }

    --m_parse_pos;
    // after `--m_parse_pos`, we are here ' }',  so parse get chance to process '{'
    //                                     ^
    //                                     m_parse_pos
    __log_e = m_parse_pos;
    print("skip: \"%.*s\"", (__log_e + 2) /* include char at __log_e*/ - __log_b, &m_arithmetic_expression[__log_b]);

    return found;
}

bool ArithmeticExpression::handleRightCurlyBrace() {
    ++m_token_idx;
    ++m_stmt_count;

    if (m_lp_count > 0) {
        error("'}' not allowed inside ()");
        handleError();
        exit(E_SYNTAX);
    }

    // firstly, ensure balanced left curly brace exists
    if (m_lcb_count == 0) {
        error("no balanced left curly brace found");
        printErrorLocation();
        exit(E_GRAMMAR);
    }

    ////////////////////////////////////////////////////////////////
    // support syntax: "idf}", due to last statement of block can ignore statement
    // terminal marker: ';'
    // TODO: maybe normalize syntax: *ALWAYS && MUST* use ';' as statement terminal
    //
    // x=2; y=0; if 1{y=x} print(y)
    //              ^^

    // prepare opds for last ';'-missing statement in block
    Opd top_opd = m_opd_stack.top();
    if (_isidf(top_opd) && top_opd.token_idx == m_token_idx-1) {
        m_opd_stack.pop();
        pushOpdIdfValue(_2idf(top_opd));
    }

    // calculate for last ';'-missing statement in block
    int top_opr;
    while (!m_opr_stack.empty() && (top_opr = m_opr_stack.top().v) != LOWEST_PRIO_OP) {
        if (!calculate(top_opr)) return false;
        m_opr_stack.pop();
    }

    // NOTE: block exists in following cases:
    //
    // 1) `{}`                  : pure empty block
    // 2) `if/elif/else {}`     : if control flow block
    // 3) `function fn(...) {}` : function block
    // ...
    // check one by one

    // NOTE: find context type to which current block's openning '{' belongs
    int step = 0;
    ContextType lcb_ctxt = m_ctxt_stack.top(step);

    // skip CTXT_BGN, CTXT_EXP
    while (-1*step < m_ctxt_stack.count() && (lcb_ctxt == CTXT_BGN || lcb_ctxt == CTXT_EXP) ) {
        --step;
        lcb_ctxt = m_ctxt_stack.top(step);
    }

    if (-1*step == m_ctxt_stack.count()) {
        error("no context type found for current balanced '{' belongs");
        printErrorLocation();
        exit(E_GRAMMAR);
    }
    print("lcb_ctxt: %s", getContextTypeStr(lcb_ctxt));

    if (lcb_ctxt == STMT_IF_BLOCK || lcb_ctxt == STMT_ELIF_BLOCK || lcb_ctxt == STMT_ELSE_BLOCK) {
        int if_cond_key = m_lcb_count - 1;
        std::vector<IfCond>& if_elif_conds = m_stmt_if_cond[if_cond_key];
        IfCond last_cond = if_elif_conds[if_elif_conds.size()-1];
        bool last_cond_is_ending_if_seq = false;

        if (last_cond.type == IF_COND_TYPE_IF || last_cond.type == IF_COND_TYPE_ELIF) {
            std::string token = getNextToken();
            print("next token:\"%s\"", token.c_str());
            last_cond_is_ending_if_seq = !(token == "elif" || token == "else");
        } else if (last_cond.type == IF_COND_TYPE_ELSE) {
            last_cond_is_ending_if_seq = true;
        } else {                // else
            error("unsupported IfCond.t:%d, should not happen", last_cond.type);
            printErrorLocation();
            exit(E_SHOULD_NOT_HAPPEN);
        }
        print("last_cond_is_ending_if_seq: %d", last_cond_is_ending_if_seq);

        if (last_cond_is_ending_if_seq) {
            // clean m_stmt_if_cond
            m_stmt_if_cond.erase(if_cond_key);

            // clean context stack: pop to STMT_IF
            ContextType top;
            while (!m_ctxt_stack.empty() && (top = m_ctxt_stack.top()) != STMT_IF) {
                print("pop %s", getContextTypeStr(top));
                m_ctxt_stack.pop();
            }
            if ((top = m_ctxt_stack.top()) == STMT_IF) {
                print("pop %s", getContextTypeStr(top));
                m_ctxt_stack.pop();
            }
        }

        goto handleRightCurlyBrace_common;

    } else if (lcb_ctxt == FUNCTION_BLOCK) {
        // clean context stack: pop to FUNCTION_DEFINITION
        ContextType top;
        while (!m_ctxt_stack.empty() && (top = m_ctxt_stack.top()) != FUNCTION_DEFINITION) {
            print("pop %s", getContextTypeStr(top));
            m_ctxt_stack.pop();
        }
        if ((top = m_ctxt_stack.top()) == FUNCTION_DEFINITION) {
            print("pop %s", getContextTypeStr(top));
            m_ctxt_stack.pop();
        }

        goto handleRightCurlyBrace_common;

    } else {
        ContextType ctxt = m_ctxt_stack.top();
        print("ctxt: %s", getContextTypeStr(ctxt));

        if (ctxt == STMT_DECLARE_ASSIGN_VALUE) {
            if (m_opd_stack.count() < 3) { // base + 2
                error("two opds need to form a assign statement, while \"%d\" gotten", m_opd_stack.count());
                printErrorLocation();
                exit(E_GRAMMAR);
            }

            Opd opdv = m_opd_stack.pop();
            double assign_v = _isidf(opdv)? getIdfValue(opdv.str): opdv.num;

            std::string assign_idf = _2idf(m_opd_stack.pop());
            assignValueToIdf(assign_idf, assign_v, false);

            goto handleRightCurlyBrace_common;

        } else if (ctxt == STMT_SCOPE_DECLARE_ASSIGN) {
            if (m_opd_stack.count() < 3) { // base + 2
                error("two opds need to form a assign statement, while \"%d\" gotten", m_opd_stack.count());
                printErrorLocation();
                exit(E_GRAMMAR);
            }

            Opd opdv = m_opd_stack.pop();
            double assign_v = _isidf(opdv)? getIdfValue(opdv.str): opdv.num;

            std::string assign_idf = _2idf(m_opd_stack.pop());
            assignValueToIdf(assign_idf, assign_v, true);

            goto handleRightCurlyBrace_common;

        } else if (ctxt == FUNCTION_BLOCK) {
            goto handleRightCurlyBrace_common;

        } else if (ctxt == FUNCTION_RETURN) { // `{return}` `{....;return}`
            m_parse_pos = m_arithmetic_expression.size();
            m_expect_char_types = {};
            goto handleRightCurlyBrace_common;

        } else if (ctxt == FUNCTION_RETURN_VALUE) { // `{....; return value }`, not ends with `;`
            print("TODO handle } for FUNCTION_RETURN_VALUE");
            m_parse_pos = m_arithmetic_expression.size();
            m_expect_char_types = {};
            goto handleRightCurlyBrace_common;

        } else {
            print("un-specially-processed contextType: %s", getContextTypeStr(ctxt));
            goto handleRightCurlyBrace_common;
        }
    }

    // TODO: function (fn declare block)
    // TODO: else if (elif block)
    // TODO: else if (else block)
    // TODO: else { /*do nothing*/ }

    // common:

handleRightCurlyBrace_common:

    // clear scope-declared idf, so later same-depth scope will not re-use the old, wrong data
    if (m_idf_scope_declared.find(m_depth + m_lcb_count) != m_idf_scope_declared.end()) { // scope has idf scope-declared
        // 1) clear scope-declared idfs from m_idf_std::map<type1> var;
        std::unordered_map<std::string,int>& idf1map = m_idf_scope_declared[m_depth + m_lcb_count];

        for (auto it = idf1map.begin(); it != idf1map.end(); ++it) {
            std::string idf = it->first;
            print("processing idf:%s", idf.c_str());

            if (m_idf_map.find(idf) == m_idf_map.end()) {
                error("no such idf:%s found when clearing, while it exists in m_idf_scope_declared", idf.c_str());
                exit(E_SHOULD_NOT_HAPPEN);
            }

            std::unordered_map<int,double>& dv = m_idf_map[idf];
            if (dv.find(m_depth + m_lcb_count) == dv.end()) {
                error("no such dv found for current AE depth:%d, scope depth:%d for idf:%s from m_idf_map when clearning, while idf exists in m_idf_scope_declared", m_depth, m_lcb_count, idf.c_str());
                exit(E_SHOULD_NOT_HAPPEN);
            }

            dv.erase(m_depth + m_lcb_count);

            if (dv.empty()) {
                m_idf_map.erase(idf);
            }
        }

        // 2) clear scope-declared [current_scope_depth]
        m_idf_scope_declared.erase(m_depth + m_lcb_count);
    }

    // chars left, goto st_bgn
    if (m_parse_pos < m_arithmetic_expression.size()-1) {
        m_state_next_certained = ST_BGN;
    }

    pushContextBGN();

    --m_lcb_count;
    return true;
}

bool ArithmeticExpression::handleColon() {
    if (nextChar(false) == '=') return true;
    error("':' only allow be part of `:=`");
    printErrorLocation();
    exit(E_GRAMMAR);
}

bool ArithmeticExpression::handleDoubleQuote() {
    int size = m_arithmetic_expression.size();
    char c;
    int m_str_start_pos = m_parse_pos;
    while ( ++m_parse_pos < size ) {
        c = m_arithmetic_expression[m_parse_pos];
        if (c == '"') {
            int n = 0;
            int i = m_parse_pos - 1;
            while (i >= 0 && m_arithmetic_expression[i] == '\\') {
                n++;
                i--;
            }
            bool escaped = (n % 2 == 1);
            print("escaped:%d, n:%d, i:%d", escaped, n, i);
            if (!escaped) break; // ok
        }
    }

    if (m_parse_pos == size) {
        error("no ending '\"' found from %d to end", m_str_start_pos);
        printErrorLocation();
        exit(E_SYNTAX);
    }

    // string found, ignore both beginning and ending '"'
    ++m_token_idx;

    int start = m_str_start_pos+1;
    int end = m_parse_pos;      // exclusive
    std::string dbq_content = m_arithmetic_expression.substr(start, end-start);
    pushOpdStr(dbq_content);

    return true;
}

void ArithmeticExpression::pushOpdIdf(std::string idf) {
    m_opd_stack.push({ m_token_idx, m_stmt_count, OPD_IDF, 0, idf });
}

void ArithmeticExpression::pushOpdIdfValue(const std::string& idf) {
    pushOpdNum(getIdfValue(idf));
}

void ArithmeticExpression::pushOpdNum(double num) {
    m_opd_stack.push({ m_token_idx, m_stmt_count, OPD_NUM, num, "" });
}

void ArithmeticExpression::pushOpdStr(std::string str) {
    // use std::move to avoid unnecessary string copy
    m_opd_stack.push({ m_token_idx, m_stmt_count, OPD_STR, 0, std::move(str) });
}

bool ArithmeticExpression::_isbase(const Opd& item) {
    return item.type == OPD_STACK_BOTTOM_BASE;
}

bool ArithmeticExpression::_isidf(const Opd& item) {
    return item.type == OPD_IDF;
}

bool ArithmeticExpression::_isnum(const Opd& item) {
    return item.type == OPD_NUM;
}

bool ArithmeticExpression::_isstr(const Opd& item) {
    return item.type == OPD_STR;
}

std::string ArithmeticExpression::_2idf(const Opd& item) {
    assert(_isidf(item));
    return item.str;
}

double ArithmeticExpression::_2num(const Opd& item) {
    if (_isidf(item)) return getIdfValue(item.str);
    if (_isnum(item)) return item.num;
    error("unsupported type:%d found for _2num", item.type);
    printErrorLocation();
    exit(E_RUN);
}

std::string ArithmeticExpression::_2str(const Opd& item) {
    assert(_isstr(item));
    return item.str;
}

// NOTE: for easier maintainance, only modify m_opd_stack when calculating, *KEEP m_opr_stack STAY*.
bool ArithmeticExpression::calculate(int opr)
{
    print("opr=<'%c',%d>", opr, opr);

    if ( '(' == opr || ')' == opr || LOWEST_PRIO_OP == opr ) {
        print("do nothing for opr='<%c %d>'", opr, opr);
        return true;
    }

    if ('f' == opr) {
        print("opr is 'f'");
        int fni = m_opr_stack.top(-1).v; // get fni

        int fncmcnt = m_fncmcnt_stack.top();
        // FunctionMeta fm = m_function_meta[fni];

        // print("fni=%d, fm.args_va=%d fm.args_count=%d fncmcnt=%d", fni, fm.args_va, fm.args_count, fncmcnt);

        // if (fm.args_va) {
        //     if (fncmcnt < fm.args_count-1) {
        //         error("function: '%s' expect at least %d arguments, while %d given. (fncmcnt=%d)", fm.name, fm.args_count, fncmcnt+1, fncmcnt);
        //         printErrorLocation();
        //         exit(E_GRAMMAR);
        //     }
        // } else {
        //     if (fncmcnt != fm.args_count-1) {
        //         error("function: '%s' expect %d arguments, while %d given. (fncmcnt=%d)", fm.name, fm.args_count, fncmcnt+1, fncmcnt);
        //         printErrorLocation();
        //         exit(E_GRAMMAR);
        //     }
        // }

        switch(fni) {

        case FABS: {
            pushOpdNum(fabs(_2num(m_opd_stack.pop())));
            break;
        }
        case FMOD: {
            Opd r = m_opd_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Opd l = m_opd_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum(fmod(l_opd, r_opd));
            break;
        }
        case REMAINDER: {
            Opd r = m_opd_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Opd l = m_opd_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum(remainder(l_opd, r_opd));
            break;
        }
        case CEIL: {
            pushOpdNum(ceil(_2num(m_opd_stack.pop())));
            break;
        }
        case FLOOR: {
            pushOpdNum(floor(_2num(m_opd_stack.pop())));
            break;
        }
        case ROUND: {
            pushOpdNum(round(_2num(m_opd_stack.pop())));
            break;
        }
        case TRUNC: {
            pushOpdNum(trunc(_2num(m_opd_stack.pop())));
            break;
        }

        case POW: {
            Opd r = m_opd_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Opd l = m_opd_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum(pow(l_opd, r_opd));
            break;
        }
        case SQRT: {
            double opd = _2num(m_opd_stack.pop());
            if (opd < 0) {
                print("sqrt expects unsigned operand, while %f gotten", opd);
                return false;
            }
            pushOpdNum(sqrt(opd));
            break;
        }
        case CBRT: {
            pushOpdNum(cbrt(_2num(m_opd_stack.pop())));
            break;
        }
        case HYPOT: {
            Opd r = m_opd_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Opd l = m_opd_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum(hypot(l_opd, r_opd));
            break;
        }
        case EXP: {
            pushOpdNum(exp(_2num(m_opd_stack.pop())));
            break;
        }
        case EXP2: {
            pushOpdNum(exp2(_2num(m_opd_stack.pop())));
            break;
        }
        case EXPM1: {
            pushOpdNum(expm1(_2num(m_opd_stack.pop())));
            break;
        }
        case LOG: {
            pushOpdNum(log(_2num(m_opd_stack.pop())));
            break;
        }
        case LOG10: {
            pushOpdNum(log10(_2num(m_opd_stack.pop())));
            break;
        }
        case LOG2: {
            pushOpdNum(log2(_2num(m_opd_stack.pop())));
            break;
        }
        case LOG1P: {
            pushOpdNum(log1p(_2num(m_opd_stack.pop())));
            break;
        }

        case SIN: {
            pushOpdNum(sin(_2num(m_opd_stack.pop())));
            break;
        }
        case COS: {
            pushOpdNum(cos(_2num(m_opd_stack.pop())));
            break;
        }
        case TAN: {
            pushOpdNum(tan(_2num(m_opd_stack.pop())));
            break;
        }
        case ASIN: {
            pushOpdNum(asin(_2num(m_opd_stack.pop())));
            break;
        }
        case ACOS: {
            pushOpdNum(acos(_2num(m_opd_stack.pop())));
            break;
        }
        case ATAN: {
            pushOpdNum(atan(_2num(m_opd_stack.pop())));
            break;
        }
        case ATAN2: {
            Opd r = m_opd_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Opd l = m_opd_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum(atan2(l_opd,r_opd));
            break;
        }

        case SINH: {
            pushOpdNum(sinh(_2num(m_opd_stack.pop())));
            break;
        }
        case COSH: {
            pushOpdNum(cosh(_2num(m_opd_stack.pop())));
            break;
        }
        case TANH: {
            pushOpdNum(tanh(_2num(m_opd_stack.pop())));
            break;
        }
        case ASINH: {
            pushOpdNum(asinh(_2num(m_opd_stack.pop())));
            break;
        }
        case ACOSH: {
            pushOpdNum(acosh(_2num(m_opd_stack.pop())));
            break;
        }
        case ATANH: {
            pushOpdNum(atanh(_2num(m_opd_stack.pop())));
            break;
        }

        case FMIN: {
            double x = _2num(m_opd_stack.pop());
            double y = _2num(m_opd_stack.pop());
            pushOpdNum(fmin(y,x));
            break;
        }
        case FMAX: {
            double x = _2num(m_opd_stack.pop());
            double y = _2num(m_opd_stack.pop());
            pushOpdNum(fmax(y,x));
            break;
        }
        case FDIM: {
            double x = _2num(m_opd_stack.pop());
            double y = _2num(m_opd_stack.pop());
            pushOpdNum(fdim(y,x));
            break;
        }
        case COPYSIGN: {
            double x = _2num(m_opd_stack.pop());
            double y = _2num(m_opd_stack.pop());
            pushOpdNum(copysign(y,x));
            break;
        }
        case ISFINITE: {
            pushOpdNum(isfinite(_2num(m_opd_stack.pop())));
            break;
        }
        case ISINF: {
            pushOpdNum(isinf(_2num(m_opd_stack.pop())));
            break;
        }
        case ISNAN: {
            pushOpdNum(isnan(_2num(m_opd_stack.pop())));
            break;
        }
        case ISNORMAL: {
            pushOpdNum(isnormal(_2num(m_opd_stack.pop())));
            break;
        }
        case SIGNBIT: {
            pushOpdNum(signbit(_2num(m_opd_stack.pop())));
            break;
        }

        case INT: {
            pushOpdNum((int)_2num(m_opd_stack.pop()));
            break;
        }
        case MAX: {
            int args_count = fncmcnt + 1;
            double v = FLT_MIN;
            int opd;
            while (!m_opd_stack.empty() && args_count--) {
                opd = _2num(m_opd_stack.pop());
                if (opd > v) v = opd;
            }

            // FIXME: [2026-10-23] support checking later
            // if (args_count != 0) {
            //     error("no args found in operand stack, should not happen");
            //     return false;
            // }

            pushOpdNum(v);
            break;
        }
        case MIN: {
            int args_count = fncmcnt + 1;
            double v = FLT_MAX;
            int opd;
            while (!m_opd_stack.empty() && args_count--) {
                opd = _2num(m_opd_stack.pop());
                if (opd < v) v = opd;
            }

            // if (args_count != 0) {
            //     error("no args found in operand stack, should not happen");
            //     return false;
            // }

            pushOpdNum(v);
            break;
        }
        case CMP: {
            if (m_opd_stack.count() < 3) { // base + 2
                error("cpm(v1,v2,v3) expects three operands, while %d left", m_opd_stack.count());
                printErrorLocation();
                exit(E_GRAMMAR);
            }
            double v3 = _2num(m_opd_stack.pop());
            double v2 = _2num(m_opd_stack.pop());
            double v1 = _2num(m_opd_stack.pop());
            double v = v1 != 0? v2: v3;
            pushOpdNum(v);
            break;
        }

        case PRINT: {
            if (m_opd_stack.empty()) { // "print()"
                printf("\n");
                break;
            }
            const Opd& opd = m_opd_stack.pop();
            if (_isidf(opd)) {
                printf("%g\n", getIdfValue(opd.str)); // current only support double
            } else if (_isnum(opd)) {
                printf("%g\n", _2num(opd));
            } else if (_isstr(opd)) {
                printf("%s\n", _2str(opd).c_str());
            } else {
                error("un-specially-processed Opd (type:%d) called by PRINT", opd.type);
            }
            break;
        }

        case PRINTF: {
            // TODO: to support real printf
            const Opd& opd = m_opd_stack.pop();
            if (_isidf(opd)) {
                printf("%g", getIdfValue(opd.str)); // current only support double
            } else if (_isnum(opd)) {
                printf("%g", _2num(opd));
            } else if (_isstr(opd)) {
                printf("%s", _2str(opd).c_str());
            } else {
                error("un-specially-processed Opd (type:%d) called by PRINT", opd.type);
            }
            break;
        }

        default:
            error("unsupported fni: %d", fni);
            return false;
        }

        // NOTE: always pop when fn has successfully been processed
        // m_fncmcnt_stack.pop();
        return true;
    } else if ('F' == opr) {
        int exec_begin = m_opr_stack.top(-1).v; // function exec_begin
        const std::string& fnn = m_function_declare_exec_begin_name_map[exec_begin];
        print("call self-defined function: exec_begin=%d fnn:\"%s\"", exec_begin, fnn.c_str());
        std::string scope_name_old = scope_name;
        scope_name = scope_name+"/"+fnn;
        call(fnn);
        scope_name = scope_name_old;
        return true;
    } else {
        switch (opr) {
        case '^': {
            Opd r = m_opd_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Opd l = m_opd_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum( pow(l_opd,r_opd) ); break;
        }
        case '+': {
            Opd r = m_opd_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Opd l = m_opd_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum( l_opd + r_opd ); break;
        }
        case '-': {
            Opd r = m_opd_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Opd l = m_opd_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum( l_opd - r_opd ); break;
        }
        case '*': {
            Opd r = m_opd_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Opd l = m_opd_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum( l_opd * r_opd ); break;
        }
        case '/': {
            Opd r = m_opd_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            if ( 0 == r_opd ) {
                error("divide 0");
                return false;
            }
            Opd l = m_opd_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum( l_opd / r_opd);
            break;
        }
        case '>': {
            Opd r = m_opd_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Opd l = m_opd_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum(l_opd > r_opd? 1: 0);
            break;
        }
        case '<': {
            Opd r = m_opd_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Opd l = m_opd_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum(l_opd < r_opd? 1: 0);
            break;
        }
        case 'e': {             // "=="
            Opd r = m_opd_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Opd l = m_opd_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            print("e: l_opd=%g r_opd=%g ?=%d", l_opd, r_opd, l_opd == r_opd);
            pushOpdNum(l_opd == r_opd? 1: 0);
            break;
        }
        case 'a': {             // "&&" and
            Opd r = m_opd_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Opd l = m_opd_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum(l_opd !=0 && r_opd != 0? 1: 0);
            break;
        }
        case 'o': {             // "||" and
            Opd r = m_opd_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Opd l = m_opd_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum(l_opd !=0 || r_opd != 0? 1: 0);
            break;
        }
        case '!': {
            Opd r = m_opd_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            pushOpdNum(r_opd == 0? 1: 0);
            break;
        }
        default: {
            error("unsupported operator='<%c %d>'", opr, opr);
            printErrorLocation();
            return false;
        }
        }
        return true;
    }
}

// bool ArithmeticExpression::call_old(const std::string function_name) {
//     if (m_function_declare_map.find(function_name) == m_function_declare_map.end()) {
//         error("no such self-defined function: \"%s\"", function_name.c_str());
//         exit(E_DEV_ERROR);
//     }

//     // TODO: m_xx vars sucks for parsing block

//     // TODO: use call_stack to remember current exec_begin
//     // switch to function exec_begin

//     int parse_pos_to_back = m_parse_pos + 1;
//     print("remember parse_pos_to_back:%d, start:\"%s\"", parse_pos_to_back, &m_arithmetic_expression[parse_pos_to_back]);

//     // jump to function
//     m_state = ST_BGN;
//     m_parse_pos = m_function_declare_map[function_name][0];
//     int end_pos = m_function_declare_map[function_name][1];

//     print("EXECUTE CODE: \"%.*s\"", end_pos - m_parse_pos, &m_arithmetic_expression[m_parse_pos]);
//     parse(end_pos);

//     // switch back when done
//     // TODO: m_xx maybe polluted
//     print("switch back to pos:%d", parse_pos_to_back);
//     m_parse_pos = parse_pos_to_back;
//     parse(-1);

//     return true;
// }

bool ArithmeticExpression::call(const std::string function_name) {
    print("function_name = %s", function_name.c_str());

    if (m_function_declare_map.find(function_name) == m_function_declare_map.end()) {
        error("no such self-defined function: \"%s\"", function_name.c_str());
        exit(E_DEV_ERROR);
    }

    // TODO: use ArithmeticExpression recursively
    // so run state of each block is stored automatically

    int begin = m_function_declare_map[function_name].exec_begin;
    int end = m_function_declare_map[function_name].exec_end;

    // TODO: support multiple type for value part
    std::unordered_map<std::string,double> argm = {};
    std::vector<std::string>& args = m_function_declare_map[function_name].args;

    for (int i = (int)args.size() - 1; i >= 0; i--) {
        std::string& key = args[i];
        if (m_opd_stack.empty()) {
            error("function %s expects %zu args, current argv[%d] name:%s, while no operand any more", function_name.c_str() , args.size(), i, key.c_str());
            exit(E_INPUT_ERROR);
        }
        argm[key] = _2num(m_opd_stack.pop());
    }

    printnoln("exec function \"%s\" with argm: ", function_name.c_str());
    for (const auto& item : argm) {
        __print(" %s=%g", item.first.c_str(), item.second);
    }
    __print("\n");

    ArithmeticExpression fnblock(
        m_arithmetic_expression,
        getDepth()+1,
        getLCBCount(),
        getIdfMap(),        // FIXME: maybe just leak global scope idf to valid idf value pollution
        argm,
        FUNCTION_CALL,
        g_print_enable
        );
    fnblock.setMeta(
        getFunctionDeclareMap(),
        getFunctionDeclareExecBeginNameMap(),
        getIdfScopeDeclared() // FIXME: maybe just leak global scope idf to valid idf value pollution
        );

    g_depth = getDepth()+1;

    if (! fnblock.parse(begin, end)) {
        exit(E_EVALUATE);
    }

    Opd *return_value_ptr = fnblock.getReturnValuePtr();
    if (return_value_ptr != NULL) {
        // TODO: now, only support num
        pushOpdNum(_2num(*return_value_ptr));
    }

    print("merge fnblock's outer scope reassigned map to caller's m_idf_map");
    std::unordered_map<std::string,double>& m = fnblock.getIdfOuterScopeReassignedMap();
    for (const auto& item : m) {
        assignValueToIdf(item.first, item.second, false);
    }
    dia("after outer scope reassigned map merge");

    --g_depth;
    return true;
}

void ArithmeticExpression::setMeta(
    std::unordered_map<std::string,FunctionDeclare>& function_map,
    std::unordered_map<int,std::string>& function_exec_begin_name_map,
    std::unordered_map<int, std::unordered_map<std::string,int>>& idf_scope_declared
    )
{
    m_function_declare_map = function_map;
    m_function_declare_exec_begin_name_map = function_exec_begin_name_map;
    m_idf_scope_declared = idf_scope_declared;
}

// @end_pos is exclusive
bool ArithmeticExpression::parse(int begin_pos, int end_pos)
{
    if (end_pos == -1) {
        int size = m_arithmetic_expression.size();
        if ( size == 0 ) {
            print("empty arithmetic expression");
            return true;
        }
        end_pos = size;
    }

    m_parse_pos = begin_pos;

    print("\"%.*s\"", end_pos - m_parse_pos, &m_arithmetic_expression[m_parse_pos]);
    printnoln("\u200B\n");

    while ( m_parse_pos < end_pos ) {
        const char *start = &m_arithmetic_expression[m_parse_pos];
        const char *end = strchr(start, '\n');
        if (!end) end = strchr(start, '\0');
        // FIXME: [2026-10-28] end may exceeds end_pos

        const char& c = m_arithmetic_expression.at(m_parse_pos);

        if (c=='\n') {
            ++m_ln_count;
            g_ln_count = m_ln_count;
        }

        // FIXME: [2026-10-28]  buggy
        print("> \"%.*s\"", (int)(end - start), start);

        print("c = '%c', m_parse_pos = %d", c, m_parse_pos);

        if ( ! handle(c) ) {
            error("failed to handle, "
                  "m_id=\"%s\", "
                  "tailing=\"%s\", pos=%d, "
                  "char='%c', raw exp=\"%s\", "
                  "state=\"%s\"",
                  m_id.c_str(),
                  &m_arithmetic_expression[m_parse_pos],
                  m_parse_pos,
                  m_arithmetic_expression[m_parse_pos],
                  m_arithmetic_expression.c_str(),
                  getStateStr(m_state)
                );
            return false;
        }

        ++m_parse_pos;

        __print("\n");
    }
    dia("parse:+");

    return handleInputAllParsed();
}

bool ArithmeticExpression::handleInputAllParsed()
{
    __print("\n");

    dia("-:handleInputAllParsed");

    ContextType ctxt = m_ctxt_stack.top();

    if (ctxt == STMT_DECLARE_ASSIGN) {
        error("invalid assign statement. syntax: \"idf = ...;\"");
        printErrorLocation();
        exit(E_GRAMMAR);

    } else if (ctxt == FUNCTION_RETURN) { // 'return$'
        m_expect_char_types = {};

    } else if (ctxt == FUNCTION_RETURN_VALUE) { // 'return $'
        m_expect_char_types = {};

    } else {
        print("ignore context type: %s", getContextTypeStr(ctxt));
    }

    if (!m_expect_char_types.empty()) {
        dia("handleInputAllParsed");
        errornoln("all input parsed, but m_expect_char_types not empty, context = %s,", getContextTypeStr(ctxt));
        __error(" m_expect_char_types = [");

        for (const auto& item : m_expect_char_types) {
            __error("'%s', ", getCharTypeStr(item));
        }
        __error("]\n");

        printErrorLocation();
        syntax(ctxt);
        exit(E_GRAMMAR);
    }

    // handle '\0' : all input been parsed
    print("handle '\\0', all input been parsed");

    // check program terminal state
    if ( ! isOnTerminalState() ) {
        error("program in not on terminal state, current state=\"%s\"", getStateStr(m_state));
        return false;
    }

    if (m_state == ST_IDF) {
        if (m_id_spc_occurred) m_id_spc_occurred = false;
        if (!m_id.empty()) {
            if (m_id == "if" || m_id == "elif" || m_id == "else") {
                error("wrong syntax for %s statement", m_id.c_str());
                printErrorLocation();
                exit(E_GRAMMAR);
            }
        }
    }

    // check whether parenthesis matches
    if ( 0 != m_lp_count ) {
        error("open left parenthesis exists, count=\"%d\"", m_lp_count);
        syntax(ctxt);
        return false;
    }

    // check whether parenthesis matches
    if ( m_lcb_count_init != m_lcb_count ) {
        if (m_return_stmt_exists) {/*ok*/} else {
            error("open left curly brace exists, count=\"%d\"", m_lcb_count);
            return false;
        }
    }

    // fulfill m_rpn_expression using operator stack
    int top;
    while ( ! m_opr_stack.empty() && (top = m_opr_stack.top().v) != LOWEST_PRIO_OP) {
        dia("-:post-process");
        if (! calculate(top)) return false;
        // always pop to go on to next opr
        m_opr_stack.pop();
        dia("post-process:+");
    }

    // clean extra CTXT_EXP if exits
    while (!m_ctxt_stack.empty()) {
        if (m_ctxt_stack.top() == CTXT_EXP) {
            m_ctxt_stack.pop();
        } else {
            break;
        }
    }

    print("m_opd_stack.count() = %d", m_opd_stack.count());
    dia("-:handleInputAllParsed:1");

    ctxt = m_ctxt_stack.top();

    if (ctxt == STMT_DECLARE_ASSIGN_VALUE) { // `a = xxxx` but without ';' as ending char
        if (m_opd_stack.count() < 3) { // base + 2
            error("two opds need to form a assign statement, while \"%d\" gotten", m_opd_stack.count());
            printErrorLocation();
            exit(E_GRAMMAR);
        }

        Opd opdv = m_opd_stack.pop();
        double assign_v = _isidf(opdv)? getIdfValue(opdv.str): opdv.num;

        std::string assign_idf = _2idf(m_opd_stack.pop());
        assignValueToIdf(assign_idf, assign_v, false);

    } else if (ctxt == FUNCTION_RETURN_VALUE) { // `return xxxx` in global scope
        if (m_opd_stack.count() >= 2) {
            m_return_value_ptr = &(m_opd_stack.top());
            // FIXME: assert failed for recursively call (eg. fib.al)
            // assert(m_return_value_ptr->stmt_count == m_stmt_count-1);
        }
    }

    print("arithmetic expression successfully parsed");
    dia("handleInputAllParsed:+");

    return true;
}
