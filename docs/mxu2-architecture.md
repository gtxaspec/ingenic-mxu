# MXU2 Architecture Reference

## Register file

MXU2 uses COP2 coprocessor registers. 32 vector registers ($vr0-$vr31), each 128 bits wide.

```
$vr0  [127:96] [95:64] [63:32] [31:0]
      elem[3]  elem[2] elem[1] elem[0]
```

Each register can be viewed as:
- 16 x 8-bit (byte)
- 8 x 16-bit (halfword)
- 4 x 32-bit (word) or float
- 2 x 64-bit (doubleword) or double

$vr31 is reserved by the GCC backend (used as a scratch register during register spills).

## XBurst COP2 differences

Standard MIPS provides mtc2/mfc2/lwc2/swc2 for COP2 register access. **XBurst does NOT implement these** -- they all cause SIGILL. MXU2 replaces COP2 entirely with its own transfer instructions:

| Standard MIPS | XBurst MXU2 | Direction |
|---------------|-------------|-----------|
| `mtc2 $gp, $vr` | `insfcpuw $vr[elem], $gp` | GPR -> COP2 (element insert) |
| `mfc2 $gp, $vr` | `mtcpusw $gp, $vr[elem]` | COP2 -> GPR (element extract, signed) |
| `lwc2 $vr, mem` | `lu1q $vr, offset($base)` | MEM -> COP2 (128-bit load) |
| `swc2 $vr, mem` | `su1q $vr, offset($base)` | COP2 -> MEM (128-bit store) |

The naming is from Ingenic's perspective: "move to CPU" (mtcpus) extracts FROM COP2 TO the CPU, "insert from CPU" (insfcpu) inserts FROM CPU INTO COP2.

## Instruction encoding

All MXU2 instructions are 32-bit COP2 instructions. The primary opcode is 0x1C (bits 31:26 = 011100).

```
31    26 25   21 20   16 15         6 5      0
[011100] [ vt  ] [ vs  ] [  varies  ] [ func ]
```

The `func` field (bits 5:0) determines the operation. The source/dest register fields at bits 21:16, 16:11, and 6:11 carry $vr register numbers (0-31) or immediates depending on the instruction.

### Branch encoding

MXU2 branch instructions (`bnez`/`beqz` variants) encode a 10-bit signed PC-relative offset at bit position 6 with a 2-bit right shift (word-aligned targets). Range: +/- 2048 bytes.

```
31    26 25   21 20   16 15       6 5      0
[011100] [sub ] [  vs  ] [offset10] [28/29]
```

## Instruction categories

### Arithmetic (per-element, all widths b/h/w/d)
- `add`, `sub`, `mul`: wrapping arithmetic
- `addss`/`adduu`, `subss`/`subuu`: saturating signed/unsigned
- `divs`/`divu`, `mods`/`modu`: division and modulo
- `madd`/`msub`: multiply-accumulate/subtract (3-operand: acc + a*b)

### Floating-point (w = float32, d = float64)
- `fadd`, `fsub`, `fmul`, `fdiv`, `fsqrt`
- `fmadd`, `fmsub`: fused multiply-add/subtract
- `fmax`, `fmin`, `fmaxa`, `fmina`: max/min (including absolute)
- `fceq`, `fcle`, `fclt`, `fcor`: comparisons
- `fclass`: classify (NaN, Inf, zero, etc.)

**Note:** MXU2 uses `w` suffix for single-precision float (not `s`). `faddw` not `fadds`.

### Shifts
- Variable: `sll`, `sra`, `srl` (shift amount from vector)
- Immediate: `slli`, `srai`, `srli` (compile-time constant)
- Rounding: `srari`, `srlri` (arithmetic/logical right shift with rounding)

### Dot product and widening
- `dotps`/`dotpu`: dot product (signed/unsigned), result widens
- `dadds`/`daddu`/`dsubs`/`dsubu`: double-width add/subtract
- `dpmuls`/`dpmulu`: double-width multiply

### Type conversion
- `vcvts`/`vcvtu`: float-to-int and int-to-float
- Width conversions between adjacent sizes

### Element operations
- `mfcpu`: broadcast GPR scalar to all elements
- `mtcpus`/`mtcpuu`: extract element to GPR (signed/unsigned)
- `insfcpu`: insert GPR scalar into specific element
- `insfmxu`: insert element from another vector
- `repx`/`repi`: replicate element to all lanes
- `shufv`: byte shuffle with control vector

### FPU bridge
- `mffpu`: broadcast FPU register to MXU2 vector
- `mtfpu`: extract element from MXU2 vector to FPU register
- `insffpu`: insert FPU value into MXU2 vector element

### Load/Store
- `lu1q`/`su1q`: unaligned 128-bit load/store with immediate offset
- `lu1qx`/`su1qx`: unaligned with register offset
- `la1q`/`sa1q`: aligned 128-bit load/store with immediate offset
- `la1qx`/`sa1qx`: aligned with register offset

All load/store require base + offset addressing. Data must be 16-byte aligned for aligned variants.

### Control
- `cfcmxu`: read MXU2 control/status register
- `ctcmxu`: write MXU2 control/status register
- `li`: load immediate (15-bit signed, broadcast to all elements)

## Control registers

| Register | Name | Description |
|----------|------|-------------|
| 0 | MIR | MXU2 Implementation Register (read-only) |
| 31 | MSR | MXU2 Status Register |

## Kernel interaction

The first MXU2 instruction in a process triggers a COP2 Unusable exception. The kernel handles this by:
1. Setting CU2 bit in CP0 Status register
2. Returning to retry the instruction

Subsequent MXU2 instructions execute without exception overhead. Context switches save/restore COP2 state.
