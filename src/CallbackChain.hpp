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