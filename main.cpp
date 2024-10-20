#include "MemManage.hpp"
#include <iostream>
#include <cstdio>

using AutomaticMemory::heap; 

struct A {
    A() : str{"Hello World!"} { }
    std::string str; 
};

auto test() {
    auto x = heap.allocate<A>(1024 * 1024 * 10);
    std::cout << "Press enter to move out pointer." << std::endl;
    getchar();
    return std::move(x);
}

auto foo() {
    auto x = test();
    std::cout << x->str << std::endl; // validate pointer still lives.
    std::cout << "Pointer now belongs to foo(). When foo ends, pointer will die. Press enter to kill pointer." << std::endl;
    
    getchar();
} // now the allocated space is released.

auto main() -> int {
    foo();
    getchar();
    return 0; 
} 