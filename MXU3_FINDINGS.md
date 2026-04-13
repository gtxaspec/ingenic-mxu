# MXU3 (XBurst2 SIMD512) Findings

## Summary

MXU3 is Ingenic's 512-bit SIMD extension for XBurst2 cores (T40, T41). It replaces MXU2 entirely — MXU2 instructions SIGILL on XBurst2. MXU3 uses the COP2 register file with 32x512-bit VPR registers.

## Device Status

| Device | SoC | MXU3 Status |
|--------|-----|-------------|
| T41 (10.25.1.157) | XBurst2 V0.0 | **WORKING** on both CPUs |
| T40 (10.25.1.158) | XBurst2 V0.0 | **WORKING on CPU0 only** — kernel bug on CPU1 |

### T40 Kernel Bug

The T40 vendor kernel (`ingenic-t40` branch) has a broken COP2 handler in `arch/mips/kernel/traps.c:do_cpu()` case 2: it only enables CU2 on CPU0 and force-SIGILLs on CPU1. The T41 kernel fixed this. Patch saved as `t40-mxu3-fix.patch`.

## Architecture

- 32 x 512-bit VPR registers (vr0-vr31), 64-byte aligned
- 4 x MLEN-bit VSR (vector sum registers): vs0-vs3
- VWR (vector write registers): alias for vr31 word slots
- Data types: 8/16/32/64/128/256/512/1024-bit integers, 32-bit float
- MLEN = 512 bits on T40/T41
- IEEE 754-2008 compliant FPU

## Instruction Encoding (from PM section 3.4)

### COP2 format (arithmetic)

```
31-26  25-21        20-16  15-11  10-6   5-0
COP2   Minor opcode vrp    vrs    vrd    funct
010010 xxxxx        xxxxx  xxxxx  xxxxx  xxxxxx
```

### SPECIAL2 format (load/store, misc)

```
31-26    25-21  20-16  15-11  10-6   5-0
SPECIAL2 rs     rt     rd     funct  Minor opcode
011100   xxxxx  xxxxx  xxxxx  xxxxx  xxxxxx
```

## Verified Encodings

### Load/Store

Sub-register indexing: VPR field encodes both register number and sub-position.

| Instruction | Width | Encoding | Format |
|-------------|-------|----------|--------|
| LUW vrd[n], base | 32-bit | SPECIAL2, minor=0x12 | `0x70000012 \| (base<<21) \| (vrd_n<<6)` |
| LUD vrd[n], base | 64-bit | SPECIAL2, minor=0x13 | `0x70000013 \| (base<<21) \| (vrd_n<<6)` |
| LUQ vrd[n], base | 128-bit | SPECIAL2, minor=0x13, bit12 | `0x70000813 \| (base<<21) \| (vrd_n<<6)` |
| LUO vrd[n], base | 256-bit | SPECIAL2, minor=0x13, bits13-12 | `0x70001813 \| (base<<21) \| (vrd_n<<6)` |
| SUW vrp[n], base | 32-bit | SPECIAL2, minor=0x16 | `0x70000016 \| (base<<21) \| (vrp_n<<11)` |
| SUQ vrp[n], base | 128-bit | SPECIAL2, minor=0x57 | `0x70000057 \| (base<<21) \| (vrp_n<<11)` |

Sub-register `vrd[n]` / `vrp[n]` encoding in the 5-bit VPR field:
- LUW (word, 1/16): `VPR[0] | position[3:0]` (only VPR0-1 accessible)
- LUD (double, 1/8): `VPR[1:0] | position[2:0]` (VPR0-3)
- LUQ (quad, 1/4): `VPR[2:0] | quarter[1:0]` (VPR0-7)
- LUO (oct, 1/2): `VPR[3:0] | half[0]` (VPR0-15)

### COP2 Arithmetic (verified on T41)

| Instruction | Encoding | Notes |
|-------------|----------|-------|
| ADDW vrd, vrs, vrp | `0x4a800002 \| (vrp<<16) \| (vrs<<11) \| (vrd<<6)` | 16x32-bit add |
| ANDV vrd, vrs, vrp | `0x4a600002 \| (vrp<<16) \| (vrs<<11) \| (vrd<<6)` | 512-bit AND |

### Confirmed Working Roundtrip

Using 4x LUQ (128-bit each) to fill a 512-bit VPR, ADDW to compute, 4x SUQ to store:
- T41: 16-lane word add verified correct (addw: 1+10=11 through 4+40=44, first 4 lanes)
- T40: same, when pinned to CPU0

## Instruction Count

From binutils opcode table: **516 opcode entries** (MXU512 flag)
From PM table of contents: ~200 unique instruction mnemonics across categories:
- Branch (4), Compare (24), Integer Arithmetic (~100+), Bitwise (11)
- Float Arithmetic (12), Float Compare (4), Float Conversion (7)
- Shift (30), Shuffle/Permute (50+), Register Load/Misc (17)
- Memory Load/Store (60+), Neural Network Accelerate (6)

## Toolchain

- Ingenic GCC 7.2 source has MXU3 support: `mips-mxu3.md` (5112 lines), `ingenic-mxu3.def` (572 lines), `mxu3.h` (48K)
- GCC flag: `-mmxu3` (not in upstream GCC)
- Thingino XBurst2 toolchain (GCC 15.2): no MXU3 support, needs shim

## References

- `XBurst_ISA_MXU3_PM_1.0.pdf` — complete instruction set programming manual (183 pages)
- `XBurst2_Core_PM.pdf` — core architecture reference
- Ingenic GCC source: `src/gcc-7-2017.11/gcc/config/mips/mips-mxu3.md`
- Binutils source: `src/binutils-2017.11/opcodes/mips-opc.c` (MXU512 entries)
