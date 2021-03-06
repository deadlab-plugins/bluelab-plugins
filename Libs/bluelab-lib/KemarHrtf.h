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
 
//
//  HRTF.h
//  Spatializer
//
//  Created by Pan on 16/11/17.
//
//

#ifndef __Spatializer__KemarHrtf__
#define __Spatializer__KemarHrtf__

// NOTE: compact sounds better !
// (bigger sensation of space, less low frequencies)

// For compact
#define RESP_EXT "dat"

// NOTE: not sure about that
#define HRTF_SAMPLERATE 44100

// For diffuse resampled
//#define RESP_EXT "res"

#define MIN_ELEV -40
#define MAX_ELEV  90
#define STEP_ELEV 10

#define MIN_AZIM  0
#define MAX_AZIM  180
#define STEP_AZIM 1 // They are not all defined

// (MAX_ELEV - MIN_ELEV)/STEP_ELEV
#define NUM_ELEV 14

#define NUM_AZIM 181

// Source is at 1m40 from the head in Kemar measurements
#define KEMAR_SOURCE_DISTANCE 1.4

// We can use symetry: theta for left, 360 - theta for right

#include <BLTypes.h>

#include "IPlug_include_in_plug_hdr.h"

using namespace iplug::igraphics;

class HRTF;

class KemarHRTF
{
public:
	static bool Load(const char *resDir, HRTF **outHrtf);
    
	// NIKO-WIN
	// Convenient method, to generate .rc lines for Windows
	static void DumpWinRcFile(const char *dumpRcFileName, int startRcId);

protected:
	// Load from file 
	static bool LoadMacLinux(const char *resDir, HRTF **outHrtf);
	static bool ReadOneFile(WDL_TypedBuf<BL_FLOAT> *outImpulseResponses[2], const char *fileName);

	// NIKO-WIN
#ifdef WIN32  // Load from Windows resources
	static bool LoadWin(HRTF **outHrtf);
	static bool ReadOneFileWin(WDL_TypedBuf<BL_FLOAT>* outImpulseResponses[2],
							   //int rcId);
							   const char* rcFn);
#endif
};


#endif /* defined(__Spatializer__KemarHrtf__) */
