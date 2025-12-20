/**
 * Unity Test Framework Configuration
 *
 * Required by Unity for native builds.
 */

#ifndef UNITY_CONFIG_H
#define UNITY_CONFIG_H

// Use standard integer types
#include <stdint.h>

// Output configuration - use printf
#include <stdio.h>
#define UNITY_OUTPUT_CHAR(c) putchar(c)
#define UNITY_OUTPUT_FLUSH() fflush(stdout)

// Enable 64-bit integer support
#define UNITY_SUPPORT_64

// Enable floating point
#define UNITY_INCLUDE_FLOAT
#define UNITY_INCLUDE_DOUBLE

// Pointer size for native platform
#define UNITY_POINTER_WIDTH 64

#endif // UNITY_CONFIG_H
