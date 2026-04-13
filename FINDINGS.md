# Ingenic XBurst MXU/MXU2 Findings

## Summary

Both **T20** (XBurst1 V0.1) and **T31** (XBurst1 V0.0) support **full MXU2 VPR arithmetic** from userspace, including 128-bit logical ops and parallel integer arithmetic. MXU1 SPECIAL2 instructions SIGILL due to a kernel detection bug.

---

## Device Inventory

| Device | IP | SoC | XBurst Gen |
|--------|----|-----|-----------|
| bull   | 10.25.1.153 | T20 | XBurst1 (V0.1) |
| swan   | 10.25.1.155 | T31 | XBurst1 (V0.0) |

---

## Test Results

| Test | T20 | T31 |
|------|-----|-----|
| MXU1 S32M2I (read MXU_CR) | **SIGILL** | **SIGILL** |
| MXU1 full (enable + S32AND) | **SIGILL** | **SIGILL** |
| MXU2 VPR cold (LU1Q/SU1Q, no prior COP2) | **WORKING** | **WORKING** |
| CFCMXU MIR (COP2 cofun) | **PRESENT**, MIR=0 | **PRESENT**, MIR=0 |
| CFCMXU MCSR | **OK**, MCSR=0 | **OK**, MCSR=0 |
| VPR load/store roundtrip | **WORKING** | **WORKING** |
| VPR andv (128-bit AND) | **CORRECT** | **CORRECT** |
| VPR addw (4×32-bit add) | **CORRECT** | **CORRECT** |
| VPR mulw (4×32-bit multiply) | **CORRECT** | **CORRECT** |

---

## Why MXU1 SIGILLs

The kernel detects MXU capabilities in `arch/mips/kernel/cpu-probe.c`:

```c
if (config1 & MIPS_CONF1_C2) {
    if (soc_support_mxuv2())
        c->ases |= MIPS_ASE_XBURSTMXUV2;
    else
        c->ases |= MIPS_ASE_XBURSTMXU;
}
```

`MIPS_CONF1_C2` is **NOT set** in CP0 Config1 on T20/T31. So this entire block is skipped, `cpu_has_mxu` is false, `__init_mxu()` never runs, the MXU_EN bit in MXU_CR (XR16) is never set, and all MXU1 SPECIAL2 instructions (funct 0x2E–0x3F) raise Reserved Instruction (SIGILL).

This is almost certainly fixable by either:
1. Setting MXU_EN from userspace before use (but S32M2I XR16 itself SIGILLs)
2. Patching the kernel to call `__init_mxu()` unconditionally for XBurst1 SoCs
3. Using MXU2 VPR instructions instead (they work)

The FAAC branch (`nschimme/faac:mips-mxu2-support`) failed because it used MXU1 SPECIAL2 funct codes with wrong COP2 sub-opcodes.

---

## Why MXU2 Works Despite Blank ASEs

A separate kernel mechanism bypasses the detection path entirely:

`arch/mips/xburst/core/mxu-v2-ex.obj` (prebuilt blob, identical in T20/T31 kernels) registers `xburst_mxu_call` as a COP2 Unusable exception notifier via `__initcall`. On first VPR or COP2 access:

1. CPU raises COP2 Unusable exception (CU2 bit = 0 in CP0 Status)
2. Kernel notifier `xburst_mxu_call` fires
3. `efuse_data_read` checks eFuse bits at offsets 0, 16, 51, 58, 60
4. Both T20 and T31 production chips pass the license check
5. Kernel sets CU2 bit in CP0 Status for the faulting process
6. Instruction retries successfully

After the first trigger, all VPR/COP2 instructions work without further overhead.

`soc_support_mxuv2()` always returns 1 — it's a red herring, only used in the dead detection path above.

---

## Instruction Encodings

### MXU1 (SPECIAL2, opcode=0x1C=28) — SIGILLs on T20/T31 without kernel patch

```
S32M2I XRa, rb  = (0x1C<<26) | (0<<21) | (rb<<16) | (0<<11) | (XRa<<6) | 0x2E
S32I2M XRa, rb  = (0x1C<<26) | (0<<21) | (rb<<16) | (0<<11) | (XRa<<6) | 0x2F
S32AND XRa,b,c  = (0x1C<<26) | (0<<21) | (4<<18)  | (XRc<<14)| (XRb<<10)| (XRa<<6) | 0x27

S32M2I XR16, $t0 = 0x7008042E  (read MXU_CR)
S32I2M XR16, $t0 = 0x7008042F  (write MXU_CR, MXU_EN = bit 0)
```

### MXU2 VPR Load/Store (SPECIAL2, funct 0x14/0x1C) — WORKING

```
LU1Q (load 128-bit):  (0x1C<<26) | (base<<21) | (offset_idx<<11) | (vpr_num<<6) | 0x14
SU1Q (store 128-bit): (0x1C<<26) | (base<<21) | (offset_idx<<11) | (vpr_num<<6) | 0x1C

  address = base + offset_idx * 16

LU1Q $vr0, 0($t3) = 0x71600014   ($t3=11, VPR0, offset=0)
SU1Q $vr0, 0($t3) = 0x7160001C
LU1Q $vr0, 0($t0) = 0x71000014   ($t0=8)
LU1Q $vr1, 0($t0) = 0x71000054   (VPR1)
SU1Q $vr2, 0($t0) = 0x7100009C   (VPR2)
```

### MXU2 Control (COP2 CF format) — WORKING

```
CFCMXU rd, mcsrs:
  (18<<26) | (30<<21) | (1<<16) | (rd<<11) | (mcsrs<<6) | (30<<1) | 1

CFCMXU $t0, MIR  (mcsrs=0)  = 0x4BC1403D   — reads MXU Implementation Register
CFCMXU $t0, MCSR (mcsrs=31) = 0x4BC147FD   — reads MXU Control/Status Register
```

### MXU2 VPR Arithmetic (COP2 CO format) — WORKING

```
General: (18<<26) | (1<<25) | (major<<21) | (vt<<16) | (vs<<11) | (vd<<6) | minor

  andv  major=6, minor=0x38   (128-bit AND)
  orv   major=6, minor=0x3a   (128-bit OR)
  xorv  major=6, minor=0x3b   (128-bit XOR)
  norv  major=6, minor=?      (128-bit NOR)
  addw  major=1, minor=0x22   (parallel 4×32-bit integer add)
  mulw  major=2, minor=0x06   (parallel 4×32-bit integer multiply, low 32 bits)

Specific examples (vd=VPR2, vs=VPR0, vt=VPR1):
  andv $vr2, $vr0, $vr1 = 0x4AC100B8
  addw $vr2, $vr0, $vr1 = 0x4A2100A2
  mulw $vr2, $vr0, $vr1 = 0x4A410086
```

---

## MIR and MCSR Values

| Register | T20 | T31 |
|----------|-----|-----|
| MIR (Implementation, mcsrs=0) | 0x00000000 | 0x00000000 |
| MCSR (Control/Status, mcsrs=31) | 0x00000000 | 0x00000000 |

MIR=0 means Version=0, ProcessorID=0 — "minimal/unknown" variant. Not MXU3 (would be version ≥3).

---

## Implications for FAAC (Audio Codec)

FAAC needs integer SIMD: multiply-accumulate, shifts, adds on int32 data. MXU2 provides exactly this:
- `mulw` — parallel 4×int32 multiply (lower 32 bits)
- `addw` — parallel 4×int32 add
- `andv/orv/xorv` — bitwise ops across 128 bits

The instruction set can replace MXU1 for these operations. The key change needed in FAAC: use MXU2 COP2/VPR instructions instead of MXU1 SPECIAL2, and use LU1Q/SU1Q for 128-bit loads/stores.

The FAAC branch bug was in the CFCMXU encoding (used wrong sub-opcodes) AND used MXU1 ops which SIGILL on modern kernels.

---

## Toolchain

Ingenic GCC 4.7.2 (with `-mmxu2`) is available at:
`/home/turismo/projects/_archive/toolchain/mips-gcc472-glibc216-64bit/`

Headers: `lib/gcc/mips-linux-gnu/4.7.2/include/mxu2.h` — 736 intrinsics.

Intrinsic naming:
- Load:  `__builtin_mxu2_lu1q(void *ptr, int offset_idx)` → `v16i8`
- Store: `__builtin_mxu2_su1q(v16i8 val, void *ptr, int offset_idx)`
- Logic: `__builtin_mxu2_andv(v16i8, v16i8)`, `orv`, `xorv`, `norv`
- Arith: `__builtin_mxu2_add_w(v4i32, v4i32)`, `mul_w`, `sub_w`, etc.

---

## Probe Binary

Source: `/home/turismo/projects/mxu-probe/mxu_probe.c`
Binary: `/home/turismo/projects/mxu-probe/mxu_probe` (statically linked mipsel)
Run on devices via NFS: `/mnt/nfs/projects/mxu-probe/mxu_probe`

Build: `mipsel-linux-gnu-gcc -O0 -o mxu_probe mxu_probe.c -static`

---

## TODO / Next Steps

- [x] Test additional VPR arithmetic — all 368 ops tested, 431 PASS on T20+T31
- [x] Test MXU2 with Ingenic GCC 4.7.2 toolchain — native path verified, shim auto-delegates
- [ ] Test T32 (updated XBurst1 core)
- [ ] Determine if MCSR dirty bit gets set after VPR write (read MCSR *after* arithmetic)
- [ ] Port Ingenic GCC 7.2 MXU2 patches to modern GCC (13/14) for Buildroot/Thingino
- [ ] Determine shufv control vector format for stereo interleave/deinterleave
- [ ] FAAC/Opus MXU2 optimization using mxu2_dsp.h kernels
