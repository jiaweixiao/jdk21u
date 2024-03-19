/*
 * Copyright (c) 2017, 2022, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_GC_G1_G1FULLGCCOMPACTTASK_HPP
#define SHARE_GC_G1_G1FULLGCCOMPACTTASK_HPP

#include "gc/g1/g1FullGCCompactionPoint.hpp"
#include "gc/g1/g1FullGCScope.hpp"
#include "gc/g1/g1FullGCTask.hpp"
#include "gc/g1/heapRegionManager.hpp"
#include "gc/shared/referenceProcessor.hpp"

class G1CollectedHeap;
class G1CMBitMap;
class G1FullCollector;

class G1FullGCCompactTask : public G1FullGCTask {
  G1FullCollector* _collector;
  HeapRegionClaimer _claimer;
  G1CollectedHeap* _g1h;
  // [gc breakdown][mem copy]
  jlong* _copy_cycle_array;
  size_t* _copy_size_array;
  size_t* _copy_count_array;

  void compact_region(HeapRegion* hr, uint worker_id);
  void compact_humongous_obj(HeapRegion* hr);
  void free_non_overlapping_regions(uint src_start_idx, uint dest_start_idx, uint num_regions);

  static void copy_object_to_new_location(oop obj, jlong* cycles);

public:
  G1FullGCCompactTask(G1FullCollector* collector) :
    G1FullGCTask("G1 Compact Task", collector),
    _collector(collector),
    _claimer(collector->workers()),
    _g1h(G1CollectedHeap::heap()) {
      uint num_workers = collector->workers();
      _copy_cycle_array = NEW_C_HEAP_ARRAY(jlong,  num_workers, mtGC);
      _copy_size_array  = NEW_C_HEAP_ARRAY(size_t, num_workers, mtGC);
      _copy_count_array = NEW_C_HEAP_ARRAY(size_t, num_workers, mtGC);
    }

  ~G1FullGCCompactTask() {
    FREE_C_HEAP_ARRAY(jlong,  _copy_cycle_array);
    FREE_C_HEAP_ARRAY(size_t, _copy_size_array);
    FREE_C_HEAP_ARRAY(size_t, _copy_count_array);
  }

  void work(uint worker_id);
  void serial_compaction();
  void humongous_compaction();

  inline void inc_copy_cycle(uint worker_id, jlong c)  { _copy_cycle_array[worker_id] += c; }
  inline void inc_copy_size(uint worker_id, size_t s)  { _copy_size_array[worker_id] += s; }
  inline void inc_copy_count(uint worker_id, size_t c) { _copy_count_array[worker_id] += c; }
  inline double get_copy_cycle_in_ms(uint worker_id)   { return (double)(_copy_cycle_array[worker_id] / 2400000); }
  inline double get_copy_size_in_mb(uint worker_id)    { return _copy_size_array[worker_id] * HeapWordSize / (1024 * 1024); }
  inline size_t get_copy_count(uint worker_id)         { return _copy_count_array[worker_id]; }

  class G1CompactRegionClosure : public StackObj {
    G1CMBitMap* _bitmap;
    void clear_in_bitmap(oop object);
  public:
    jlong _copy_cycle;
    size_t _copy_size;
    size_t _copy_count;

    G1CompactRegionClosure(G1CMBitMap* bitmap) :
      _bitmap(bitmap), _copy_cycle(0), _copy_size(0), _copy_count(0) { }
    size_t apply(oop object);
  };
};

#endif // SHARE_GC_G1_G1FULLGCCOMPACTTASK_HPP
