#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

bool overflow(int64_t *A, size_t n) {
    // All elements of A are non-negative
    int64_t sum = 0;
    for ( size_t i = 0; i < n; i++ ) {
        sum += A[i];
        if ( sum < A[i] ) {
            return true;
        }
    }
    return false;
}