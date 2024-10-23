#pragma once

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <type_traits>
#include <utility>
#include <vector>
#include <algorithm>
#include <string>

/* 
    This namespace provides passive automatic memory management
    and memory safety (not security) features for heap allocated
    objects.
*/
namespace AutomaticMemory {
    /* 
        This class is an interface class for Pointer types.
        Essentially, this provides a common ground for heap::pointer
        and stack::pointer (if stack gets implemented).
    */
    namespace Errors {
        class base_error {
            public: 
            base_error() = default;
            base_error(base_error const&) = delete; 
            base_error(base_error&& other) : message(other.message), error_code(other.error_code), exit(other.exit) {
                other.dont_exit();
            }
            base_error(std::string const& error_message, int error_code = -1) : message(error_message), error_code(error_code), exit(true) { 
                std::cerr << "Error: \"" << message << "\" at line " << __LINE__ << " in file \"" << __FILE_NAME__ << "\"." << std::endl; 
            }
            ~base_error() {
                if (exit) {
                    std::exit(error_code);
                }
            }
            base_error& operator=(base_error&& other) {
                message = std::move(other.message);
                error_code = std::move(other.error_code);
                other.dont_exit();
                return *this;
            }
            void dont_exit() const {
                exit = false; 
            }
            std::string what() const {
                return message; 
            }
            protected: 
            std::string message{};
            int error_code = -1; 
            mutable bool exit = false;
        };

        class BadConstruct : public base_error {
            public: 
            BadConstruct() : base_error("An error occurred while constructing object.", -2) {}
            BadConstruct(std::string const& message) : base_error(message, -2) {}
            BadConstruct(BadConstruct&& other) : base_error(other.message, other.error_code) { other.dont_exit(); } 
        };

        class IndexOutOfBounds : public base_error {
            public: 
            IndexOutOfBounds() : base_error("Index out of bounds", -3) {}
            IndexOutOfBounds(std::string const& message) : base_error(message, -3) {}
            IndexOutOfBounds(IndexOutOfBounds&& other) : base_error(other.message, other.error_code) { other.dont_exit(); }
        };

        
    }

    template<typename T_, typename D_>
    class base_pointer {
        public: 
        /* 
            This structure SHOULD NOT be copied. Copying heap allocated pointers around
            is not a good idea if you ARE NOT counting references to free up the memory.
            It can cause dangling pointers OR free after use. But also not being able to
            pass memory that is allocated is not a good idea. So we transfer the pointer 
            instead. Please check Heap::Pointer::Pointer(Pointer&& other) to see move 
            (transfer) operations.   
        */
        base_pointer(base_pointer const&) = delete; 
        /* 
            Resource Acquisition is Initialization. 
            RAII Ensures that the object is VALID at any given time of it's lifetime. IF 
            object is alive, and still accessable, it ensures the data or space that object
            uses is also available. And also ensures the memory is released after it's use
            is complete. 
        */
        ~base_pointer() {
            free(); 
        }

        // Arrow operator for pointer access.
        T_ * operator->() {
            return m_Ptr; 
        }
        // Dereferencing operator for pointer access.
        T_& operator*() {
            return *m_Ptr; 
        }
        protected:
        // Default initializer. This class should only be constructed with a pointer or moved.
        base_pointer(T_ * pointer) : m_Ptr{pointer} {}
        // Default move constructor. 
        base_pointer(T_*&& pointer, bool&& freed) : m_Ptr(pointer), freed(freed) {}
        bool freed = false; 
        T_ * m_Ptr;
        private:
        /* 
            Freeing is handled by the derived class. Free should never be used by hand. 
        */
        void free() {
            if (freed) return; 
            std::cout << "Free called!" << std::endl;
            static_cast<D_*>(this)->free_impl();
            freed = true; 
        }
    };
    

    enum class SizeTypes : size_t {
        Byte     = 1,
        Kibibyte = 1024,
        Mibibyte = 1048576,
        Gibibyte = 1073741824,
        Kilobyte = 1000,
        Megabyte = 1000000,
        Gigabyte = 1000000000,
    }; 
    
    /*
        Global heap class. 
        Memory is handled in this way;
        We have a vector (or list) of segments. A segment is an individual memory 
        space lives parallel to other memory spaces (segments).
        Eg: 
        Vector<Segment>::Begin()---Segment------Segment------Segment------Segment---Vector<Segment>::End()
                                      |            |            |            |
                                    Raw mem     Raw mem      Raw mem      Raw mem
                                    sizeof(32)  sizeof(1024) sizeof(16)   sizeof(1024^2)
                                    bytes       bytes        bytes        bytes

        As the diagram shows, they live parallel to each other so no memory collision or overlap. (Except where memory 
        overflow, check compiler specifications) The main memory management is provided by standard library since the Segment 
        is a fancy wrapper for std::vector<unsigned char> and Heap is a fancy wrapper and manager for std::vector<Segment>.
        So it's as much memory safe as std::vector.   
    */
    class Heap {
    private:
        /* Segment
            This class represents a memory segment. Each segment is a memory space.
            std::vector<unsigned char> is the holder for raw memory. 
        */
        struct Segment {
            Segment() : size(0) {}
            Segment(size_t size) : size{size} { m_Memory.reserve(size); }
            void reserve(size_t const& size) { m_Memory.reserve(size);  this->size = size; }
            void resize(size_t const& size) { m_Memory.resize(size);  this->size = size; }
            bool operator==(Segment const& other) const { return m_Memory == other.m_Memory and size == other.size; }
            auto begin() { return m_Memory.begin(); }
            auto end() { return m_Memory.end(); }
            std::vector<unsigned char> m_Memory;
            size_t size;
        };
        /* 
            Low level allocation.
            We create a segment in the segments vector with a provided size.  As the segment gets constructed,
            it reserves the requested memory size using std::vector<unsigned char>::reserve();
            Returns reference to allocated segment. 
        */
        Segment& allocate(size_t size) {
            memory_in_use += size;
            auto& segment = m_Segments.emplace_back(Segment{size});
            return segment;
        }

        std::vector<Segment> m_Segments;
        size_t memory_in_use; 
    public:
        /*
            Heap::Pointer class template. 
            This class template provides RAII for safe pointer handling. When a pointer reaches it's end of scope,
            it will be nulled. And the resources that it was using will be returned back to OS immediately if not moved.

            If this any object holding this type is moved, it's going to transfer it's ownership to new scope.
            The object will die in the new scope's end.

            Type can be array. This type supports both array like usage and normal pointer usage.
            If array = true, subscripting operator will be enabled to supply array like usage that you can normally do with
            raw pointers.
        */
        template<typename T_, bool array>
        class Pointer : public base_pointer<T_, Pointer<T_, array>> {
            public: 
            using base_type = base_pointer<T_, Pointer<T_, array>>;
            
            Pointer() = delete;
            Pointer(Pointer const& other) = delete;
            Pointer(Pointer&& other) : memsegment(other.memsegment), owner(std::move(other.owner)), base_type{std::move(other.m_Ptr), std::move(other.freed)} { other.moved = true; other.error.dont_exit(); }

            template<bool _array = array>
            typename std::enable_if<_array, T_&>::type operator[](size_t index) {
                if (index < 0 or index > array_size) {
                     
                }
                return *(base_type::m_Ptr + index); 
            }

            Errors::base_error& Error() {
                return error;
            }

            private:
            friend class base_pointer<T_, Pointer>;
            friend class Heap; 
            
            Pointer(T_ * pointer, Heap * owner, Segment& memsegment) : base_type{pointer}, owner{owner}, memsegment(memsegment) {}

            Segment& memsegment;
            Heap * owner;
            Errors::base_error error;
            size_t array_size; 
            bool moved;

            template<bool _array = array>
            std::enable_if_t<_array, Pointer> SetSize(size_t size) {
                array_size = size;
                return *this;
            } 

            Pointer& SetError(Errors::base_error&& error) {
                error = std::move(error);
                return *this;
            }

            void free_impl() {
                if (moved) { return; }
                if constexpr (array) {
                    for (size_t i = 0; i < memsegment.size / sizeof(T_); ++i) {
                        (base_type::m_Ptr + i)->~T_();
                    }
                } else {
                    base_type::m_Ptr->~T_();
                }
                owner->free(*this);
            }

        }; 
        
        Heap() { setatexit(); }; 

        /*
            Allocates x amount of objects on the memory.
            Allocation gets the size of type and multiplies it with count of objects, then reserves the exact size on the memory. 
            When reserving is complete, reserved Segment will be allocated with default constructor of the type. If default constructor
            is not available or no constructor parameters are supplied, build will fail.
        */
        template<typename T_, typename... ConstructorArgs>
        Pointer<T_, true> allocate_constructed_n(size_t count, ConstructorArgs&&... args) {
            Segment& allocated = allocate(sizeof(T_) * count); 
            T_ * f_Ptr = static_cast<T_*>(static_cast<void*>(allocated.m_Memory.data()));
            try {
                if constexpr (sizeof...(ConstructorArgs) > 0) {
                    for (int i = 0; i < count; i++) {
                        new(f_Ptr + i) T_(std::forward<ConstructorArgs>(args)...); 
                    }
                }
                else if constexpr (std::is_default_constructible_v<T_>) {
                    for(int i = 0; i < count; i++) {
                        new(f_Ptr + i) T_{};
                    }
                } else {
                    static_assert(false, "If type is not default constructible, you have to give constructor parameters!");
                }
            }
            catch(std::exception const& e) {
                return Pointer<T_, true>{f_Ptr, this, allocated}.SetSize(count).SetError(std::move(Errors::BadConstruct{"Exception while constructing, construction stopped!\n  What: " + std::string(e.what())}));
            }

            return Pointer<T_, true>{f_Ptr, this, allocated}.SetSize(count); 
        }

        /* 
            Allocates an object on the memory.
            Allocation gets the size of type then reserves the exact size on the memory. When reserving is complete,
            reserved Segment will be allocated with default constructor.  If default constructor is not available or 
            no constructor parameters are supplied, build will fail.
        */
        template<typename T_, typename... ConstructorArgs>
        Pointer<T_, false> allocate_constructed(ConstructorArgs&&... args) {
            Segment& allocated = allocate(sizeof(T_)); 
            T_ * f_Ptr = static_cast<T_*>(static_cast<void*>(allocated.m_Memory.data()));
            try {
                if constexpr (sizeof...(ConstructorArgs) > 0) {
                    new(f_Ptr) T_{std::forward<ConstructorArgs>(args)...}; 
                }
                else if constexpr (std::is_default_constructible_v<T_>) {
                    new(f_Ptr) T_{}; 
                } else {
                    static_assert(false, "If type is not default constructible, you have to give constructor parameters!");
                }
            } catch(std::exception const& e) {
                return std::move(Pointer<T_, false>{f_Ptr, this, allocated}.SetError(std::move(Errors::BadConstruct{"Exception while constructing, construction stopped!\n  What: " + std::string(e.what())})));
            }
            return std::move(Pointer<T_, false>{f_Ptr, this, allocated}); 
        }
        /*
            Returns the estimated used memory. 
            This is not an exact measurement. This basically calculates the supposed memory usage by holding the size of each allocation.
        */
        float used_memory(SizeTypes const& convert = SizeTypes::Kibibyte) {
            return static_cast<float>(memory_in_use) / static_cast<size_t>(convert);
        }
        
        private:
        /*
            Internal free method. 
            When a pointer is ready to die, this method is called. Releases memory immediately.  
        */
        template<typename T_, bool array>
        void free(Pointer<T_, array>& pointer) {
            memory_in_use -= pointer.memsegment.size;
            auto it = std::find(m_Segments.begin(), m_Segments.end(), pointer.memsegment);
            if (it == m_Segments.end()) { return; }
            m_Segments.erase(it);
            // release back memory.
            m_Segments.shrink_to_fit();
        }
        /*
            Internal free method reserved for further implementations on different data structures
            that this class can be used as the allocator. 
        */
        void free(Segment& segment) {
            memory_in_use -= segment.size;
            auto it = std::find(m_Segments.begin(), m_Segments.end(), segment); 
            if (it == m_Segments.end()) { return; }
            m_Segments.erase(it);
            m_Segments.shrink_to_fit();
        }

        void free_all() {
            memory_in_use = 0; 
            m_Segments.clear();
            m_Segments.shrink_to_fit();
        }

        void setatexit(); 
    };

    inline Heap heap;

    inline void Heap::setatexit() {
        std::atexit([] () {
            heap.free_all();           
        }); 
    }
}