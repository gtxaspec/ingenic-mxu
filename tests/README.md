# MXU2 toolchain regression suite

Catches the classes of bugs we hit during the v2..v12 fix work, plus
likely-untested neighbours. Runs in <2 minutes (Layers A+B are pure-host
compile; C+D need NFS-mounted device).

## Layout

| Layer | Dir | What | Catches |
|-------|-----|------|---------|
| A | `codegen/` | 144 generated C files: 6 modes × 8 patterns × 3 opts. Pass = compiles. | ICEs (all 5 we hit + neighbours) |
| B | `codegen/` | Hand-written C + golden grep counts on `.s`. | Silent perf regression (e.g. NREGS reverts to 4 → spilling reappears) |
| C | `bench/` | Compile, scp via NFS, run on device, threshold metric. | Real-world perf regression |
| D | `builtin/` | Pass-by-value wrapper around add for each mode, run on device. | Calling-convention regressions per mode |

## Run

```bash
./run_all.sh                   # everything
SKIP_DEVICE=1 ./run_all.sh     # A+B only (no T20/T31 needed)

# Individual layers:
codegen/gen_matrix.py codegen/out/
codegen/run_matrix.sh codegen/out/
codegen/golden.sh
bench/run_bench.sh
builtin/run_pbv.sh
```

## Environment

| Var | Default | Purpose |
|-----|---------|---------|
| `GCC` | thingino xb1 cc1 | Compiler under test |
| `DEVICE` | *(required)* | Test device IP, e.g. a T20 or T31 board (must NFS-mount `NFS_SHARE`) |
| `NFS_SHARE` | /home/turismo | NFS server export path |
| `NFS_MOUNT` | /mnt/nfs | NFS mount path on device |
| `SKIP_DEVICE` | 0 | If 1, skip layers C+D |

## Adding tests

- **New ICE class**: add a `pat_*` function in `gen_matrix.py`. Will fan out across all modes and opt levels automatically.
- **New codegen invariant**: add a `check ...` line in `golden.sh`.
- **New benchmark**: drop `bench_NAME.c` in `bench/`, add a threshold to `THRESHOLDS` in `run_bench.sh`.

## Why this catches what `test_builtins_full.c` misses

Per-builtin functional tests (382/382 passing) didn't catch any of the v2..v12
bugs because each bug needed an *integration pattern*: chained ops, function
calling convention, register pressure, or shuffle inside a loop. This suite
exercises the patterns explicitly — A as cheap compile-only matrix, B as
golden codegen assertions, C as throughput regression on the device.
