#include "arithmetic_expression.hh"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#define LOWEST_PRIO_OP '#'
#define FLG_NEGATIVE -1
#define FLG_ACTIVE 1

#define E_DEV_ERROR 1       // error happened during development, *must* to be fixed by coding
#define E_INPUT_ERROR 10          // *fundamental* parse error
#define E_SYNTAX 20         // parse error
#define E_GRAMMAR 30        // language rule error
#define E_EVALUATE 50       // evaluating error
#define E_OTHER 99          // other undefined error

#define E_SHOULD_NOT_HAPPEN 99

#define print(...)                                  \
    do {                                            \
        if (g_print_enable) {                       \
            fprintf(stderr,"%04d %d:%s:%d ", __LINE__, g_depth, scope_name.c_str(), g_ln_count); \
            fprintf(stderr,__VA_ARGS__);                    \
            fprintf(stderr,"\n");                           \
        }                                           \
    } while (0)

#define printnoln(...)                              \
    do {                                            \
        if (g_print_enable) {                       \
            fprintf(stderr,"%04d %d:%s:%d ", __LINE__, g_depth, scope_name.c_str(), g_ln_count); \
            fprintf(stderr,__VA_ARGS__);                    \
        }                                           \
    } while (0)

#define __print(...)                            \
    do {                                        \
        if (g_print_enable) {                   \
            fprintf(stderr,__VA_ARGS__);                \
        }                                       \
    } while (0)

#define error(...)                              \
    do {                                        \
        fprintf(stderr,"%04d %d:%s:%d error ", __LINE__, g_depth, scope_name.c_str(), g_ln_count); \
        fprintf(stderr,__VA_ARGS__);                    \
        fprintf(stderr,"\n");                           \
    } while (0)

#define errornoln(...)                          \
    do {                                        \
        fprintf(stderr,"%04d %d:%s:%d error ", __LINE__, g_depth, scope_name.c_str(), g_ln_count); \
        fprintf(stderr,__VA_ARGS__);                    \
    } while (0)

#define __error(...)                            \
    do {                                        \
        fprintf(stderr,__VA_ARGS__);                    \
    } while (0)

#define warn(...)                               \
    do {                                        \
        fprintf(stderr,"%04d %d:%s:%d warning ", __LINE__, g_depth, scope_name.c_str(), g_ln_count); \
        fprintf(stderr,__VA_ARGS__);                    \
        fprintf(stderr,"\n");                           \
    } while (0)

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

      m_function_name(""),
      m_function_map({}),
      m_function_exec_pos_name_map({}),
      m_function_args({}),
      m_expect_char_types({}),

      m_id(""),
      m_id_spc_occurred(false),

      m_stmt_assign_key("")
{
    m_token_idx = 0;
    pushOpr(LOWEST_PRIO_OP);
    m_operand_stack.push({m_token_idx, -9, OPD_STACK_BOTTOM_BASE, 0, ""}); // to ensure top() always have stuff to avoid break
    m_ctxt_stack.push(CTXT_STACK_BOTTOM_BASE); // to ensure top() always have stuff to avoid break

    m_ctxt_stack.push(contextType);
    m_idf_map = idf_map;
    m_pre_defined_idf_map = pre_defined_idf_map;

    g_print_enable = print_enable;
    print("g_print_enable: %d", g_print_enable);

    dia("ArithmeticExpression:+");
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
    /* ST_BGN */ {{AT_NON, ST_BGN}, {AT_ERR, ST_ERR}, {AT_OPD, ST_OPD}, {AT_LPS, ST_LPS}, {AT_RPS, ST_UNC}, {AT_FLG, ST_FLG}, {AT_ALP, ST_IDF}, {AT_ERR, ST_ERR}, {AT_SMC, ST_STT}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_EXM, ST_OPR}, {AT_LCB, ST_LCB}, {AT_RCB, ST_RCB}, {AT_ERR, ST_ERR}, {AT_DBQ, ST_OPD}},
    /* ST_OPR */ {{AT_NON, ST_OPR}, {AT_ERR, ST_ERR}, {AT_OPD, ST_OPD}, {AT_LPS, ST_LPS}, {AT_ERR, ST_ERR}, {AT_FLG, ST_FLG}, {AT_ALP, ST_IDF}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_EXM, ST_OPR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_DBQ, ST_OPD}},
    /* ST_OPD */ {{AT_NON, ST_OPD}, {AT_OPR, ST_OPR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_RPS, ST_RPS}, {AT_OPR, ST_OPR}, {AT_ERR, ST_ERR}, {AT_CMA, ST_BGN}, {AT_SMC, ST_STT}, {AT_SEQ, ST_UNC}, {AT_AMP, ST_UNC}, {AT_VBR, ST_UNC}, {AT_ERR, ST_ERR}, {AT_LCB, ST_LCB}, {AT_RCB, ST_RCB}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}},
    /* ST_LPS */ {{AT_NON, ST_LPS}, {AT_ERR, ST_ERR}, {AT_OPD, ST_OPD}, {AT_LPS, ST_LPS}, {AT_ERR, ST_ERR}, {AT_FLG, ST_FLG}, {AT_ALP, ST_IDF}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_EXM, ST_OPR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_DBQ, ST_OPD}},
    /* ST_RPS */ {{AT_NON, ST_RPS}, {AT_OPR, ST_OPR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_RPS, ST_RPS}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_CMA, ST_BGN}, {AT_SMC, ST_STT}, {AT_SEQ, ST_UNC}, {AT_AMP, ST_UNC}, {AT_VBR, ST_UNC}, {AT_ERR, ST_ERR}, {AT_LCB, ST_LCB}, {AT_RCB, ST_RCB}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}},
    /* ST_FLG */ {{AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_OPD, ST_OPD}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ALP, ST_IDF}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_EXM, ST_OPR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}},
    /* ST_IDF */ {{AT_SPC, ST_UNC}, {AT_OPR, ST_OPR}, {AT_ALP, ST_IDF}, {AT_LPS, ST_BGN}, {AT_RPS, ST_RPS}, {AT_FLG, ST_FLG}, {AT_ALP, ST_IDF}, {AT_CMA, ST_BGN}, {AT_SMC, ST_STT}, {AT_SEQ, ST_UNC}, {AT_AMP, ST_UNC}, {AT_VBR, ST_UNC}, {AT_ERR, ST_ERR}, {AT_LCB, ST_LCB}, {AT_RCB, ST_RCB}, {AT_CLN, ST_CLN}, {AT_CLN, ST_CLN}},
    /* ST_LCB */ {{AT_NON, ST_LCB}, {AT_ERR, ST_ERR}, {AT_OPD, ST_OPD}, {AT_LPS, ST_BGN}, {AT_ERR, ST_ERR}, {AT_FLG, ST_FLG}, {AT_ALP, ST_IDF}, {AT_ERR, ST_ERR}, {AT_SMC, ST_STT}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_LCB, ST_LCB}, {AT_NON, ST_UNC}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}},
    /* ST_RCB */ {{AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}, {AT_NON, ST_UNC}},
    /* ST_RCB */ {{AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_SEQ, ST_UNC}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}, {AT_ERR, ST_ERR}},
};

ArithmeticExpression::CharTypeInfo
ArithmeticExpression::m_char_type_info[] = {
    {CT_SPC, " "},
    {CT_OP,  "operators"},
    {CT_NUM, "0-9"},
    {CT_LP,  "("},
    {CT_RP,  ")"},
    {CT_FLG, "flag:+-"},
    {CT_ALP, "alphas"},
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
    case AT_CMA: rc = handleComma(); break;
    case AT_SMC: rc = handleSemicolon(); break;
    case AT_SEQ: rc = handleSymbolEqual(); break;
    case AT_AMP: rc = handleSymbolAmpersand(); break;
    case AT_VBR: rc = handleSymbolVbar(); break;
    case AT_EXM: rc = handleExclamation(); break;
    case AT_LCB: rc = handleLeftCurlyBrace(); break;
    case AT_RCB: rc = handleRightCurlyBrace(); break;
    case AT_CLN: rc = handleColon(); break;
    case AT_DBQ: rc = handleDoubleQuote(); break;
    // more here
    case AT_ERR: handleError(); rc= false; break;
    default:
        print("no handler found for action type:%d", type);
        rc = false;
        break;
    }
    return rc;
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
        case ST_OPD:
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

void ArithmeticExpression::dia(const std::string& marker="") {
    const char& c = m_arithmetic_expression[m_parse_pos];
    CharType type = getCharType(c);
    StateTable* table_item = &m_state_table[m_state][type];

    printnoln("dia: %s @%d ", marker.c_str(), m_parse_pos);

    if (g_print_enable & 1) {   // st
        __print("[%s:%s + '%c'(CT=%d,AT=%d) => %s]",
                getStateStr(m_state_prev),
                getStateStr(m_state),
                c,
                type,
                table_item->action_type,
                getStateStr(table_item->new_state)
            );
        if (g_print_enable <1*2) __print("\n");
    }

    if (g_print_enable & 2) {   // opr, opd
        __print(" opd_stack="); m_operand_stack.dia(g_print_enable);
        __print(" opr_stack="); m_operator_stack.dia(g_print_enable);
        if (g_print_enable <2*2) __print("\n");
    }
    if (g_print_enable & 4) {__print(" cmc_stack="); m_fncmcnt_stack.dia(g_print_enable);}

    if (g_print_enable & 8) {
        __print(" m_ctxt_stack="); m_ctxt_stack.dia(g_print_enable);
        __print(" m_expect_char_types=[");
        for (const auto& item : m_expect_char_types) {
            __print("'%s' ", getCharTypeStr(item));
        }
        __print("]");
        if (g_print_enable <8*2) __print("\n");
    }

    if (g_print_enable & 16) {  // idf
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
        if (g_print_enable <16*2) __print("\n");
    }

    if (g_print_enable & 32) {  // idf (scope_declared)
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

        if (g_print_enable <32*2) __print("\n");
    }

    if (g_print_enable & 64) {  // if
        for (const auto& item : m_stmt_if_cond) {
            __print(" m_stmt_if_cond[\"%d\"]=[", item.first);
            for (int x : item.second) {
                __print("%d ", x);
            }
            __print("]");
        }

        if (g_print_enable <64*2) __print("\n");
    }

    if (g_print_enable & 128) { // fn
        __print(" m_function_name = %s", m_function_name.c_str());

        __print(" m_function_map");
        for (const auto& item : m_function_map) {
            __print("[\"%s\"]=", item.first.c_str());
            __print("[%d,%d] ", item.second[0], item.second[1]);
        }

        __print(" m_function_pos2name_name");
        for (const auto& item : m_function_exec_pos_name_map) {
            __print("[%d]=%s ", item.first, item.second.c_str());
        }

        __print(" m_function_args=[");
        for (const auto& item : m_function_args) {
            __print("\"%s\" ", item.c_str());
        }
        __print("]");

        if (g_print_enable <128*2) __print("\n");
    }

    if (g_print_enable & 256) {
        __print(" m_depth = %d", m_depth);
        __print(" m_stmt_count=%d", m_stmt_count);
        __print(" m_token_idx = %d", m_token_idx);

        __print(" m_id = \"%s\" m_id_spc_occurred = %s m_lp_count = %d m_lcb_count = %d m_state_next_certained = %s rpn = \"%s\"\n",
                m_id.c_str(),
                m_id_spc_occurred?"true":"false",
                m_lp_count,
                m_lcb_count,
                getStateStr(m_state_next_certained),
                m_rpn_expression.c_str()
            );
    }
}

void ArithmeticExpression::appendToRpnExpression(char c) {
    // if (c == 'f') {
    //     m_rpn_expression.append( fni2fnn(m_operator_stack.top(-1)) );
    // } else {
    //     m_rpn_expression.push_back(c);
    // }
}

bool ArithmeticExpression::isOnTerminalState() const {
    return (
        ST_BGN == m_state
        || ST_OPD == m_state
        || ST_IDF == m_state
        || ST_RPS == m_state
        || ST_RCB == m_state
        || ST_STT == m_state
        );
}

bool ArithmeticExpression::isCharTypeExpected(CharType type) {
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
            errornoln("%s expects char type: [", getContextTypeStr(m_ctxt_stack.top()));
            for (const auto& item : m_expect_char_types) {
                __error("'%s' ", getCharTypeStr(item));
            }
            __error("], while '%s' gotten\n", getCharTypeStr(type));
            error("TODO: show the right syntax");
            printErrorLocation();
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
                error("uncertained state occurs");
                printErrorLocation();
                dia("handle[uncertained state]");
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
        print("handle: after action_type:%d, state manual-switched %s -> %s", table_item->action_type, getStateStr(table_item->new_state), getStateStr(m_state));
    }

    return rc;
}

bool ArithmeticExpression::handleNone()
{
    return true;
}

void ArithmeticExpression::pushOpr(int v) {
    m_operator_stack.push({m_token_idx, v});
}

bool ArithmeticExpression::handleFlag(char flag)
{
    print("handing flags ...");
    if ( '-' == flag) {
        pushOpdNum(-1);
        pushOpr('*');
    }

    ContextType ctxt = m_ctxt_stack.top();

    if (ctxt == STMT_DECLARE_ASSIGN) {
        m_ctxt_stack.push(STMT_DECLARE_ASSIGN_VALUE);
        m_expect_char_types = {};
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
    for (int i=m_depth + m_lcb_count; i>=0; --i) {
        if (dv.find(i) != dv.end()) { // found
            v = dv[i];
            print("getIdfValue: found, idf:%s, i:%d, value:%g", idf.c_str(), i, v);
            return v;
        }
    }
    if (isnan(v)) {
        error("no value found for idf:%s, but idf is registered, should not happen", idf.c_str());
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

    print("assignValueToIdf: : idf=%s, m_depth=%d, m_lcb_count=%d, value=%g, is_scope_declared=%d", idf.c_str(), m_depth, m_lcb_count, value, is_scope_declared);

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
            for (int i=m_depth + m_lcb_count; i>=0; --i) {
                if (dv.find(i) != dv.end()) { // found
                    dv[i] = value;
                    assign_success = true;
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

        } else {            // scope has idf scope-declared
            std::unordered_map<std::string,int>& idf1map = m_idf_scope_declared[m_depth + m_lcb_count];
            if (idf1map.find(idf) != idf1map.end()) {
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

    char top = m_operator_stack.top().v;
    while ( getPriority(top) >= getPriority(opr) && '(' != top ) {

        // NOTE: to support "right-associative ^", eg: 2^3^2, if both
        // opr and top are '^', do not calculate immediately.
        if (opr == top && opr == '^') break;

        // right-associative for !, do not calculate immediately, case: !!1
        if (opr == top && opr == '!') break;

        if (! calculate(top)) return false;

        // one opr ops, one ctxt clear
        m_ctxt_stack.pop();

        // if calcute success, means cur opr consumed,
        // so pop, and go on to next opr.
        m_operator_stack.pop();
        top = m_operator_stack.top().v;
    }

    print("pushing operator:'%c', rpn:\"%s\"", opr, m_rpn_expression.c_str());
    pushOpr(opr);

    // STMT_DECLARE_ASSIGN_VALUE is a special case of CTXT_EXP
    ContextType ctxt = m_ctxt_stack.top();
    print("handleOperator: current ctxt:%s", getContextTypeStr(ctxt));

    if (ctxt == STMT_DECLARE_ASSIGN) {
        m_ctxt_stack.push(STMT_DECLARE_ASSIGN_VALUE);
        return true;

    } else if (ctxt == STMT_DECLARE_ASSIGN_VALUE) {
        // do nothing
        return true;

    } else {
        warn("handleOperator: un-specially-processed contextType: %s", getContextTypeStr(ctxt));
        m_ctxt_stack.push(CTXT_EXP);
        return true;
    }

    return true;
}

void ArithmeticExpression::printErrorLocation() {
    errornoln("\u200B\n");
    errornoln("%s\n", m_arithmetic_expression.c_str());
    errornoln("%*s^\n", m_parse_pos, "");
    errornoln("%*s%d\n", m_parse_pos, "", m_parse_pos);
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
    __error("%s expects", getStateStr(m_state));
    StateTable* l = m_state_table[m_state];
    for (int i=0; i<CT_UPPER; i++) {
        if (l[i].action_type != AT_ERR) {
            __error(" '%s'", getCharTypeStr(i));
        }
    }
    __error(", while '%c' gotten\n", m_arithmetic_expression[m_parse_pos]);

    exit(E_SYNTAX);
}

bool ArithmeticExpression::handleOperand()
{
    ++m_token_idx;

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
    if (ctxt == STMT_DECLARE_ASSIGN) {
        m_ctxt_stack.push(STMT_DECLARE_ASSIGN_VALUE);
        m_expect_char_types = {};
    }

    return true;
}

bool ArithmeticExpression::handleLeftParenthesis()
{
    ++m_token_idx;
    ++m_lp_count;               // *always*

    if (m_id_spc_occurred) {
        m_id_spc_occurred = false;
    }

    print("handleLeftParenthesis: m_id=%s", m_id.c_str());
    // todo: replace with m_operand_stack.top()

    ////////////////////////////////////////////////////////////////
    // TODO: to support "if(" "elif("
    // no need set if/elif context here,coz that can be done in handleAlpha immediately
    // in where we know what m_id is. Here, we only process the following stuff of if/elif context

    // Operand opd = m_operand_stack.top();
    // if (_isidf(opd) || _iss(opd)) {

    //     std::string idf = _2s(opd);

    //     if (idf == "if") {     // keyword
    //         ++m_lp_count;
    //         m_ctxt_stack.push(STMT_IF_COND);
    //         m_state_next_certained = ST_BGN;

    //     } else if (idf == "elif") {     // keyword
    //         ++m_lp_count;
    //         m_ctxt_stack.push(STMT_ELIF_COND);
    //         m_state_next_certained = ST_BGN;

    //     } else {                // normal idf
    //     }
    // }

    ////////////////////////////////////////////////////////////////

    // check context first
    ContextType ctxt = m_ctxt_stack.top();
    print("handleLeftParenthesis in context: %s", getContextTypeStr(ctxt));

    if (ctxt == CTXT_BGN || ctxt == CTXT_EXP || ctxt == STMT_DECLARE_ASSIGN_VALUE) {

        // MARK

        // We need check wether current '(' is a function call openning parentheses
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
        // if yes, opr is a function opening parentheses, else, is exp grouping parentheses

        Operand opd = m_operand_stack.top();

        if (_isbase(opd)) {
            pushOpr('(');
            return true;

        } else if (_isidf(opd)) {

            bool is_lp_fn_opening_parentheses = (opd.token_idx == m_token_idx-1);

            if (is_lp_fn_opening_parentheses) {
                std::string idf = opd.str;

                if (m_function_map.find(idf) != m_function_map.end()) {
                    int exec_pos = m_function_map[idf][0];
                    pushOpr(exec_pos); // uniq
                    pushOpr('F'); // self-defined function

                    // TODO, use m_function_call_args_count_stack directly, comma can't tell 0 argument and 1 argument
                    m_fncmcnt_stack.push(0);

                    m_operand_stack.pop(); // pop idf opd after been registered in to opr stack

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

                    m_operand_stack.pop(); // pop idf opd after been registered in to opr stack

                    return true;
                }
            } else {                // normal exp
                print("TODO");
                exit(0);
            }

        } else if (_isn(opd)) {
            pushOpr('(');
            return true;

        }  else if (_iss(opd)) {
            print("TODO");
            exit(0);
        }

    } else if (ctxt == STMT_DECLARE_ASSIGN) {
        pushOpr('(');

        m_ctxt_stack.push(STMT_DECLARE_ASSIGN_VALUE);
        m_expect_char_types = {};
        return true;

    } else if (m_ctxt_stack.top() == FUNCTION_DEFINITION) {
        if (m_function_name == "") {
            error("no function name found after keyword \"function\"");
            printErrorLocation();
            exit(E_GRAMMAR);
        }
        m_id = "";
        m_expect_char_types = { CT_ALP, CT_RP, CT_SPC };
        m_ctxt_stack.push(FUNCTION_ARGS);
        return true;

    } else {
        warn("handleLeftParenthesis: un-specially-processed contextType: %s", getContextTypeStr(ctxt));
        return true;
    }

    error("should not come here");
    printErrorLocation();
    exit(E_DEV_ERROR);
    return false;
}

bool ArithmeticExpression::handleRightParenthesis()
{
    --m_lp_count;               // always

    if ( m_lp_count < 0 ) {
        error("exists extra right parenthesis");
        return false;
    }

    //++m_token_idx;

    if (m_ctxt_stack.top() == FUNCTION_ARGS) {
        if (m_id != "") {
            m_function_args.push_back(m_id);
        }
        m_id = "";
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
    while ( ! m_operator_stack.empty() && (top = m_operator_stack.top().v) != LOWEST_PRIO_OP) {
        dia("-:handleRightParenthesis loop");
        if (top == '(') {
            // done for whole, (...), so pop '('
            Opr x = m_operator_stack.pop();
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
            m_operator_stack.pop(); // 'f', 'F'
            m_operator_stack.pop(); // fni, exec_pos
            break;
        } else {
            if (! calculate(top)) return false;
            // NOTE: if calculate success, means the top opr been consumed,
            // so pop it. and go on to next opr
            Opr x = m_operator_stack.pop();
            print("pop opr:<%d '%c'> after calculate", x.v, x.v);
        }

        dia("handleRightParenthesis:+ loop");
    }

    // DO NOT save this operator:')' into stack
    return true;
}

bool ArithmeticExpression::handleAlpha(char _c)
{
    ++m_token_idx;

    // TODO: [2026-10-10] parse like handleOperand to ignore m_id_spc_occurred.

    // process m_id_spc_occurred firstly
    do {
        if (m_id_spc_occurred) {
            if (m_ctxt_stack.top() == FUNCTION_DEFINITION) { // function fnn
                // ok
                break;
            }
            if (m_id == "else") {
                error("identifer can not follow keyword: \"%s\"", m_id.c_str());
                printErrorLocation();
                exit(E_SYNTAX);
            }

            // error("space not allowed between alphas, current m_id:%s, m_id_spc_occurred: %d", m_id.c_str(), m_id_spc_occurred);
            // printErrorLocation();
            // exit(E_SYNTAX);

        }
    } while (0);

    if (m_id_spc_occurred)
        m_id_spc_occurred = false;

    m_id = "";

    ////////////////////////////////////////////////////////////////

    // get token: "idf"
    // maybe we will remove m_id, coz idf now saved in Opd, can access anyway.
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

    dia();

    ContextType ctxt = m_ctxt_stack.top();
    print("handleAlpha: ctxt: %s", getContextTypeStr(ctxt));

    if (ctxt == CTXT_BGN || ctxt == CTXT_EXP) {
        if (m_id == "if") {
            m_ctxt_stack.push(STMT_IF_COND);
            m_state_next_certained = ST_BGN;
            return true;        // for less code-reading

        } else if (m_id == "function") {
            m_ctxt_stack.push(FUNCTION_DEFINITION);
            m_state_next_certained = ST_BGN;
            return true;

        } else {
            // fnn or var
            // always treat it as opd first, if is turn out be fnn,
            // it must be followed by '(', we will check out there.
            pushOpdIdf(m_id);
            return true;
        }

    } else if (ctxt == FUNCTION_DEFINITION) { // function FNN
        m_function_name = m_id;
        m_function_map[m_function_name][0] = 0;
        m_expect_char_types = { CT_LP, CT_SPC };
        return true;

    } else if (ctxt == FUNCTION_ARGS) {                // function name(ARG or function name(arg, ARG
        m_expect_char_types = { CT_CMA, CT_RP, CT_SPC };
        return true;

    } else if (ctxt == STMT_DECLARE_ASSIGN) {

        pushOpdIdf(m_id);

        m_ctxt_stack.push(STMT_DECLARE_ASSIGN_VALUE);
        m_expect_char_types = {};
        return true;

    } else if (ctxt == STMT_DECLARE_ASSIGN_VALUE) {
        pushOpdIdf(m_id);
        return true;
    } else {
        warn("handleAlpha: un-specially-processed contextType: %s", getContextTypeStr(ctxt));

        // PATTERN: context + expects

        // if (m_id == "if") {
        //     m_ctxt_stack.push(STMT_IF_COND);
        //     m_expect_char_types = { CT_SPC, CT_LP, CT_FLG };
        // }
    }

    dia("handleAlpha:+");

    return true;
}

bool ArithmeticExpression::handleSpace() {

    if (m_state == ST_IDF) {
        if (m_id == "if") {
            m_ctxt_stack.push(STMT_IF_COND);
            m_state_next_certained = ST_BGN;
        } else if (m_id == "elif") {
            m_ctxt_stack.push(STMT_ELIF_COND);
            m_state_next_certained = ST_BGN;
        } else if (m_id == "function") {
            m_ctxt_stack.push(FUNCTION_DEFINITION);
            m_state_next_certained = ST_BGN;
        } else {
            m_id_spc_occurred = true;
            m_state_next_certained = ST_IDF;
        }
    }

    ContextType ctxt = m_ctxt_stack.top();
    if (ctxt == STMT_DECLARE_ASSIGN) {
        m_ctxt_stack.push(STMT_DECLARE_ASSIGN_VALUE);
        m_expect_char_types = {};
    }

    return true;
}

bool ArithmeticExpression::handleComma() {
    ++m_token_idx;

    if (m_id_spc_occurred) m_id_spc_occurred = false;

    ContextType ctxt = m_ctxt_stack.top();

    if (ctxt == FUNCTION_ARGS) {
        if (m_id != "") {
            m_function_args.push_back(m_id);
        }
        m_id = "";
        m_expect_char_types = { CT_ALP, CT_SPC };
        return true;
    }

    int top;
    while (!m_operator_stack.empty() && (top = m_operator_stack.top().v) != LOWEST_PRIO_OP) {
        if (top == 'f' || top == 'F') {
            break;
        }
        dia("-:handleComma");
        if(!calculate(top)) return false;
        dia("handleComma:+");
        m_operator_stack.pop();
    }
    if (m_operator_stack.empty()) {
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

    print("TODO: handleSemicolon should consume everything.");
    dia("-:handleSemicolon");

    if (m_lp_count > 0) {
        error("';' not allowed inside ()");
        handleError();
        exit(E_SYNTAX);
    }

    // if (m_state == ST_IDF) {
    //     if (m_id_spc_occurred) m_id_spc_occurred = false;
    //     if (m_id != "") {
    //         if (!pushOpdIdfValue(m_id)) return false;
    //         m_id = ""; // reset after consumed
    //     }
    // }

    dia("-:handleSemicolon+1");

    ContextType ctxt = m_ctxt_stack.top();

    // we can check very firstly, for cases that should not happend
    switch(ctxt) {
    case STMT_DECLARE_ASSIGN:
        error("invalid assign statement. syntax: \"idf = ...;\"");
        printErrorLocation();
        exit(E_GRAMMAR);

    case STMT_IF_COND:
        error("invalid syntax: \"if exp ;\"");
        handleError();
        exit(E_SYNTAX);

    default:
        print("handleInputAllPassed ignore context type: %s", getContextTypeStr(ctxt));
        break;
    }

    // prepare opd

    int top;
    while (!m_operator_stack.empty() && (top = m_operator_stack.top().v) != LOWEST_PRIO_OP) {
        if (!calculate(top)) return false;
        m_operator_stack.pop();
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
        error("handleSemicolon: STMT_DECLARE_ASSIGN can not be terminal context type for a statement (marked';')");
        printErrorLocation();
        exit(E_GRAMMAR);

    } else if (ctxt == STMT_DECLARE_ASSIGN_VALUE) {
        // at least two opd need
        if (m_operand_stack.count() < 3) {
            error("two opds need to form a assign statement, while \"%d\" gotten", m_operand_stack.count());
            printErrorLocation();
            exit(E_GRAMMAR);
        } else {
            m_expect_char_types = {};
        }

        Operand opdv = m_operand_stack.pop();
        double assign_v = _isidf(opdv)? getIdfValue(opdv.str): opdv.num;
        std::string assign_idf = _2idf(m_operand_stack.pop());
        assignValueToIdf(assign_idf, assign_v, false);

    } else if (ctxt == STMT_SCOPE_DECLARE_ASSIGN) {
        if (m_operand_stack.count() < 3) { // base + 2
            error("two opds need to form a assign statement, while \"%d\" gotten", m_operand_stack.count());
            printErrorLocation();
            exit(E_GRAMMAR);
        } else {
            m_expect_char_types = {};
        }

        Operand opdv = m_operand_stack.pop();
        double assign_v = _isidf(opdv)? getIdfValue(opdv.str): opdv.num;
        std::string assign_idf = _2idf(m_operand_stack.pop());
        assignValueToIdf(assign_idf, assign_v, true);

    } else if (ctxt == STMT_IF_COND) {
        print("TODO");
        exit(0);
    } else {
        print("TODO");
        exit(0);
    }

    print("handleSemicolon: clean context stack ...");
    while(!m_ctxt_stack.empty() && (ctxt = m_ctxt_stack.pop()) != CTXT_BGN) {
        print("m_ctxt_stack.pop %s", getContextTypeStr(ctxt));
    }

    // reset everything
    m_id = "";
    m_id_spc_occurred = false;
    m_lp_count = 0;
    m_expect_char_types = {};

    pushContextBGNToContextStack();

    m_stmt_assign_key = "";

    while (!m_operand_stack.empty() && m_operand_stack.top().stmt_count == m_stmt_count)
        m_operand_stack.pop();

    if (m_operand_stack.count() > 2) { // base + 1
        error("more than 2 (base + real last opd) operand left");
        dia("handleSemicolon");
        printErrorLocation();
        exit(E_DEV_ERROR);
    }

    dia("handleSemicolon:+");

    return true;
}

void ArithmeticExpression::pushContextBGNToContextStack() {
    print("pushContextBGNToContextStack ...");
    bool top_already_ctxt_non = !m_ctxt_stack.empty() && m_ctxt_stack.top() == CTXT_BGN;
    if (!top_already_ctxt_non) {
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

        if (ctxt == CTXT_BGN || ctxt == STMT_IF_BLOCK) {
            if (m_state == ST_IDF && (m_state_prev == ST_BGN || m_state_prev == ST_IDF)) { // more restrict grammer
                // treat it as assign statement
                m_ctxt_stack.push(STMT_DECLARE_ASSIGN);
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

bool ArithmeticExpression::handleLeftCurlyBrace() {
    dia("-:handleLeftCurlyBrace");

    ++m_token_idx;

    ContextType ctxt = m_ctxt_stack.top();

    if (ctxt == FUNCTION_CALL) {
        ++m_lcb_count;

        // merge m_pre_defined_idf_map into m_idf_map;
        // why put here?
        // we wanna treat m_pre_defined_idf_map as local variables, mainly for
        // 1) passing function arguments which need treated as local vars
        // 2) passing built-in { built_in_idf:value } to program
        // 3) with right scope (depth + lcb count)
        // and now, with m_dpeth and m_lcb_count properly set, we are in right
        // scope so assignValueToIdf works as expected.

        for (const auto& item : m_pre_defined_idf_map) {
            assignValueToIdf(item.first, item.second, false);
        }

        pushContextBGNToContextStack();
        m_state_next_certained = ST_BGN;
        return true;
    }

    if (ctxt == FUNCTION_ARGS) {
        m_ctxt_stack.push(FUNCTION_BLOCK);
        m_expect_char_types = {};

        // TODO register function definition meta
        // record from definition's '{', so we can run from here
        int exec_pos = m_parse_pos;
        m_function_map[m_function_name][0] = exec_pos;
        m_function_exec_pos_name_map[m_parse_pos] = m_function_name;

        if (!findBalancedRightCurlyBraceAndSkipToIt(true /*skipRigthCurlyBraceAlong*/)) {
            error("function: no balanced ending-block symbol '}' found");
            return false;
        }

        // '}*' end_pos (exclusive)
        //   ^
        m_function_map[m_function_name][1] = m_parse_pos + 1;

        m_id = "";
        pushContextBGNToContextStack();
        m_state_next_certained = ST_BGN;
        return true;
    }

    if (m_lp_count > 0) {
        error("'{' not allowed inside ()");
        printErrorLocation();
        exit(E_SYNTAX);
    }

    print("handleLeftCurlyBrace: ctxt=%d", ctxt);

    print("handleLeftCurlyBrace: calculate all oprs");

    // NOTE: process case "if|elif idf {}" specifically
    // get opd stack well-prepared before calculate with opr stack.
    if (m_id == "if" || m_id == "elif") {
        if (m_operand_stack.empty()) {
            error("no condtion found for \"%s\"", m_id.c_str());
            printErrorLocation();
            exit(E_GRAMMAR);
        }
    }

    if (m_operand_stack.empty() && !m_id.empty() && ( m_id != "else" )) {
        pushOpdIdfValue(m_id);
        m_id = ""; // reset after consumed
    }

    int top;
    while (!m_operator_stack.empty() && (top = m_operator_stack.top().v) != LOWEST_PRIO_OP) {
        if (!calculate(top)) return false;
        m_operator_stack.pop();
    }

    dia("-:handleLeftCurlyBrace:after-calculate");

    if (m_id_spc_occurred) m_id_spc_occurred = false;

    m_state_next_certained = ST_BGN;

    // NOTE: clear m_stmt_if_cond
    // for those key larger than m_lcb_count, clear the value
    // TODO: maybe just clean vector value is good enough, for later reuse.

    for (auto it = m_stmt_if_cond.begin(); it != m_stmt_if_cond.end();) {
        if (it->first > m_lcb_count) {
            print("NOTE: erase m_stmt_if_cond[%d], coz m_lcb_count=%d", it->first, m_lcb_count);
            it = m_stmt_if_cond.erase(it);
        } else {
            ++it;
        }
    }

    // check conditions
    std::vector<int>& v = m_stmt_if_cond[m_lcb_count];

    if (ctxt == STMT_IF_COND) {
        int cond = _2n(m_operand_stack.pop()) != 0? 1: 0;
        v.push_back(cond);

        if (cond != 0) {
            m_ctxt_stack.push(STMT_IF_BLOCK);
            print("TODO: to process if-block");
        } else {
            print("TODO: to ignore if-block, just skip to balanced '}', no syntax check for skipped content");
            if (!findBalancedRightCurlyBraceAndSkipToIt()) {
                error("if: no balanced } found");
                return false;
            }
        }
    } else if (ctxt == STMT_ELIF_COND) {
        if (v.size() == 0) {
            error("elif statement must follow if-statement");
            return false;
        }

        double elif_cond = _2n(m_operand_stack.pop()) !=0? 1: 0;

        bool pre_cond_all_zero = true;
        for (int i = 0; i < v.size(); i++) {
            if (v[i] != 0) {
                pre_cond_all_zero = false;
                break;
            }
        }

        // always record elif_cond
        v.push_back(elif_cond);

        int cond = (pre_cond_all_zero == true && elif_cond != 0)? 1: 0;

        if (cond != 0) {
            m_ctxt_stack.push(STMT_ELIF_BLOCK);
            print("TODO: to process elif-block");
        } else {
            print("TODO: to ignore elif-block, just skip to balanced '}', no syntax check for skipped content");
            if (!findBalancedRightCurlyBraceAndSkipToIt()) {
                error("elif: no balanced } found");
                return false;
            }
        }

    } else {
        dia("xxxxxxxxxxxxxxxx");
        if (m_id == "else") {
            if (v.size() == 0) {
                error("else statement must follow if-statement");
                return false;
            }

            if (m_id_spc_occurred) m_id_spc_occurred = false;

            bool pre_cond_all_zero = true;
            for (int i = 0; i < v.size(); i++) {
                if (v[i] != 0) {
                    pre_cond_all_zero = false;
                    break;
                }
            }

            if (pre_cond_all_zero == true) {
                m_ctxt_stack.push(STMT_ELSE_BLOCK);
                print("TODO: to process else-block");
            } else {
                print("TODO: to ignore else-block, just skip to balanced '}', no syntax check for skipped content");
                if (!findBalancedRightCurlyBraceAndSkipToIt()) {
                    error("else: no balanced } found");
                    return false;
                }
            }
        } else {
            if (m_id.empty()) {
                // ok
            } else {
                error("unsupported id:\"%s\" before {", m_id.c_str());
                return false;
            }
        }
    }

    dia("handleLeftCurlyBrace:+");

    ++m_lcb_count;
    return true;
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
        print("findBalancedRightCurlyBraceAndSkipToIt: c='%c'", c);

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

    print("findBalancedRightCurlyBraceAndSkipToIt: '}' found at:%d, skipRigthCurlyBraceAlong:%d", (int)m_parse_pos, skipRigthCurlyBraceAlong);
    if (!skipRigthCurlyBraceAlong) {
        --m_parse_pos;          // restore '}', so parse get chance to process it
    }

    __log_e = m_parse_pos;
    print("skip: \"%.*s\"", (__log_e + 1) /* include char at __log_e*/ - __log_b, &m_arithmetic_expression[__log_b]);

    return found;
}

bool ArithmeticExpression::handleRightCurlyBrace() {
    print("handleRightCurlyBrace");

    ++m_token_idx;

    if (m_lp_count > 0) {
        error("'}' not allowed inside ()");
        handleError();
        exit(E_SYNTAX);
    }

    ////////////////////////////////////////////////////////////////
    // support syntax: "idf}", so last statement of block can ignore ';'
    // TODO: maybe normalize syntax to *always && must* use ';' as statement terminal
    //
    // x=2; if 1{y=x} print(y)
    //              ^^

    if (m_state == ST_IDF) {
        if (m_id_spc_occurred) m_id_spc_occurred = false;
        if (!m_id.empty()) {
            pushOpdIdfValue(m_id);
            m_id = ""; // reset after consumed
        }
    }

    int top;
    while (!m_operator_stack.empty() && (top = m_operator_stack.top().v) != LOWEST_PRIO_OP) {
        if (!calculate(top)) return false;
        m_operator_stack.pop();
    }

    ContextType ctxt = m_ctxt_stack.top();


    if (ctxt == STMT_DECLARE_ASSIGN_VALUE) {
        if (m_operand_stack.count() < 3) { // base + 2
            error("two opds need to form a assign statement, while \"%d\" gotten", m_operand_stack.count());
            printErrorLocation();
            exit(E_GRAMMAR);
        }

        Operand opdv = m_operand_stack.pop();
        double assign_v = _isidf(opdv)? getIdfValue(opdv.str): opdv.num;

        std::string assign_idf = _2idf(m_operand_stack.pop());
        assignValueToIdf(assign_idf, assign_v, false);

    } else if (ctxt == STMT_SCOPE_DECLARE_ASSIGN) {
        if (m_operand_stack.count() < 3) { // base + 2
            error("two opds need to form a assign statement, while \"%d\" gotten", m_operand_stack.count());
            printErrorLocation();
            exit(E_GRAMMAR);
        }

        Operand opdv = m_operand_stack.pop();
        double assign_v = _isidf(opdv)? getIdfValue(opdv.str): opdv.num;

        std::string assign_idf = _2idf(m_operand_stack.pop());
        assignValueToIdf(assign_idf, assign_v, true);
    } else {}

    ////////////////////////////////////////////////////////////////

    // firstly, ensure balanced left curly brace exists
    if (m_lcb_count == 0) {
        error("no balanced left curly brace found");
        printErrorLocation();
        exit(E_GRAMMAR);
    }

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

    --m_lcb_count;

    // chars left, goto st_bgn
    if (m_parse_pos < m_arithmetic_expression.size()-1) {
        m_state_next_certained = ST_BGN;
    }

    pushContextBGNToContextStack();

    return true;
}

bool ArithmeticExpression::handleColon() {

    // ++m_token_idx;

    print("handleColon: expects '=' immediately");
    return true;
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
    m_operand_stack.push({ m_token_idx, m_stmt_count, OPD_IDF, 0, idf });
}

void ArithmeticExpression::pushOpdIdfValue(const std::string& idf) {
    pushOpdNum(getIdfValue(idf));
}

void ArithmeticExpression::pushOpdNum(double num) {
    m_operand_stack.push({ m_token_idx, m_stmt_count, OPD_NUM, num, "" });
}

void ArithmeticExpression::pushOpdStr(std::string str) {
    // use std::move to avoid unnecessary string copy
    m_operand_stack.push({ m_token_idx, m_stmt_count, OPD_STR, 0, std::move(str) });
}

bool ArithmeticExpression::_isbase(const Operand& item) {
    return item.type == OPD_STACK_BOTTOM_BASE;
}

bool ArithmeticExpression::_isidf(const Operand& item) {
    return item.type == OPD_IDF;
}

bool ArithmeticExpression::_isn(const Operand& item) {
    return item.type == OPD_NUM;
}

bool ArithmeticExpression::_iss(const Operand& item) {
    return item.type == OPD_STR;
}

std::string ArithmeticExpression::_2idf(const Operand& item) {
    return item.str;
}

double ArithmeticExpression::_2n(const Operand& item) {
    return item.num;
}

std::string ArithmeticExpression::_2s(const Operand& item) {
    return item.str;
}

// NOTE: for easier maintainance, only modify m_operand_stack when calculating, *KEEP m_operator_stack STAY*.
bool ArithmeticExpression::calculate(int opr)
{
    print("calculate: opr=<%c %d>", opr, opr);

    if ( '(' == opr || ')' == opr || LOWEST_PRIO_OP == opr ) {
        print("do nothing for opr='<%c %d>'", opr, opr);
        return true;
    }

    if ('f' == opr) {
        print("opr is 'f'");
        int fni = m_operator_stack.top(-1).v; // get fni
        int fncmcnt = m_fncmcnt_stack.top();
        FunctionMeta fm = m_function_meta[fni];

        print("calculate: fni=%d, fm.args_va=%d fm.args_count=%d fncmcnt=%d", fni, fm.args_va, fm.args_count, fncmcnt);

        if (fm.args_va) {
            if (fncmcnt < fm.args_count-1) {
                error("function: '%s' expect at least %d arguments, while %d given. (fncmcnt=%d)", fm.name, fm.args_count, fncmcnt+1, fncmcnt);
                printErrorLocation();
                exit(E_GRAMMAR);
            }
        } else {
            if (fncmcnt != fm.args_count-1) {
                error("function: '%s' expect %d arguments, while %d given. (fncmcnt=%d)", fm.name, fm.args_count, fncmcnt+1, fncmcnt);
                printErrorLocation();
                exit(E_GRAMMAR);
            }
        }

        switch(fni) {

        case FABS: {
            pushOpdNum(fabs(_2n(m_operand_stack.pop())));
            break;
        }
        case FMOD: {
            Operand r = m_operand_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Operand l = m_operand_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum(fmod(l_opd, r_opd));
            break;
        }
        case REMAINDER: {
            Operand r = m_operand_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Operand l = m_operand_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum(remainder(l_opd, r_opd));
            break;
        }
        case CEIL: {
            pushOpdNum(ceil(_2n(m_operand_stack.pop())));
            break;
        }
        case FLOOR: {
            pushOpdNum(floor(_2n(m_operand_stack.pop())));
            break;
        }
        case ROUND: {
            pushOpdNum(round(_2n(m_operand_stack.pop())));
            break;
        }
        case TRUNC: {
            pushOpdNum(trunc(_2n(m_operand_stack.pop())));
            break;
        }

        case POW: {
            Operand r = m_operand_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Operand l = m_operand_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum(pow(l_opd, r_opd));
            break;
        }
        case SQRT: {
            double opd = _2n(m_operand_stack.pop());
            if (opd < 0) {
                print("sqrt expects unsigned operand, while %f gotten", opd);
                return false;
            }
            pushOpdNum(sqrt(opd));
            break;
        }
        case CBRT: {
            pushOpdNum(cbrt(_2n(m_operand_stack.pop())));
            break;
        }
        case HYPOT: {
            Operand r = m_operand_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Operand l = m_operand_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum(hypot(l_opd, r_opd));
            break;
        }
        case EXP: {
            pushOpdNum(exp(_2n(m_operand_stack.pop())));
            break;
        }
        case EXP2: {
            pushOpdNum(exp2(_2n(m_operand_stack.pop())));
            break;
        }
        case EXPM1: {
            pushOpdNum(expm1(_2n(m_operand_stack.pop())));
            break;
        }
        case LOG: {
            pushOpdNum(log(_2n(m_operand_stack.pop())));
            break;
        }
        case LOG10: {
            pushOpdNum(log10(_2n(m_operand_stack.pop())));
            break;
        }
        case LOG2: {
            pushOpdNum(log2(_2n(m_operand_stack.pop())));
            break;
        }
        case LOG1P: {
            pushOpdNum(log1p(_2n(m_operand_stack.pop())));
            break;
        }

        case SIN: {
            pushOpdNum(sin(_2n(m_operand_stack.pop())));
            break;
        }
        case COS: {
            pushOpdNum(cos(_2n(m_operand_stack.pop())));
            break;
        }
        case TAN: {
            pushOpdNum(tan(_2n(m_operand_stack.pop())));
            break;
        }
        case ASIN: {
            pushOpdNum(asin(_2n(m_operand_stack.pop())));
            break;
        }
        case ACOS: {
            pushOpdNum(acos(_2n(m_operand_stack.pop())));
            break;
        }
        case ATAN: {
            pushOpdNum(atan(_2n(m_operand_stack.pop())));
            break;
        }
        case ATAN2: {
            Operand r = m_operand_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Operand l = m_operand_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum(atan2(l_opd,r_opd));
            break;
        }

        case SINH: {
            pushOpdNum(sinh(_2n(m_operand_stack.pop())));
            break;
        }
        case COSH: {
            pushOpdNum(cosh(_2n(m_operand_stack.pop())));
            break;
        }
        case TANH: {
            pushOpdNum(tanh(_2n(m_operand_stack.pop())));
            break;
        }
        case ASINH: {
            pushOpdNum(asinh(_2n(m_operand_stack.pop())));
            break;
        }
        case ACOSH: {
            pushOpdNum(acosh(_2n(m_operand_stack.pop())));
            break;
        }
        case ATANH: {
            pushOpdNum(atanh(_2n(m_operand_stack.pop())));
            break;
        }

        case FMIN: {
            double x = _2n(m_operand_stack.pop());
            double y = _2n(m_operand_stack.pop());
            pushOpdNum(fmin(y,x));
            break;
        }
        case FMAX: {
            double x = _2n(m_operand_stack.pop());
            double y = _2n(m_operand_stack.pop());
            pushOpdNum(fmax(y,x));
            break;
        }
        case FDIM: {
            double x = _2n(m_operand_stack.pop());
            double y = _2n(m_operand_stack.pop());
            pushOpdNum(fdim(y,x));
            break;
        }
        case COPYSIGN: {
            double x = _2n(m_operand_stack.pop());
            double y = _2n(m_operand_stack.pop());
            pushOpdNum(copysign(y,x));
            break;
        }
        case ISFINITE: {
            pushOpdNum(isfinite(_2n(m_operand_stack.pop())));
            break;
        }
        case ISINF: {
            pushOpdNum(isinf(_2n(m_operand_stack.pop())));
            break;
        }
        case ISNAN: {
            pushOpdNum(isnan(_2n(m_operand_stack.pop())));
            break;
        }
        case ISNORMAL: {
            pushOpdNum(isnormal(_2n(m_operand_stack.pop())));
            break;
        }
        case SIGNBIT: {
            pushOpdNum(signbit(_2n(m_operand_stack.pop())));
            break;
        }

        case INT: {
            pushOpdNum((int)_2n(m_operand_stack.pop()));
            break;
        }
        case MAX: {
            int args_count = fncmcnt + 1;
            double v = FLT_MIN;
            int opd;
            while (!m_operand_stack.empty() && args_count--) {
                opd = _2n(m_operand_stack.pop());
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
            while (!m_operand_stack.empty() && args_count--) {
                opd = _2n(m_operand_stack.pop());
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
            if (m_operand_stack.count() < 3) { // base + 2
                error("cpm(v1,v2,v3) expects three operands, while %d left", m_operand_stack.count());
                printErrorLocation();
                exit(E_GRAMMAR);
            }
            double v3 = _2n(m_operand_stack.pop());
            double v2 = _2n(m_operand_stack.pop());
            double v1 = _2n(m_operand_stack.pop());
            double v = v1 != 0? v2: v3;
            pushOpdNum(v);
            break;
        }

        case PRINT: {
            if (m_operand_stack.empty()) { // "print()"
                printf("\n");
                break;
            }
            const Operand& opd = m_operand_stack.pop();
            if (_isidf(opd)) {
                printf("%g\n", getIdfValue(opd.str)); // current only support double
            } else if (_isn(opd)) {
                printf("%g\n", _2n(opd));
            } else if (_iss(opd)) {
                printf("%s\n", _2s(opd).c_str());
            } else {
                error("un-specially-processed Operand (type:%d) called by PRINT", opd.type);
            }
            break;
        }

        case PRINTF: {
            // TODO: to support real printf
            const Operand& opd = m_operand_stack.pop();
            if (_isidf(opd)) {
                printf("%g", getIdfValue(opd.str)); // current only support double
            } else if (_isn(opd)) {
                printf("%g", _2n(opd));
            } else if (_iss(opd)) {
                printf("%s", _2s(opd).c_str());
            } else {
                error("un-specially-processed Operand (type:%d) called by PRINT", opd.type);
            }
            break;
        }

        default:
            error("unsupported fni: %d", fni);
            return false;
        }

        // NOTE: always pop when fn has successfully been processed
        m_fncmcnt_stack.pop();
        return true;
    } else if ('F' == opr) {
        int exec_pos = m_operator_stack.top(-1).v; // function exec_pos
        const std::string& fnn = m_function_exec_pos_name_map[exec_pos];
        print("call self-defined function: exec_pos=%d fnn:\"%s\"", exec_pos, fnn.c_str());
        std::string scope_name_old = scope_name;
        scope_name = scope_name+"/"+fnn;
        call(fnn);
        scope_name = scope_name_old;
        return true;
    } else {
        switch (opr) {
        case '^': {
            Operand r = m_operand_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Operand l = m_operand_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum( pow(l_opd,r_opd) ); break;
        }
        case '+': {
            Operand r = m_operand_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Operand l = m_operand_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum( l_opd + r_opd ); break;
        }
        case '-': {
            Operand r = m_operand_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Operand l = m_operand_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum( l_opd - r_opd ); break;
        }
        case '*': {
            Operand r = m_operand_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Operand l = m_operand_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum( l_opd * r_opd ); break;
        }
        case '/': {
            Operand r = m_operand_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            if ( 0 == r_opd ) {
                error("divide 0");
                return false;
            }
            Operand l = m_operand_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum( l_opd / r_opd);
            break;
        }
        case '>': {
            Operand r = m_operand_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Operand l = m_operand_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum(l_opd > r_opd? 1: 0);
            break;
        }
        case '<': {
            Operand r = m_operand_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Operand l = m_operand_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum(l_opd < r_opd? 1: 0);
            break;
        }
        case 'e': {             // "=="
            Operand r = m_operand_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Operand l = m_operand_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            print("e: l_opd=%g r_opd=%g ?=%d", l_opd, r_opd, l_opd == r_opd);
            pushOpdNum(l_opd == r_opd? 1: 0);
            break;
        }
        case 'a': {             // "&&" and
            Operand r = m_operand_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Operand l = m_operand_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum(l_opd !=0 && r_opd != 0? 1: 0);
            break;
        }
        case 'o': {             // "||" and
            Operand r = m_operand_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
            Operand l = m_operand_stack.pop(); double l_opd = _isidf(l)? getIdfValue(l.str): l.num;
            pushOpdNum(l_opd !=0 || r_opd != 0? 1: 0);
            break;
        }
        case '!': {
            Operand r = m_operand_stack.pop(); double r_opd = _isidf(r)? getIdfValue(r.str): r.num;
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
//     if (m_function_map.find(function_name) == m_function_map.end()) {
//         error("no such self-defined function: \"%s\"", function_name.c_str());
//         exit(E_DEV_ERROR);
//     }

//     // TODO: m_xx vars sucks for parsing block

//     // TODO: use call_stack to remember current exec_pos
//     // switch to function exec_pos

//     int parse_pos_to_back = m_parse_pos + 1;
//     print("remember parse_pos_to_back:%d, start:\"%s\"", parse_pos_to_back, &m_arithmetic_expression[parse_pos_to_back]);

//     // jump to function
//     m_state = ST_BGN;
//     m_parse_pos = m_function_map[function_name][0];
//     int end_pos = m_function_map[function_name][1];

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
    print("CALL: function_name = %s", function_name.c_str());

    if (m_function_map.find(function_name) == m_function_map.end()) {
        error("no such self-defined function: \"%s\"", function_name.c_str());
        exit(E_DEV_ERROR);
    }

    // TODO: use ArithmeticExpression recursively
    // so run state of each block is stored automatically

    int begin = m_function_map[function_name][0];
    int end = m_function_map[function_name][1];

    // TODO: support multiple type for value part
    std::unordered_map<std::string,double> argm = {};

    for (int i = (int)m_function_args.size() - 1; i >= 0; i--) {
        std::string& key = m_function_args[i];

        if (m_operand_stack.empty()) {
            error("function %s expects %zu args, current argv[%d] name:%s, while no operand any more", function_name.c_str() , m_function_args.size(), i, key.c_str());
            exit(E_INPUT_ERROR);
        }

        argm[key] = _2n(m_operand_stack.pop());
        print("argm[%s] = %g", key.c_str(), argm[key]);
    }
    m_function_args = {};       // reset after consumed

    ArithmeticExpression fnblock(m_arithmetic_expression,
                                 getDepth()+1,
                                 getLCBCount(),
                                 getIdfMap(),        // FIXME: maybe just leak global scope idf to valid idf value pollution
                                 argm,
                                 FUNCTION_CALL,
                                 g_print_enable
        );
    fnblock.setMeta(
        getFunctionMap(),
        getFunctionExecPosNameMap(),
        getIdfScopeDeclared() // FIXME: maybe just leak global scope idf to valid idf value pollution
        );

    g_depth = getDepth()+1;
    if (! fnblock.parse(begin, end)) {
        exit(E_EVALUATE);
    }
    --g_depth;
    return true;
}

void ArithmeticExpression::setMeta(
    std::unordered_map<std::string,std::array<int,2>>& function_map,
    std::unordered_map<int,std::string>& function_exec_pos_name_map,
    std::unordered_map<int, std::unordered_map<std::string,int>>& idf_scope_declared
    )
{
    m_function_map = function_map;
    m_function_exec_pos_name_map = function_exec_pos_name_map;
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

    // print("parse \"%d %d %s\"", m_parse_pos, end_pos, m_arithmetic_expression.c_str());
    print("PARSE: \"%.*s\"\n", end_pos - m_parse_pos, &m_arithmetic_expression[m_parse_pos]);

    while ( m_parse_pos < end_pos ) {
        const char *start = &m_arithmetic_expression[m_parse_pos];
        const char *end = strchr(start, '\n');
        if (!end) end = strchr(start, '\0');

        const char& c = m_arithmetic_expression.at(m_parse_pos);

        if (c=='\n') {
            ++m_ln_count;
            g_ln_count = m_ln_count;
        }

        print("> \"%.*s\"", (int)(end - start), start);

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

    return handleInputAllPassed();
}

bool ArithmeticExpression::handleInputAllPassed()
{
    dia("-:handleInputAllPassed");

    ContextType ctxt = m_ctxt_stack.top();

    switch(ctxt) {
    case STMT_DECLARE_ASSIGN:
        error("invalid assign statement. syntax: \"idf = ...;\"");
        printErrorLocation();
        exit(E_GRAMMAR);
    default:
        print("handleInputAllPassed ignore context type: %s", getContextTypeStr(ctxt));
        break;
    }

    if (!m_expect_char_types.empty()) {
        dia("handleInputAllPassed");
        error("all input parsed, but m_expect_char_types not empty");
        printErrorLocation();
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
        return false;
    }

    // check whether parenthesis matches
    if ( m_lcb_count_init != m_lcb_count ) {
        error("open left curly brace exists, count=\"%d\"", m_lcb_count);
        return false;
    }

    // fulfill m_rpn_expression using operator stack
    int top;
    while ( ! m_operator_stack.empty() && (top = m_operator_stack.top().v) != LOWEST_PRIO_OP) {
        dia("-:post-process");
        if (! calculate(top)) return false;
        // always pop to go on to next opr
        m_operator_stack.pop();
        if (m_ctxt_stack.top() == CTXT_EXP) {
            m_ctxt_stack.pop();
        }
        dia("post-process:+");
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

    print("m_operand_stack.count() = %d", m_operand_stack.count());
    dia("-:handleInputAllPassed:1");

    ctxt = m_ctxt_stack.top();

    if (ctxt == STMT_DECLARE_ASSIGN_VALUE) {
        if (m_operand_stack.count() < 3) { // base + 2
            error("two opds need to form a assign statement, while \"%d\" gotten", m_operand_stack.count());
            printErrorLocation();
            exit(E_GRAMMAR);
        }

        Operand opdv = m_operand_stack.pop();
        double assign_v = _isidf(opdv)? getIdfValue(opdv.str): opdv.num;

        std::string assign_idf = _2idf(m_operand_stack.pop());
        assignValueToIdf(assign_idf, assign_v, false);
    }

    print("arithmetic expression successfully parsed");
    dia("handleInputAllPassed:+");

    return true;
}
