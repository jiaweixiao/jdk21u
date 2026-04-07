# G1 Young-to-Young RemSet Notes

## Goal

This change extends the existing G1 old-to-young remembered-set pipeline so that
it can also preserve a restricted subset of young-to-young cross-region
references.

For a store:

```text
x.field = y
```

we log the source card only when all of the following hold:

1. `x` is in young region `X`
2. `y` is in young region `Y`
3. `X != Y`
4. `&x.field < y`
5. the source card is still plain `g1_young_gen`

If those checks pass, we log the source card once and later add that card to
`Y`'s remset.

## JVM Option

The entire extension is guarded by a new experimental JVM option:

```bash
-XX:+UnlockExperimentalVMOptions -XX:+G1EnableYoungToYoungLowToHighRSet
```

Default:

```bash
-XX:-G1EnableYoungToYoungLowToHighRSet
```

There is also a second experimental switch for profiling the young-GC dequeue
fallback:

```bash
-XX:+UnlockExperimentalVMOptions -XX:+G1YoungToYoungLowToHighRSetPauseScan
```

That option defaults to `false`. With the default setting, young GC does not do
the expensive pause-time scan of `young_logged` cards.

Design intent when the option is disabled:

- G1 keeps the original old-to-young remembered-set behavior
- no young-to-young cards are logged
- no paused dequeue work is added for `young_logged`
- evacuation does not rebuild young-to-young remset entries

The name deliberately encodes the direction: only lower-address young sources
that point to higher-address young targets are recorded. In the current layout
that corresponds to keeping links from more recently allocated young regions
towards earlier young regions.

In other words, disabling the option should fall back to the pre-extension
execution paths, so there is no steady-state performance cost beyond carrying
the compiled code and the extra card value definition.

## Card-State Design

The main additional state is:

```cpp
g1_young_gen_logged
```

It means "this young card has already been enqueued once for the custom
young-to-young remset pipeline".

Why this extra state is needed:

- It avoids repeated enqueue of the same young source card.
- It keeps young cards distinct from ordinary dirty old cards.
- It lets refinement restore the card back to plain young after processing.

This is different from ordinary `dirty_card_val()` on purpose. Reusing the old
dirty state for young cards would confuse existing G1 logic that interprets dirty
cards as old-root scanning work.

## Fast and Slow Barrier Paths

### C2

The C2 post-barrier handles the common case directly:

- old-source cards continue to use the normal dirty/enqueue path
- young-source cards use the custom path and transition
  `g1_young_gen -> g1_young_gen_logged`

When `G1EnableYoungToYoungLowToHighRSet` is disabled, C2 does not emit this extra
young-card logging branch and falls back to the original old-card-only path.

### x86 runtime / interpreter / C1 slow path

The x86 slow path does **not** trust the incoming `new_val` register to already
be in canonical `oop` form. This matters when `UseCompressedOops = true`.

Instead, the slow helper now receives only `field_addr` and reloads the value
from memory:

- if `UseCompressedOops`, load as `narrowOop` and decode
- otherwise load as ordinary `oop`

This was not just a cleanup. An earlier version passed `new_val` directly into
the runtime helper, and `make CONF=linux-x86_64-server-release images` crashed in
`jmod create` because the runtime helper treated a non-canonical value like a
normal heap oop.

With `G1EnableYoungToYoungLowToHighRSet` disabled, this runtime helper also falls back to
the original behavior and ignores young cards completely.

## Where the Real Filtering Happens

The runtime/barrier path only decides whether a source card should be logged.
The final remembered-set insertion is filtered later:

- source region must still be young
- target region must still be young
- regions must differ
- source field address must still be lower than the target object address

That final filter lives in `G1ConcurrentRefineOopClosure` and in the evacuation
path for surviving objects.

This split is intentional:

- barrier path stays cheap
- target liveness / region-kind checks happen where heap state is reloaded
- stale queue entries can safely be ignored later

## Why The Young-GC Dequeue Path Needed Special Handling

The first implementation tried to handle queued `young_logged` cards during
young GC by reusing the usual "card -> block_start() -> walk objects" style.

That was attractive because it mirrored the existing old-to-young machinery, but
it failed in practice. During:

```bash
make CONF=linux-x86_64-server-release images
```

the VM crashed in `HeapRegion::block_start()` while processing dequeued young
cards.

The issue was not the queue itself. The issue was the object-discovery step:

- the queue may contain **stale young source cards**
- those cards are valid as "source cards we still want to account for"
- but feeding their `card_start` back into `block_start()` turned out to be too
  brittle for this new use case

This strongly suggests that the current BOT / `block_start()` contract is well
tuned for the existing old/humongous card-scanning paths, but is not a good
foundation for parsing arbitrary stale young source cards during dequeue.

## Final Young-GC Strategy

Young GC still treats merge-roots as an **old/humongous to young** root-scanning
pipeline, but the dequeue point itself now classifies the two kinds of cards:

1. **dirty old/humongous cards**
   These continue to go through the original merge-roots / scan-roots pipeline.

2. **`young_logged` cards**
   These are never added to the merge-roots dirty-region set. By default the
   dequeue path only clears them back to plain young cards and leaves the real
   remembered-set work to concurrent refinement plus evacuation-time rebuild.
   The older pause-time scan is still available behind
   `-XX:+G1YoungToYoungLowToHighRSetPauseScan` for profiling experiments.

This default was chosen because a direct pause-time scan of dequeued
`young_logged` cards made `Merge Heap Roots` extremely expensive. Profiling
showed that the dominant cost was not scanning objects inside the card, but
repeatedly locating the first overlapping object for almost every dequeued card.
Sorting cards by address helped, but the safest default is still to keep that
work out of the pause.

### Optional Pause-Time Scan

When `G1YoungToYoungLowToHighRSetPauseScan` is enabled, the dequeue path uses a
young-only forward walk and still avoids `block_start(card_start, ...)`. It:

1. find the source young region
2. use `region->bottom()` as the stable object-layout base
3. walk objects forward until we reach the first object overlapping the card
4. scan only the dequeued card's covered range
5. add matching `young -> young`, cross-region, `p < obj` references to the
   target remset

This preserves the desired "handle it during dequeue" semantics while avoiding
the brittle BOT-based reverse lookup that previously crashed.

### Pause-Time Optimization

Scanning from `region->bottom()` is robust, but naive repeated use would be
expensive if the same young region contributes many dequeued cards in one pause.

To avoid that, the implementation keeps a **per-worker, per-region cursor
cache**:

- `last_obj_start[region]`
- `last_obj_end[region]`

When the same worker sees another `young_logged` card from the same region:

- if the new card falls inside the cached object interval, that object is reused
- if the new card is later in the region, scanning resumes from `last_obj_end`
- only out-of-order cards fall back to `region->bottom()`

This keeps the implementation independent of BOT-based reverse lookup while
avoiding repeated full prefix scans for the common in-order case.

Young-to-young internal references are therefore handled by these paths:

1. **Concurrent refinement before the pause**
   If a logged young card is refined before young GC starts, the target young
   region remset is updated there.

2. **Evacuation-time rebuild**
   If the queue entry survives until young GC, evacuation still rebuilds the
   live subset of young-to-young references through
   `G1ParScanThreadState::enqueue_card_if_tracked()`.

3. **Optional young-GC dequeue scan**
   Only when `-XX:+G1YoungToYoungLowToHighRSetPauseScan` is enabled, the young-GC
   dequeue path scans the card directly using the young-only forward walk
   described above.

## Concurrent-Refinement Note

The same BOT-based reverse lookup that was fragile in young-GC dequeue turned
out to be unsafe for concurrent refinement too. A real application run crashed
in `G1 Refine#0` at:

```text
HeapRegion::oops_on_memregion_iterate_with_nullptr<...>
```

while `G1RemSet::refine_card_concurrently()` was processing a `young_logged`
card.

The fix mirrors the pause-time helper:

- old/humongous cards still use the original careful iterator
- young logged cards use a young-only forward walk from `region->bottom()`
- any unparsable state causes re-enqueue rather than a crash

This keeps concurrent refinement responsible for most young-to-young remset
updates while avoiding the BOT / `block_start()` contract for arbitrary stale
young source cards.

Both of these paths are gated by `G1EnableYoungToYoungLowToHighRSet`. With the option
disabled, concurrent refinement, dequeue, and evacuation all behave exactly like
the original old-to-young-only implementation.

This has an important semantic consequence:

- dead young objects do not matter anymore
- dead but still parseable young objects may still contribute their source card
  during dequeue, which is acceptable because remsets store source cards rather
  than individual references
- evacuation still rebuilds the live subset
- young GC root merging still ignores these internal young-only links

## Correctness Summary

The intended properties are:

1. Record only cross-region young-to-young references with `field_addr < target`.
2. Avoid duplicate enqueue of the same source card.
3. Ensure the remembered set is updated by concurrent refinement and evacuation;
   the young-GC dequeue path is optional and only used for profiling
   experiments when explicitly enabled.
4. Keep young GC merge-roots focused on old-to-young scanning and ignore these
   young-internal links there.

The current implementation follows exactly that design.

## Files Worth Reviewing

- `src/hotspot/share/gc/g1/g1CardTable.hpp`
- `src/hotspot/share/gc/g1/c2/g1BarrierSetC2.cpp`
- `src/hotspot/share/gc/g1/g1BarrierSetRuntime.cpp`
- `src/hotspot/cpu/x86/gc/g1/g1BarrierSetAssembler_x86.cpp`
- `src/hotspot/share/gc/g1/g1OopClosures.inline.hpp`
- `src/hotspot/share/gc/g1/g1ParScanThreadState.inline.hpp`
- `src/hotspot/share/gc/g1/g1RemSet.cpp`

## Validation

The following command now passes with `UseCompressedOops = true`:

```bash
make CONF=linux-x86_64-server-release images
```

The feature is disabled by default. To exercise it explicitly:

```bash
java -XX:+UnlockExperimentalVMOptions -XX:+G1EnableYoungToYoungLowToHighRSet ...
```
