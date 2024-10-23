#include "MemManage.hpp"
#include <iostream>
#include <cstdio>
#include <string>

using AutomaticMemory::heap; 

struct A {
    A() : str{"Hello World!"} { }
    A(std::string const& str) : str(str) {}
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
    AutomaticMemory::string str{"Hello world!"};
    AutomaticMemory::wstring wstr{L"Hello Wide World!"}; 

    str.resize(123);
    AutomaticMemory::vector<int> vec{1, 2, 3}; 
    vec.resize(1024 * 1024);
    std::cout << str << std::endl; 
    std::cout << heap.used_memory(AutomaticMemory::SizeTypes::Kilobyte) << std::endl; 
    std::cout << vec[1] << std::endl;
    
    std::wcout << wstr << std::endl;
    wstr.resize(64);

    getchar();
    return 0; 
} 