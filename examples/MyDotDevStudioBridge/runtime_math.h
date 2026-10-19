#ifndef MYDOT_RUNTIME_MATH_H
#define MYDOT_RUNTIME_MATH_H

// Pure numeric operations for the Dev Studio Bridge. Parsing, variable
// substitution and result storage remain in the sketch so this file can be
// checked independently of the selected Arduino board.
#include <math.h>
#include <stdint.h>
#include <string.h>

enum MyDotMathStatus : uint8_t {
  MYDOT_MATH_OK,
  MYDOT_MATH_UNKNOWN_OPERATION,
  MYDOT_MATH_DOMAIN_ERROR,
  MYDOT_MATH_NONFINITE_RESULT
};

static const double MYDOT_MATH_PI = 3.14159265358979323846;
static const double MYDOT_MATH_E = 2.71828182845904523536;

inline bool myDotMathIs(const char* operation, const char* expected) {
  return strcmp(operation, expected) == 0;
}

// The final token of every MATH command is always the destination variable.
// The arity excludes that token; -1 means the operation is not supported.
inline int8_t myDotMathArity(const char* operation) {
  if (myDotMathIs(operation, "PI") || myDotMathIs(operation, "TAU") ||
      myDotMathIs(operation, "E")) return 0;

  if (myDotMathIs(operation, "ABS") || myDotMathIs(operation, "NEG") ||
      myDotMathIs(operation, "SIGN") || myDotMathIs(operation, "SQRT") ||
      myDotMathIs(operation, "CBRT") || myDotMathIs(operation, "EXP") ||
      myDotMathIs(operation, "LN") || myDotMathIs(operation, "LOG10") ||
      myDotMathIs(operation, "LOG2") || myDotMathIs(operation, "FLOOR") ||
      myDotMathIs(operation, "CEIL") || myDotMathIs(operation, "ROUND") ||
      myDotMathIs(operation, "TRUNC") || myDotMathIs(operation, "SIN") ||
      myDotMathIs(operation, "COS") || myDotMathIs(operation, "TAN") ||
      myDotMathIs(operation, "ASIN") || myDotMathIs(operation, "ACOS") ||
      myDotMathIs(operation, "ATAN") || myDotMathIs(operation, "SIN_DEG") ||
      myDotMathIs(operation, "COS_DEG") || myDotMathIs(operation, "TAN_DEG") ||
      myDotMathIs(operation, "ASIN_DEG") || myDotMathIs(operation, "ACOS_DEG") ||
      myDotMathIs(operation, "ATAN_DEG") || myDotMathIs(operation, "TO_DEG") ||
      myDotMathIs(operation, "TO_RAD") || myDotMathIs(operation, "SINH") ||
      myDotMathIs(operation, "COSH") || myDotMathIs(operation, "TANH") ||
      myDotMathIs(operation, "ASINH") || myDotMathIs(operation, "ACOSH") ||
      myDotMathIs(operation, "ATANH")) return 1;

  if (myDotMathIs(operation, "ADD") || myDotMathIs(operation, "SUB") ||
      myDotMathIs(operation, "MUL") || myDotMathIs(operation, "DIV") ||
      myDotMathIs(operation, "MOD") || myDotMathIs(operation, "POW") ||
      myDotMathIs(operation, "MIN") || myDotMathIs(operation, "MAX") ||
      myDotMathIs(operation, "AVG") || myDotMathIs(operation, "HYPOT") ||
      myDotMathIs(operation, "ATAN2") || myDotMathIs(operation, "ATAN2_DEG") ||
      myDotMathIs(operation, "ROUND_TO")) return 2;

  if (myDotMathIs(operation, "CLAMP") || myDotMathIs(operation, "LERP") ||
      myDotMathIs(operation, "NORM3")) return 3;

  if (myDotMathIs(operation, "MAP")) return 5;
  return -1;
}

inline MyDotMathStatus myDotMathEvaluate(const char* operation,
                                         const double* value, double& result) {
  const double a = value[0];
  const double b = value[1];
  const double c = value[2];

  if (myDotMathIs(operation, "PI")) result = MYDOT_MATH_PI;
  else if (myDotMathIs(operation, "TAU")) result = 2.0 * MYDOT_MATH_PI;
  else if (myDotMathIs(operation, "E")) result = MYDOT_MATH_E;
  else if (myDotMathIs(operation, "ADD")) result = a + b;
  else if (myDotMathIs(operation, "SUB")) result = a - b;
  else if (myDotMathIs(operation, "MUL")) result = a * b;
  else if (myDotMathIs(operation, "DIV")) {
    if (b == 0.0) return MYDOT_MATH_DOMAIN_ERROR;
    result = a / b;
  } else if (myDotMathIs(operation, "MOD")) {
    if (b == 0.0) return MYDOT_MATH_DOMAIN_ERROR;
    result = fmod(a, b);
  } else if (myDotMathIs(operation, "POW")) result = pow(a, b);
  else if (myDotMathIs(operation, "MIN")) result = a < b ? a : b;
  else if (myDotMathIs(operation, "MAX")) result = a > b ? a : b;
  else if (myDotMathIs(operation, "AVG")) result = a / 2.0 + b / 2.0;
  else if (myDotMathIs(operation, "HYPOT")) result = hypot(a, b);
  else if (myDotMathIs(operation, "ATAN2") ||
           myDotMathIs(operation, "ATAN2_DEG")) {
    // atan2(0, 0) has no meaningful direction for accelerometer data.
    if (a == 0.0 && b == 0.0) return MYDOT_MATH_DOMAIN_ERROR;
    result = atan2(a, b);  // Arguments are y, x, in that order.
    if (myDotMathIs(operation, "ATAN2_DEG")) result *= 180.0 / MYDOT_MATH_PI;
  } else if (myDotMathIs(operation, "ROUND_TO")) {
    if (b < 0.0 || b > 8.0 || floor(b) != b) return MYDOT_MATH_DOMAIN_ERROR;
    const double scale = pow(10.0, b);
    result = round(a * scale) / scale;
  } else if (myDotMathIs(operation, "ABS")) result = fabs(a);
  else if (myDotMathIs(operation, "NEG")) result = -a;
  else if (myDotMathIs(operation, "SIGN")) result = (a > 0.0) - (a < 0.0);
  else if (myDotMathIs(operation, "SQRT")) {
    if (a < 0.0) return MYDOT_MATH_DOMAIN_ERROR;
    result = sqrt(a);
  } else if (myDotMathIs(operation, "CBRT")) {
    result = a < 0.0 ? -pow(-a, 1.0 / 3.0) : pow(a, 1.0 / 3.0);
  } else if (myDotMathIs(operation, "EXP")) result = exp(a);
  else if (myDotMathIs(operation, "LN") || myDotMathIs(operation, "LOG10") ||
           myDotMathIs(operation, "LOG2")) {
    if (a <= 0.0) return MYDOT_MATH_DOMAIN_ERROR;
    if (myDotMathIs(operation, "LN")) result = log(a);
    else if (myDotMathIs(operation, "LOG10")) result = log10(a);
    else result = log(a) / log(2.0);
  } else if (myDotMathIs(operation, "FLOOR")) result = floor(a);
  else if (myDotMathIs(operation, "CEIL")) result = ceil(a);
  else if (myDotMathIs(operation, "ROUND")) result = round(a);
  else if (myDotMathIs(operation, "TRUNC")) result = trunc(a);
  else if (myDotMathIs(operation, "SIN")) result = sin(a);
  else if (myDotMathIs(operation, "COS")) result = cos(a);
  else if (myDotMathIs(operation, "TAN")) {
    if (fabs(cos(a)) < 1e-12) return MYDOT_MATH_DOMAIN_ERROR;
    result = tan(a);
  } else if (myDotMathIs(operation, "ASIN") || myDotMathIs(operation, "ASIN_DEG") ||
             myDotMathIs(operation, "ACOS") || myDotMathIs(operation, "ACOS_DEG")) {
    if (a < -1.0 || a > 1.0) return MYDOT_MATH_DOMAIN_ERROR;
    result = (myDotMathIs(operation, "ASIN") || myDotMathIs(operation, "ASIN_DEG"))
               ? asin(a) : acos(a);
    if (myDotMathIs(operation, "ASIN_DEG") || myDotMathIs(operation, "ACOS_DEG"))
      result *= 180.0 / MYDOT_MATH_PI;
  } else if (myDotMathIs(operation, "ATAN")) result = atan(a);
  else if (myDotMathIs(operation, "ATAN_DEG")) result = atan(a) * 180.0 / MYDOT_MATH_PI;
  else if (myDotMathIs(operation, "SIN_DEG")) result = sin(a * MYDOT_MATH_PI / 180.0);
  else if (myDotMathIs(operation, "COS_DEG")) result = cos(a * MYDOT_MATH_PI / 180.0);
  else if (myDotMathIs(operation, "TAN_DEG")) {
    const double radians = a * MYDOT_MATH_PI / 180.0;
    if (fabs(cos(radians)) < 1e-12) return MYDOT_MATH_DOMAIN_ERROR;
    result = tan(radians);
  } else if (myDotMathIs(operation, "TO_DEG")) result = a * 180.0 / MYDOT_MATH_PI;
  else if (myDotMathIs(operation, "TO_RAD")) result = a * MYDOT_MATH_PI / 180.0;
  else if (myDotMathIs(operation, "SINH")) result = sinh(a);
  else if (myDotMathIs(operation, "COSH")) result = cosh(a);
  else if (myDotMathIs(operation, "TANH")) result = tanh(a);
  else if (myDotMathIs(operation, "ASINH")) result = asinh(a);
  else if (myDotMathIs(operation, "ACOSH")) {
    if (a < 1.0) return MYDOT_MATH_DOMAIN_ERROR;
    result = acosh(a);
  } else if (myDotMathIs(operation, "ATANH")) {
    if (a <= -1.0 || a >= 1.0) return MYDOT_MATH_DOMAIN_ERROR;
    result = atanh(a);
  } else if (myDotMathIs(operation, "CLAMP")) {
    if (b > c) return MYDOT_MATH_DOMAIN_ERROR;
    result = a < b ? b : (a > c ? c : a);
  } else if (myDotMathIs(operation, "LERP")) result = a + (b - a) * c;
  else if (myDotMathIs(operation, "NORM3")) result = hypot(hypot(a, b), c);
  else if (myDotMathIs(operation, "MAP")) {
    if (value[1] == value[2]) return MYDOT_MATH_DOMAIN_ERROR;
    result = value[3] + (a - value[1]) * (value[4] - value[3]) /
                          (value[2] - value[1]);
  } else return MYDOT_MATH_UNKNOWN_OPERATION;

  return isfinite(result) ? MYDOT_MATH_OK : MYDOT_MATH_NONFINITE_RESULT;
}

#endif  // MYDOT_RUNTIME_MATH_H
