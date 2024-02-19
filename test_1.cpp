/**
 * @author Jelle Hellings
 * 
 * @license
 * Copyright 2024 Jelle Hellings.
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
 * Performance measurements for the parity library.
 */
#include "parity.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

// #define MODERN_x86

#if defined(MODERN_x86)
    /* Support for 128bit, 256bit instructions. */
    #include <immintrin.h>
    #include <emmintrin.h>

    /*
     * We cannot redefine operators for built-in types, hence make a wrapper for
     * those types (the wrapper will be optimized out).
     */
    struct my_int128 {
        __m128i field;
    };
    struct my_int256 {
        __m256i field;
    };
    struct my_int512 {
        __m512i field;
    };

    
    /*
     * XOR-operator and XOR-assignment for 128bit values.
     */
    inline my_int128 operator^(my_int128 a, my_int128 b)
    {
        return {_mm_xor_si128(a.field, b.field)};
    }

    inline my_int128& operator^=(my_int128& a, my_int128 b)
    {
        a.field = _mm_xor_si128(a.field, b.field);
        return a;
    }


    /*
     * XOR-operator and XOR-assignment for 256bit values.
     */
    inline my_int256 operator^(my_int256 a, my_int256 b)
    {
        return {_mm256_xor_si256(a.field, b.field)};
    }

    inline my_int256& operator^=(my_int256& a, my_int256 b)
    {
        a.field = _mm256_xor_si256(a.field, b.field);
        return a;
    }

 
    /*
     * XOR-operator and XOR-assignment for 512bit values. My machine does not
     * support operations on these types. Hence, usage will crash my machine.
     */
    inline my_int512 operator^(my_int512 a, my_int512 b)
    {
        return {_mm512_xor_si512(a.field, b.field)};
    };

    inline my_int512& operator^=(my_int512& a, my_int512 b)
    {
        a.field = _mm512_xor_si512(a.field, b.field);
        return a;
    };
#endif
    

/*
 * Generate and return a @{std::vector} holding @{size} bytes.
 */
auto generate_data(std::size_t size) 
{
    std::vector<std::byte> data;
    data.reserve(size);
    while (data.size() != size) {
        data.emplace_back(
                static_cast<std::byte>(
                    static_cast<std::uint8_t>(data.size() % 256)));
    }
    return data;
}


/*
 * Return the MB/s value for @{size} bytes processed in @{ms} millisecond.
 */
size_t mbs(auto size, auto ms)
{
    if (ms == 0) {
        return 0ul;
    }
    else {
        return (1000 * size / ms) / (1024 * 1024);
    }
}


/*
 * Run an experiment using @{UIntT} chunks.
 */
template<class UIntT>
void run_experiment()
{
    using namespace parity;
    using namespace std::chrono;

    constexpr std::size_t n = 5;
    std::size_t max_size = 1024 * 1024 * 64;
    std::size_t step_size = max_size / 16;
    
    auto encode_buffer = std::make_unique<std::byte[]>(max_size * (n + 1));
    auto decode_buffer = std::make_unique<std::byte[]>(max_size * (n + 1));
    for (std::size_t slice_size = 0; slice_size < max_size; slice_size += step_size) {
        auto data = generate_data(slice_size * n);

        /* Encoding. */
        std::array<pointer<byte>, 6> write_to = {
            &encode_buffer[0 + 0 * slice_size],
            &encode_buffer[0 + 1 * slice_size],
            &encode_buffer[0 + 2 * slice_size],
            &encode_buffer[0 + 3 * slice_size],
            &encode_buffer[0 + 4 * slice_size],
            &encode_buffer[0 + 5 * slice_size]
        };

        auto start_enc = steady_clock::now();
        auto z = encode_1<UIntT>(data.data(), data.data() + data.size(), write_to.data(), n);
        auto end_enc = steady_clock::now();


        /* Decoding, without parity slice. */
        std::array<pointer<byte>, 6> read_from = {
            &encode_buffer[0 + 0 * slice_size],
            &encode_buffer[0 + 1 * slice_size],
            &encode_buffer[0 + 2 * slice_size],
            &encode_buffer[0 + 3 * slice_size],
            &encode_buffer[0 + 4 * slice_size],
            nullptr
        };

        auto start_dec = steady_clock::now();
        decode<UIntT>(read_from.data(), n, decode_buffer.get(), z.num_chunks());
        auto end_dec = steady_clock::now();

        auto correct = std::equal(data.cbegin(), data.cend(), decode_buffer.get());

        /* Decoding, without non-parity slice. */
        std::array<pointer<byte>, 6> read_fromp = {
            &encode_buffer[0 + 0 * slice_size],
            &encode_buffer[0 + 1 * slice_size],
            &encode_buffer[0 + 2 * slice_size],
            &encode_buffer[0 + 3 * slice_size],
            nullptr,
            &encode_buffer[0 + 5 * slice_size],
        };

        auto start_decp = steady_clock::now();
        decode<UIntT>(read_fromp.data(), n, decode_buffer.get(), z.num_chunks());
        auto end_decp = steady_clock::now();

        auto correctp = std::equal(data.cbegin(), data.cend(), decode_buffer.get());

        std::cout << slice_size << '\t'
                  << sizeof(UIntT) << '\t'
                  << correct << '\t'
                  << correctp << '\t'
                  << duration_cast<milliseconds>(end_enc - start_enc).count() << '\t'
                  << duration_cast<milliseconds>(end_dec - start_dec).count() << '\t'
                  << duration_cast<milliseconds>(end_decp - start_decp).count() << '\t'
                  << mbs(slice_size * n, duration_cast<milliseconds>(end_enc - start_enc).count()) << "MB/s\t"
                  << mbs(slice_size * n, duration_cast<milliseconds>(end_dec - start_dec).count()) << "MB/s\t"
                  << mbs(slice_size * n, duration_cast<milliseconds>(end_decp - start_decp).count()) << "MB/s\t"
                  << std::endl;
    }
}



int main(int argc, char* argv[])
{
    std::cout << "size per slice\t"
              << "chunk size\t"
              << "decode correct\t"
              << "decode correct (parity)\t"
              << "encode runtime\t"
              << "decode runtime\t"
              << "decode runtime (parity)\t"
              << "encode speed\t"
              << "decode speed\t"
              << "decode speed (parity)\t"
              << std::endl;

    run_experiment<std::uint8_t>();
    run_experiment<std::uint16_t>();
    run_experiment<std::uint32_t>();
    run_experiment<std::uint64_t>();
    
    #if defined(MODERN_x86)
    run_experiment<my_int128>();
    run_experiment<my_int256>();
    run_experiment<my_int512>();
    #endif
}



