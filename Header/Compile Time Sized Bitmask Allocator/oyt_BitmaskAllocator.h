/**
 * Compile time sized bitmask allocator. Not thread safe.
 *
 * Using the header (somewhat like a cpp template):
 * #define OYT_BITMASK_ALLOC_SIZE 256
 * #include "oyt_BitmaskAllocator.h"
 * // OYT_BITMASK_ALLOC_SIZE is no longer defined
 *
 * Note: OYT_BITMASK_ALLOC_SIZE must be a plain integer literal.
 *
 * You then have:
 * Constant:
 *    OYT_BITMASK_ALLOC_CAPACITY - maximum value that OYT_BITMASK_ALLOC_SIZE can be defined with.
 *
 * Type:
 *     oyt_BitmaskAlloc256
 *
 * Functions:
 *     oyt_BitmaskAlloc256_init()
 *     oyt_BitmaskAlloc256_alloc()
 *     oyt_BitmaskAlloc256_free()
 *
 * The header can be included multiple times.
 * Defining OYT_BITMASK_ALLOC_SIZE as a previously defined value will occur in a redefinition error.
 */

#include <limits.h>

#if defined(_MSC_VER) && !defined(__clang__)
	#include <intrin.h>
#endif

/* Verify OYT_BITMASK_ALLOC_SIZE exists */
#ifndef OYT_BITMASK_ALLOC_SIZE
	#error "OYT_BITMASK_ALLOC_SIZE isn't defined."
#endif

#define OYTBA_CAT_(a_, b_) a_##b_
#define OYTBA_CAT(a_, b_)  OYTBA_CAT_(a_, b_)

#define OYTBA_T OYTBA_CAT(oyt_BitmaskAlloc, OYT_BITMASK_ALLOC_SIZE)

/* C standard */
#if defined(__STDC_VERSION__)
	#define OYTBA_C_VERSION __STDC_VERSION__
#else
	#define OYTBA_C_VERSION 0
#endif

/* C++ standard */
#if defined(_MSVC_LANG)
	#define OYTBA_CPP_VERSION _MSVC_LANG
#elif defined(__cplusplus)
	#define OYTBA_CPP_VERSION __cplusplus
#else
	#define OYTBA_CPP_VERSION 0
#endif

/* Inline function-specifier */
#if OYTBA_CPP_VERSION >= 199711L || OYTBA_C_VERSION >= 199901L
	#define OYTBA_INLINE inline
#else
	#define OYTBA_INLINE
#endif

/* Bitmask type */
#define OYTBA_MASK_USING_ULONG_LONG 1
#define OYTBA_MASK_USING_ULONG 2
#if OYTBA_CPP_VERSION >= 201103L || OYTBA_C_VERSION >= 199901L
	#define OYTBA_MASK_USING OYTBA_MASK_USING_ULONG_LONG
	#define OYTBA_MASK_T unsigned long long
	#define OYTBA_MASK_WIDTH 64ull
#else
	#define OYTBA_MASK_USING OYTBA_MASK_USING_ULONG
	#define OYTBA_MASK_T unsigned long

	/* Width of long > 32bits */
	#if ULONG_MAX > 0xFFFFFFFFUL
		#if ULONG_MAX >= 0xFFFFFFFFFFFFFFFFUL
			#define OYTBA_MASK_WIDTH 64ul
		#endif
	#else
		#define OYTBA_MASK_WIDTH 32ul
	#endif
#endif

#if !defined(OYTBA_MASK_USING) || !defined(OYTBA_MASK_T) || !defined(OYTBA_MASK_WIDTH)
	#error "No type defined or width"
#endif

#define OYTBA_MASK_WIDTH_POW2 (OYTBA_MASK_WIDTH      * OYTBA_MASK_WIDTH)
#define OYTBA_MASK_WIDTH_POW3 (OYTBA_MASK_WIDTH_POW2 * OYTBA_MASK_WIDTH)
#define OYTBA_MASK_WIDTH_POW4 (OYTBA_MASK_WIDTH_POW3 * OYTBA_MASK_WIDTH)

#if OYTBA_MASK_WIDTH == 32
	#define OYT_BITMASK_ALLOC_CAPACITY (32ul * 32ul * 32ul * 32ul)
#elif OYTBA_MASK_WIDTH == 64
	#define OYT_BITMASK_ALLOC_CAPACITY (64ul * 64ul * 64ul * 64ul)
#else
	#error "OYT_BITMASK_ALLOC_CAPACITY cant be defined due to an unsupported mask width."
#endif

#define OYTBA_CEIL_DIV(a_, b_) ( ((a_) - 1 + (b_)) / (b_) )
#define OYTBA_LEN(x_) (sizeof(x_) / sizeof((x_)[0]))

/**
 * @struct Bitmask allocator type.
 */
typedef struct {
#if OYT_BITMASK_ALLOC_SIZE <= 0 || OYT_BITMASK_ALLOC_SIZE > OYT_BITMASK_ALLOC_CAPACITY
	char empty_struct_warning_be_quiet;
	#error "OYT_BITMASK_ALLOC_SIZE has to be a positive integer less than or equal to OYT_BITMASK_ALLOC_CAPACITY."
#elif OYT_BITMASK_ALLOC_SIZE <= OYTBA_MASK_WIDTH
	#define OYTBA_LEN1 OYT_BITMASK_ALLOC_SIZE

	OYTBA_MASK_T mask0;
#elif OYT_BITMASK_ALLOC_SIZE <= OYTBA_MASK_WIDTH_POW2
	#define OYTBA_LEN1 OYTBA_CEIL_DIV(OYT_BITMASK_ALLOC_SIZE, OYTBA_MASK_WIDTH)
	#define OYTBA_LEN2 OYT_BITMASK_ALLOC_SIZE

	OYTBA_MASK_T mask0;
	OYTBA_MASK_T mask1[OYTBA_LEN1];
#elif OYT_BITMASK_ALLOC_SIZE <= OYTBA_MASK_WIDTH_POW3
	#define OYTBA_LEN1 OYTBA_CEIL_DIV(OYT_BITMASK_ALLOC_SIZE, OYTBA_MASK_WIDTH_POW2)
	#define OYTBA_LEN2 OYTBA_CEIL_DIV(OYT_BITMASK_ALLOC_SIZE, OYTBA_MASK_WIDTH)
	#define OYTBA_LEN3 OYT_BITMASK_ALLOC_SIZE

	OYTBA_MASK_T mask0;
	OYTBA_MASK_T mask1[OYTBA_LEN1];
	OYTBA_MASK_T mask2[OYTBA_LEN2];
#elif OYT_BITMASK_ALLOC_SIZE <= OYTBA_MASK_WIDTH_POW4
	#define OYTBA_LEN1 OYTBA_CEIL_DIV(OYT_BITMASK_ALLOC_SIZE, OYTBA_MASK_WIDTH_POW3)
	#define OYTBA_LEN2 OYTBA_CEIL_DIV(OYT_BITMASK_ALLOC_SIZE, OYTBA_MASK_WIDTH_POW2)
	#define OYTBA_LEN3 OYTBA_CEIL_DIV(OYT_BITMASK_ALLOC_SIZE, OYTBA_MASK_WIDTH)
	#define OYTBA_LEN4 OYT_BITMASK_ALLOC_SIZE

	OYTBA_MASK_T mask0;
	OYTBA_MASK_T mask1[OYTBA_LEN1];
	OYTBA_MASK_T mask2[OYTBA_LEN2];
	OYTBA_MASK_T mask3[OYTBA_LEN3];
#endif
} OYTBA_T;

#if OYTBA_MASK_USING == OYTBA_MASK_USING_ULONG
	#define OYTBA_MSB ( ~((~0ul)  >> 1) )
#elif OYTBA_MASK_USING == OYTBA_MASK_USING_ULONG_LONG
	#define OYTBA_MSB ( ~((~0ull) >> 1) )
#endif

#ifndef OYTBA_CLZ_DEFINED
#define OYTBA_CLZ_DEFINED
static OYTBA_INLINE unsigned oytba_clz(OYTBA_MASK_T x) {
	if (x == 0) return OYTBA_MASK_WIDTH;
#if defined(__GNUC__) || defined(__clang__)
	#if OYTBA_MASK_USING == OYTBA_MASK_USING_ULONG
		return (unsigned)__builtin_clzl(x);
	#elif OYTBA_MASK_USING == OYTBA_MASK_USING_ULONG_LONG
		return (unsigned)__builtin_clzll(x);
	#endif
#elif defined(_MSC_VER)
	{
	unsigned long index;
	#if OYTBA_MASK_USING == OYTBA_MASK_USING_ULONG
		_BitScanReverse(&index, x);
		return (unsigned)(OYTBA_MASK_WIDTH - 1 - index);
	#elif defined(_M_X64) || defined(_M_ARM64)
		_BitScanReverse64(&index, x);
		return (unsigned)(OYTBA_MASK_WIDTH - 1 - index);
	#endif
	}
#endif
	{
	unsigned shift, n = 0;

	for (shift = OYTBA_MASK_WIDTH / 2; shift != 0; shift >>= 1) {
		if ((x >> (OYTBA_MASK_WIDTH - shift)) == 0) {
			n += shift;
			x <<= shift;
		}
	}
	return n;
	}
}
#endif



#define OYTBA_SET_BIT_FREE(mask_, idx_)     (mask_) |=  (OYTBA_MSB >> (idx_))
#define OYTBA_SET_BIT_OCCUPIED(mask_, idx_) (mask_) &= ~(OYTBA_MSB >> (idx_))

#define OYTBA_SET_ARRAY_ALL_ONES(array_) do {unsigned long i; for (i = 0; i < OYTBA_LEN(array_); i++) (array_)[i] = ~((OYTBA_MASK_T)0);} while(0)

/**
 * @brief Initialize.
 *
 * @param[in,out]	pAllocator	Memory for a bitmask allocator
 */
static OYTBA_INLINE void OYTBA_CAT(OYTBA_T, _init)(OYTBA_T *pAllocator) {
#if OYT_BITMASK_ALLOC_SIZE > OYTBA_MASK_WIDTH_POW3
	OYTBA_SET_ARRAY_ALL_ONES(pAllocator->mask3);
#endif
#if OYT_BITMASK_ALLOC_SIZE > OYTBA_MASK_WIDTH_POW2
	OYTBA_SET_ARRAY_ALL_ONES(pAllocator->mask2);
#endif
#if OYT_BITMASK_ALLOC_SIZE > OYTBA_MASK_WIDTH
	OYTBA_SET_ARRAY_ALL_ONES(pAllocator->mask1);
#endif
	pAllocator->mask0 =~((OYTBA_MASK_T)0);
}

#define OYTBA_ALLOC_GET_BIT_CODE(level_, levelPlusOne_) \
unsigned idx##level_ = oytba_clz(pAllocator->mask##level_[queryIndex]); \
out = queryIndex * OYTBA_MASK_WIDTH + idx##level_; \
if (out >= OYTBA_LEN##levelPlusOne_) return OYT_BITMASK_ALLOC_SIZE

#define OYTBA_ALLOC_CLEAR_BIT_CODE(level_, levelPlusOne_) \
OYTBA_SET_BIT_OCCUPIED(pAllocator->mask##level_[queryIndex], idx##level_); \
idx##level_ = oytba_clz(pAllocator->mask##level_[queryIndex]); \
if (idx##level_ < OYTBA_MASK_WIDTH && queryIndex * OYTBA_MASK_WIDTH + idx##level_ < OYTBA_LEN##levelPlusOne_) \
	return out; \
queryIndex /= OYTBA_MASK_WIDTH

/**
 * @brief Allocate an index.
 *
 * @param[in]	pAllocator	Pointer to a bitmask allocator
 *
 * @return Allocated index or OYT_BITMASK_ALLOC_SIZE if allocator is full.
 */
static OYTBA_INLINE unsigned long OYTBA_CAT(OYTBA_T, _alloc)(OYTBA_T *pAllocator) {
	unsigned long out;
	const unsigned idx0 = oytba_clz(pAllocator->mask0);
	out = idx0;
	if (out >= OYTBA_LEN1)
		return OYT_BITMASK_ALLOC_SIZE;

	#if OYT_BITMASK_ALLOC_SIZE > OYTBA_MASK_WIDTH
	{
		unsigned long queryIndex = out;
		OYTBA_ALLOC_GET_BIT_CODE(1, 2);

		#if OYT_BITMASK_ALLOC_SIZE > OYTBA_MASK_WIDTH_POW2
		queryIndex = out;
		{
			OYTBA_ALLOC_GET_BIT_CODE(2, 3);

			#if OYT_BITMASK_ALLOC_SIZE > OYTBA_MASK_WIDTH_POW3
			queryIndex = out;
			{
				OYTBA_ALLOC_GET_BIT_CODE(3, 4);
				OYTBA_ALLOC_CLEAR_BIT_CODE(3, 4);
			}
			#endif

			OYTBA_ALLOC_CLEAR_BIT_CODE(2, 3);
		}
		#endif

		OYTBA_ALLOC_CLEAR_BIT_CODE(1, 2);
	}
	#endif

	OYTBA_SET_BIT_OCCUPIED(pAllocator->mask0, idx0);

	return out;
}

/**
 * @brief Free an index.
 *
 * @param[in]	pAllocator	Pointer to a bitmask allocator
 * @param[in]	index		Index to free
 */
static OYTBA_INLINE void OYTBA_CAT(OYTBA_T, _free)(OYTBA_T *pAllocator, unsigned long index) {
	unsigned long bitIdx;
	if (index >= OYT_BITMASK_ALLOC_SIZE) return;
#if OYT_BITMASK_ALLOC_SIZE > OYTBA_MASK_WIDTH_POW3
	bitIdx = index % OYTBA_MASK_WIDTH;
	index /= OYTBA_MASK_WIDTH;
	OYTBA_SET_BIT_FREE(pAllocator->mask3[index], bitIdx);
#endif
#if OYT_BITMASK_ALLOC_SIZE > OYTBA_MASK_WIDTH_POW2
	bitIdx = index % OYTBA_MASK_WIDTH;
	index /= OYTBA_MASK_WIDTH;
	OYTBA_SET_BIT_FREE(pAllocator->mask2[index], bitIdx);
#endif
#if OYT_BITMASK_ALLOC_SIZE > OYTBA_MASK_WIDTH
	bitIdx = index % OYTBA_MASK_WIDTH;
	index /= OYTBA_MASK_WIDTH;
	OYTBA_SET_BIT_FREE(pAllocator->mask1[index], bitIdx);
#endif
	bitIdx = index % OYTBA_MASK_WIDTH;
	OYTBA_SET_BIT_FREE(pAllocator->mask0, bitIdx);
}

#undef OYTBA_ALLOC_CLEAR_BIT_CODE
#undef OYTBA_ALLOC_GET_BIT_CODE

#undef OYTBA_SET_ARRAY_ALL_ONES
#undef OYTBA_SET_BIT_OCCUPIED
#undef OYTBA_SET_BIT_FREE
#undef OYTBA_MSB

#undef OYTBA_LEN4
#undef OYTBA_LEN3
#undef OYTBA_LEN2
#undef OYTBA_LEN1

#undef OYTBA_LEN
#undef OYTBA_CEIL_DIV

#undef OYTBA_MASK_WIDTH_POW4
#undef OYTBA_MASK_WIDTH_POW3
#undef OYTBA_MASK_WIDTH_POW2
#undef OYTBA_MASK_WIDTH
#undef OYTBA_MASK_T

#undef OYTBA_MASK_USING
#undef OYTBA_MASK_USING_ULONG
#undef OYTBA_MASK_USING_ULONG_LONG

#undef OYTBA_INLINE
#undef OYTBA_CPP_VERSION
#undef OYTBA_C_VERSION

#undef OYTBA_T
#undef OYTBA_CAT
#undef OYTBA_CAT_

#undef OYT_BITMASK_ALLOC_SIZE
