#ifndef FORMULA_PARSER_H
#define FORMULA_PARSER_H
#include <Arduino.h>

// Supported math functions: +, -, *, /, sqrt(x), log(x), pow(x, y)
// Only one variable: x (the raw sensor value)
// Example formulas: "(x * 1.8) + 32", "sqrt(x * 9.8)", "log(x + 1) * 100"

double applyFormula(const char* formula, double x);

#endif // FORMULA_PARSER_H
