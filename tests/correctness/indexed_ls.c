/* Indexed load/store: lu1qx/su1qx use register+register addressing
   (su1qx Rd, Rs, Rt → store Rd to [Rs+Rt]). The non-indexed lu1q/su1q
   only support immediate offset.

   Verifies: backend recognizes indexed addressing patterns and emits
   the right instruction (not a manual addiu+lu1q sequence). */
#include <mxu2.h>
#include <stdio.h>
#include <stdint.h>

static int8_t buf[256] __attribute__((aligned(16)));
static volatile int8_t out[256] __attribute__((aligned(16)));

__attribute__((noinline))
static void copy_indexed(int idx) {
    /* Force runtime index so compiler must use lu1qx/su1qx. */
    v16i8 v = __builtin_mxu2_lu1qx(buf, idx * 16);
    __builtin_mxu2_su1qx(v, (void *)out, idx * 16);
}

int main(void) {
    for (int i = 0; i < 256; i++) buf[i] = (int8_t)(i & 0xff);
    for (int i = 0; i < 256; i++) out[i] = 0;

    for (int i = 0; i < 16; i++) copy_indexed(i);

    int fails = 0;
    for (int i = 0; i < 256; i++) {
        if (out[i] != buf[i]) { fails++; if (fails <= 3) printf("FAIL [%d] got %d want %d\n", i, out[i], buf[i]); }
    }
    if (fails == 0) puts("INDEXED OK");
    return fails;
}
