/*
 * mxu_probe.c - Ingenic XBurst MXU1/MXU2/MXU3 capability probe
 *
 * Tests whether MXU1 and MXU2/MXU3 instructions execute on the current SoC.
 * Run on each device type (T10-T41) to build the support matrix.
 *
 * Compile:
 *   mipsel-linux-gnu-gcc -O0 -o mxu_probe mxu_probe.c -static
 *
 * -------------------------------------------------------------------------
 * ENCODING REFERENCE (derived from Ingenic XBurst ISA manuals)
 *
 * MXU1 — SPECIAL2 opcode (0x1C = 28), format:
 *   bits 31-26: 011100 (SPECIAL2)
 *   bits 25-21: rs / eptn
 *   bits 20-16: rt / rb
 *   bits 15-11: rd (5-bit)
 *   bits 10-6:  XRa (5-bit, XR0-XR16)
 *   bits  5-0:  funct
 *
 *   S32M2I XRa, rb   = (0x1C<<26)|(0<<21)|(rb<<16)|(0<<11)|(XRa<<6)|0x2E
 *   S32I2M XRa, rb   = (0x1C<<26)|(0<<21)|(rb<<16)|(0<<11)|(XRa<<6)|0x2F
 *   S32AND XRa,XRb,XRc = (0x1C<<26)|(0<<21)|(4<<18)|(XRc<<14)|(XRb<<10)|(XRa<<6)|0x27
 *
 *   S32M2I XR16,$t0  = 0x7008042E   (read  MXU_CR -> $t0)
 *   S32I2M XR16,$t0  = 0x7008042F   (write $t0 -> MXU_CR, enables MXU if bit0=1)
 *   S32I2M XR1, $t1  = 0x7009006F   (write $t1 -> XR1)
 *   S32AND XR1,XR1,XR1 = 0x70104467 (XR1 &= XR1, noop-like, just verifies execution)
 *   S32M2I XR1, $t2  = 0x700A006E   (read  XR1 -> $t2)
 *
 * MXU2/MXU3 — COP2 opcode (0x12 = 18) for control instructions, format:
 *   CFCMXU rd, mcsrs:
 *     bits 31-26: 010010 (COP2)
 *     bits 25-21: 11110  (30)
 *     bits 20-16: 00001  (1, fixed)
 *     bits 15-11: rd     (destination GPR)
 *     bits 10-6:  mcsrs  (control reg: 0=MIR, 31=MCSR)
 *     bits  5-1:  11110  (30)
 *     bit   0:    1
 *
 *   CFCMXU $t0, MIR  = 0x4BC1403D   (rd=$t0=$8, mcsrs=MIR=0)
 *
 * MIR layout (MXU2/3 Implementation Register, read-only):
 *   bits 31-16: reserved (0)
 *   bits 15-8:  Processor ID
 *   bits  7-0:  Version
 *
 * MXU2 VPR arithmetic — COP2 CO format (opcode=18, bit25=1):
 *   (18<<26) | (1<<25) | (major<<21) | (vt<<16) | (vs<<11) | (vd<<6) | minor
 *
 *   andv  vd,vs,vt  major=6, minor=0x38  (128-bit AND)
 *   orv   vd,vs,vt  major=6, minor=0x3a
 *   xorv  vd,vs,vt  major=6, minor=0x3b
 *   addw  vd,vs,vt  major=1, minor=0x22  (parallel 4x32-bit add)
 *   mulw  vd,vs,vt  major=2, minor=0x06  (parallel 4x32-bit multiply, lower 32)
 *
 *   andv $vr2,$vr0,$vr1 = 0x4AC100B8
 *   addw $vr2,$vr0,$vr1 = 0x4A2100A2
 *   mulw $vr2,$vr0,$vr1 = 0x4A410086
 *
 * LU1Q/SU1Q using $t0 (reg 8) as base, various VPRs:
 *   LU1Q $vr0, 0($t0) = 0x71000014  (load 16 bytes from [$t0] to VPR0)
 *   LU1Q $vr1, 0($t0) = 0x71000054  (load 16 bytes from [$t0] to VPR1)
 *   SU1Q $vr2, 0($t0) = 0x7100009C  (store VPR2 to [$t0])
 * -------------------------------------------------------------------------
 */

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>

static sigjmp_buf g_jmp;

static void on_sigill(int sig)
{
    (void)sig;
    siglongjmp(g_jmp, 1);
}

static void install_sigill(struct sigaction *old)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigill;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGILL, &sa, old);
}

/*
 * probe_mxu1_basic()
 *
 * Tests only S32M2I XR16, $t0 in isolation — this should be the very
 * first MXU instruction to execute. If this SIGILLs, MXU1 is completely
 * absent (kernel or hardware). If it succeeds, the full sequence can run.
 *
 * Returns: 1 = executed (no SIGILL), 0 = SIGILL
 */
static int probe_mxu1_basic(unsigned int *mcr_out)
{
    struct sigaction old;
    int rc = 0;

    install_sigill(&old);
    if (sigsetjmp(g_jmp, 1) == 0) {
        unsigned int val = 0xDEADBEEFu;
        __asm__ __volatile__ (
            ".set push\n\t"
            ".set noreorder\n\t"
            ".set noat\n\t"
            "move  $t0, %[sv]\n\t"       /* sentinel in $t0 */
            ".word 0x7008042E\n\t"        /* S32M2I XR16, $t0 -- read MXU_CR */
            "move  %[out], $t0\n\t"
            ".set pop\n\t"
            : [out] "=r" (val)
            : [sv]  "r"  (0xDEADBEEFu)
            : "$t0", "memory"
        );
        *mcr_out = val;
        rc = 1;
    }

    sigaction(SIGILL, &old, NULL);
    return rc;
}

/*
 * probe_mxu1()
 *
 * Enables MXU via read-modify-write on MXU_CR (XR16), then executes
 * S32AND XR1, XR1, XR1 using a known test value.
 *
 * Returns:
 *   1  = hardware present, enable succeeded, correct result verified
 *  -1  = hardware present (no SIGILL), but result was wrong (enable failed?)
 *   0  = absent — SIGILL on MXU instruction
 */
static int probe_mxu1(unsigned int *readback_out)
{
    struct sigaction old;
    int rc = 0;

    install_sigill(&old);
    if (sigsetjmp(g_jmp, 1) == 0) {
        const unsigned int testval = 0xA5A5A5A5u;
        unsigned int readback = 0xDEADBEEFu;

        __asm__ __volatile__ (
            ".set push\n\t"
            ".set noreorder\n\t"
            ".set noat\n\t"

            /*
             * Enable MXU1: read MXU_CR (XR16) into $t0, set MXU_EN (bit 0),
             * write back. Then >=3 non-MXU instructions before first MXU use.
             */
            ".word 0x7008042E\n\t"   /* S32M2I XR16, $t0  -- read MXU_CR  */
            "ori   $t0, $t0, 1\n\t" /* set MXU_EN bit                     */
            ".word 0x7008042F\n\t"   /* S32I2M XR16, $t0  -- write MXU_CR */
            "nop\n\t"                /* mandatory pad 1                    */
            "nop\n\t"                /* mandatory pad 2                    */
            "nop\n\t"                /* mandatory pad 3                    */

            /* Load test value into XR1 */
            "move  $t1, %[tv]\n\t"
            ".word 0x7009006F\n\t"   /* S32I2M XR1, $t1 */

            /* S32AND XR1, XR1, XR1  -- AND XR1 with itself (result = XR1) */
            ".word 0x70104467\n\t"

            /* Read XR1 back into $t2, then out */
            ".word 0x700A006E\n\t"   /* S32M2I XR1, $t2 */
            "move  %[rb], $t2\n\t"

            ".set pop\n\t"
            : [rb] "=r" (readback)
            : [tv] "r"  (testval)
            : "$t0", "$t1", "$t2", "memory"
        );

        *readback_out = readback;
        rc = (readback == testval) ? 1 : -1;
    }
    /* else: SIGILL, rc = 0 */

    sigaction(SIGILL, &old, NULL);
    return rc;
}

/*
 * probe_mxu2()
 *
 * Attempts CFCMXU $t0, MIR — reads the MXU2 Implementation Register.
 * This instruction is COP2-encoded and only exists on MXU2/MXU3 hardware.
 *
 * Returns: MIR value (>=0) if present, -1 if absent (SIGILL).
 *
 * Note: MIR = 0 is a valid value on some silicon, distinct from -1 (absent).
 */
static int probe_mxu2(void)
{
    struct sigaction old;
    int mir = -1;

    install_sigill(&old);
    if (sigsetjmp(g_jmp, 1) == 0) {
        /* Sentinel: if CFCMXU is a kernel NOP, $t0 stays 0xDEADBEEF.
         * If it actually reads MIR, $t0 is overwritten with the MIR value. */
        unsigned int val = 0xDEADBEEFu;

        __asm__ __volatile__ (
            ".set push\n\t"
            ".set noreorder\n\t"
            ".set noat\n\t"
            "move  $t0, %[sv]\n\t"       /* load sentinel into $t0 */
            ".word 0x4BC1403D\n\t"       /* CFCMXU $t0, MIR */
            "nop\n\t"
            "move  %[out], $t0\n\t"
            ".set pop\n\t"
            : [out] "=r" (val)
            : [sv]  "r"  (0xDEADBEEFu)
            : "$t0", "memory"
        );

        mir = (int)(unsigned int)val;
    }

    sigaction(SIGILL, &old, NULL);
    return mir;
}

/*
 * probe_mxu2_mcsr()
 *
 * Reads MCSR (MXU Control and Status Register, mcsrs=31) via CFCMXU.
 * MCSR contains per-thread MXU2 state flags:
 *   bit 31: VPR dirty (set by hardware on any VPR data operation)
 *   bit  1: MXU2 enable (must be set for VPR data ops to work?)
 *   bit  0: VPR writeback enable
 *
 * CFCMXU $t0, MCSR encoding:
 *   (18<<26)|(30<<21)|(1<<16)|(8<<11)|(31<<6)|(30<<1)|1 = 0x4BC147FD
 *
 * Returns: MCSR value (>=0) if COP2 accessible, -1 if SIGILL.
 */
static int probe_mxu2_mcsr(void)
{
    struct sigaction old;
    int mcsr = -1;

    install_sigill(&old);
    if (sigsetjmp(g_jmp, 1) == 0) {
        unsigned int val = 0xDEADBEEFu;
        __asm__ __volatile__ (
            ".set push\n\t"
            ".set noreorder\n\t"
            ".set noat\n\t"
            "move  $t0, %[sv]\n\t"
            ".word 0x4BC147FD\n\t"   /* CFCMXU $t0, MCSR  (mcsrs=31) */
            "nop\n\t"
            "move  %[out], $t0\n\t"
            ".set pop\n\t"
            : [out] "=r" (val)
            : [sv]  "r"  (0xDEADBEEFu)
            : "$t0", "memory"
        );
        mcsr = (int)(unsigned int)val;
    }

    sigaction(SIGILL, &old, NULL);
    return mcsr;
}

/*
 * probe_mxu2_vpr()
 *
 * Tests VPR (128-bit Vector Processing Register) load/store via SPECIAL2:
 *   LU1Q rb, VPRn: load 128 bits from [rb + n*16] to VPRn   funct=0x14
 *   SU1Q rb, VPRn: store VPRn to [rb + n*16]                funct=0x1C
 *
 * Encoding formula:
 *   (0x1C<<26) | (rb<<21) | ((n>>1)<<16) | ((n&1)<<15) | (n<<6) | funct
 *
 * Using $t3 ($11) as base, VPR0 (n=0):
 *   LU1Q $t3, VPR0 = 0x71600014
 *   SU1Q $t3, VPR0 = 0x7160001C
 *
 * These are SPECIAL2-encoded (not COP2). Even if CFCMXU works (CU2 enabled),
 * these may still SIGILL if the kernel never enabled MXU2 SPECIAL2 ops.
 *
 * Returns: 1 = roundtrip succeeded (VPR load/store work)
 *          0 = SIGILL (VPR SPECIAL2 not enabled)
 */
static int probe_mxu2_vpr(void)
{
    struct sigaction old;
    int rc = 0;

    /* 16-byte aligned buffers for VPR load/store */
    static unsigned int src[4] __attribute__((aligned(16))) =
        {0xA5A5A5A5u, 0xDEADBEEFu, 0x12345678u, 0xCAFEBABEu};
    static unsigned int dst[4] __attribute__((aligned(16))) =
        {0, 0, 0, 0};

    install_sigill(&old);
    if (sigsetjmp(g_jmp, 1) == 0) {
        __asm__ __volatile__ (
            ".set push\n\t"
            ".set noreorder\n\t"
            ".set noat\n\t"

            /* $t3 = &src[0] */
            "move  $t3, %[psrc]\n\t"
            ".word 0x71600014\n\t"    /* LU1Q $t3, VPR0 -- load [src] -> VPR0 */
            "nop\n\t"
            "nop\n\t"

            /* $t3 = &dst[0] */
            "move  $t3, %[pdst]\n\t"
            ".word 0x7160001C\n\t"    /* SU1Q $t3, VPR0 -- store VPR0 -> [dst] */
            "nop\n\t"
            "nop\n\t"

            ".set pop\n\t"
            :
            : [psrc] "r" (src), [pdst] "r" (dst)
            : "$t3", "memory"
        );

        /* Verify roundtrip */
        if (dst[0] == src[0] && dst[1] == src[1] &&
            dst[2] == src[2] && dst[3] == src[3])
            rc = 1;
        else
            rc = -1;  /* No SIGILL but data mismatch */
    }

    sigaction(SIGILL, &old, NULL);
    return rc;
}

/*
 * probe_mxu2_arith()
 *
 * Tests VPR arithmetic via COP2 CO-format instructions:
 *   andv, addw, mulw — all operating on 4×32-bit lanes in parallel.
 *
 * Sequence:
 *   LU1Q $vr0 ← src_a[4]   (= {1, 2, 3, 4})
 *   LU1Q $vr1 ← src_b[4]   (= {10, 20, 30, 40})
 *   andv $vr2 = $vr0 AND $vr1  → expected {0, 0, 2, 0}
 *   addw $vr2 = $vr0 + $vr1   → expected {11, 22, 33, 44}
 *   mulw $vr2 = $vr0 * $vr1   → expected {10, 40, 90, 160}
 *
 * Returns:
 *   1  = all arithmetic correct
 *  -1  = no SIGILL but result mismatch
 *   0  = SIGILL
 */
static int probe_mxu2_arith(void)
{
    struct sigaction old;
    int rc = 0;

    /* 16-byte aligned input/output buffers */
    static const unsigned int src_a[4] __attribute__((aligned(16))) = {1, 2, 3, 4};
    static const unsigned int src_b[4] __attribute__((aligned(16))) = {10, 20, 30, 40};
    static unsigned int dst_and[4] __attribute__((aligned(16))) = {0xDEADBEEFu, 0xDEADBEEFu, 0xDEADBEEFu, 0xDEADBEEFu};
    static unsigned int dst_add[4] __attribute__((aligned(16))) = {0xDEADBEEFu, 0xDEADBEEFu, 0xDEADBEEFu, 0xDEADBEEFu};
    static unsigned int dst_mul[4] __attribute__((aligned(16))) = {0xDEADBEEFu, 0xDEADBEEFu, 0xDEADBEEFu, 0xDEADBEEFu};

    install_sigill(&old);
    if (sigsetjmp(g_jmp, 1) == 0) {
        __asm__ __volatile__ (
            ".set push\n\t"
            ".set noreorder\n\t"
            ".set noat\n\t"

            /* Load VPR0 from src_a, VPR1 from src_b — reuse $t0 as base */
            "move  $t0, %[pa]\n\t"
            ".word 0x71000014\n\t"   /* LU1Q $vr0, 0($t0) — VPR0 = src_a */
            "move  $t0, %[pb]\n\t"
            ".word 0x71000054\n\t"   /* LU1Q $vr1, 0($t0) — VPR1 = src_b */

            /* andv VPR2 = VPR0 AND VPR1, store to dst_and */
            ".word 0x4AC100B8\n\t"   /* andv $vr2, $vr0, $vr1 */
            "move  $t0, %[pand]\n\t"
            ".word 0x7100009C\n\t"   /* SU1Q $vr2, 0($t0) */

            /* addw VPR2 = VPR0 + VPR1 (4x32-bit parallel), store to dst_add */
            ".word 0x4A2100A2\n\t"   /* addw $vr2, $vr0, $vr1 */
            "move  $t0, %[padd]\n\t"
            ".word 0x7100009C\n\t"   /* SU1Q $vr2, 0($t0) */

            /* mulw VPR2 = VPR0 * VPR1 (4x32-bit parallel, lower 32), store to dst_mul */
            ".word 0x4A410086\n\t"   /* mulw $vr2, $vr0, $vr1 */
            "move  $t0, %[pmul]\n\t"
            ".word 0x7100009C\n\t"   /* SU1Q $vr2, 0($t0) */

            ".set pop\n\t"
            :
            : [pa]   "r" (src_a),
              [pb]   "r" (src_b),
              [pand] "r" (dst_and),
              [padd] "r" (dst_add),
              [pmul] "r" (dst_mul)
            : "$t0", "memory"
        );
        rc = 1;
    }
    sigaction(SIGILL, &old, NULL);

    if (!rc) return 0;

    /* src_a={1,2,3,4}, src_b={10,20,30,40} */
    static const unsigned int exp_and[4] = {1&10, 2&20, 3&30, 4&40};   /* {0,0,2,0} */
    static const unsigned int exp_add[4] = {11, 22, 33, 44};
    static const unsigned int exp_mul[4] = {10, 40, 90, 160};

    int and_ok = (memcmp(dst_and, exp_and, 16) == 0);
    int add_ok = (memcmp(dst_add, exp_add, 16) == 0);
    int mul_ok = (memcmp(dst_mul, exp_mul, 16) == 0);

    printf("  andv [%u,%u,%u,%u] expected [%u,%u,%u,%u] %s\n",
           dst_and[0], dst_and[1], dst_and[2], dst_and[3],
           exp_and[0], exp_and[1], exp_and[2], exp_and[3],
           and_ok ? "OK" : "MISMATCH");
    printf("  addw [%u,%u,%u,%u] expected [%u,%u,%u,%u] %s\n",
           dst_add[0], dst_add[1], dst_add[2], dst_add[3],
           exp_add[0], exp_add[1], exp_add[2], exp_add[3],
           add_ok ? "OK" : "MISMATCH");
    printf("  mulw [%u,%u,%u,%u] expected [%u,%u,%u,%u] %s\n",
           dst_mul[0], dst_mul[1], dst_mul[2], dst_mul[3],
           exp_mul[0], exp_mul[1], exp_mul[2], exp_mul[3],
           mul_ok ? "OK" : "MISMATCH");

    return (and_ok && add_ok && mul_ok) ? 1 : -1;
}

static void print_cpuinfo(void)
{
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) return;

    const char *keys[] = {
        "system type", "machine", "cpu model", "isa", "ASEs implemented", NULL
    };

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        for (int i = 0; keys[i]; i++) {
            if (strncmp(line, keys[i], strlen(keys[i])) == 0) {
                /* strip trailing newline for cleaner output */
                line[strcspn(line, "\n")] = 0;
                printf("  %s\n", line);
                break;
            }
        }
    }
    fclose(f);
}

int main(void)
{
    printf("========================================\n");
    printf("  Ingenic XBurst MXU Probe\n");
    printf("========================================\n\n");

    printf("CPU info:\n");
    print_cpuinfo();
    printf("\n");

    /* --- MXU1 basic: just S32M2I XR16 in isolation --- */
    printf("MXU1 basic probe (S32M2I XR16, $t0 only):\n");
    unsigned int mcr_val = 0;
    if (probe_mxu1_basic(&mcr_val)) {
        printf("  S32M2I executed OK -- MXU_CR raw = 0x%08X\n", mcr_val);
        if (mcr_val == 0xDEADBEEFu)
            printf("  WARNING: value unchanged from sentinel -- may be kernel NOP\n");
        else
            printf("  MXU_EN (bit 0) = %d\n", mcr_val & 1);
    } else {
        printf("  SIGILL -- MXU1 SPECIAL2 instructions not supported\n");
    }
    printf("\n");

    /* --- MXU1 full --- */
    printf("MXU1 full probe (enable + S32AND + readback):\n");
    unsigned int readback = 0;
    int mxu1 = probe_mxu1(&readback);
    switch (mxu1) {
    case 1:
        printf("  PRESENT and WORKING\n");
        printf("  Result verified: 0x%08X == 0xA5A5A5A5\n", readback);
        break;
    case -1:
        printf("  HARDWARE PRESENT (no SIGILL) but wrong result\n");
        printf("  Got: 0x%08X  Expected: 0xA5A5A5A5\n", readback);
        printf("  Possible cause: MXU_EN enable sequence failed\n");
        break;
    case 0:
        printf("  ABSENT (SIGILL)\n");
        break;
    }
    printf("\n");

    /* --- MXU2 VPR cold test (before any COP2 ever executed in this process) --- */
    printf("MXU2 VPR cold probe (LU1Q/SU1Q with NO prior COP2 call):\n");
    int vpr_cold = probe_mxu2_vpr();
    switch (vpr_cold) {
    case 1:
        printf("  WORKING cold -- VPR is accessible without prior COP2 trigger\n");
        printf("  LU1Q/SU1Q either self-trigger CU2 notifier or are always-on\n");
        break;
    case -1:
        printf("  No SIGILL but data mismatch (unexpected)\n");
        break;
    case 0:
        printf("  SIGILL cold -- VPR SPECIAL2 requires prior COP2 trigger to set CU2\n");
        break;
    }
    printf("\n");

    /* --- MXU2/MXU3 CFCMXU MIR --- */
    printf("MXU2/MXU3 probe (CFCMXU $t0, MIR with sentinel 0xDEADBEEF):\n");
    int mir = probe_mxu2();
    if (mir < 0) {
        printf("  ABSENT (SIGILL)\n");
    } else if ((unsigned int)mir == 0xDEADBEEFu) {
        printf("  UNCERTAIN -- instruction executed but $t0 unchanged\n");
        printf("  Kernel likely handled COP2 as a NOP (no real MXU2 hardware)\n");
    } else {
        unsigned int umit = (unsigned int)mir;
        printf("  PRESENT -- CFCMXU wrote MIR (overwrote sentinel)\n");
        printf("  MIR = 0x%08X\n", umit);
        printf("    Processor ID : 0x%02X (%u)\n", (umit >> 8) & 0xFF, (umit >> 8) & 0xFF);
        printf("    Version      : 0x%02X (%u)\n",  umit & 0xFF, umit & 0xFF);
        if ((umit & 0xFF) >= 3)
            printf("    -> MXU3 or later\n");
        else if ((umit & 0xFF) == 2)
            printf("    -> MXU2\n");
        else if ((umit & 0xFF) == 1)
            printf("    -> MXU1 (via CFCMXU, unusual)\n");
        else
            printf("    -> Version 0 (minimal/unknown implementation)\n");
    }
    printf("\n");

    /* --- MXU2 MCSR --- */
    printf("MXU2 MCSR probe (CFCMXU $t0, MCSR -- control reg 31):\n");
    int mcsr = probe_mxu2_mcsr();
    if (mcsr < 0) {
        printf("  ABSENT (SIGILL)\n");
    } else if ((unsigned int)mcsr == 0xDEADBEEFu) {
        printf("  UNCERTAIN -- sentinel unchanged (kernel NOP?)\n");
    } else {
        unsigned int umcsr = (unsigned int)mcsr;
        printf("  OK -- MCSR = 0x%08X\n", umcsr);
        printf("    VPR dirty (bit 31) = %d\n", (umcsr >> 31) & 1);
        printf("    bits [7:0]         = 0x%02X\n", umcsr & 0xFF);
    }
    printf("\n");

    /* --- MXU2 VPR load/store (SPECIAL2) --- */
    printf("MXU2 VPR load/store probe (LU1Q + SU1Q, SPECIAL2 funct 0x14/0x1C):\n");
    int vpr = probe_mxu2_vpr();
    switch (vpr) {
    case 1:
        printf("  WORKING -- LU1Q/SU1Q roundtrip succeeded\n");
        printf("  VPR SPECIAL2 data ops are accessible from userspace\n");
        break;
    case -1:
        printf("  HARDWARE PRESENT (no SIGILL) but data mismatch\n");
        printf("  VPR loaded/stored but content differs from source\n");
        break;
    case 0:
        printf("  ABSENT (SIGILL) -- VPR SPECIAL2 not enabled\n");
        printf("  Note: even if CFCMXU works (COP2/CU2 enabled),\n");
        printf("  SPECIAL2 MXU2 ops need separate kernel-side enable\n");
        break;
    }
    printf("\n");

    /* --- MXU2 VPR arithmetic (andv / addw / mulw) --- */
    printf("MXU2 VPR arithmetic probe (andv / addw / mulw via COP2 cofun):\n");
    int arith = probe_mxu2_arith();
    switch (arith) {
    case 1:
        printf("  ALL CORRECT -- VPR arithmetic fully functional\n");
        break;
    case -1:
        printf("  EXECUTED (no SIGILL) but result mismatch -- encoding may be wrong\n");
        break;
    case 0:
        printf("  SIGILL -- VPR arithmetic COP2 cofun not supported\n");
        break;
    }
    printf("\n");

    printf("========================================\n");
    return 0;
}
