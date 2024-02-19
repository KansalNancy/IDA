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
 * A basic IDA-like library that can encode data into slices and can recover the
 * original data with only a subset of the slices.
 */
#ifndef INCLUDE_PARITY_HPP
#define INCLUDE_PARITY_HPP
#include <algorithm>
#include <iterator>

#include "utility.hpp"



namespace parity
{
    using namespace ut;


    /*
     * The properties of an IDA encoding. See @{ida_properties} for details.
     */
    struct ida_properties_t
    {
        /* Number of full chunks written to a slice. */
        size_t full_rounds;

        /* @{true} if a single additional chunk is written to each slice.  */
        bool partial_round;
        

        /*
         * Compute the number of chunks represented by this encoding.
         */
        size_t num_chunks() const
        {
            return full_rounds + partial_round;
        }
    };

    /*
     * Return the properties of an IDA encoding that can encode the byte
     * sequence starting at @{in_begin} and ending at @{in_end}. The IDA
     * encoding will generate $(@{n} + p)$ slices and can recover from any
     * selection of @{n} slices.
     */
    template<class UIntT>
    ida_properties_t ida_properties(pointer<const byte> in_begin, pointer<const byte> in_end, size_t n)
    {
        /* Number of bytes to encode. */
        size_t s = std::distance(in_begin, in_end);

        /* Number of bytes in a single chunk. */
        size_t chunk_size = sizeof(UIntT);

        /* Number of bytes in a single round (each slice one chunk). Note that
         * we have @{n} normal slices and @{f} parity slices. */
        size_t round_size = n * chunk_size;

        return ida_properties_t{
            /* Number of full rounds we need to store @{s} bytes. */
            .full_rounds = s / round_size,

            /* If there are remaining bytes, then we need one partial round.
             * Otherwise, we do not. */
            .partial_round = (s % round_size) != 0
        };        
    }
    
    
    /*
     * Perform an IDA encoding for the byte sequence starting at @{in_begin} and
     * ending at @{in_end} that generates $@{n} + 1$ slices written to @{slices}
     * and can recover the original byte sequence from any selection of @{n} of
     * these slices.
     *
     * In this specific encoding, the byte sequence is split up in chunks of
     * @{sizeof(UIntT)} bytes that are dispersed over the @{n} slices pointed to
     * by @{slices[0]}, \dots, @{slices[n]} and the parity value for these
     * chunks are written to the $(@{n} + 1)$-th slice at @{slices[n]}.
     *
     * This function returns the properties of this IDA encoding that specify
     * how many chunks are written to each slice. 
     */
    template<class UIntT>
    ida_properties_t encode_1(pointer<const byte> in_begin, pointer<const byte> in_end,
                              pointer<pointer<byte>> slices, size_t n)
    {
        const auto p = ida_properties<UIntT>(in_begin, in_end, n);

        /* We can read full rounds without worrying about the endianness (byte
         * layout of @{UIntT}) of the underlying machine: we are reading and
         * writing on the same machine. */
        for (size_t round = 0; round < p.full_rounds; ++round) {
            UIntT parity = {};

            /* Copy over the @{n} chunks and compute the parity chunk. */
            for (size_t i = 0; i < n; ++i) {
                /* Copy over chunk from input to the slice. */
                auto chunk = from_bytes_cast<UIntT>(in_begin);
                to_bytes_cast(chunk, slices[i]);

                /* Add the chunk to the parity chunk. */
                parity ^= chunk;

                /* Point to where the chunk can be read from and to where the
                 * next chunk of the @{i}-th slice needs to be written to. */
                in_begin += sizeof(UIntT);
                slices[i] += sizeof(UIntT);
            }

            /* Write the parity chunk and point to where the next parity chunk
             * needs to be written to. */
            to_bytes_cast(parity, slices[n]);
            slices[n] += sizeof(UIntT);
        }
        
        /* Perform the partial round, if necessary. Mostly follows the same
         * approach as above. */
        if (p.partial_round) {
            UIntT parity = {};
            for (size_t i = 0; i < n; ++i) {
                /* We can only read at-most @{size} bytes from @{in_begin} and
                 * a chunk requires at most @{sizeof(UIntT} bytes. */
                auto size = std::distance(in_begin, in_end);
                auto size_chunk = std::min<size_t>(size, sizeof(UIntT));
                auto chunk = from_bytes_cast_padded<UIntT>(in_begin, size_chunk);
                to_bytes_cast(chunk, slices[i]);
                parity ^= chunk;

                in_begin += size_chunk;
                slices[i] += sizeof(UIntT);
            }


            /* Write the parity chunk and point to where the next parity chunk
             * needs to be written to. */
            to_bytes_cast(parity, slices[n]);
            slices[n] += sizeof(UIntT);
        }
        return p;
    }


    /*
     * Perform an IDA decoding for the encoding written using @{encode_1} to the
     * $@{n} + 1$ slices at @{slices}. We will deocde @{m} chunks from each of
     * the slices at @{slices}. Only @i{one} slice at @{slices} can be set to
     * @{nullptr}, indicating that we lost that slice. The decoded result will
     * be written to @{to}.
     */
    template<class UIntT>
    void decode(pointer<pointer<byte>> slices, size_t n,
                pointer<byte> to, size_t chunks) 
    {
        /* Compute the index of the missing slice, if any. */
        auto missing_it = std::find(slices, slices + (n + 1), nullptr);
        auto missing_idx = std::distance(slices, missing_it);


        /* Case: no slices are missing or we are missing the parity slice. */
        if (missing_idx >= n) {
            for (size_t round = 0; round < chunks; ++round) {
                for (size_t i = 0; i < n; ++i) {
                    std::memcpy(to, slices[i], sizeof(UIntT));
                    slices[i] += sizeof(UIntT);
                    to += sizeof(UIntT);
                }
            }
        }

        /* Case: the slice at @{missing_idx} is missing and that slice is not a
         * parity slice. We need to reconstruct the @{missing_idx}-th slice. */
        else {
            for (size_t round = 0; round < chunks; ++round) {
                /* Read the parity chunk. */
                auto recovery = from_bytes_cast<UIntT>(slices[n]);
                slices[n] += sizeof(UIntT);

                /* Copy over chunks from slices $0, \dots, @{missing_idx} - 1$
                 * to the output and cancel out these chunks from the parity. */
                for (size_t i = 0; i < missing_idx; ++i) {
                    auto chunk = from_bytes_cast<UIntT>(slices[i]);
                    to_bytes_cast(chunk, to);
                    recovery ^= chunk;
                    slices[i] += sizeof(UIntT);
                    to += sizeof(UIntT);
                }

                /* The current position in @{to} is where the missing slice
                 * needs to go to. Keep track of that value. */
                auto recovery_to = to;
                to += sizeof(UIntT);

                /* Copy over chunks from slices ${missing_idx} + 1, \dots, n$ to
                 * the output and cancel out these chunks from the parity. */
                for (size_t i = missing_idx + 1; i < n; ++i) {
                    auto chunk = from_bytes_cast<UIntT>(slices[i]);
                    to_bytes_cast(chunk, to);
                    recovery ^= chunk;
                    slices[i] += sizeof(UIntT);
                    to += sizeof(UIntT);
                }

                /* Write the recovered data to the position it belongs at. */
                to_bytes_cast(recovery, recovery_to);
            }
        }
    }
}

#endif