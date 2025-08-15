#include <utility>
#include <cstddef>
#include <type_traits>

// constexpr log2 helper
constexpr size_t ceilLog2(size_t x) {
    size_t bits = 0;
    size_t value = 1;
    while (value < x) {
        value <<= 1;
        ++bits;
    }
    return bits;
}

template<size_t MaxA, size_t MaxB>
struct AzPair {
    static constexpr size_t bitsB = ceilLog2(MaxB + 1);
    static constexpr size_t bitsA = ceilLog2(MaxA + 1);
    static_assert(bitsA + bitsB <= sizeof(size_t) * 8, "Not enough bits to pack both values");

    static constexpr size_t maskB = (size_t(1) << bitsB) - 1;

    static constexpr size_t encode(size_t a, size_t b) {
        return (a << bitsB) | (b & maskB);
    }

    static constexpr std::pair<size_t, size_t> decode(size_t encoded) {
        return { encoded >> bitsB, encoded & maskB };
    }
};
