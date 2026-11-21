/*
 * tinymath.h
 *
 * Single precision maths for the floating point build of TinyELF Basic,
 * see tinymath.c. main.c includes this after basic.h when number_t is
 * float: the interpreter keeps calling the C library names, which are
 * mapped to the compact versions here instead of newlib's libm.
 */
#ifndef TINYMATH_H
#define TINYMATH_H

float tm_trunc(float x);
float tm_floor(float x);
float tm_sqrt(float x);
float tm_exp(float x);
float tm_log(float x);
float tm_sin(float x);
float tm_cos(float x);
float tm_tan(float x);
float tm_atan(float x);
float tm_pow(float x, float y);

#define trunc tm_trunc
#define floor tm_floor
#define sqrt  tm_sqrt
#define exp   tm_exp
#define log   tm_log
#define sin   tm_sin
#define cos   tm_cos
#define tan   tm_tan
#define atan  tm_atan
#define pow   tm_pow
#define fabs  __builtin_fabsf

#endif
