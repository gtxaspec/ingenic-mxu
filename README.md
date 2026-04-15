# ingenic-mxu

SIMD intrinsics and toolchain patches for Ingenic MIPS processors.

| Component | ISA | Width | SoCs | Status |
|-----------|-----|-------|------|--------|
| `mxu2_shim.h` | MXU2 | 128-bit | T20, T21, T23, T30, T31, T32 | 368 ops, 431 tests |
| `mxu3_shim.h` | MXU3 | 512-bit | T40, T41 | 498 ops, 455 tests |
| GCC 15.2 + binutils 2.44 patches | MXU2 | 128-bit | T20, T21, T23, T30, T31, T32 | 382 tests, all pass |

MXU2 and MXU3 are completely separate ISAs -- MXU2 instructions SIGILL on XBurst2 and vice versa.

---

## Native toolchain (GCC 15.2 + binutils 2.44)

The `patches/` directory contains toolchain patches that add native MXU2 support to GCC and binutils. With these patches, the compiler manages vector registers directly -- no inline assembly needed.

### What you get

- `-mmxu2` flag for GCC and the assembler
- 382 `__builtin_mxu2_*` intrinsics with full register allocation
- `<mxu2.h>` header with vector types and convenience macros
- Vectors stay in COP2 registers across operations (no stack round-trips)

### Quick start (patched toolchain)

```c
#include <mxu2.h>

void vector_add(const int *a, const int *b, int *out, int n) {
    for (int i = 0; i < n; i += 4) {
        v4i32 va = *(v4i32*)&a[i];
        v4i32 vb = *(v4i32*)&b[i];
        v4i32 vc = __builtin_mxu2_add_w(va, vb);
        *(v4i32*)&out[i] = vc;
    }
}
```

Build:
```sh
mipsel-linux-gcc -mmxu2 -O2 -static -o myprog myprog.c
```

### Test your toolchain

```sh
mipsel-linux-gcc -mmxu2 -O2 -static -o test_mxu2 test_builtins_full.c -lm
scp test_mxu2 root@<device>:/tmp/
ssh root@<device> /tmp/test_mxu2
# Expected: 382 pass, 0 fail
```

### Patches

**For buildroot / Thingino** (single monolithic patches):

| File | Target | Description |
|------|--------|-------------|
| `patches/0001-add-ingenic-mxu2-support.patch` | binutils 2.44 | Assembler, disassembler, 364 opcodes |
| `patches/0001-add-ingenic-mxu2-backend.patch` | GCC 15.2.0 | Backend, patterns, 382 builtins |

Copy to `package/all-patches/binutils/2.44/` and `package/all-patches/gcc/15.2.0/` in your buildroot tree.

**For upstream submission** (split patch series):

Binutils (4 patches in `patches/binutils-split/`):
1. ASE flag and register types
2. Opcode table (364 instructions)
3. Assembler and disassembler support
4. Branch instruction encoding

GCC (4 patches in `patches/gcc-split/`):
1. ISA option and definitions
2. Backend register and builtin support
3. Instruction patterns
4. Builtin intrinsics and header

### Performance vs shim

| Metric | Shim (`mxu2_shim.h`) | Native (`-mmxu2`) |
|--------|----------------------|-------------------|
| Chained ops `(a+b)*c` | ~72 insns (stack round-trip) | ~8 insns (registers) |
| Register allocation | Manual (inline asm) | Automatic (GCC) |
| Compiler optimization | Opaque asm blocks | Full -O2 scheduling |
| Toolchain required | Any MIPS32 GCC | Patched GCC 15.2 |

---

## Shim headers (any compiler)

For environments without the patched toolchain, the shim headers provide all MXU2/MXU3 operations as inline functions using `.word`-encoded instructions. Works with any MIPS32 cross-compiler.

### MXU2 shim (`mxu2_shim.h`)

```c
#include "mxu2_shim.h"

if (mxu2_available()) {
    mxu2_v4i32 va = MXU2_LOAD(ptr);
    mxu2_v4i32 vb = MXU2_LOAD(ptr2);
    mxu2_v4i32 vc = mxu2_add_w(va, vb);
    MXU2_STORE(out, vc);
}
```

Build with any MIPS32 compiler:
```sh
mipsel-linux-gcc -O2 -static -o myprog myprog.c
```

When compiled with the patched toolchain (`-mmxu2`), the shim delegates to native builtins automatically.

### MXU3 shim (`mxu3_shim.h`)

512-bit SIMD for XBurst2 (T40/T41). Each VPR is 512 bits divided into 4 quarters, operated on individually. The shim handles this transparently.

```c
#include "mxu3_shim.h"

if (mxu3_available()) {
    mxu3_v16i32 va = MXU3_LOAD(ptr);  // 16 x int32 = 512 bits
    mxu3_v16i32 vb = MXU3_LOAD(ptr2);
    mxu3_v16i32 vc = mxu3_addw(va, vb);
    MXU3_STORE(out, vc);
}
```

### DSP kernels (`mxu2_dsp.h`)

Optimized DSP functions using chained MXU2 inline assembly blocks (no stack round-trips):

- Radix-2 butterfly
- Q15 multiply-accumulate
- FIR filter tap
- Vector dot product

---

## API overview

### Types

| MXU2 Type | MXU3 Type | Description |
|-----------|-----------|-------------|
| `v16i8` / `mxu2_v16i8` | `mxu3_v64i8` | Signed 8-bit |
| `v8i16` / `mxu2_v8i16` | `mxu3_v32i16` | Signed 16-bit |
| `v4i32` / `mxu2_v4i32` | `mxu3_v16i32` | Signed 32-bit |
| `v2i64` | -- | Signed 64-bit |
| `v4f32` / `mxu2_v4f32` | `mxu3_v16f32` | 32-bit float |
| `v2f64` | -- | 64-bit double |

Native types (`v4i32` etc.) are available with `<mxu2.h>`. Shim types use the `mxu2_`/`mxu3_` prefix.

### Operations (MXU2, by suffix: `_b`/`_h`/`_w`/`_d`)

| Category | Operations |
|----------|------------|
| Arithmetic | `add`, `sub`, `mul`, `divs`, `divu`, `mods`, `modu`, `madd`, `msub` |
| Saturating | `addss`, `adduu`, `subss`, `subuu`, `sats`, `satu` |
| Compare | `ceq`, `cne`, `clts`, `cltu`, `cles`, `cleu`, `ceqz`, `cnez`, `cltz`, `clez` |
| Min/Max | `maxs`, `maxu`, `mins`, `minu`, `maxa`, `mina` |
| Average | `aves`, `aveu`, `avers`, `averu` |
| Shift | `sll`, `sra`, `srl`, `slli`, `srai`, `srli`, `srari`, `srlri` |
| Bitwise | `andv`, `orv`, `xorv`, `norv`, `bselv`, `andib`, `orib`, `xorib`, `norib` |
| Bit count | `bcnt`, `lzc`, `loc` |
| Float | `fadd`, `fsub`, `fmul`, `fdiv`, `fsqrt`, `fmadd`, `fmsub`, `fmax`, `fmin` |
| Float compare | `fceq`, `fcle`, `fclt`, `fcor`, `fclass` |
| Dot product | `dotps`, `dotpu`, `dadds`, `daddu`, `dsubs`, `dsubu` |
| Convert | `vcvts`, `vcvtu`, `vshf`, `sat`, various width conversions |
| Element | `mfcpu`, `mtcpus`, `mtcpuu`, `insfcpu`, `insfmxu`, `repx`, `repi`, `shufv` |
| Load/Store | `lu1q`, `lu1qx`, `la1q`, `la1qx`, `su1q`, `su1qx`, `sa1q`, `sa1qx` |
| FPU bridge | `mtfpu`, `mffpu`, `insffpu` |
| Branch | `bnez1q`/`16b`/`8h`/`4w`/`2d`, `beqz1q`/`16b`/`8h`/`4w`/`2d` |
| Control | `cfcmxu`, `ctcmxu`, `li` |

### Alignment

All data accessed by MXU2 must be 16-byte aligned. MXU3 requires 64-byte alignment.

```c
int data[4] __attribute__((aligned(16)));    // MXU2
int data[16] __attribute__((aligned(64)));   // MXU3
```

---

## Hardware

| SoC | Core | MXU2 | MXU3 | VPR width |
|-----|------|------|------|-----------|
| T20 | XBurst1 V0.1 | Yes | -- | 128-bit |
| T21 | XBurst1 | Yes | -- | 128-bit |
| T23 | XBurst1 | Yes | -- | 128-bit |
| T30 | XBurst1 | Yes | -- | 128-bit |
| T31 | XBurst1 V0.0 | Yes | -- | 128-bit |
| T32 | XBurst1 | Yes | -- | 128-bit |
| T40 | XBurst2 | -- | MXU3.0 | 512-bit |
| T41 | XBurst2 | -- | MXU3.1 | 512-bit |

### Runtime detection

```c
if (mxu2_available()) { /* MXU2 path */ }
if (mxu3_available()) { /* MXU3 path */ }
```

Both probe the CPU using SIGILL trapping. Safe on any MIPS CPU. Results are cached.

### Kernel requirements

MXU2/MXU3 use COP2 -- the kernel must handle COP2 Unusable exceptions by setting CU2 in CP0 Status. All Ingenic production kernels and Thingino kernels include this handler.

---

## Files

| File | Description |
|------|-------------|
| `mxu2_shim.h` | MXU2 128-bit shim (368 ops, any compiler) |
| `mxu3_shim.h` | MXU3 512-bit shim (498 ops, any compiler) |
| `mxu2_dsp.h` | Optimized MXU2 DSP kernels |
| `test_builtins_full.c` | MXU2 native builtin test (382 tests) |
| `test_shim_mxu2.c` | MXU2 shim test (431 tests) |
| `test_shim_mxu3.c` | MXU3 shim test (455 tests) |
| `test_dsp.c` | DSP kernel tests |
| `mxu_probe.c` | Hardware capability probe |
| `t40-mxu3-fix.patch` | T40 kernel patch (MXU3 on CPU1) |
| `FINDINGS.md` | MXU2 reverse-engineering notes |
| `MXU3_FINDINGS.md` | MXU3 encoding reference |
| `patches/` | GCC and binutils toolchain patches |
| `docs/` | Engineering documentation |

## License

GPL-2.0 -- see [LICENSE](LICENSE)
