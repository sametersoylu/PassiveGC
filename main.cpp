#include "MemManage.hpp"
#include <iostream>
#include <cstdio>
#include <stdexcept>

using AutomaticMemory::heap; 

struct A {
    A() : str{"Hello World!"} { }
    A(std::string const& str) : str(str) {
        throw std::runtime_error("some error");
    }
    std::string str; 
};

auto test() {
    auto x = heap.allocate_constructed<A>("Hi World!");
    
    x.Error().dont_exit();

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