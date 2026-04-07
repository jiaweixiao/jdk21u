/*
 * Copyright (c) 2018, 2020, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "precompiled.hpp"
#include "gc/g1/g1BarrierSet.inline.hpp"
#include "gc/g1/g1BarrierSetRuntime.hpp"
#include "gc/g1/g1CardTable.inline.hpp"
#include "gc/g1/g1CollectedHeap.inline.hpp"
#include "gc/g1/g1_globals.hpp"
#include "gc/g1/g1ThreadLocalData.hpp"
#include "gc/g1/heapRegion.inline.hpp"
#include "oops/access.inline.hpp"
#include "oops/compressedOops.inline.hpp"
#include "runtime/interfaceSupport.inline.hpp"
#include "runtime/orderAccess.hpp"
#include "utilities/macros.hpp"

void G1BarrierSetRuntime::write_ref_array_pre_oop_entry(oop* dst, size_t length) {
  G1BarrierSet *bs = barrier_set_cast<G1BarrierSet>(BarrierSet::barrier_set());
  bs->write_ref_array_pre(dst, length, false);
}

void G1BarrierSetRuntime::write_ref_array_pre_narrow_oop_entry(narrowOop* dst, size_t length) {
  G1BarrierSet *bs = barrier_set_cast<G1BarrierSet>(BarrierSet::barrier_set());
  bs->write_ref_array_pre(dst, length, false);
}

void G1BarrierSetRuntime::write_ref_array_post_entry(HeapWord* dst, size_t length) {
  G1BarrierSet *bs = barrier_set_cast<G1BarrierSet>(BarrierSet::barrier_set());
  bs->G1BarrierSet::write_ref_array(dst, length);
}

// G1 pre write barrier slowpath
JRT_LEAF(void, G1BarrierSetRuntime::write_ref_field_pre_entry(oopDesc* orig, JavaThread* thread))
  assert(thread == JavaThread::current(), "pre-condition");
  assert(orig != nullptr, "should be optimized out");
  assert(oopDesc::is_oop(orig, true /* ignore mark word */), "Error");
  // store the original value that was in the field reference
  SATBMarkQueue& queue = G1ThreadLocalData::satb_mark_queue(thread);
  G1BarrierSet::satb_mark_queue_set().enqueue_known_active(queue, orig);
JRT_END

// G1 post write barrier slowpath
JRT_LEAF(void, G1BarrierSetRuntime::write_ref_field_post_entry(volatile G1CardTable::CardValue* card_addr,
                                                               JavaThread* thread))
  assert(thread == JavaThread::current(), "pre-condition");
  G1DirtyCardQueue& queue = G1ThreadLocalData::dirty_card_queue(thread);
  G1BarrierSet::dirty_card_queue_set().enqueue(queue, card_addr);
JRT_END

namespace {

inline oop load_post_barrier_target(HeapWord* field_addr) {
  // The x86 interpreter/C1 slow path only passes the field address. Reload the
  // just-stored heap oop here so the runtime path works for both compressed and
  // uncompressed oops without relying on the register representation chosen by
  // the caller.
  if (UseCompressedOops) {
    narrowOop heap_oop = RawAccess<MO_RELAXED>::oop_load(reinterpret_cast<narrowOop*>(field_addr));
    return CompressedOops::decode(heap_oop);
  }

  oop heap_oop = RawAccess<MO_RELAXED>::oop_load(reinterpret_cast<oop*>(field_addr));
  return CompressedOops::decode(heap_oop);
}

} // namespace

JRT_LEAF(void, G1BarrierSetRuntime::write_ref_field_post_slow_entry(HeapWord* field_addr,
                                                                    JavaThread* thread))
  assert(thread == JavaThread::current(), "pre-condition");
  assert(field_addr != nullptr, "pre-condition");

  G1CollectedHeap* g1h = G1CollectedHeap::heap();
  G1CardTable* ct = g1h->card_table();
  G1CardTable::CardValue* card_addr = ct->byte_for(field_addr);
  G1DirtyCardQueue& queue = G1ThreadLocalData::dirty_card_queue(thread);

  OrderAccess::storeload();

  G1CardTable::CardValue card_value = *card_addr;
  // When the experimental young-to-young remset extension is disabled, keep the
  // original G1 slow-path behavior and only ever enqueue old dirty cards.
  if (G1EnableYoungToYoungLowToHighRSet && G1CardTable::is_young_card_val(card_value)) {
    if (card_value != G1CardTable::g1_young_card_val()) {
      return;
    }

    oop new_val = load_post_barrier_target(field_addr);
    if (new_val == nullptr) {
      return;
    }

    // For the young-to-young extension the runtime path only decides whether the
    // source card should be logged once. The final "is the target still young?"
    // decision is intentionally deferred to refinement / evacuation-time rebuild:
    // this avoids depending on transient register encodings here and keeps the
    // slow path cheap.
    if (!HeapRegion::is_in_same_region(field_addr, new_val) && p2i(field_addr) < p2i(new_val)) {
      if (ct->mark_young_card_as_logged(card_addr)) {
        G1BarrierSet::dirty_card_queue_set().enqueue(queue, card_addr);
      }
    }
    return;
  }

  if (*card_addr != G1CardTable::dirty_card_val()) {
    *card_addr = G1CardTable::dirty_card_val();
    G1BarrierSet::dirty_card_queue_set().enqueue(queue, card_addr);
  }
JRT_END
