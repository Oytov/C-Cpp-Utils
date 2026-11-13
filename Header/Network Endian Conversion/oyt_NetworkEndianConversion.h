#ifndef oyt_NetworkEndianConversion_h
#define oyt_NetworkEndianConversion_h
#include <stdint.h>
#include <limits.h>

#if CHAR_BIT != 8
	#error "char is required to be 8 bits wide"
#endif

#if !defined(UINT32_MAX) || !defined(UINT64_MAX)
	#error "Cannot implement endian conversion for floats as either uint32_t or uint64_t isn't available"
#endif
#include <string.h>
#include <float.h>
#include <math.h>

#define OYTNE_HTON16(Type, shift) Type o; unsigned char *p = (unsigned char*)&o; \
	p[0] = (unsigned char)(x >> (shift -  0)); p[1] = (unsigned char)(x >> (shift -  8))
#define OYTNE_HTON32(Type, shift) OYTNE_HTON16(Type, shift); \
	p[2] = (unsigned char)(x >> (shift - 16)); p[3] = (unsigned char)(x >> (shift - 24))
#define OYTNE_HTON64(Type, shift) OYTNE_HTON32(Type, shift); \
	p[4] = (unsigned char)(x >> (shift - 32)); p[5] = (unsigned char)(x >> (shift - 40)); p[6] = (unsigned char)(x >> (shift - 48)); p[7] = (unsigned char)(x >> (shift - 56))

#define OYTNE_NTOH16(Type, shift) const unsigned char *p = (const unsigned char*)&x; return \
	(Type)p[0] << (shift -  0) | (Type)p[1] << (shift -  8)
#define OYTNE_NTOH32(Type, shift) OYTNE_NTOH16(Type, shift) | \
	(Type)p[2] << (shift - 16) | (Type)p[3] << (shift - 24)
#define OYTNE_NTOH64(Type, shift) OYTNE_NTOH32(Type, shift) | \
	(Type)p[4] << (shift - 32) | (Type)p[5] << (shift - 40) | (Type)p[6] << (shift - 48) | (Type)p[7] << (shift - 56)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Host uint8_t to big endian, button that does nothing.
 */
static inline uint8_t oyt_htonU8(uint8_t x) {
	return x;
}

/**
 * @brief Big endian uint8_t to host, button that does nothing.
 */
static inline uint8_t oyt_ntohU8(uint8_t x) {
	return x;
}

/**
 * @brief Host uint16_t to big endian.
 */
static inline uint16_t oyt_htonU16(uint16_t x) {
	OYTNE_HTON16(uint16_t, 8);
	return o;
}

/**
 * @brief Big endian uint16_t to host.
 */
static inline uint16_t oyt_ntohU16(uint16_t x) {
	OYTNE_NTOH16(uint16_t, 8);
}

/**
 * @brief Host uint32_t to big endian.
 */
static inline uint32_t oyt_htonU32(uint32_t x) {
	OYTNE_HTON32(uint32_t, 24);
	return o;
}

/**
 * @brief Big endian uint32_t to host.
 */
static inline uint32_t oyt_ntohU32(uint32_t x) {
	OYTNE_NTOH32(uint32_t, 24);
}

/**
 * @brief Host uint64_t to big endian.
 */
static inline uint64_t oyt_htonU64(uint64_t x) {
	OYTNE_HTON64(uint64_t, 56);
	return o;
}

/**
 * @brief Big endian uint64_t to host.
 */
static inline uint64_t oyt_ntohU64(uint64_t x) {
	OYTNE_NTOH64(uint64_t, 56);
}

#undef OYTNE_NTOH64
#undef OYTNE_NTOH32
#undef OYTNE_NTOH16
#undef OYTNE_HTON64
#undef OYTNE_HTON32
#undef OYTNE_HTON16


#define OYTNE_MASK32(n) ((UINT32_C(1) << (n)) - 1)
#define OYTNE_MASK64(n) ((UINT64_C(1) << (n)) - 1)

/* To stop an implicit cast from a uint32_t to a float when calling oyt_htonFP32() */
typedef struct {uint32_t f;} oyt_BigEndianFP32;

/**
 * @brief Host float to big endian IEEE-754 single precision.
 */
static inline oyt_BigEndianFP32 oyt_htonFP32(float x) {
#if FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MIN_EXP == -125 && FLT_MAX_EXP == 128
	if (sizeof(float) == 4) {
		const float f = -1234.5f;
		uint32_t check;
		memcpy(&check, &f, 4);

		if (check == UINT32_C(0xc49a5000)) {
			oyt_BigEndianFP32 out;
			memcpy(&out.f, &x, 4);
			out.f = oyt_htonU32(out.f);
			return out;
		}
	}
#endif
	uint32_t iRepr = 0;
	if (signbit(x)) {
		iRepr |= UINT32_C(1) << 31;
		x = copysignf(x, 1.0f);
	}

	switch (fpclassify(x)) {
	case FP_ZERO:
		break;

	case FP_NAN:
		iRepr |= UINT32_C(1) << 22;
	case FP_INFINITE:
		iRepr |= OYTNE_MASK32(8) << 23;
		break;

	default: {
		int exponent;
		uint32_t mantissa = (uint32_t)ldexpf(frexpf(x, &exponent), 24);
		const int biasedExp = exponent + 126;

		// Too large -> infinite
		if (biasedExp >= 255) {
			iRepr |= OYTNE_MASK32(8) << 23;
		}

		// Normal
		else if (biasedExp >= 1) {
			iRepr |= (uint32_t)biasedExp << 23;
			iRepr |= mantissa & OYTNE_MASK32(23);
		}

		// Sub-normal
		else {
			const unsigned rShift = (unsigned)(1 - biasedExp);

			// Too small -> zero
			if (rShift > 24) break;

			// Round to nearest even
			uint32_t rem  = mantissa & OYTNE_MASK32(rShift);
			uint32_t half = UINT32_C(1) << (rShift - 1);
			mantissa >>= rShift;
			mantissa += rem > half || (rem == half && (mantissa & 1));

			iRepr |= mantissa;
		}
		break;
	}
	}

	oyt_BigEndianFP32 out;
	out.f = iRepr;
	return out;
}

/**
 * @brief Big endian IEEE-754 single precision to host float.
 */
static inline float oyt_ntohFP32(oyt_BigEndianFP32 x) {
	x.f = oyt_ntohU32(x.f);
#if FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MIN_EXP == -125 && FLT_MAX_EXP == 128
	if (sizeof(float) == 4) {
		const float f = -1234.5f;
		uint32_t check;
		memcpy(&check, &f, 4);

		if (check == UINT32_C(0xc49a5000)) {
			float out;
			memcpy(&out, &x.f, 4);
			return out;
		}
	}
#endif
	const float sign = (x.f & (UINT32_C(1) << 31)) ? -1.0f : 1.0f;

	const int biasedExp = (x.f >> 23) & OYTNE_MASK32(8);
	uint32_t mantissa = x.f & OYTNE_MASK32(23);
	if (biasedExp == OYTNE_MASK32(8)) {
		if (mantissa)
			return copysignf(NAN, sign);
		else
			return copysignf(INFINITY, sign);
	}

	if (biasedExp == 0)
		return copysignf(ldexpf((float)mantissa, -149), sign);

	mantissa |= (UINT32_C(1) << 23);
	return copysignf(ldexpf((float)mantissa, biasedExp - 126 - 24), sign);
}

/* To stop an implicit cast from a uint32_t to a double when calling oyt_htonFP64() */
typedef struct {uint64_t f;} oyt_BigEndianFP64;

/**
 * @brief Host float to big endian IEEE-754 double precision.
 */
static inline oyt_BigEndianFP64 oyt_htonFP64(double x) {
#if FLT_RADIX == 2 && DBL_MANT_DIG == 53 && DBL_MIN_EXP == -1021 && DBL_MAX_EXP == 1024
	if (sizeof(double) == 8) {
		const double f = -1e19;
		uint64_t check;
		memcpy(&check, &f, 8);

		if (check == UINT64_C(0xc3e158e460913d00)) {
			oyt_BigEndianFP64 out;
			memcpy(&out.f, &x, 8);
			out.f = oyt_htonU64(out.f);
			return out;
		}
	}
#endif
	uint64_t iRepr = 0;
	if (signbit(x)) {
		iRepr |= UINT64_C(1) << 63;
		x = copysign(x, 1.0);
	}

	switch (fpclassify(x)) {
	case FP_ZERO:
		break;

	case FP_NAN:
		iRepr |= UINT64_C(1) << 51;
	case FP_INFINITE:
		iRepr |= OYTNE_MASK64(11) << 52;
		break;

	default: {
		int exponent;
		uint64_t mantissa = (uint64_t)ldexp(frexp(x, &exponent), 53);
		const int biasedExp = exponent + 1022;

		// Too large -> infinite
		if (biasedExp >= 2047) {
			iRepr |= OYTNE_MASK64(11) << 52;
		}

		// Normal
		else if (biasedExp >= 1) {
			iRepr |= (uint64_t)biasedExp << 52;
			iRepr |= mantissa & OYTNE_MASK64(52);
		}

		// Sub-normal
		else {
			const unsigned rShift = (unsigned)(1 - biasedExp);

			// Too small -> zero
			if (rShift > 53) break;

			// Round to nearest even
			const uint64_t rem  = mantissa & OYTNE_MASK64(rShift);
			const uint64_t half = UINT64_C(1) << (rShift - 1);
			mantissa >>= rShift;
			mantissa += rem > half || (rem == half && (mantissa & 1));

			iRepr |= mantissa;
		}
		break;
	}
	}

	oyt_BigEndianFP64 out;
	out.f = iRepr;
	return out;
}

/**
 * @brief Big endian IEEE-754 double precision to host float.
 */
static inline double oyt_ntohFP64(oyt_BigEndianFP64 x) {
	x.f = oyt_ntohU64(x.f);
#if FLT_RADIX == 2 && DBL_MANT_DIG == 53 && DBL_MIN_EXP == -1021 && DBL_MAX_EXP == 1024
	if (sizeof(double) == 8) {
		const double f = -1e19;
		uint64_t check;
		memcpy(&check, &f, 8);

		if (check == UINT64_C(0xc3e158e460913d00)) {
			double out;
			memcpy(&out, &x.f, 8);
			return out;
		}
	}
#endif
	const double sign = (x.f & (UINT64_C(1) << 63)) ? -1.0 : 1.0;

	const int biasedExp = (x.f >> 52) & OYTNE_MASK64(11);
	uint64_t mantissa   = x.f & OYTNE_MASK64(52);
	if (biasedExp == OYTNE_MASK64(11)) {
		if (mantissa)
			return copysign(NAN, sign);
		else
			return copysign(INFINITY, sign);
	}

	if (biasedExp == 0)
		return copysign(ldexp((double)mantissa, - 1022 - 52), sign);

	mantissa |= (UINT64_C(1) << 52);
	return copysign(ldexp((double)mantissa, biasedExp - 1022 - 53), sign);
}

#undef OYTNE_MASK64
#undef OYTNE_MASK32

#ifdef __cplusplus
}
#endif

#endif // oyt_NetworkEndianConversion_h
