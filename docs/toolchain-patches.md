# MXU2 Toolchain Patches

## Overview

Two patches add native MXU2 support to the GNU toolchain:
- **Binutils 2.44**: assembler, disassembler, 364 opcodes
- **GCC 15.2.0**: backend, instruction patterns, 382 builtins

## Applying patches

### Buildroot / Thingino

Copy the monolithic patches:
```sh
cp patches/0001-add-ingenic-mxu2-support.patch <buildroot>/package/all-patches/binutils/2.44/
cp patches/0001-add-ingenic-mxu2-backend.patch <buildroot>/package/all-patches/gcc/15.2.0/
```

Rebuild the toolchain:
```sh
make host-binutils-dirclean && make rebuild-host-binutils
make host-gcc-initial-dirclean && make rebuild-host-gcc-initial
```

### Standalone (non-buildroot)

Apply to extracted source trees:
```sh
cd binutils-2.44 && patch -p1 < 0001-add-ingenic-mxu2-support.patch
cd gcc-15.2.0 && patch -p1 < 0001-add-ingenic-mxu2-backend.patch
```

### Upstream submission

The `patches/binutils-split/` and `patches/gcc-split/` directories contain the same changes split into logical patch series (4 patches each) suitable for submission to `binutils@sourceware.org` and `gcc-patches@gcc.gnu.org`.

## Two-stage build (before full toolchain integration)

Until the full toolchain (including C library) is rebuilt with MXU2 support, use a two-stage build:

```sh
# Stage 1: compile + assemble with patched tools
$PATCHED_GCC -mmxu2 -O2 -S -o prog.s prog.c
$PATCHED_AS  -mmxu2 -o prog.o prog.s

# Stage 2: link with existing toolchain (has libc)
$OLD_GCC -static -o prog prog.o -lm
```

Once the full toolchain ships with the patches, a single command works:
```sh
mipsel-linux-gcc -mmxu2 -O2 -static -o prog prog.c -lm
```

## Binutils patch details

### Files modified
| File | Changes |
|------|---------|
| `include/opcode/mips.h` | `OP_REG_MXU128`, `OP_REG_MXU128_CR`, `ASE_MXU128` |
| `opcodes/mips-opc.c` | `=` operand prefix handler, 364 instruction opcodes |
| `opcodes/mips-dis.c` | `-M mxu2` option, `$vr0-$vr31` register printing |
| `gas/config/tc-mips.c` | `-mmxu2` option, register parsing, branch encoding |

### Operand format prefix `=`

MXU2 introduces the `=` operand format prefix (alongside existing `+`, `m`, `-`):

| Format | Type | Description |
|--------|------|-------------|
| `=s` | REG(5,16) | Source $vr register |
| `=t` | REG(5,21) | Target $vr register |
| `=h` | REG(5,6) | Dest $vr register |
| `=j` | REG(5,11) | Third $vr register |
| `=k` | SINT(15,11) | 15-bit signed immediate |
| `=l` | SINT(10,11) | 10-bit signed immediate |
| `=g` | BRANCH(10,6,2) | 10-bit PC-relative branch offset |
| `=v` | IMM_INDEX(5,16) | 5-bit element index |
| `=w` | IMM_INDEX(8,16) | 8-bit element index |
| `=x` | REG(5,6,CR) | Control register |
| `=y` | UINT(8,16) | 8-bit unsigned immediate |
| `=L` | INT_ADJ(10,11) | 10-bit aligned offset |

### Branch encoding

MXU2 branches encode a 10-bit signed offset at bit position 6 (not position 0 like standard MIPS). The assembler detects MXU2 branches by:
- `append_insn`: checks `ASE_MXU128` flag on the matched opcode
- `md_apply_fix`: checks instruction bits (opcode 0x1C, function 0x28/0x29)

MXU2 branches are excluded from branch relaxation (no macro expansion for out-of-range branches).

## GCC patch details

### Files modified/added
| File | Type | Description |
|------|------|-------------|
| `mips.opt` | Modified | `-mmxu2` option |
| `mips.h` | Modified | ISA flags, register defs, mode predicates |
| `mips-protos.h` | Modified | Function prototypes |
| `constraints.md` | Modified | `q` constraint for COP2_REGS |
| `predicates.md` | Modified | MXU2-specific operand predicates |
| `mips.cc` | Modified | Register handling, moves, builtin expansion |
| `mips.md` | Modified | Include mips-mxu2.md |
| `config.gcc` | Modified | Install mxu2.h header |
| `mips-ftypes.def` | Modified | 4 new function types for FPU bridge |
| `mips-mxu2.md` | New | 141 instruction patterns |
| `ingenic-mxu2.def` | New | 382 builtin registrations |
| `mxu2.h` | New | Public intrinsics header |

### Key architectural decisions

**4-register-per-vector model**: `hard_regno_nregs` returns 4 for 128-bit vector modes in COP2. Each vector occupies 4 consecutive register numbers (e.g., $vr0 uses COP2 regs 0-3). This makes `subreg:SI` at byte offsets 0/4/8/12 map to consecutive registers, preserving element indices through regalloc.

**No standard COP2 instructions**: XBurst has NO `mtc2`/`mfc2`/`lwc2`/`swc2`. All cause SIGILL. The backend uses:
- `insfcpuw $vrN[elem], $gp` for GPR-to-COP2 element insert
- `mtcpusw $gp, $vrN[elem]` for COP2-to-GPR element extract
- Multi-instruction `.set noat` sequences for MEM<->COP2 transfers

**V4SF format suffix**: MXU2 uses `w` for single-precision float (`faddw`), not `s` (`fadds`). The `mxu2fmt` attribute maps V4SF to "w".

**Builtin expansion**: Load/store builtins bypass `maybe_expand_insn` and call gen functions directly to avoid MEM operand mismatch. Store builtins force the value operand into a register to prevent mem-to-mem moves.

### Register allocation

```
COP2 reg 0-3   -> $vr0  (V4SI/V8HI/V16QI/V2DI/V4SF/V2DF)
COP2 reg 4-7   -> $vr1
...
COP2 reg 120-123 -> $vr30
COP2 reg 124-127 -> $vr31 (RESERVED - scratch for spills)
```

All COP2 registers are call-clobbered. The `q` constraint maps to `COP2_REGS`.
