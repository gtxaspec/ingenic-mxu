/* Layer C bench 2: ChaCha20 round body. Stresses long chain + splat
   + shuffle simultaneously. Was the trigger for the v8/v10/v12 fixes.  */
#include <mxu2.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#define N (200 * 1024)
static int32_t state[16] __attribute__((aligned(16)));

#define ROT(v, n) ((v4i32)__builtin_mxu2_orv( \
    (v16i8)__builtin_mxu2_sll_w((v), ((v4i32){(n),(n),(n),(n)})), \
    (v16i8)__builtin_mxu2_srl_w((v), ((v4i32){32-(n),32-(n),32-(n),32-(n)}))))
#define SHUF(v, a, b, c, d) __builtin_shuffle((v), (v4i32){(a),(b),(c),(d)})

__attribute__((noinline))
static void chacha20(int32_t *st) {
    v4i32 a = (v4i32)__builtin_mxu2_lu1q((v16i8 *)st, 0);
    v4i32 b = (v4i32)__builtin_mxu2_lu1q((v16i8 *)st, 16);
    v4i32 c = (v4i32)__builtin_mxu2_lu1q((v16i8 *)st, 32);
    v4i32 d = (v4i32)__builtin_mxu2_lu1q((v16i8 *)st, 48);
    for (int i = 0; i < 10; i++) {
        a = __builtin_mxu2_add_w(a, b);
        d = (v4i32)__builtin_mxu2_xorv((v16i8)d, (v16i8)a); d = ROT(d, 16);
        c = __builtin_mxu2_add_w(c, d);
        b = (v4i32)__builtin_mxu2_xorv((v16i8)b, (v16i8)c); b = ROT(b, 12);
        a = __builtin_mxu2_add_w(a, b);
        d = (v4i32)__builtin_mxu2_xorv((v16i8)d, (v16i8)a); d = ROT(d, 8);
        c = __builtin_mxu2_add_w(c, d);
        b = (v4i32)__builtin_mxu2_xorv((v16i8)b, (v16i8)c); b = ROT(b, 7);
        b = SHUF(b, 1,2,3,0); c = SHUF(c, 2,3,0,1); d = SHUF(d, 3,0,1,2);
        a = __builtin_mxu2_add_w(a, b);
        d = (v4i32)__builtin_mxu2_xorv((v16i8)d, (v16i8)a); d = ROT(d, 16);
        c = __builtin_mxu2_add_w(c, d);
        b = (v4i32)__builtin_mxu2_xorv((v16i8)b, (v16i8)c); b = ROT(b, 12);
        a = __builtin_mxu2_add_w(a, b);
        d = (v4i32)__builtin_mxu2_xorv((v16i8)d, (v16i8)a); d = ROT(d, 8);
        c = __builtin_mxu2_add_w(c, d);
        b = (v4i32)__builtin_mxu2_xorv((v16i8)b, (v16i8)c); b = ROT(b, 7);
        b = SHUF(b, 3,0,1,2); c = SHUF(c, 2,3,0,1); d = SHUF(d, 1,2,3,0);
    }
    __builtin_mxu2_su1q((v16i8)a, (v16i8 *)st, 0);
    __builtin_mxu2_su1q((v16i8)b, (v16i8 *)st, 16);
    __builtin_mxu2_su1q((v16i8)c, (v16i8 *)st, 32);
    __builtin_mxu2_su1q((v16i8)d, (v16i8 *)st, 48);
}

int main(void) {
    for (int i = 0; i < 16; i++) state[i] = i * 0x12345 + 1;
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < N; i++) chacha20(state);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long ns = (t1.tv_sec-t0.tv_sec)*1000000000L + (t1.tv_nsec-t0.tv_nsec);
    /* Each block is 64 bytes. Print MB/s.  */
    double mb = (double)N * 64.0 / (1024 * 1024);
    double sec = ns / 1e9;
    printf("CHACHA mb_per_sec=%.2f sum=%d\n", mb / sec, state[0]);
    return 0;
}
