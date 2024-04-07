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
  if (UseProfileSwapFault) { 
    _stt_kernel_swap_stats = NEW_RESOURCE_OBJ(os::kernel_swap_stats);
    _end_kernel_swap_stats = NEW_RESOURCE_OBJ(os::kernel_swap_stats);
  }
}

GCMajfltStats::~GCMajfltStats() {
  if (UseProfileSwapFault) { 
    FREE_RESOURCE_ARRAY(os::kernel_swap_stats, _stt_kernel_swap_stats, 1);
    FREE_RESOURCE_ARRAY(os::kernel_swap_stats, _end_kernel_swap_stats, 1);
  }
}

void GCMajfltStats::start() {
  if (UseProfileSwapFault) { 
    _stt_majflt = os::get_accum_majflt();
    os::get_accum_kernel_swap_stats(_stt_kernel_swap_stats);
  }
}

void GCMajfltStats::end_and_log(const char* cause) {
  if (UseProfileSwapFault) { 
    size_t _end_majflt = os::get_accum_majflt();
    log_info(gc)("Majflt(%s)=%ld (%ld -> %ld)", cause, _end_majflt - _stt_majflt , _stt_majflt, _end_majflt);

    os::get_accum_kernel_swap_stats(_end_kernel_swap_stats);
    log_info(gc)("DemandSwapin(%s)=%ld(%ld -> %ld) %.0fms(%.0fms -> %.0fms), "\
                 "NonDemandSwapin=%ld(%ld -> %ld) %.0fms(%.0fms -> %.0fms), "\
                 "SwapinBlockedBySwapout=%ld(%ld -> %ld) %.0fms(%.0fms -> %.0fms), "
                 "NonSwap=%ld(%ld -> %ld) %.0fms(%.0fms -> %.0fms)",
                cause,
                _end_kernel_swap_stats->demand_swapin_cnt                        - _stt_kernel_swap_stats->demand_swapin_cnt,                       _stt_kernel_swap_stats->demand_swapin_cnt,                       _end_kernel_swap_stats->demand_swapin_cnt,
                _end_kernel_swap_stats->demand_swapin_time_us / 1e3              - _stt_kernel_swap_stats->demand_swapin_time_us / 1e3,             _stt_kernel_swap_stats->demand_swapin_time_us / 1e3,             _end_kernel_swap_stats->demand_swapin_time_us / 1e3,
                _end_kernel_swap_stats->non_demand_swapin_cnt                    - _stt_kernel_swap_stats->non_demand_swapin_cnt,                   _stt_kernel_swap_stats->non_demand_swapin_cnt,                   _end_kernel_swap_stats->non_demand_swapin_cnt,
                _end_kernel_swap_stats->non_demand_swapin_time_us  / 1e3         - _stt_kernel_swap_stats->non_demand_swapin_time_us / 1e3,         _stt_kernel_swap_stats->non_demand_swapin_time_us / 1e3,         _end_kernel_swap_stats->non_demand_swapin_time_us / 1e3,
                _end_kernel_swap_stats->swapin_blocked_by_swapout_cnt            - _stt_kernel_swap_stats->swapin_blocked_by_swapout_cnt,           _stt_kernel_swap_stats->swapin_blocked_by_swapout_cnt,           _end_kernel_swap_stats->swapin_blocked_by_swapout_cnt,
                _end_kernel_swap_stats->swapin_blocked_by_swapout_time_us  / 1e3 - _stt_kernel_swap_stats->swapin_blocked_by_swapout_time_us / 1e3, _stt_kernel_swap_stats->swapin_blocked_by_swapout_time_us / 1e3, _end_kernel_swap_stats->swapin_blocked_by_swapout_time_us / 1e3,
                _end_kernel_swap_stats->non_swap_cnt                             - _stt_kernel_swap_stats->non_swap_cnt,                            _stt_kernel_swap_stats->non_swap_cnt,                            _end_kernel_swap_stats->non_swap_cnt,
                _end_kernel_swap_stats->non_swap_time_us / 1e3                   - _stt_kernel_swap_stats->non_swap_time_us / 1e3,                  _stt_kernel_swap_stats->non_swap_time_us / 1e3,                  _end_kernel_swap_stats->non_swap_time_us / 1e3
    );
  }
}
