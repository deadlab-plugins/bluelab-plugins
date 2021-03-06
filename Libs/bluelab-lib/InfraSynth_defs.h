/* Copyright (C) 2022 Nicolas Dittlo <deadlab.plugins@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this software; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */
 
#ifndef INFRA_SYNTH_DEFS_H
#define INFRA_SYNTH_DEFS_H

// Optimizations
#define OPTIM1 1
#define OPTIM2 1

#define INFRA_SYNTH_OPTIM3 1

#define ADSR_SMOOTH_FEATURE 1
#define ADSR_ADVANCED_PARAMS_FEATURE 1

// Sync phantom oscs (slave), with main osc (master)
#define OSC_SYNC_FEATURE 1
// Real Osc sync. See: https://en.wikipedia.org/wiki/Oscillator_sync
// NOTE: not a good solution: all the phantom oscs have frequencies
// multiple of main osc freq (real sync is not a solution)
#define OSC_SYNC_REAL_SYNC 0 // 1
// Manage to make start the phantom oscillators synchronized
// For this, start them with a phase of Pi/2, so the peaks will synchronize
#define OSC_SYNC_PHASES 1

//
#define GAIN_PAD -10.0

// See: https://en.wikipedia.org/wiki/Missing_fundamental#cite_note-20
// and: https://www.youtube.com/watch?v=yGgX9SmhHOQ

#define MUTE_MAIN_OSC_FEATURE 0 //1

#define OPTIM_APPLY_PARAM_SHAPE 1

// Mix main osc (instead of only muting it)
#define MIX_MAIN_OSC 1

// Debug
#define DEBUG_DUMP_ENVELOPES 0 //1

#define NOISE_GEN_FEATURE 1
#define NOISE_COEFF 1.0 //10.0
#define NOISE_FILTER_DEFAULT_FREQ 1.0

#define MIN_DB -200.0

#endif
