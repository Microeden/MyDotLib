#pragma once

#include <Arduino.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

// Some Arduino cores ship a reduced <limits.h> that does not expose the
// C99 LLONG_MIN/LLONG_MAX macros. Keep the parser self-contained instead of
// depending on those optional names.
static const long long MYDOT_INTEGER_MIN = (-9223372036854775807LL - 1LL);
static const long long MYDOT_INTEGER_MAX = 9223372036854775807LL;

class MyDotIntegerExpressionParser {
 public:
  explicit MyDotIntegerExpressionParser(const String& expression)
      : expression_(expression), position_(0) {}

  bool parse(long long& result) {
    if (!parseSum(result)) return false;
    skipSpaces();
    return position_ == expression_.length();
  }

 private:
  const String& expression_;
  size_t position_;

  void skipSpaces() {
    while (position_ < expression_.length() &&
           isspace((unsigned char)expression_[position_])) {
      ++position_;
    }
  }

  bool parseSum(long long& result) {
    if (!parseProduct(result)) return false;
    while (true) {
      skipSpaces();
      if (position_ >= expression_.length() ||
          (expression_[position_] != '+' && expression_[position_] != '-')) {
        return true;
      }
      const char operation = expression_[position_++];
      long long right = 0;
      if (!parseProduct(right)) return false;
      if (operation == '+') {
        if (!checkedAdd(result, right, result)) return false;
      } else if (!checkedSubtract(result, right, result)) {
        return false;
      }
    }
  }

  bool parseProduct(long long& result) {
    if (!parseUnary(result)) return false;
    while (true) {
      skipSpaces();
      if (position_ >= expression_.length() ||
          (expression_[position_] != '*' && expression_[position_] != '/' &&
           expression_[position_] != '%')) {
        return true;
      }
      const char operation = expression_[position_++];
      long long right = 0;
      if (!parseUnary(right)) return false;
      if (operation == '*') {
        if (!checkedMultiply(result, right, result)) return false;
      } else {
        if (right == 0 || (result == MYDOT_INTEGER_MIN && right == -1)) return false;
        result = operation == '/' ? result / right : result % right;
      }
    }
  }

  bool parseUnary(long long& result) {
    skipSpaces();
    if (position_ < expression_.length() &&
        (expression_[position_] == '+' || expression_[position_] == '-')) {
      const char operation = expression_[position_++];
      if (!parseUnary(result)) return false;
      if (operation == '-') {
        if (result == MYDOT_INTEGER_MIN) return false;
        result = -result;
      }
      return true;
    }
    return parsePrimary(result);
  }

  bool parsePrimary(long long& result) {
    skipSpaces();
    if (position_ >= expression_.length()) return false;
    if (expression_[position_] == '(') {
      ++position_;
      if (!parseSum(result)) return false;
      skipSpaces();
      if (position_ >= expression_.length() || expression_[position_] != ')') return false;
      ++position_;
      return true;
    }

    const char* start = expression_.c_str() + position_;
    const int base = start[0] == '0' && (start[1] == 'x' || start[1] == 'X') ? 16 : 10;
    errno = 0;
    char* end = nullptr;
    const long long parsed = strtoll(start, &end, base);
    if (end == start || errno == ERANGE) return false;
    position_ += (size_t)(end - start);
    result = parsed;
    return true;
  }

  static bool checkedAdd(long long left, long long right, long long& result) {
    if ((right > 0 && left > MYDOT_INTEGER_MAX - right) ||
        (right < 0 && left < MYDOT_INTEGER_MIN - right)) return false;
    result = left + right;
    return true;
  }

  static bool checkedSubtract(long long left, long long right, long long& result) {
    if ((right < 0 && left > MYDOT_INTEGER_MAX + right) ||
        (right > 0 && left < MYDOT_INTEGER_MIN + right)) return false;
    result = left - right;
    return true;
  }

  static bool checkedMultiply(long long left, long long right, long long& result) {
    if (left == 0 || right == 0) {
      result = 0;
      return true;
    }
    if ((left == -1 && right == MYDOT_INTEGER_MIN) ||
        (right == -1 && left == MYDOT_INTEGER_MIN)) return false;
    if (left > 0) {
      if ((right > 0 && left > MYDOT_INTEGER_MAX / right) ||
          (right < 0 && right < MYDOT_INTEGER_MIN / left)) return false;
    } else {
      if ((right > 0 && left < MYDOT_INTEGER_MIN / right) ||
          (right < 0 && left < MYDOT_INTEGER_MAX / right)) return false;
    }
    result = left * right;
    return true;
  }
};

inline bool myDotEvaluateIntegerExpression(const String& expression, long long& result) {
  MyDotIntegerExpressionParser parser(expression);
  return parser.parse(result);
}

inline String myDotIntegerToString(long long value) {
  const bool negative = value < 0;
  unsigned long long magnitude = negative
      ? (unsigned long long)(-(value + 1)) + 1ULL
      : (unsigned long long)value;
  char digits[21];
  size_t cursor = sizeof(digits) - 1;
  digits[cursor] = '\0';
  do {
    digits[--cursor] = (char)('0' + (magnitude % 10ULL));
    magnitude /= 10ULL;
  } while (magnitude > 0);
  String result;
  if (negative) result += '-';
  result += &digits[cursor];
  return result;
}
