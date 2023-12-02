#pragma once

#include "Constants.h"
#include <format>
#include <cstdint>
#include <iterator>
#include <cstddef>
#include <assert.h>
#include <algorithm>

#define MIN_ALIGN 16

#undef min
#undef max

/// <summary>
/// Takes the value and returns the next higher multiple of alignment.
/// </summary>
size_t Align(size_t value, size_t alignment);

/// <summary>
/// Custom allocation, still figuring out how to use this best.
/// WARNING: Anything allocated inside a memory arena won't get it's desctructor called (intentionally).
/// Don't store std::string or similar in here!
/// </summary>
class MemoryArena
{
public:
    const size_t capacity = 0;
    size_t allocationGranularity = 1024 * 64;

    uint8_t* base;
    size_t used = 0;
    size_t committed = 0;

    MemoryArena(size_t capacity = 1024 * 1024 * 1024);

    template <typename T>
    T* Allocate(size_t objectCount = 1)
    {
        void* ptr = AllocateRaw(sizeof(T) * objectCount, alignof(T));
        return reinterpret_cast<T*>(ptr);
    }

    void* AllocateRaw(size_t byteCount, size_t alignment = MIN_ALIGN);
    void Reset(bool freePages = false);

    // Copying this thing is probably a very bad idea (and moving it shouldn't be necessary).
    MemoryArena(const MemoryArena& other) = delete;
    MemoryArena(MemoryArena&& other) noexcept = delete;
    MemoryArena& operator=(const MemoryArena& other) = delete;
    MemoryArena& operator=(MemoryArena&& other) noexcept = delete;
    ~MemoryArena();
};

template <typename T>
class TypedMemoryArena : public MemoryArena
{
public:
    struct Iterator
    {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = T*;
        using reference = T&;

        Iterator(pointer ptr) : m_ptr(ptr) {}

        reference operator*() const { return *m_ptr; }
        pointer operator->() { return m_ptr; }
        Iterator& operator++() { m_ptr++; return *this; }
        Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }
        friend bool operator== (const Iterator& a, const Iterator& b) { return a.m_ptr == b.m_ptr; };
        friend bool operator!= (const Iterator& a, const Iterator& b) { return a.m_ptr != b.m_ptr; };

    private:
        pointer m_ptr;
    };

    Iterator begin() { return Iterator(reinterpret_cast<T*>(base)); }
    Iterator end() { return Iterator(reinterpret_cast<T*>(base + used)); }

    size_t Count() { return used / sizeof(T); }
};

#define NewObject(arena, type, ...) new((arena).Allocate<type>()) type(__VA_ARGS__)
#define NewArray(arena, type, count, ...) new((arena).Allocate<type>(count)) type[count](__VA_ARGS__)

namespace ArrayFunc
{
    template <typename T>
    T& OpArray(T* base, const size_t capacity, const size_t index)
    {
        assert(index >= 0);
        assert(index < capacity);
        return base[index];
    }

    template <typename T>
    const T& OpArrayConst(const T* base, const size_t capacity, const size_t index)
    {
        assert(index >= 0);
        assert(index < capacity);
        return base[index];
    }

    template <typename T>
    T& At(T* base, const size_t capacity, const size_t size, const size_t index)
    {
        assert(index >= 0);
        assert(index < size);
        return base[index];
    }

    template <typename T>
    const T& AtConst(const T* base, const size_t capacity, const size_t size, const size_t index)
    {
        assert(index >= 0);
        assert(index < size);
        return base[index];
    }

    template <typename T>
    const void RemoveAt(T* base, const size_t capacity, size_t& size, const size_t removeIndex)
    {
        assert(removeIndex >= 0);
        if (removeIndex < 0) return;

        assert(removeIndex < size);
        if (removeIndex >= size) return;

        size--;
        for (int i = removeIndex; i < size; i++)
        {
            base[removeIndex] = base[removeIndex + 1];
        }
    }

    template <typename T>
    void RemoveAllEqual(T* base, const size_t capacity, size_t& size, const T& element)
    {
        for (int i = 0; i < size; i++)
        {
            if (base[i] == element)
            {
				RemoveAt(base, capacity, size, i);
			}
		}
	}

    template <typename T>
    bool contains(const T* base, const size_t size, const T& element)
    {
        for (int i = 0; i < size; i++)
        {
            if (base[i] == element)
            {
				return true;
			}
		}
		return false;
	}
}

/// <summary>
/// Stack allocated array, counts the number of elements if used with newElement().
/// </summary>
template <typename T, std::size_t CAPACITY>
class CountingArray
{
public:
    T& operator[](const size_t index)
    {
        return ArrayFunc::OpArray(base, CAPACITY, index);
    }

    const T& operator[](const size_t index) const
    {
        return ArrayFunc::OpArrayConst(base, CAPACITY, index);
    }

    T& at(const size_t index)
    {
        return ArrayFunc::At(base, CAPACITY, size, index);
    }

    const T& at(const size_t index) const
    {
        return ArrayFunc::AtConst(base, CAPACITY, size, index);
    }

    template <typename... Args>
    T& newElement()
    {
        assert(size < CAPACITY);
        return *new(reinterpret_cast<void*>(&base[size++])) T(Args...);
    }

    bool removeAt(const size_t removeIndex)
    {
        ArrayFunc::RemoveAt(base, CAPACITY, size, removeIndex);
    }

    void removeAllEqual(const T& element)
    {
        ArrayFunc::RemoveAllEqual(base, CAPACITY, size, element);
    }

    bool contains(const T& element)
    {
        return ArrayFunc::contains(base, size, element);
    }

    void clear()
    {
        size = 0;
    }

    T base[CAPACITY]{};
    size_t size = 0;
    size_t capacity = CAPACITY;

    struct Iterator
    {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = T*;
        using reference = T&;

        Iterator(pointer ptr) : m_ptr(ptr) {}

        reference operator*() const { return *m_ptr; }
        pointer operator->() { return m_ptr; }
        Iterator& operator++() { m_ptr++; return *this; }
        Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }
        friend bool operator== (const Iterator& a, const Iterator& b) { return a.m_ptr == b.m_ptr; };
        friend bool operator!= (const Iterator& a, const Iterator& b) { return a.m_ptr != b.m_ptr; };

    private:
        pointer m_ptr;
    };

    Iterator begin() { return Iterator(reinterpret_cast<T*>(base)); }
    Iterator end() { return Iterator(reinterpret_cast<T*>(base + size)); }
};

/// <summary>
/// Arena allocated array, counts the number of elements if used with newElement().
/// </summary>
template <typename T>
class FixedList
{
public:
    FixedList(MemoryArena& arena, size_t capacity) : capacity(capacity)
    {
		assert(this->base == nullptr);
		this->base = NewArray(arena, T, capacity);
        this->size = 0;
    }

    T& operator[](const size_t index)
    {
        return ArrayFunc::OpArray(base, capacity, index);
    }

    const T& operator[](const size_t index) const
    {
        return ArrayFunc::OpArrayConst(base, capacity, index);
    }

    T& at(const size_t index)
    {
        return ArrayFunc::At(base, capacity, size, index);
    }

    const T& at(const size_t index) const
    {
        return ArrayFunc::AtConst(base, capacity, size, index);
    }
    
    template <typename... Args>
    T& newElement()
    {
		assert(size < capacity);
		return *new(reinterpret_cast<void*>(&base[size++])) T(Args...);
    }

    bool contains(const T& element)
    {
        return ArrayFunc::contains(base, size, element);
    }

    void clear()
    {
		size = 0;
    }

    void removeAt(const size_t removeIndex)
    {
		ArrayFunc::RemoveAt(base, capacity, size, removeIndex);
	}

    void removeAllEqual(const T& element)
    {
		ArrayFunc::RemoveAllEqual(base, capacity, size, element);
	}

    T* base = nullptr;
    const size_t capacity = 0;
    size_t size = 0;

    struct Iterator
    {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using pointer = T*;
        using reference = T&;

        Iterator(pointer ptr) : m_ptr(ptr) {}

        reference operator*() const { return *m_ptr; }
        pointer operator->() { return m_ptr; }
        Iterator& operator++() { m_ptr++; return *this; }
        Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }
        friend bool operator== (const Iterator& a, const Iterator& b) { return a.m_ptr == b.m_ptr; };
        friend bool operator!= (const Iterator& a, const Iterator& b) { return a.m_ptr != b.m_ptr; };

    private:
        pointer m_ptr;
    };

    Iterator begin() { return Iterator(reinterpret_cast<T*>(base)); }
    Iterator end() { return Iterator(reinterpret_cast<T*>(base + size)); }
};

// Used for short strings, to avoid std. This might not be a great idea but i'm trying it for fun here.
struct FixedStr
{
    constexpr static size_t SIZE = 128;
    char str[SIZE]{};

    FixedStr& operator=(const char* other)
    {
        strcpy_s((char*)str, SIZE, other);
        return *this;
    }

    // TODO: this isn't efficient, but we'll have to use std::move or something like that to make it better.
    FixedStr(const char* other)
    {
        strcpy_s((char*)str, SIZE, other);
    }

    bool operator==(const FixedStr& other) const
    {
        return strcmp(str, other.str) == 0;
    }

    bool operator==(const char* other) const
    {
        return strcmp(str, other) == 0;
    }
};

template <>
struct std::formatter<FixedStr> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    auto format(const FixedStr& obj, std::format_context& ctx) {
        return std::format_to(ctx.out(), "{}", obj.str);
    }
};
