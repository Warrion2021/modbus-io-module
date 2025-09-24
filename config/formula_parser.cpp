#include "formula_parser.h"
#include <math.h>
#include <string.h>
#include <ctype.h>

// Simple parser: supports +, -, *, /, sqrt(x), log(x), pow(x, y)
// Only one variable: x
// No external libraries, safe for embedded

static double parseNumber(const char*& s) {
    double val = 0.0;
    bool neg = false;
    while (isspace(*s)) ++s;
    if (*s == '-') { neg = true; ++s; }
    while (isdigit(*s) || *s == '.') {
        char buf[16] = {0}; int i = 0;
        while ((isdigit(*s) || *s == '.') && i < 15) buf[i++] = *s++;
        val = atof(buf);
    }
    return neg ? -val : val;
}

static double evalSimple(const char* formula, double x) {
    // Only supports: x, +, -, *, /
    // Example: "x * 1.8 + 32"
    double result = x;
    const char* s = formula;
    while (*s) {
        while (isspace(*s)) ++s;
        if (*s == 'x') { ++s; }
        else if (*s == '*') { ++s; result *= parseNumber(s); }
        else if (*s == '/') { ++s; result /= parseNumber(s); }
        else if (*s == '+') { ++s; result += parseNumber(s); }
        else if (*s == '-') { ++s; result -= parseNumber(s); }
        else { ++s; }
    }
    return result;
}

double applyFormula(const char* formula, double x) {
    // Handle sqrt(x), log(x), pow(x, y)
    if (strstr(formula, "sqrt")) {
        return sqrt(evalSimple(strchr(formula, '(') + 1, x));
    } else if (strstr(formula, "log")) {
        return log(evalSimple(strchr(formula, '(') + 1, x));
    } else if (strstr(formula, "pow")) {
        // Format: pow(x, y)
        const char* args = strchr(formula, '(') + 1;
        double base = x, exp = 1.0;
        sscanf(args, "x,%lf", &exp);
        return pow(base, exp);
    } else {
        return evalSimple(formula, x);
    }
}
