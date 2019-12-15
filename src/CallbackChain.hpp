#pragma once

template<typename ChainID, typename ... Args>
class CallbackChain{
public:
    using func = void(*)(Args...);
    CallbackChain(func f);
    static void fire(Args... args);
protected:
    static CallbackChain<ChainID, Args...>* last;
    CallbackChain<ChainID, Args...>* next;
    func f;
};

// ----- Implementation -----

template<typename ChainID, typename ... Args>
CallbackChain<ChainID, Args...>* CallbackChain<ChainID, Args...>::last = nullptr;

template<typename ChainID, typename ... Args>
CallbackChain<ChainID, Args...>::CallbackChain(func f) : next(last), f(f){
    last = this;
}

template<typename ChainID, typename ... Args>
void CallbackChain<ChainID, Args...>::fire(Args... args){
    CallbackChain<ChainID, Args...>* obj = last;
    while(obj){
        obj->f(args...);
        obj = obj->next;
    }
}
