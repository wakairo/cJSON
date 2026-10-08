#!/usr/bin/env bash
# NON-NORMATIVE / NON-FROZEN Track V evidence; no alteration of cJSON.
set -euo pipefail
ROOT=$(git rev-parse --show-toplevel)
OUT="$ROOT/v204-results"
PIN=6d9f2443ab071f86e5d9b43025a40929ec41c46c
EVID=docs/evidence/cjson_reference_c_baseline
mkdir -p "$OUT"
exec > >(tee "$OUT/console.log") 2>&1
printf 'branch_HEAD=%s\nfrozen_upstream=%s\n' "$(git rev-parse HEAD)" "$PIN" | tee "$OUT/provenance.txt"
git merge-base --is-ancestor "$PIN" HEAD || { echo 'ERROR: pin not ancestor';exit 12; }
while IFS= read -r file;do
  case "$file" in
    "$EVID"/*|.github/workflows/v204-cjson-reference.yml) echo "auxiliary_diff=$file";;
    *) echo "UNAUTHORIZED_FROZEN_DIFF=$file";exit 13;;
  esac
done < <(git diff --name-only "$PIN" HEAD)
GIT_TMP="$(mktemp -d)"
trap 'git worktree remove --force "$GIT_TMP" 2>/dev/null || true' EXIT
git worktree add --detach "$GIT_TMP" "$PIN"
[[ $(git -C "$GIT_TMP" rev-parse HEAD) == "$PIN" ]] || exit 14
[[ -z "$(git -C "$GIT_TMP" status --porcelain -- cJSON.c cJSON.h tests/misc_tests.c tests/common.h)" ]] || exit 15
for line in \
 'cJSON.c 88c2d95b313132abde96d0f6e200789c771fc1e1' \
 'cJSON.h cab5feb427725f8e5c82287f7fe59481b609b9b5' \
 'tests/misc_tests.c fe2325e964485adad2a14e46c103a27ab6b00fd4' \
 'tests/common.h 4db6bf8c2d1fd9c794697dc0ef9bda6209be2cdd';do
 read -r file want <<<"$line"
 got=$(git hash-object "$GIT_TMP/$file")
 printf 'blob %s %s expected %s\n' "$file" "$got" "$want" | tee -a "$OUT/provenance.txt"
 [[ "$got" == "$want" ]] || exit 16
done
for cc in gcc clang;do
 command -v "$cc" || exit 17
 "$cc" --version | head -n1
 "$cc" -std=c11 -Wall -Wextra -Werror -pedantic -O1 -g -I"$GIT_TMP" "$ROOT/$EVID/baseline.c" "$GIT_TMP/cJSON.c" -lm -o "$OUT/v204-$cc"
 set +e
 "$OUT/v204-$cc" >"$OUT/$cc.jsonl" 2>"$OUT/$cc.err"
 result=$?
 set -e
 printf 'RESULT compiler=%s scenario=ordinary exit=%s\n' "$cc" "$result"
 [[ $result == 0 ]] || { cat "$OUT/$cc.err";exit 20; }
 grep -q 'REFERENCE_C_BASELINE_ALL_CHECKS_PASS' "$OUT/$cc.jsonl" || exit 21
 for neg in perturb-edge perturb-free;do
  set +e
  "$OUT/v204-$cc" "--$neg" >"$OUT/$cc.$neg.jsonl" 2>"$OUT/$cc.$neg.err"
  result=$?
  set -e
  printf 'RESULT compiler=%s scenario=%s exit=%s expected=3\n' "$cc" "$neg" "$result"
  [[ $result == 3 ]] || exit 22
  grep -q 'DETECTED_NEGATIVE_EXPECTATION_OR_ERROR' "$OUT/$cc.$neg.err" || exit 23
 done
 set +e
 "$OUT/v204-$cc" --probe-orphan >"$OUT/$cc.orphan.jsonl" 2>"$OUT/$cc.orphan.err"
 result=$?
 set -e
 printf 'RESULT compiler=%s scenario=orphan-diagnostic exit=%s expected-4-if-reproduced\n' "$cc" "$result"
 grep '"event":"potential_upstream_leak"' "$OUT/$cc.orphan.jsonl" || echo 'no orphan event'
done
if command -v clang >/dev/null;then
 clang -std=c11 -Wall -Wextra -Werror -pedantic -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -I"$GIT_TMP" "$ROOT/$EVID/baseline.c" "$GIT_TMP/cJSON.c" -lm -o "$OUT/v204-sanitized"
 set +e
 ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1 "$OUT/v204-sanitized" >"$OUT/sanitized.jsonl" 2>"$OUT/sanitized.err"
 result=$?
 set -e
 printf 'RESULT compiler=clang-asan-ubsan scenario=ordinary exit=%s\n' "$result"
 [[ $result == 0 ]] || { cat "$OUT/sanitized.err";exit 24; }
fi
printf 'BEGIN_FINISH_RECORDS\n'
grep '"event":"finish"' "$OUT"/gcc.jsonl "$OUT"/clang.jsonl "$OUT"/sanitized.jsonl || exit 25
printf 'BEGIN_PINNED_IDENTITY_TRACE\n'
cat "$OUT/gcc.jsonl"
printf 'BEGIN_NEGATIVE_AND_FAILURE_TRACE\n'
grep -E 'CHECK_FAILED|DETECTED_NEGATIVE' "$OUT"/{gcc,clang}.perturb-*.err || true
grep '"event":"malloc_NULL"' "$OUT/gcc.jsonl" || true
printf 'FROZEN_CJSON_REFERENCE_HARNESS_EXECUTED\n'
