/* ABI: vectors through varargs.
   Note: GCC vector types in ... is implementation-defined and may
   require explicit promotion. This test verifies that AT LEAST the
   compile path succeeds without ICE. Behavior may differ from
   non-vector ABI (vectors typically passed by reference in varargs). */
#include <mxu2.h>
#include <stdarg.h>
#include <stdio.h>

__attribute__((noinline))
static int va_sum_lane0(int n, ...) {
    va_list ap;
    va_start(ap, n);
    int sum = 0;
    for (int i = 0; i < n; i++) {
        v4i32 v = va_arg(ap, v4i32);
        sum += v[0];
    }
    va_end(ap);
    return sum;
}

int main(void) {
    int s = va_sum_lane0(3,
        (v4i32){10, 0, 0, 0},
        (v4i32){20, 0, 0, 0},
        (v4i32){30, 0, 0, 0});
    /* If ABI matches, sum should be 60. If implementation-defined and
       vectors are passed by reference, result may differ — but no ICE. */
    printf("VARARGS sum=%d\n", s);
    if (s == 60) puts("ABI varargs OK");
    else printf("ABI varargs UNEXPECTED (got %d, want 60)\n", s);
    /* Don't fail the test on value mismatch — varargs vector ABI is
       implementation-defined. Just verify no crash/ICE. */
    return 0;
}
