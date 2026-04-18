#!/bin/bash
# Per-builtin correctness oracle.
#
# 1) Run builtin_oracle_gen.py to (re)generate oracle_all.c
# 2) Build it with our GCC15 toolchain and the vendor R5.2.1 toolchain
# 3) Stage both binaries via NFS and run on T20
# 4) Diff outputs line-by-line; report builtins that differ
#
# Exit 0 iff every builtin produced bit-identical output (FP-tolerant
# names listed in oracle_gen/fp_tolerant.txt are exempt and reported
# separately).
set -u

OURS_GCC="${OURS_GCC:-/home/turismo/projects/thingino/thingino-firmware/output/master/toolchain_xburst1_uclibc_gcc15-3.10.14-musl/host/bin/mipsel-linux-gcc}"
VENDOR_GCC="${VENDOR_GCC:-/home/turismo/toolchains/vendor/mips-ingenic-xburst1-linux-glibc2.38-tools-r5.2.1.sr03/bin/mips-linux-gnu-gcc}"
DEVICE="${DEVICE:-192.0.2.53}"
NFS_LOCAL="${NFS_LOCAL:-/home/turismo}"
NFS_REMOTE="${NFS_REMOTE:-/mnt/nfs}"
WORKDIR="$NFS_LOCAL/.oracle_builtin"
# Optimization level for OUR build. Vendor is always built at -O2. Set
# OPTLEVEL to e.g. "-O0 -O2 -O3" to sweep multiple levels (each compared
# against vendor -O2 — they should match at every level).
OPTLEVEL="${OPTLEVEL:--O2}"

cd "$(dirname "$0")"
mkdir -p "$WORKDIR"
# NFS root_squash maps device-side root to nobody; allow remote root writes.
chmod 0777 "$WORKDIR"

# 1) Generate
python3 ./builtin_oracle_gen.py || { echo "generator failed"; exit 1; }
SRC="oracle_gen/oracle_all.c"
[ -f "$SRC" ] || { echo "generator did not produce $SRC"; exit 1; }

# 2) Build vendor reference once (always -O2)
echo "== building with vendor R5.2.1 toolchain (-O2) =="
if ! "$VENDOR_GCC" -O2 -mmxu2 -EL -march=mips32r2 -static "$SRC" \
        -o "$WORKDIR/oracle_vendor" 2>"$WORKDIR/build_vendor.log"; then
    echo "FAIL: vendor did not build"
    head -40 "$WORKDIR/build_vendor.log"
    grep -E "implicit declaration|unknown builtin|undefined reference" \
        "$WORKDIR/build_vendor.log" || true
    exit 1
fi

# Filter "warning: unknown builtin" / similar — track if any were emitted.
unsupported=$(grep -E "unknown builtin|implicit declaration" \
    "$WORKDIR/build_vendor.log" | sed -E 's/.*\b(__builtin_mxu2_[a-z0-9_]+)\b.*/\1/' \
    | sort -u || true)
if [ -n "$unsupported" ]; then
    echo "vendor does not support these builtins (excluded from comparison):"
    echo "$unsupported" | sed 's/^/  /'
fi

# Run vendor once, write directly into NFS workdir (which we made 0777
# so device-side nobody can write).
echo "== running vendor binary on $DEVICE =="
ssh "root@$DEVICE" "$NFS_REMOTE/.oracle_builtin/oracle_vendor > $NFS_REMOTE/.oracle_builtin/o_vendor.txt 2>&1; chmod 0666 $NFS_REMOTE/.oracle_builtin/o_vendor.txt; wc -l $NFS_REMOTE/.oracle_builtin/o_vendor.txt" \
    || { echo "FAIL: vendor run failed"; exit 1; }

OVERALL_FAIL=0

for opt in $OPTLEVEL; do
echo
echo "================ OPTLEVEL $opt ================"
# Build ours at this opt level
echo "== building with our toolchain ($opt) =="
if ! "$OURS_GCC" $opt -mmxu2 -EL -static "$SRC" -o "$WORKDIR/oracle_ours" \
        2>"$WORKDIR/build_ours.log"; then
    echo "FAIL: ours did not build at $opt"
    head -40 "$WORKDIR/build_ours.log"
    OVERALL_FAIL=$((OVERALL_FAIL+1))
    continue
fi

echo "== running our binary on $DEVICE =="
ssh "root@$DEVICE" "$NFS_REMOTE/.oracle_builtin/oracle_ours > $NFS_REMOTE/.oracle_builtin/o_ours.txt 2>&1; chmod 0666 $NFS_REMOTE/.oracle_builtin/o_ours.txt; wc -l $NFS_REMOTE/.oracle_builtin/o_ours.txt" \
    || { echo "FAIL: ours run failed"; OVERALL_FAIL=$((OVERALL_FAIL+1)); continue; }

OUR_OUT="$WORKDIR/o_ours.txt"
VEN_OUT="$WORKDIR/o_vendor.txt"

# 4) Diff
echo "== diffing outputs =="
ours_lines=$(wc -l <"$OUR_OUT" 2>/dev/null || echo 0)
ven_lines=$(wc -l <"$VEN_OUT" 2>/dev/null || echo 0)
emitted=$(wc -l <oracle_gen/emitted.txt 2>/dev/null || echo 0)

# Identify lines that differ between the two outputs.
diff -u "$VEN_OUT" "$OUR_OUT" >"$WORKDIR/diff.txt" || true
# Each diff hunk contains '-' (vendor) and '+' (ours) lines. Extract the
# builtin names (first token of the line) of every '+' line that begins
# with a builtin name and has a matching '-' counterpart.
python3 - "$VEN_OUT" "$OUR_OUT" "$WORKDIR/diff_builtins.txt" <<'PY'
import sys
ven_path, our_path, out_path = sys.argv[1], sys.argv[2], sys.argv[3]
with open(ven_path) as f: ven = {l.split()[0]: l.rstrip("\n") for l in f if l.strip()}
with open(our_path) as f: our = {l.split()[0]: l.rstrip("\n") for l in f if l.strip()}
diffs = []
for k, ourl in our.items():
    venl = ven.get(k)
    if venl is None:
        diffs.append((k, "missing-in-vendor", ourl, "(absent)"))
    elif venl != ourl:
        diffs.append((k, "differ", ourl, venl))
for k, venl in ven.items():
    if k not in our:
        diffs.append((k, "missing-in-ours", "(absent)", venl))
with open(out_path, "w") as f:
    for k, why, a, b in diffs:
        f.write(f"{why}\t{k}\n  ours:   {a}\n  vendor: {b}\n")
print(f"diffs: {len(diffs)}")
for k, why, _, _ in diffs[:30]:
    print(f"  {why}\t{k}")
if len(diffs) > 30:
    print(f"  ... +{len(diffs)-30} more (see {out_path})")
PY

# Compare against FP-tolerant list.
fp_file=oracle_gen/fp_tolerant.txt
fp_set=""
if [ -f "$fp_file" ]; then fp_set=$(tr '\n' '|' <"$fp_file" | sed 's/|$//'); fi

# Count diffs that are NOT in the FP-tolerant set.
unexpected=0
fp_diff=0
if [ -s "$WORKDIR/diff_builtins.txt" ]; then
    while IFS=$'\t' read -r why name; do
        case "$why" in differ|missing-*) ;; *) continue ;; esac
        if [ -n "$fp_set" ] && echo "$name" | grep -Eq "^($fp_set)$"; then
            fp_diff=$((fp_diff+1))
        else
            unexpected=$((unexpected+1))
        fi
    done < <(grep -E '^(differ|missing-)' "$WORKDIR/diff_builtins.txt" | sed 's/  ours.*$//;s/  vendor.*$//' | sort -u)
fi

echo
echo "----- $opt summary -----"
echo "  builtins emitted: $emitted"
echo "  ours output lines:   $ours_lines"
echo "  vendor output lines: $ven_lines"
total_diffs=$(grep -cE "^(differ|missing-)" "$WORKDIR/diff_builtins.txt" 2>/dev/null || echo 0)
echo "  total per-builtin diffs:   $total_diffs"
echo "  FP-tolerant diffs (OK):    $fp_diff"
echo "  unexpected miscompiles:    $unexpected"
if [ "$unexpected" -gt 0 ]; then
    echo "  see $WORKDIR/diff_builtins.txt for full diff at $opt"
    cp "$WORKDIR/diff_builtins.txt" "$WORKDIR/diff_builtins.${opt#-}.txt"
    OVERALL_FAIL=$((OVERALL_FAIL+1))
fi

done  # opt

echo
echo "============ FINAL ============"
if [ "$OVERALL_FAIL" -eq 0 ]; then
    echo "  all $OPTLEVEL passed: 368 builtins bit-equal to vendor"
    exit 0
else
    echo "  $OVERALL_FAIL opt level(s) had unexpected miscompiles"
    exit 1
fi
