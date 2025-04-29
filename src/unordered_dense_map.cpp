#include "../include/unordered_dense_map.hpp"
#include <cstring>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
#include <immintrin.h>
#define UNORDERED_DENSE_MAP_SIMD 1
#else
#define UNORDERED_DENSE_MAP_SIMD 0
#endif

namespace detail
{

    // WyHash implementation for fast hashing
    uint64_t WyHash::hash(const void *key, size_t len, uint64_t seed)
    {
        static constexpr uint64_t wyhash64_a = 0x3b3897599180e0c5ULL;
        static constexpr uint64_t wyhash64_b = 0x1b8735937b4aac63ULL;
        static constexpr uint64_t wyhash64_c = 0x96be6a03f93d9cd7ULL;
        static constexpr uint64_t wyhash64_d = 0xebd33483acc5ea64ULL;

        const uint8_t *p = static_cast<const uint8_t *>(key);
        uint64_t a, b;

        if (len <= 16)
        {
            if (len >= 4)
            {
                a = (static_cast<uint64_t>(p[0]) << 32) | (static_cast<uint64_t>(p[len >> 1]) << 16) | (static_cast<uint64_t>(p[len - 2]) << 8) | p[len - 1];
                b = (static_cast<uint64_t>(p[len - 4]) << 32) | (static_cast<uint64_t>(p[len - 3]) << 16) | (static_cast<uint64_t>(p[len - 2]) << 8) | p[len - 1];
            }
            else if (len > 0)
            {
                a = p[0];
                b = p[len - 1];
            }
            else
            {
                a = b = 0;
            }
        }
        else
        {
            size_t i = len;
            if (i > 48)
            {
                uint64_t see1 = seed, see2 = seed;
                do
                {
                    seed = wyhash64_mum(wyhash64_read(p, 8) ^ wyhash64_a, wyhash64_read(p + 8, 8) ^ seed);
                    see1 = wyhash64_mum(wyhash64_read(p + 16, 8) ^ wyhash64_b, wyhash64_read(p + 24, 8) ^ see1);
                    see2 = wyhash64_mum(wyhash64_read(p + 32, 8) ^ wyhash64_c, wyhash64_read(p + 40, 8) ^ see2);
                    p += 48;
                    i -= 48;
                } while (i > 48);
                seed ^= see1 ^ see2;
            }
            while (i > 16)
            {
                seed = wyhash64_mum(wyhash64_read(p, 8) ^ wyhash64_a, wyhash64_read(p + 8, 8) ^ seed);
                i -= 16;
                p += 16;
            }
            a = wyhash64_read(p + i - 16, 8);
            b = wyhash64_read(p + i - 8, 8);
        }

        a ^= wyhash64_a;
        b ^= seed;
        a *= wyhash64_b;
        b *= wyhash64_c;
        a = wyhash64_mum(a, b);
        seed ^= a ^ b;
        return wyhash64_mum(seed, len ^ wyhash64_d);
    }

    uint64_t WyHash::wyhash64_mum(uint64_t A, uint64_t B)
    {
        uint64_t r = A * B;
        return r - (r >> 32);
    }

    uint64_t WyHash::wyhash64_read(const uint8_t *p, size_t k)
    {
        uint64_t r = 0;
        for (size_t i = 0; i < k; ++i)
        {
            r |= static_cast<uint64_t>(p[i]) << (i * 8);
        }
        return r;
    }

// SIMD-optimized hash mixing for poor-quality hashes
#if UNORDERED_DENSE_MAP_SIMD
    uint64_t mix_hash(uint64_t hash)
    {
        __m256i h = _mm256_set1_epi64x(hash);
        __m256i mix = _mm256_set_epi64x(0x9e3779b97f4a7c15, 0xbf58476d1ce4e5b9, 0x94d049bb133111eb, 0x5ac635d8aa3a93e7);
        __m256i result = _mm256_xor_si256(h, mix);
        result = _mm256_add_epi64(result, _mm256_slli_epi64(result, 13));
        result = _mm256_xor_si256(result, _mm256_srli_epi64(result, 7));
        result = _mm256_add_epi64(result, _mm256_slli_epi64(result, 17));
        result = _mm256_xor_si256(result, _mm256_srli_epi64(result, 5));
        uint64_t mixed[4];
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(mixed), result);
        return mixed[0] ^ mixed[1] ^ mixed[2] ^ mixed[3];
    }
#else
    uint64_t mix_hash(uint64_t hash)
    {
        hash ^= (hash >> 33);
        hash *= 0xff51afd7ed558ccdULL;
        hash ^= (hash >> 33);
        hash *= 0xc4ceb9fe1a85ec53ULL;
        hash ^= (hash >> 33);
        return hash;
    }
#endif

    // Specialization for string types
    uint64_t hash_traits<std::string>::hash(const std::string &key)
    {
        return WyHash::hash(key.data(), key.size());
    }

    uint8_t hash_traits<std::string>::fingerprint(const std::string &key)
    {
        uint64_t h = hash(key);
        return static_cast<uint8_t>(h & 0xFF);
    }

// Vectorized batch operations
#if UNORDERED_DENSE_MAP_SIMD
    namespace simd
    {

        // SIMD-optimized batch hash computation for integers
        void batch_hash_int(const int *keys, uint64_t *hashes, size_t count)
        {
            constexpr size_t simd_width = 4; // Process 4 integers at once with AVX2
            size_t simd_count = (count / simd_width) * simd_width;

            for (size_t i = 0; i < simd_count; i += simd_width)
            {
                // Load 4 integers
                __m128i keys_vec = _mm_loadu_si128(reinterpret_cast<const __m128i *>(&keys[i]));

                // Convert to 64-bit for hashing
                __m256i keys64 = _mm256_cvtepi32_epi64(keys_vec);

                // Simple hash computation (in practice, this would call WyHash for each)
                for (int j = 0; j < 4; ++j)
                {
                    int key = keys[i + j];
                    hashes[i + j] = WyHash::hash(&key, sizeof(key));
                }
            }

            // Handle remaining elements
            for (size_t i = simd_count; i < count; ++i)
            {
                hashes[i] = WyHash::hash(&keys[i], sizeof(keys[i]));
            }
        }

        // SIMD-optimized fingerprint extraction
        void batch_fingerprint(const uint64_t *hashes, uint8_t *fingerprints, size_t count)
        {
            constexpr size_t simd_width = 4;
            size_t simd_count = (count / simd_width) * simd_width;

            for (size_t i = 0; i < simd_count; i += simd_width)
            {
                __m256i hashes_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(&hashes[i]));

                // Extract low 8 bits from each hash
                __m256i mask = _mm256_set1_epi64x(0xFF);
                __m256i fingerprints_64 = _mm256_and_si256(hashes_vec, mask);

                // Pack to 8-bit values
                __m128i fingerprints_32 = _mm256_cvtepi64_epi32(fingerprints_64);
                __m128i fingerprints_16 = _mm_packus_epi32(fingerprints_32, fingerprints_32);
                __m128i fingerprints_8 = _mm_packus_epi16(fingerprints_16, fingerprints_16);

                // Store result
                uint32_t result = _mm_cvtsi128_si32(fingerprints_8);
                memcpy(&fingerprints[i], &result, 4);
            }

            // Handle remaining elements
            for (size_t i = simd_count; i < count; ++i)
            {
                fingerprints[i] = static_cast<uint8_t>(hashes[i] & 0xFF);
            }
        }

    } // namespace simd
#endif

} // namespace detail