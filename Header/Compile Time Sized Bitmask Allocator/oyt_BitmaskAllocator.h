/*
Provides compile time sized bitmask/slab allocators, using a nested approach for O(logn) allocations.


Public API:
	OYT_BITMASK_MAXIMUM_ALLOCATIONS
	OYT_BITMASK_FAILED_ALLOCATION

	OYT_BITMASK_DEFINE_STRUCT(maxAllocations_, StructName_)
	OYT_BITMASK_DEFINE(maxAllocations_, variableName_)
	OYT_BITMASK_INIT(allocator_, maxAllocations_)
	OYT_BITMASK_ALLOCATE(allocator_, out_)
	OYT_BITMASK_FREE(allocator_, allocatedIndex_)

Detailed descriptions can be found in doxygen comment style at the macros' definition.


Maybe good to keep in mind:
1.	Instead of invoking the macro in multiple places, define a function for allocating, freeing, etc.
	Should reduce code size (untested).
	```c
	#define ALLOCATIONS_REQUIRED 42
	OYT_BITMASK_DEFINE_STRUCT(ALLOCATIONS_REQUIRED, Foo);

	size_t allocate(struct Foo *pAllocator) {
		size_t out;
		OYT_BITMASK_ALLOCATE(*pAllocator, out);
		return out;
	}
	
	int main(void) {
		struct Foo allocator;
		OYT_BITMASK_INIT(allocator, ALLOCATIONS_REQUIRED);

		size_t allocatedIndex = allocate(&allocator);
		...
	}
	```
*/

#ifndef oyt_BitmaskAllocator_h
#define oyt_BitmaskAllocator_h
#include <stddef.h>
#include <limits.h>

#define OYTBA__WIDTH_OF_SIZE_T   (sizeof(size_t)*CHAR_BIT)
#define OYTBA__WIDTH_OF_SIZE_T_2 (OYTBA__WIDTH_OF_SIZE_T   * OYTBA__WIDTH_OF_SIZE_T)
#define OYTBA__WIDTH_OF_SIZE_T_3 (OYTBA__WIDTH_OF_SIZE_T_2 * OYTBA__WIDTH_OF_SIZE_T)
#define OYTBA__WIDTH_OF_SIZE_T_4 (OYTBA__WIDTH_OF_SIZE_T_2 * OYTBA__WIDTH_OF_SIZE_T_2)

/**
 * @brief Absolute maximum allocations a bitmask allocator can have
 */
#define OYT_BITMASK_MAXIMUM_ALLOCATIONS OYTBA__WIDTH_OF_SIZE_T_4

#if defined(_MSC_VER)
# include <intrin.h>
#endif

#if (defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L) || (defined(__cplusplus) && __cplusplus >= 199711L)
	inline
#endif
static size_t oytba__clz(size_t x) { /* NOLINT(clang-diagnostic-unused-function) */

	if (x == 0) return OYTBA__WIDTH_OF_SIZE_T;

#if defined(__GNUC__) || defined(__clang__)

	if (sizeof(size_t) == sizeof(unsigned int))
		return (size_t)__builtin_clz((unsigned int)x);

	if (sizeof(size_t) == sizeof(unsigned long))
		return (size_t)__builtin_clzl((unsigned long)x);

	if (sizeof(size_t) == __extension__(sizeof(unsigned long long)))
		return (size_t)__builtin_clzll(__extension__((unsigned long long)x));

#elif defined(_MSC_VER)

	if (sizeof(size_t) <= sizeof(unsigned int)) {
		unsigned long index;
		_BitScanReverse(&index, (unsigned long)x);
		return (size_t)(OYTBA__WIDTH_OF_SIZE_T - 1 - (size_t)index);
	}

	# if defined(_M_X64) || defined(_M_ARM64)

	if (sizeof(size_t) <= sizeof(unsigned __int64)) {
		unsigned long index;
		_BitScanReverse64(&index, (unsigned __int64)x);
		return (size_t)(OYTBA__WIDTH_OF_SIZE_T - 1 - (size_t)index);
	}

	# endif /* _M_X64 || _M_ARM64 */

#endif

	{ /* Portable O(log W) fallback. */
	size_t lo = 0;
	size_t hi = OYTBA__WIDTH_OF_SIZE_T;

	while (lo < hi) {
		const size_t mid = lo + (hi - lo + 1) / 2;

		if ((x >> (OYTBA__WIDTH_OF_SIZE_T - mid)) == 0)
			lo = mid;
		else
			hi = mid - 1;
	}

	return  lo;
	}
}

#define OYTBA__ALL_ONES (~(size_t)0)

#define OYTBA__DIVIDE_CEIL(dividend_, divisor_) ( ((dividend_) + ((divisor_) - 1)) / (divisor_) )

#define OYTBA__LENGTH(array_) ( sizeof(array_) / sizeof((array_)[0]) )

#define OYTBA__LOGWIDTH(x_) \
((x_) == 0 ? -1 : \
 (x_) <= OYTBA__WIDTH_OF_SIZE_T   ? 1 : \
 (x_) <= OYTBA__WIDTH_OF_SIZE_T_2 ? 2 : \
 (x_) <= OYTBA__WIDTH_OF_SIZE_T_3 ? 3 : \
 (x_) <= OYTBA__WIDTH_OF_SIZE_T_4 ? 4 : -1)

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
	#define OYTBA__STATIC_ASSERT(cond_, msg_) _Static_assert(cond_, msg_);
#elif defined(__cplusplus) && __cplusplus >= 201103L
	#define OYTBA__STATIC_ASSERT(cond_, msg_) static_assert(cond_, msg_);
#else
	#define OYTBA__STATIC_ASSERT(cond_, msg_)
#endif

/**
 * @brief Value specifying that the allocation failed, i.e. no more slots to be allocated.
 */
#define OYT_BITMASK_FAILED_ALLOCATION OYTBA__ALL_ONES

/**
 * @brief Define a struct for a bitmask allocator
 *
 * Struct can then be used with `struct StructName_`
 *
 * @param[in]	maxAllocations_		Maximum allocations for this allocator struct to have
 * @param[in]	StructName_			Name for the struct
 */
#define OYT_BITMASK_DEFINE_STRUCT(maxAllocations_, StructName_) \
OYTBA__STATIC_ASSERT((maxAllocations_) != 0, "Maximum allocations cannot be 0") \
OYTBA__STATIC_ASSERT((maxAllocations_) <= OYTBA__WIDTH_OF_SIZE_T_4, "Maximum allocations is too large") \
struct StructName_ { \
	size_t lengths[OYTBA__LOGWIDTH(maxAllocations_)]; \
	size_t bitmask[ \
		1 + \
		OYTBA__DIVIDE_CEIL(maxAllocations_, OYTBA__WIDTH_OF_SIZE_T) + \
		OYTBA__DIVIDE_CEIL(maxAllocations_, OYTBA__WIDTH_OF_SIZE_T_2) + \
		OYTBA__DIVIDE_CEIL(maxAllocations_, OYTBA__WIDTH_OF_SIZE_T_3) + \
		OYTBA__DIVIDE_CEIL(maxAllocations_, OYTBA__WIDTH_OF_SIZE_T_4) - \
		(5 - OYTBA__LOGWIDTH(maxAllocations_)) \
	]; \
}

#define OYTBA__EMPTY

/**
 * @brief Define a variable for a bitmask allocator, an anonymous struct is used
 *
 * Struct variable can then be accessed by `variableName_`
 *
 * @param[in]	maxAllocations_		Maximum allocations for this allocator to have
 * @param[in]	variableName_		Name for the struct variable
 */
#define OYT_BITMASK_DEFINE(maxAllocations_, variableName_) OYT_BITMASK_DEFINE_STRUCT(maxAllocations_, OYTBA__EMPTY) variableName_

/**
 * @brief Initializes a struct variable
 *
 * @param[out]	allocator_			Allocator struct variable
 * @param[in]	maxAllocations_		Maximum allocations - same value used while defining the struct or struct variable
 */
#define OYT_BITMASK_INIT(allocator_, maxAllocations_) \
do { \
	const size_t oytba__lastElementUsedBitsCount = 1 + ((maxAllocations_) - 1) % OYTBA__WIDTH_OF_SIZE_T; \
	size_t oytba__i, oytba__cumLen; \
	(allocator_).lengths[OYTBA__LENGTH((allocator_).lengths) - 1] = OYTBA__DIVIDE_CEIL(maxAllocations_, OYTBA__WIDTH_OF_SIZE_T); \
	oytba__cumLen = (allocator_).lengths[OYTBA__LENGTH((allocator_).lengths) - 1]; \
	for (oytba__i = OYTBA__LENGTH((allocator_).lengths) - 1; oytba__i-- > 0;) { \
		(allocator_).lengths[oytba__i] = OYTBA__DIVIDE_CEIL((allocator_).lengths[oytba__i + 1], OYTBA__WIDTH_OF_SIZE_T); \
		oytba__cumLen += (allocator_).lengths[oytba__i]; \
	} \
	for (oytba__i = 0; oytba__i < oytba__cumLen; oytba__i++) { \
		(allocator_).bitmask[oytba__i] = OYTBA__ALL_ONES; \
	} \
	if (oytba__lastElementUsedBitsCount != OYTBA__WIDTH_OF_SIZE_T) { \
		const size_t oytba__leftShiftAmount = OYTBA__WIDTH_OF_SIZE_T - oytba__lastElementUsedBitsCount; \
		const size_t oytba__mask = (((size_t)1 << oytba__lastElementUsedBitsCount) - 1) << oytba__leftShiftAmount; \
		(allocator_).bitmask[oytba__i - 1] &= oytba__mask; \
	} \
} while (0)

#define OYTBA__MASK(index_) ( ((size_t)1) << (OYTBA__WIDTH_OF_SIZE_T - 1 - (index_)) )

/**
 * @brief Allocates an index in range [0, maxAllocations). Lowest free index first.
 *
 * OYT_BITMASK_FAILED_ALLOCATION is written to out_ on failure to allocate, i.e. no more free slots.
 *
 * @param[in]	allocator_	Allocator struct variable
 * @param[out]	out_		Allocated index output, plain size_t lvalue that doesnt appear in allocator_
 */
#define OYT_BITMASK_ALLOCATE(allocator_, out_) \
do { \
	const size_t oytba__levelCount =  OYTBA__LENGTH((allocator_).lengths); \
	size_t oytba__i, oytba__cumLen, oytba__lzIndices[4], oytba__maskIndices[4]; \
	oytba__lzIndices[0] = oytba__maskIndices[0] = oytba__cumLen = (out_) = 0; \
	for (oytba__i = 1; oytba__i < oytba__levelCount; oytba__i++) { \
		size_t oytba__maxIndex = (allocator_).lengths[oytba__i] - OYTBA__WIDTH_OF_SIZE_T * (out_); \
		if (oytba__maxIndex > OYTBA__WIDTH_OF_SIZE_T) oytba__maxIndex = OYTBA__WIDTH_OF_SIZE_T; \
		oytba__lzIndices[oytba__i] = oytba__clz( (allocator_).bitmask[oytba__maskIndices[oytba__i - 1]] ); \
		if (oytba__lzIndices[oytba__i] >= oytba__maxIndex) { (out_) = OYT_BITMASK_FAILED_ALLOCATION; break; } \
		oytba__cumLen += (allocator_).lengths[oytba__i - 1]; \
		(out_) = (out_) * OYTBA__WIDTH_OF_SIZE_T + oytba__lzIndices[oytba__i]; \
		oytba__maskIndices[oytba__i] = oytba__cumLen + (out_); \
	} \
	if ((out_) != OYT_BITMASK_FAILED_ALLOCATION) { \
		const size_t oytba__lzIdx = oytba__clz( (allocator_).bitmask[oytba__maskIndices[oytba__levelCount-1]] ); \
		size_t oytba__isBitmaskZero; \
		(out_) = (out_) * OYTBA__WIDTH_OF_SIZE_T + oytba__lzIdx; \
		if (oytba__levelCount == 1 && (out_) == OYTBA__WIDTH_OF_SIZE_T) { \
			(out_) = OYT_BITMASK_FAILED_ALLOCATION; \
		} else { \
			(allocator_).bitmask[oytba__maskIndices[oytba__levelCount-1]] &= ~OYTBA__MASK(oytba__lzIdx); \
			oytba__isBitmaskZero = (allocator_).bitmask[oytba__maskIndices[oytba__levelCount-1]] == 0; \
			for (oytba__i = oytba__levelCount - 1; oytba__i > 0; oytba__i--) { \
				if (!oytba__isBitmaskZero) break; \
				(allocator_).bitmask[oytba__maskIndices[oytba__i - 1]] &= ~(OYTBA__MASK(oytba__lzIndices[oytba__i])); \
				oytba__isBitmaskZero = (allocator_).bitmask[oytba__maskIndices[oytba__i - 1]] == 0; \
			} \
		} \
	} \
} while(0)

/**
 * @brief Frees an allocated index
 *
 * @param[in]	allocator_			Allocator struct variable
 * @param[in]	allocatedIndex_		Allocated index being freed
 *
 * @note No bounds checks are done. This results in:
 *       1. Freeing OYT_BITMASK_FAILED_ALLOCATION - or a value greater than or equal to the next multiple
 *          of OYTBA__WIDTH_OF_SIZE_T - will cause an out of bounds memory access.
 *       2. If using a maximum allocation count that isn't a multiple of OYTBA__WIDTH_OF_SIZE_T
 *          and an index in the range [maxAllocations, next multiple of OYTBA__WIDTH_OF_SIZE_T)
 *          is freed, then more allocations than maxAllocations can be allocated (padding gets corrupted).
 */
#define OYT_BITMASK_FREE(allocator_, allocatedIndex_) \
do { \
	const size_t oytba__levelCount =  OYTBA__LENGTH((allocator_).lengths); \
	size_t oytba__idx = (size_t)(allocatedIndex_); \
	size_t oytba__i, oytba__cumLen = 0; \
	for (oytba__i = 0; oytba__i < oytba__levelCount; oytba__i++) \
		oytba__cumLen += (allocator_).lengths[oytba__i]; \
	for (oytba__i = oytba__levelCount; oytba__i > 0; oytba__i--) { \
		const size_t oytba__mask = OYTBA__MASK((oytba__idx) % OYTBA__WIDTH_OF_SIZE_T); \
		size_t oytba__maskIdx; \
		oytba__idx /= OYTBA__WIDTH_OF_SIZE_T; \
		oytba__cumLen -= (allocator_).lengths[oytba__i - 1]; \
		oytba__maskIdx = oytba__cumLen + oytba__idx; \
		(allocator_).bitmask[oytba__maskIdx] |= oytba__mask; \
	} \
} while(0)

#endif /* oyt_BitmaskAllocator_h */