/**
 * @author Jelle Hellings
 * 
 * @license
 * Copyright 2020, 2021, 2022, 2023, 2024 Jelle Hellings.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * @description
 * Elementary general-purpose utilities.
 */
#ifndef INCLUDE_UT_UTILITY_HPP
#define INCLUDE_UT_UTILITY_HPP
#include <cstddef>
#include <cstring>
#include <type_traits>


namespace ut
{
    using std::byte;
    using std::size_t;


    /*
     * The type of a C-style fixed-size array of type @{T} holding @{N} values.
     * We use this type definition to simplify types involving C-style arrays.
     * For example, using @{c_array<int, 5>& arr} in the definition of a
     * parameter list of a function defines a parameter named @{arr} that is-a
     * reference to an @{int}-array holding five values. Without this helper
     * type definition, such a parameter definition would be defined by
     * @{int (&arr)[5]}, which we believe to be significantly harder to read.
     */
    template<class T, size_t N>
    using c_array = T[N];

    /*
     * The type of a pointer to a value of type @{T}. We use this type
     * definition to simplify types involving pointers. For example, the types
     * @{pointer<int>}, @{pointer<const int>}, and @{const pointer<int>} specify
     * a pointer to an @{int}, a pointer to a @{const int} (we can change the
     * pointer, but not the pointed-to value), and a @{const}-pointer to an
     * @{int} (we cannot change the pointer, but we can change the pointed-to
     * value), respectively. Without this helper type definition, these types    
     * would be defined by @{int*}, @{const int*}, and @{int* const},
     * respectively.
     */
    template<class T>
    using pointer = T*;


    /*
     * Assume that @{source} points to a valid low-level byte representation of
     * a value of type @{T}. Construct this value of type @{T} based on this
     * byte representation and return the value. The source @{source} must hold
     * at least @{sizeof(T)} bytes.
     */
    template<class T>
    requires (std::is_trivially_copyable_v<T>)
    T from_bytes_cast(pointer<const byte> source)
    {
        T value = {};

        /* We rely on the compiler to optimize away the @{memcpy}. */
        std::memcpy(&value, source, sizeof(T));
        return value;
    }



    /*
     * Interpret @{from} as a byte sequence of @{size} bytes followed by
     * @{sizeof(T) - size} bytes with value zero. Assume that this sequence
     * of @{sizeof(T)} bytes is a valid low-level byte representation of a value
     * of type @{T}. Construct this value of type @{T} based on this byte
     * representation and return the value. Requires that @{size} is at-most
     * @{sizeof(T)}.
     */
    template<class T>
    requires (std::is_trivially_copyable_v<T>)
    T from_bytes_cast_padded(pointer<const byte> from, size_t size)
    {
        /* Note that we use aggregate initialization to ensure that @{bytes} is
        * a sequence of zero bytes. */
        c_array<byte, sizeof(T)> bytes = {};

        std::memcpy(bytes, from, size);
        return from_bytes_cast<T>(bytes);
    }


    /*
     * Write the low-level byte representation of a value of type @{T} to the
     * sequence of bytes @{target}. The source @{source} must have room for at
     * least @{sizeof(T)} bytes.
     */
    template<class T>
    requires (std::is_trivially_copyable_v<T>)
    void to_bytes_cast(const T source, pointer<byte> target)
    {
        /* We rely on the compiler to optimize away the @{memcpy}. */
        std::memcpy(target, &source, sizeof(T));
    }
}

#endif