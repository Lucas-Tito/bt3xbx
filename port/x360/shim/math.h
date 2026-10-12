#ifndef X360_SHIM_MATH_H
#define X360_SHIM_MATH_H
double sqrt(double);
double fabs(double);
double floor(double);
double ceil(double);
double sin(double);
double cos(double);
double atan2(double, double);
double pow(double, double);
double fmod(double, double);
double exp(double);
double log(double);
double tan(double);
double asin(double);
double acos(double);
double atan(double);
/* as funções float: na CRT do Xbox em C são macros sobre as de double, sem símbolo; aqui são funções de verdade
   (port/x360/src/math360.c), porque o código as declara de novo (vu0_b.h) e um nome que o port_libm.h já tomou (a
   biblioteca do PS2, Port_*) tem de seguir para ele */
float fabsf(float);
float sqrtf(float);
float floorf(float);
float ceilf(float);
float expf(float);
float logf(float);
float powf(float, float);
float fmodf(float, float);
float sinf(float);
float cosf(float);
float tanf(float);
float atan2f(float, float);
float atanf(float);
float asinf(float);
float acosf(float);
#endif
