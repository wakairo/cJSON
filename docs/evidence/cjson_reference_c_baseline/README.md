# Track V / Issue #204 — frozen cJSON reference-C runtime evidence

**NON-NORMATIVE, NON-FROZEN auxiliary C reference observations; no change to pre-registered North Star oracle, corpus or PASS/FAIL.**

- Execution target is `DaveGamble/cJSON@6d9f2443ab071f86e5d9b43025a40929ec41c46c`, never the auxiliary evidence branch HEAD.
- This branch is intentionally **unmerged**, and `master`, `cJSON.c`, `cJSON.h`, `tests/misc_tests.c`, `tests/common.h` must stay unchanged.
- `run_ci.sh` checks frozen SHA ancestry and an allowlist of added paths, creates a detached pinned Git worktree, verifies four `git hash-object` identities, and builds the original frozen `cJSON.c` with `baseline.c`, using real GCC, Clang and Clang ASan/UBSan.
- `baseline.c` is an independent C11 allocator-hook observer. It does not repair any link, mint ownership, suppress frees, or run intentional double-free/use-after-free. Each allocation has a monotonic ID independent of address reuse; `edge` events snapshot only live records. Its `node_candidate` allocation class is based only on allocation size and is not a proof of type.
- Positive workflows: middle detach/reparent/real free; head and non-head replacement with real heap nodes and key allocation; reference wrapper `IsReference` with target owner; recursive mixed-ownership deletion; NULL-only failure for node, key, and wrapper creation.
- Negative controls: `--perturb-edge` and `--perturb-free` intentionally falsify *expectations only*. Each must exit 3 and emit `DETECTED_NEGATIVE_EXPECTATION_OR_ERROR`.
- Separate **diagnostic, not a passing case**: `--probe-orphan` fails the reference-to-object key allocation after wrapper allocation. Exit 4 means one outstanding original C allocation; no observer cleanup is supplied. This does not assert a NewLang defect or North Star FAIL.
- Results are exact process stdout/stderr JSONL, run metadata, per-allocation identity and actual physical addresses, terminal `finish` events and the native exit code. Address bytes are ASLR-dependent and must not be fixed-golden test data.
- The Actions workflow runs only for `v204-*` evidence branch pushes. See its job logs and archived raw `v204-results`. It is **not** a rewritten frozen experiment or a NewLang cJSON port.

### Reproduce in fork checkout (Linux)

```sh
git checkout v204-frozen-cjson-reference-evidence
bash docs/evidence/cjson_reference_c_baseline/run_ci.sh
# Results in v204-results/; all source identity checks precede compilation.
```

### Source locations in *frozen* upstream

- [Detach](https://github.com/DaveGamble/cJSON/blob/6d9f2443ab071f86e5d9b43025a40929ec41c46c/cJSON.c#L2258-L2292).
- [Reference wrapping and add](https://github.com/DaveGamble/cJSON/blob/6d9f2443ab071f86e5d9b43025a40929ec41c46c/cJSON.c#L1998-L2148).
- [Replace](https://github.com/DaveGamble/cJSON/blob/6d9f2443ab071f86e5d9b43025a40929ec41c46c/cJSON.c#L2368-L2457).
- [Recursive delete](https://github.com/DaveGamble/cJSON/blob/6d9f2443ab071f86e5d9b43025a40929ec41c46c/cJSON.c#L253-L275).

Product evidence belongs in [NewLang_Compiler Issue #204](https://github.com/wakairo/NewLang_Compiler/issues/204), reviewed by independent Coordination. Existing native two-H/terminal caller-to-callee release [PR #202](https://github.com/wakairo/NewLang_Compiler/pull/202) and Draft 17.28 source-only returned-live-owner selection are **not** proofs of cJSON's multi-node, reference policy, recursion, or OOM behavior. Preserve the existing B1–B8, topology >= 5/6, per-hop unchecked=0, and L/R/P/V/C criteria without alteration.
