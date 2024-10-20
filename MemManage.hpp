#pragma once

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <vector>
#include <algorithm>
#include <string>


namespace AutomaticMemory {
    /* 
        This class is an interface class for Pointer types.
        Essentially, this provides a common ground for heap::pointer
        and stack::pointer (not implemented, waits further implementation).
    */
    template<typename T_, typename D_>
    class base_pointer {
        public: 
        // Creates a pointer from value. 
        // Type must be copy constructable.
        base_pointer(base_pointer const&) = delete; 
        // RAII.
        ~base_pointer() {
            free(); 
        }

        /* 
            Freeing is handled by the derived class. Free should never be used by hand. 
        */
        void free() {
            if (freed) return; 
            static_cast<D_*>(this)->free_impl();
            freed = true; 
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
    };
    
    struct string; 

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
        We have a vector (or list) of segments. A segment is a individual memory 
        space lives parallel to other memory spaces (segments).
        Eg: 
        Vector<Segment>::Begin()---Segment------Segment------Segment------Segment---Vector<Segment>::End()
                                      |            |            |            |
                                    Raw mem     Raw mem      Raw mem      Raw mem
                                    sizeof(32)  sizeof(1024) sizeof(16)   sizeof(1024^2)
                                    bytes       bytes        bytes        bytes
        As the diagram shows, they live parallel to each other so no memory collision or overlap.
        The main memory management is provided by standard library since the Segment is a fancy
        wrapper for std::vector<char> and Heap is a fancy wrapper and manager for std::vector<Segment>.
        So it's as much memory safe as std::vector.   
    */
    class Heap {
    private:
        /* Segment
            This class represents a memory segment. Each segment is a memory space.
            std::vector<char> is the holder for raw memory. 
        */
        struct Segment {
            Segment() : size(0) {}
            Segment(size_t size) : size{size} { m_Memory.reserve(size); }
            void reserve(size_t const& size) { m_Memory.reserve(size);  this->size = size; }
            void resize(size_t const& size) { m_Memory.resize(size);  this->size = size; }
            bool operator==(Segment const& other) const { return m_Memory == other.m_Memory and size == other.size; }
            auto begin() { return m_Memory.begin(); }
            auto end() { return m_Memory.end(); }
            std::vector<char> m_Memory;
            size_t size;
        };
        /* 
            Low level allocation.
            We create a segment in the segments vector with a provided size.  As the segment gets constructed,
            it reserves the requested memory size using std::vector<char>::reserve();
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
            Pointer(Pointer&& other) : memsegment(other.memsegment), owner(std::move(other.owner)), base_type{std::move(other.m_Ptr), std::move(other.freed)} { other.freed = true; }

            template<bool _array = array>
            typename std::enable_if<_array, T_&>::type operator[](size_t index) {
                return *(base_type::m_Ptr + index); 
            }
            private:
            friend class base_pointer<T_, Pointer>;
            friend class Heap; 

            Pointer(T_ * pointer, Heap * owner, Segment& memsegment) : base_type{pointer}, owner{owner}, memsegment(memsegment) { }

            Segment& memsegment;
            Heap * owner;

            void free_impl() {
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
        
        Heap() = default; 

        /* 
            Allocates an object on the memory.
            Allocation gets the size of type then reserves the exact size on the memory. When reserving is complete,
            reserved Segment will be allocated with default constructor. If default constructor is not available,
            construction will be skipped.
        */
        template<typename T_>
        Pointer<T_, false> allocate() {
            Segment& allocated = allocate(sizeof(T_)); 
            T_ * f_Ptr = static_cast<T_*>(static_cast<void*>(allocated.m_Memory.data()));
            if constexpr (std::is_default_constructible_v<T_>) {
                new(f_Ptr) T_{}; 
            }
            return Pointer<T_, false>{f_Ptr, this, allocated}; 
        }
        /*
            Allocates x amount of objects on the memory.
            Allocation gets the size of type and multiplies it with count of objects, then reserves the exact size on the memory. 
            When reserving is complete, reserved Segment will be allocated with default constructor of the type. If default constructor
            is not available, constructio will be skipped.
        */
        template<typename T_>
        Pointer<T_, true> allocate(size_t count) {
            Segment& allocated = allocate(sizeof(T_) * count); 
            T_ * f_Ptr = static_cast<T_*>(static_cast<void*>(allocated.m_Memory.data())); 
            if constexpr (std::is_default_constructible_v<T_>) {
                for(int i = 0; i < count; i++) {
                    new(f_Ptr + i) T_();
                }
            }
            return Pointer<T_, true>{f_Ptr, this, allocated}; 
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
            Internal free method used by string class. 
            When a string is ready to die, this method is called to release resources. Releases memory immediately.  
        */
        void free(Segment& segment) {
            memory_in_use -= segment.size;
            auto it = std::find(m_Segments.begin(), m_Segments.end(), segment); 
            if (it == m_Segments.end()) { return; }
            m_Segments.erase(it);
            m_Segments.shrink_to_fit();
        }
        friend class string; 
    };

    inline Heap heap; 
    // Incomplete string type. WIP 
    struct string {
        public: 
        string() : m_data(heap.allocate(0)) { } ; 
        string(const char * str) : m_data(heap.allocate(strlen(str))) {
            assign(str, strlen(str)); 
        }
        string(const std::string& str) : m_data(heap.allocate(str.length())) {
            assign(str.data(), str.length()); 
        }
        string& operator=(const char * str) {
            m_data.m_Memory.clear();
            assign(str, strlen(str));
            return *this;
        }
        string& operator=(std::string const& str) {
            m_data.m_Memory.clear();
            assign(str.data(), str.length()); 
            return *this; 
        }
        void resize(size_t const& n_size) {
            m_data.resize(n_size); 
        }
        void reserve(size_t const& n_size) {
            m_data.reserve(n_size);
        }
        void append(char ch) {
            m_data.m_Memory.insert(m_data.end(), ch); 
        }
        char at(size_t index) {
            return m_data.m_Memory[index];
        }
        char * data() {
            return m_data.m_Memory.data();
        } 
        const char * c_str() {
            return m_data.m_Memory.data();
        }
        void clear() {
            return m_data.m_Memory.clear();
        }
        void free() {
            if (freed) { return; }
            freed = true; 
            heap.free(m_data);
        }
        operator const char *() {
            return data(); 
        }
        operator std::string() {
            return {data()};
        }
        ~string() {
            free();
        }
        private: 
        void assign(const char * str, size_t len) {
            m_data.m_Memory.reserve(len + 1);
            std::copy(str, str + len, std::back_inserter(m_data.m_Memory));
            m_data.m_Memory.push_back(0);
        }
        Heap::Segment& m_data;
        bool freed = false;
    }; 
}