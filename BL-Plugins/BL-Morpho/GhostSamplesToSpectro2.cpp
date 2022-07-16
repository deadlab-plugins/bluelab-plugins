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
 
#include <IControls.h>

#include <GhostTrack2.h>

#include <SamplesToMagnPhases.h>

#include "IPlug_include_in_plug_hdr.h"

#include "GhostSamplesToSpectro2.h"


GhostSamplesToSpectro2::GhostSamplesToSpectro2(GhostTrack2 *track,
                                               int editOverlapping)
{
    mTrack = track;

    mEditOverlapping = editOverlapping;

    mSaveIsLoadingSaving = false;
    mSaveOverlapping = editOverlapping;
    
    mSamplesToMagnPhases = new SamplesToMagnPhases(&track->mSamples,
                                                   track->mEditFftObj,
                                                   track->mSpectroEditObjs,
                                                   track->mSamplesPyramid);
}

GhostSamplesToSpectro2::~GhostSamplesToSpectro2()
{
    delete mSamplesToMagnPhases;
}

void
GhostSamplesToSpectro2::SaveState()
{
    mSaveIsLoadingSaving = mTrack->mIsLoadingSaving;
    mSaveOverlapping = mTrack->mEditFftObj->GetOverlapping();

    mTrack->mEditFftObj->SetOverlapping(mEditOverlapping);
}

void
GhostSamplesToSpectro2::RestoreState()
{
    if (mTrack->mCustomDrawer != NULL)
    {
        // FIX: edit during plaback, the playbar disappeared
        if (mTrack->mCustomDrawer->IsPlayBarActive())
        {
            
            mTrack->mCustomDrawer->SetSelPlayBarPos(0.0);
            
            mTrack->ResetPlayBar();
        }
    }
    
    mTrack->mEditFftObj->SetOverlapping(mSaveOverlapping);
    mTrack->mIsLoadingSaving = mSaveIsLoadingSaving;
}

void
GhostSamplesToSpectro2::
ReadSpectroDataSlice(vector<WDL_TypedBuf<BL_FLOAT> > magns[2],
                     vector<WDL_TypedBuf<BL_FLOAT> > phases[2],
                     BL_FLOAT minXNorm, BL_FLOAT maxXNorm)
{
    SaveState();
    mTrack->mIsLoadingSaving = true;

    mSamplesToMagnPhases->ReadSpectroDataSlice(magns, phases, minXNorm, maxXNorm);
    
    RestoreState();
}

void
GhostSamplesToSpectro2::
WriteSpectroDataSlice(vector<WDL_TypedBuf<BL_FLOAT> > magns[2],
                      vector<WDL_TypedBuf<BL_FLOAT> > phases[2],
                      BL_FLOAT minXNorm, BL_FLOAT maxXNorm,
                      int fadeNumSamples)
{
    SaveState();
    mTrack->mIsLoadingSaving = true;
    
    mSamplesToMagnPhases->WriteSpectroDataSlice(magns, phases,
                                                minXNorm, maxXNorm, fadeNumSamples);
    
    RestoreState();
}

void
GhostSamplesToSpectro2::
ReadSelectedSamples(vector<WDL_TypedBuf<BL_FLOAT> > *samples,
                    BL_FLOAT minXNorm, BL_FLOAT maxNormX)
{
    SaveState();
    mTrack->mIsLoadingSaving = true;
    
    mSamplesToMagnPhases->ReadSelectedSamples(samples, minXNorm, maxNormX);

    RestoreState();
}
