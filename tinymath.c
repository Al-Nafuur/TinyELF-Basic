/*
 * tinymath.c
 *
 * Single precision maths for the floating point build of TinyELF Basic:
 * SIN, COS, TAN, ATAN, LOG, EXP, SQR, INT and ^, in about 1.6 KB instead
 * of the 10 KB of newlib's libm. Relative error is a few units in the last
 * place of a float (about 1E-7), enough for the 6 digits BASIC prints.
 * Argument reduction of SIN/COS/TAN is good up to about 1E5.
 *
 * The polynomial coefficients are those of the single precision functions
 * of the Cephes Mathematical Library by Stephen L. Moshier (sinf, expf,
 * logf, atanf), which "may be used freely".
 *
 * Gopher2600 notes: it does not emulate VSQRT, so the build uses no FPU
 * and sqrt is a Newton iteration; and see the float compares at the end.
 */
#include <stdint.h>

typedef union { float f; uint32_t u; } fbits;

/* |x| >= 2^23 is already an integer (or inf/nan) */
float tm_trunc(float x) {
  fbits b;
  b.f = x;
  if (((b.u >> 23) & 0xFF) >= 150) return x;
  return (float)(int32_t)x;
}

float tm_floor(float x) {
  float t = tm_trunc(x);
  return t > x ? t - 1.0f : t;
}

/* first guess halves the exponent, four Newton steps reach full precision */
float tm_sqrt(float x) {
  fbits b;
  float y;
  int i;
  if (!(x > 0.0f)) {
    if (x == 0.0f) return x;
    b.u = 0x7FC00000u;                       /* nan for x < 0 */
    return b.f;
  }
  b.f = x;
  if (b.u >= 0x7F800000u) return x;
  b.u = (b.u >> 1) + 0x1FC00000u;
  y = b.f;
  for (i = 0; i < 4; i++) y = 0.5f * (y + x / y);
  return y;
}

/* y * 2^k by adding k to the exponent field */
static float ldexp_i(float y, int32_t k) {
  fbits b;
  int32_t e;
  b.f = y;
  e = (int32_t)((b.u >> 23) & 0xFF) + k;
  if (e >= 255) {
    b.u = (b.u & 0x80000000u) | 0x7F800000u; /* inf */
    return b.f;
  }
  if (e <= 0) return 0.0f;
  b.u = (b.u & 0x807FFFFFu) | ((uint32_t)e << 23);
  return b.f;
}

/* exp(x) = 2^k * exp(r), |r| <= ln2/2 */
float tm_exp(float x) {
  float k, r, z, y;
  if (x > 88.72283f) return ldexp_i(1.0f, 200);
  if (x < -103.0f) return 0.0f;
  k = tm_floor(x * 1.44269504088896341f + 0.5f);
  r = x - k * 0.693359375f - k * -2.12194440e-4f;
  z = r * r;
  y = ((((( 1.9875691500E-4f * r + 1.3981999507E-3f) * r + 8.3334519073E-3f) * r
        + 4.1665795894E-2f) * r + 1.6666665459E-1f) * r + 5.0000001201E-1f) * z + r + 1.0f;
  return ldexp_i(y, (int32_t)k);
}

/* log(x) = e*ln2 + log(m), sqrt(1/2) <= m < sqrt(2) */
float tm_log(float x) {
  fbits b;
  int32_t e;
  float m, z, y;
  b.f = x;
  if (!(x > 0.0f)) {
    b.u = x == 0.0f ? 0xFF800000u : 0x7FC00000u; /* -inf, nan */
    return b.f;
  }
  if (b.u >= 0x7F800000u) return x;
  e = (int32_t)(b.u >> 23) - 126;
  b.u = (b.u & 0x007FFFFFu) | 0x3F000000u;    /* m in [0.5, 1) */
  m = b.f;
  if (m < 0.70710678118654752f) {
    e--;
    m = m + m - 1.0f;
  } else
    m = m - 1.0f;
  z = m * m;
  y = (((((((( 7.0376836292E-2f * m - 1.1514610310E-1f) * m + 1.1676998740E-1f) * m
        - 1.2420140846E-1f) * m + 1.4249322787E-1f) * m - 1.6668057665E-1f) * m
        + 2.0000714765E-1f) * m - 2.4999993993E-1f) * m + 3.3333331174E-1f) * m * z;
  y += (float)e * -2.12194440e-4f;
  y += -0.5f * z;
  z = m + y;
  return z + (float)e * 0.693359375f;
}

/* sin(x) (cosine = 0) or cos(x) (cosine = 1): reduce to |r| <= pi/4 in
   quadrant k with pi/2 split into three parts, then a polynomial */
static float sincos_q(float x, int cosine) {
  float k, r, z, y;
  int32_t q;
  k = tm_floor(x * 0.63661977236758134f + 0.5f);
  q = (int32_t)k + cosine;
  r = ((x - k * 1.5703125f) - k * 4.837512969970703125e-4f) - k * 7.54978995489e-8f;
  z = r * r;
  if (q & 1)
    y = 1.0f - 0.5f * z + z * z * ((2.443315711809948E-5f * z - 1.388731625493765E-3f) * z
        + 4.166664568298827E-2f);
  else
    y = r + r * z * ((-1.9515295891E-4f * z + 8.3321608736E-3f) * z - 1.6666654611E-1f);
  return (q & 2) ? -y : y;
}

float tm_sin(float x) { return sincos_q(x, 0); }
float tm_cos(float x) { return sincos_q(x, 1); }
float tm_tan(float x) { return sincos_q(x, 0) / sincos_q(x, 1); }

float tm_atan(float x) {
  float y = 0.0f, z, s = 1.0f;
  if (x < 0.0f) {
    x = -x;
    s = -1.0f;
  }
  if (x > 2.414213562373095f) {               /* tan(3pi/8) */
    y = 1.5707963267948966f;
    x = -1.0f / x;
  } else if (x > 0.4142135623730950f) {       /* tan(pi/8) */
    y = 0.7853981633974483f;
    x = (x - 1.0f) / (x + 1.0f);
  }
  z = x * x;
  y += ((( 8.05374449538e-2f * z - 1.38776856032E-1f) * z + 1.99777106478E-1f) * z
        - 3.33329491539E-1f) * z * x + x;
  return s * y;
}

/* Integer exponents exactly by squaring (2^10 = 1024, (-2)^3 = -8), the
   rest via exp(y*log(x)). The sign of the exponent is taken in float: an
   integer abs() compiles to EOR with ASR, which Gopher2600 gets wrong for
   negative values. */
float tm_pow(float x, float y) {
  float r = 1.0f, b = x;
  int neg = y < 0.0f;
  if (neg) y = -y;
  if (y == tm_trunc(y) && y < 32768.0f) {
    uint32_t a = (uint32_t)(int32_t)y;
    while (a) {
      if (a & 1u) r *= b;
      b *= b;
      a >>= 1;
    }
    return neg ? 1.0f / r : r;
  }
  if (neg) y = -y;
  if (x == 0.0f) return y > 0.0f ? 0.0f : ldexp_i(1.0f, 200);
  return tm_exp(y * tm_log(x));
}

/* Float compares, replacing libgcc's. Its assembler version reads the
   carry flag after a TEQ without shift; Gopher2600 sets that flag from
   bit 31 of the operand, so compares of numbers with different signs come
   out wrong there (and "1E10" in a listing hangs the parser). These use
   only integer compares: the bit pattern of a float, with the sign folded
   in, orders like the value; +0 and -0 both map to 0. */
static int f_nan(uint32_t u) { return (u & 0x7FFFFFFFu) > 0x7F800000u; }
static int32_t f_key(uint32_t u) { return (u & 0x80000000u) ? -(int32_t)(u & 0x7FFFFFFFu) : (int32_t)u; }

int __aeabi_fcmpun(uint32_t a, uint32_t b) { return f_nan(a) || f_nan(b); }
int __aeabi_fcmpeq(uint32_t a, uint32_t b) { return !__aeabi_fcmpun(a, b) && f_key(a) == f_key(b); }
int __aeabi_fcmplt(uint32_t a, uint32_t b) { return !__aeabi_fcmpun(a, b) && f_key(a) < f_key(b); }
int __aeabi_fcmple(uint32_t a, uint32_t b) { return !__aeabi_fcmpun(a, b) && f_key(a) <= f_key(b); }
int __aeabi_fcmpgt(uint32_t a, uint32_t b) { return !__aeabi_fcmpun(a, b) && f_key(a) > f_key(b); }
int __aeabi_fcmpge(uint32_t a, uint32_t b) { return !__aeabi_fcmpun(a, b) && f_key(a) >= f_key(b); }
