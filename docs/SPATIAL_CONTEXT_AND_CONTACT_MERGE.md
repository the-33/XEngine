# Spatial context stabilization and dense-contact merge (v15)

## Scope

XEngine v15 changes **classification stability** for the `Collision.broadphase: "auto"`, `cellSize: "auto"` configuration and reduces redundant memory indexing in collision contact merging. It does **not** change narrowphase geometry, worker dispatch, contact order, or SDL rendering. Existing `bruteforce` / `grid` choices and numeric `cellSize` values remain supported.

## Spatial context classification

The v14 classifier creates nine independent adaptive-cost contexts: three AABB area occupancy classes times three spatial concentration classes (effective occupied bins in a normalized 4×4 histogram). The recent real Windows dynamic benchmark showed occasional repeated transitions between contexts 6 and 7 under one unchanged clustered distribution; each transition can enter a distinct training bucket.

`engine/include/SpatialContextClassifier.h` stabilizes the *context only*, not the underlying choice of broadphase algorithm:

- The original raw occupancy boundaries **0.1 and 1.0**, and effective-bin boundaries **3 and 8**, remain the nominal class divisions.
- A **15% entry/exit hysteresis band** is applied to each boundary relative to the currently committed class. A layout close to a boundary continues using its prior class instead of oscillating on measurement noise.
- A class transition requires **two consecutive observations** supporting the new class. After a genuine large spatial rearrangement this adds at most one collision pass of classifier delay; the adaptive algorithm may require further calibration passes.
- Changing the collider-count power-of-two bucket or worker availability immediately resets this local classifier state. Reconfiguring broadphase explicitly resets it too.
- Each stable context still has its own measured-cost calibration (`AdaptiveCellBroadphase`). A context class is **never** a direct cutoff selecting `grid` or `bruteforce`.

The 15% band and two-observation confirmation are engineering stabilization settings, **not empirically universal performance thresholds**. The next paired dynamic benchmark should validate fewer false context transitions without a significant loss of phase adaptation.

## Contact merge and pair-history indexing

In v14.1, each contact was inserted into both `mCurrPairs` (`unordered_set`) and `mCurrInfo` (`unordered_map` with colliders and trigger metadata). Both structures carried exactly the same membership keys for dispatch and history; the separate set duplicated hashing, allocations and memory.

In v15:

1. `mCurrInfo` / `mPrevInfo` are the sole per-pair membership and metadata indices. `insert_or_assign` retains the prior last-contact metadata behavior if more than one collider contact shares an entity-pair key.
2. Before merging, the engine counts the contacts held in ordered per-collider result vectors and calls `reserve()` on the output contact array and pair map. This avoids repeated reallocations and rehashes under high overlap.
3. The ordered `NarrowContact` sequence and contact-signature hash remain unchanged. Collision callbacks still run on the main thread, and enter/stay/exit membership checks now read the maps. As in v14.1, **the relative order of Exit callbacks between unrelated pairs is not part of a deterministic public contract**: iterating an `unordered_map` can change its order after reservation/rehashing. Enter/Stay contact order is still defined by the sorted outer-collider order. Test exit-sensitive gameplay explicitly.
4. `RemoveCollider` retains the earlier behavior of invalidating prior pair membership (to suppress `Stay` on the next dispatch) while preserving `mPrevInfo` for `Exit` callbacks. The transient flag resets after dispatch.
5. The F3 `Active Pairs` counter now reads the map size.

### Benchmark interpretation

A synthetic CPU benchmark is provided as `tools/benchmark_contact_merge_cpu.cpp` to reproduce the cost of copying/indexing **precomputed** contacts under both structures; no SDL dependencies. It excludes narrowphase, physical simulation, callbacks, sprite rendering, and the signature computation. A substantial synthetic advantage **must not** be presented as an equal improvement in whole-engine `collision_merge_ms` or frame p50.

The existing in-engine CSV `collision_merge_ms` continues to measure the full merge loop. The matched v14.1 and v15 `alternating`/`cycling` benchmarks should compare its mean/median by layout, mode and contact count, alongside frame p50/p95/p99 and contact-equivalence hashes. The previous clustered `stress` scenario (~456k contacts/fixed step) is a particularly relevant stress case, but a broadphase cost change and a pair-merge improvement are not interchangeable.

### Run the full paired benchmark

From the repository root on the Windows SDL2 development machine:

```powershell
python tools/compare_adaptive_broadphase.py --build --layouts alternating cycling --scenarios collision balanced stress --modes single collision --repetitions 3 --frames 480 --warmup 120 --phase-frames 120 --settle-frames 16
```

Use **the identical command, Debug build profile, hardware and settings** as the archived v14.1 benchmark `adaptive_pair_20261020_214628_483899.zip`. The comparator checks equivalence among the four strategies in v15; **cross-version** equivalence and measured speed differences still need to be checked against the archived v14.1 per-frame profiles.

## Validation and limitations

- The SDL-independent classifier has direct C++20 assertions for jitter, single-frame excursions, true phase changes, resets and concentration calculation.
- The engine's affected collision and benchmark C++ sources receive the existing SDL-free/stubbed syntax checks.
- User-side native Windows SDL2 build, runtime callbacks, dynamic contact equivalence, benchmark speedup and Linux cross-build have **not** yet been verified for v15.
- The classifier and merge are independent changes; compare phase-wise `collision_auto_context_transitions` and `collision_merge_ms` separately before claiming success.
