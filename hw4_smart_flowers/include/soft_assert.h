#ifndef SOFT_ASSERT_H
#define SOFT_ASSERT_H

/**
 * @brief Reports a failed condition without terminating the program.
 *
 * The macro is intended for recoverable runtime checks. It prints the failed
 * expression together with the source location and returns 0 when the
 * condition is false. The caller decides how to recover from the error.
 */
int soft_assert_impl(int condition,
                     const char *expression,
                     const char *file,
                     int line,
                     const char *message);

#define SOFT_ASSERT(condition, message) \
    soft_assert_impl((condition), #condition, __FILE__, __LINE__, (message))

#endif
