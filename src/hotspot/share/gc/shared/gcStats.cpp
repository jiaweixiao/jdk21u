/*
 * Copyright (c) 2003, 2021, Oracle and/or its affiliates. All rights reserved.
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
#include "gc/shared/gcStats.hpp"
#include "gc/shared/gcUtil.inline.hpp"
#include "gc/shared/gc_globals.hpp"

GCStats::GCStats() : _avg_promoted(new AdaptivePaddedNoZeroDevAverage(AdaptiveSizePolicyWeight, PromotedPadding)) {}

GCMajfltStats::GCMajfltStats() : _stt_majflt(0) {
  // Allocation in thread-local resource area
  _stt_sys_stats = NEW_RESOURCE_OBJ(KernelStats);
  _end_sys_stats = NEW_RESOURCE_OBJ(KernelStats);
  // _stt_proc_stats = NEW_RESOURCE_OBJ(KernelStats);
  // _end_proc_stats = NEW_RESOURCE_OBJ(KernelStats);
}

GCMajfltStats::~GCMajfltStats() {
  FREE_RESOURCE_ARRAY(KernelStats, _stt_sys_stats, 1);
  FREE_RESOURCE_ARRAY(KernelStats, _end_sys_stats, 1);
  // FREE_RESOURCE_ARRAY(KernelStats, _stt_proc_stats, 1);
  // FREE_RESOURCE_ARRAY(KernelStats, _end_proc_stats, 1);
}

void GCMajfltStats::start() {
  _stt_majflt = os::accumMajflt();
  os::get_system_kernel_majflt_stats(_stt_sys_stats);
  // os::accum_proc_range_majflt(_stt_proc_stats);
}

void GCMajfltStats::end_and_log(const char* cause) {
  size_t _end_majflt = os::accumMajflt();
  log_info(gc)("Majflt(%s)=%ld (%ld -> %ld)", cause, _end_majflt - _stt_majflt , _stt_majflt, _end_majflt);

  os::get_system_kernel_majflt_stats(_end_sys_stats);
  log_info(gc)("SysKernelStats(%s) majflt %ld (%ld -> %ld), in young %ld (%ld -> %ld), in old %ld (%ld -> %ld), swapout in heap (%ld -> %ld), swapout in heap free (%ld -> %ld)",
    cause,
    _end_sys_stats->majflt - _stt_sys_stats->majflt,
    _stt_sys_stats->majflt, _end_sys_stats->majflt,
    _end_sys_stats->majflt_in_young - _stt_sys_stats->majflt_in_young,
    _stt_sys_stats->majflt_in_young, _end_sys_stats->majflt_in_young,
    _end_sys_stats->majflt_in_old - _stt_sys_stats->majflt_in_old,
    _stt_sys_stats->majflt_in_old, _end_sys_stats->majflt_in_old,
    _stt_sys_stats->swapout_in_heap,
    _end_sys_stats->swapout_in_heap,
    _stt_sys_stats->swapout_in_heap_free_space,
    _end_sys_stats->swapout_in_heap_free_space);

  // os::accum_proc_range_majflt(_end_proc_stats);
  // log_info(gc)("RegionMajflt(%s) majflt %ld (%ld -> %ld), in young %ld (%ld -> %ld), in old %ld (%ld -> %ld)",
  //   cause,
  //   _end_proc_stats->majflt - _stt_proc_stats->majflt , _stt_proc_stats->majflt, _end_proc_stats->majflt,
  //   _end_proc_stats->majflt_in_young - _stt_proc_stats->majflt_in_young , _stt_proc_stats->majflt_in_young, _end_proc_stats->majflt_in_young,
  //   _end_proc_stats->majflt_in_old - _stt_proc_stats->majflt_in_old , _stt_proc_stats->majflt_in_old, _end_proc_stats->majflt_in_old);
}
