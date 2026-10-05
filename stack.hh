#ifndef INCLUDE_STACK_HPP
#define INCLUDE_STACK_HPP

#include <vector>
#include <algorithm>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include <string>
#include <type_traits>

#define STACK_SP_INIT -1
template <class T>
class Stack {
public:
    Stack(int size = 1) { init(size); }
    ~Stack() { destory(); }

    T& top(int step=0) { return m_tunk.at(m_sp+step); }
    const T& top(int step=0) const { return m_tunk.at(m_sp+step); }
    void push(T val);
    T pop() { return m_tunk[m_sp--]; }
    bool empty() const { return m_sp == STACK_SP_INIT; }

    void dia(int enable=0){
        if (!enable) return;
        printf("[");
        for (int i=0; i< m_sp+1; i++) {
            T& x = m_tunk[i];
            printf("<%f,%d,'%c'> ", x,x,x);
        }
        printf("]");
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
