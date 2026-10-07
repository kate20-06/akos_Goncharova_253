#include "soft_assert.h"

#include <stdio.h>

int soft_assert_impl(int condition,
                     const char *expression,
                     const char *file,
                     int line,
                     const char *message) {
    if (condition) {
        return 1;
    }

    fprintf(stderr,
            "Soft assert failed: %s\n  condition: %s\n  at %s:%d\n",
            message != NULL ? message : "runtime check failed",
            expression != NULL ? expression : "(unknown)",
            file != NULL ? file : "(unknown)",
            line);
    return 0;
}
