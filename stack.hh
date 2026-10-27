#ifndef INCLUDE_STACK_HPP
#define INCLUDE_STACK_HPP

#include <vector>
#include <algorithm>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include <string>
#include <type_traits>

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
    STMT_SCOPE_DECLARE_ASSIGN,

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

#endif /* INCLUDE_STACK_HPP */
