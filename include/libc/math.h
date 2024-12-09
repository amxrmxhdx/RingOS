#ifndef RINGOS_MATH_H
#define RINGOS_MATH_H

// Define constants
#define PI 3.14159265358979323846
#define E  2.71828182845904523536

// Basic arithmetic functions
static inline double add(double a, double b) {
    return a + b;
}

static inline double subtract(double a, double b) {
    return a - b;
}

static inline double multiply(double a, double b) {
    return a * b;
}

static inline double divide(double a, double b) {
    if (b == 0.0) {
        // Handle division by zero
        return 0.0; // You might want to define a specific behavior
    }
    return a / b;
}

// Helper function for absolute value
static inline int abs(int x) {
    return x < 0 ? -x : x;
}

// Basic power functions
double power(double base, int exp) {
    double result = 1.0;
    for (int i = 0; i < abs(exp); i++) {
        result *= base;
    }
    return exp < 0 ? 1.0 / result : result;
}

double sqrt(double x) {
    if (x < 0) {
        return -1.0; // Indicate an error for negative input
    }
    double guess = x / 2.0;
    for (int i = 0; i < 20; i++) {
        guess = (guess + x / guess) / 2.0;
    }
    return guess;
}

// Trigonometric functions (using Taylor series approximations)
double sin(double x) {
    double term = x;
    double result = x;
    int n = 1;
    for (int i = 1; i <= 10; i++) {
        n += 2;
        term *= -x * x / (n * (n - 1));
        result += term;
    }
    return result;
}

double cos(double x) {
    double term = 1.0;
    double result = 1.0;
    int n = 0;
    for (int i = 1; i <= 10; i++) {
        n += 2;
        term *= -x * x / (n * (n - 1));
        result += term;
    }
    return result;
}

double tan(double x) {
    double cosine = cos(x);
    if (cosine == 0.0) {
        return 0.0; // Handle undefined tan(x)
    }
    return sin(x) / cosine;
}

// Exponential functions
double exp(double x) {
    double term = 1.0;
    double result = 1.0;
    for (int i = 1; i <= 20; i++) {
        term *= x / i;
        result += term;
    }
    return result;
}

double log(double x) {
    if (x <= 0.0) {
        return -1.0; // Indicate an error for non-positive input
    }
    double result = 0.0;
    double term = (x - 1) / (x + 1);
    double term_squared = term * term;
    double fraction = term;
    for (int i = 1; i <= 20; i += 2) {
        result += fraction / i;
        fraction *= term_squared;
    }
    return 2 * result;
}

#endif // BAREMETAL_MATH_H
