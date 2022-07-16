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
 
#include <BLUtilsMath.h>

#include <MorphoFrame7.h>
#include <SySourceManager.h>
#include <SySourcesView.h>

#include <SoSourceManager.h>
#include <SoSource.h>

#include <SoFileSource.h>
#include <SyFileSource.h>

#include <BLUtils.h>

#include <BLDebug.h>

#include "MorphoMixer.h"

// Make gouls effect... wierd
#define INTERPOLATE_PITCH 1 //0 //1

MorphoMixer::MorphoMixer(SoSourceManager *soSourceManager,
                         SySourceManager *sySourceManager,
                         BL_FLOAT xyPadRatio)
{
    mSoSourceManager = soSourceManager;
    mSySourceManager = sySourceManager;

    mXYPadRatio = xyPadRatio;

    mLoop = false;
    mTimeStretchFactor = 1.0;
}

MorphoMixer::~MorphoMixer() {}

void
MorphoMixer::SetLoop(bool flag)
{
    mLoop = flag;
}

bool
MorphoMixer::GetLoop() const
{
    return mLoop;
}

void
MorphoMixer::SetTimeStretchFactor(BL_FLOAT factor)
{
    mTimeStretchFactor = factor;
}

void
MorphoMixer::Mix(MorphoFrame7 *result)
{
    //if (mSySourceManager->GetNumSources() < 2)
    //    return;

    if (IsPlayFinished() && !mLoop)
        return;

    UpdateSelection();
    
    vector<BL_FLOAT> weights;
    ComputeSourcesWeights(&weights);
    NormalizeSourcesWeights(&weights);

    // Compute result
    bool firstFrame = true;
    for (int i = 1; i < mSySourceManager->GetNumSources(); i++)
    {
        SySource *source = mSySourceManager->GetSource(i);

        MorphoFrame7 frame;
        source->ComputeCurrentMorphoFrame(&frame);

        if (!IsSourceMuted(i))
        {
            AddFrame(result, frame, weights, i, firstFrame);

            firstFrame = false;
        }
    }

    // Uppdate the mix source
    SySource *mixSource = mSySourceManager->GetSource(0);
    mixSource->SetMixMorphoFrame(*result);
    mixSource->ComputeCurrentMorphoFrame(result);

    if (mixSource->GetSourceMute())
    {
        // Mute the source => to get no sound
        result->Reset();
    }
    
    // Finally play by one step
    PlayAdvance();
}

// OK
void
MorphoMixer::ComputeSourcesWeights(vector<BL_FLOAT> *weights)
{
    int numSources = mSySourceManager->GetNumSources();
    if (numSources < 2) // First source is mixer
        return;
    
    weights->resize(numSources - 1);

    const SySource *mixSource = mSySourceManager->GetSource(0);
    for (int i = 0; i < weights->size(); i++)
    {
        const SySource *source = mSySourceManager->GetSource(i + 1);

        BL_FLOAT d = ComputeSourcesDistance(*mixSource, *source);

        (*weights)[i] = d;
    }
}

void
MorphoMixer::NormalizeSourcesWeights(vector<BL_FLOAT> *weights)
{
    // Normalize
    BL_FLOAT sumWeights = 0.0;
    for (int i = 0; i < weights->size(); i++)
    {
        SoSource *source = mSoSourceManager->GetSource(i);
        SoSourceType type = source->GetSourceType();
      
        sumWeights += (*weights)[i];
    }
    
    if (sumWeights > BL_EPS)
    {
        BL_FLOAT sumWeightsInv = 1.0/sumWeights;

        for (int i = 0; i < weights->size(); i++)
            (*weights)[i] *= sumWeightsInv;
    }
}

BL_FLOAT 
MorphoMixer::ComputeSourcesDistance(const SySource &source0,
                                    const SySource &source1)
{
    BL_FLOAT p0[2];
    source0.GetMixPos(&p0[0], &p0[1]);

    BL_FLOAT p1[2];
    source1.GetMixPos(&p1[0], &p1[1]);

    BL_FLOAT dx = p1[0] - p0[0];
    BL_FLOAT dy = p1[1] - p0[1];

    if (mXYPadRatio > 1.0)
        dy /= mXYPadRatio;
    else if (mXYPadRatio < 1.0)
        dx *= mXYPadRatio;

    BL_FLOAT d2 = dx*dx + dy*dy;
    BL_FLOAT d = std::sqrt(d2);

    return d;
}

SySource *
MorphoMixer::GetMasterSource()
{
    int numSources = mSySourceManager->GetNumSources();
    if (numSources < 2) // First source is mixer
        return NULL;

    for (int i = 1; i < mSySourceManager->GetNumSources(); i++)
    {
        SySource *source = mSySourceManager->GetSource(i);
        if (source->GetSourceMaster())
            return source;
    }

    return NULL;
}

void
MorphoMixer::PlayAdvance()
{
    // Get the master source
    SySource *masterSource = GetMasterSource();
    if (masterSource == NULL)
        return;
    
    BL_FLOAT t = masterSource->GetPlayPos();

    for (int i = 1; i < mSySourceManager->GetNumSources(); i++)
    {
        SySource *source = mSySourceManager->GetSource(i);

        if (source != masterSource)
            source->SetPlayPos(t);
    }
    
    masterSource->PlayAdvance(mTimeStretchFactor);
}

bool
MorphoMixer::IsPlayFinished() const
{
    SySource *masterSource = ((MorphoMixer *)this)->GetMasterSource();
    if (masterSource == NULL)
        return false;
                             
    bool finished = masterSource->IsPlayFinished();

    return finished;
}

void
MorphoMixer::AddFrame(MorphoFrame7 *result, /*const*/ MorphoFrame7 &frame,
                      const vector<BL_FLOAT> &weights, int index,
                      bool firstFrame)
{
    // Amplitude
    BL_FLOAT ampFactor = ComputeAmpFactor(weights, index);
    
    BL_FLOAT amp = frame.GetAmplitude();
    amp *= ampFactor;

    // Modify current frame, for at the end applying to partials
    frame.SetAmplitude(amp);
    
    BL_FLOAT sumAmp = amp;
    if (!firstFrame)
    {
        BL_FLOAT resultAmp = result->GetAmplitude(true/*false*/);
    
        sumAmp += resultAmp;
    }
    result->SetAmplitude(sumAmp);

#if INTERPOLATE_PITCH // Evil sounds, not sure to keep it
    // Pitch factor
    BL_FLOAT pitchFactor = ComputePitchFactor(weights, index);
    
    BL_FLOAT pitch = frame.GetFreqFactor(); // TODO: rename this stuff better...
    //pitch *= freqFactor;

    // Apply freq factor to pitch
    if ((pitch < 1.0) && (pitch > BL_EPS))
    {
        if (pitchFactor < BL_EPS)
            pitch = 1.0;
        else
            pitch = 1.0/((1.0/pitch)*pitchFactor);
    }
    else
        pitch = pitch*pitchFactor;

    // Modify current frame, for at the end applying to partials
    frame.SetFreqFactor(pitch);
    
    BL_FLOAT sumPitch = pitch;
    if (!firstFrame)
    {
        BL_FLOAT resultPitch = result->GetFreqFactor();
    
        sumPitch += resultPitch;
    }
    result->SetFreqFactor(sumPitch);
#endif

    BL_FLOAT colorFactor = ComputeColorFactor(weights, index);
    
    // Color
    WDL_TypedBuf<BL_FLOAT> color;
    frame.GetColor(&color, true);
    BLUtils::MultValues(&color, colorFactor);

    WDL_TypedBuf<BL_FLOAT> sumColor = color;
    
    if (!firstFrame)
    {
        WDL_TypedBuf<BL_FLOAT> resultColor;
        result->GetColor(&resultColor, true);
        
        BLUtils::AddValues(&sumColor, sumColor, resultColor);
    }
    result->SetColor(sumColor);
    
    // Color Raw (just for displaying)
    WDL_TypedBuf<BL_FLOAT> colorRaw;
    frame.GetColorRaw(&colorRaw, true);
    BLUtils::MultValues(&colorRaw, colorFactor);

    WDL_TypedBuf<BL_FLOAT> sumColorRaw = colorRaw;
    if (!firstFrame)
    {
        WDL_TypedBuf<BL_FLOAT> resultColorRaw;
        result->GetColorRaw(&resultColorRaw, true);

        BLUtils::AddValues(&sumColorRaw, sumColorRaw, resultColorRaw);
    }
    result->SetColorRaw(sumColorRaw);
        
    // Color processed (just for displaying)
    WDL_TypedBuf<BL_FLOAT> colorProcessed;
    frame.GetColorProcessed(&colorProcessed, true);
    BLUtils::MultValues(&colorProcessed, colorFactor);

    WDL_TypedBuf<BL_FLOAT> sumColorProcessed = colorProcessed;

    if (!firstFrame)
    {
        WDL_TypedBuf<BL_FLOAT> resultColorProcessed;
        result->GetColorProcessed(&resultColorProcessed, true);

        BLUtils::AddValues(&sumColorProcessed, sumColorProcessed,
                           resultColorProcessed);
    }
    result->SetColorProcessed(sumColorProcessed);
    
    // Warping
    BL_FLOAT warpingFactor = ComputeWarpingFactor(weights, index);
    
    WDL_TypedBuf<BL_FLOAT> warping;
    frame.GetWarping(&warping, true);

    BLUtils::AddValues(&warping, (BL_FLOAT)-1.0);
    BLUtils::MultValues(&warping, warpingFactor);
    BLUtils::AddValues(&warping, (BL_FLOAT)1.0);
        
    WDL_TypedBuf<BL_FLOAT> sumWarping = warping;

    if (!firstFrame)
    {
        WDL_TypedBuf<BL_FLOAT> resultWarping;
        result->GetWarping(&resultWarping, true);

        // Center on 0
        WDL_TypedBuf<BL_FLOAT> sumWarping0;
        BLUtils::AddValues(&sumWarping0, (BL_FLOAT)-1.0);

        // Center on 0
        WDL_TypedBuf<BL_FLOAT> resultWarping0;
        BLUtils::AddValues(&resultWarping0, (BL_FLOAT)-1.0);

        // Sum
        BLUtils::AddValues(&sumWarping0, sumWarping0, resultWarping0);

        // Re-center on 1
        BLUtils::AddValues(&sumWarping0, (BL_FLOAT)1.0);
    }
    result->SetWarping(sumWarping);

    // Warping inv
    WDL_TypedBuf<BL_FLOAT> warpingInv;
    frame.GetWarpingInv(&warpingInv, true);

    BLUtils::AddValues(&warpingInv, (BL_FLOAT)-1.0);
    BLUtils::MultValues(&warpingInv, warpingFactor);
    BLUtils::AddValues(&warpingInv, (BL_FLOAT)1.0);
        
    WDL_TypedBuf<BL_FLOAT> sumWarpingInv = warpingInv;

    if (!firstFrame)
    {
        WDL_TypedBuf<BL_FLOAT> resultWarpingInv;
        result->GetWarpingInv(&resultWarpingInv, true);
        
        // Center on 0
        WDL_TypedBuf<BL_FLOAT> sumWarpingInv0;
        BLUtils::AddValues(&sumWarpingInv0, (BL_FLOAT)-1.0);

        // Center on 0
        WDL_TypedBuf<BL_FLOAT> resultWarpingInv0;
        BLUtils::AddValues(&resultWarpingInv0, (BL_FLOAT)-1.0);

        // Sum
        BLUtils::AddValues(&sumWarpingInv0, sumWarpingInv0, resultWarpingInv0);

        // Re-center on 1
        BLUtils::AddValues(&sumWarpingInv0, (BL_FLOAT)1.0);
    }
    result->SetWarpingInv(sumWarpingInv);

    // Warping inv
    WDL_TypedBuf<BL_FLOAT> warpingProcessed;
    frame.GetWarpingProcessed(&warpingProcessed, true);

    BLUtils::AddValues(&warpingProcessed, (BL_FLOAT)-1.0);
    BLUtils::MultValues(&warpingProcessed, warpingFactor);
    BLUtils::AddValues(&warpingProcessed, (BL_FLOAT)1.0);
        
    WDL_TypedBuf<BL_FLOAT> sumWarpingProcessed = warpingProcessed;

    if (!firstFrame)
    {
        WDL_TypedBuf<BL_FLOAT> resultWarpingProcessed;
        result->GetWarpingProcessed(&resultWarpingProcessed, true);
        
        // Center on 0
        WDL_TypedBuf<BL_FLOAT> sumWarpingProcessed0;
        BLUtils::AddValues(&sumWarpingProcessed0, (BL_FLOAT)-1.0);

        // Center on 0
        WDL_TypedBuf<BL_FLOAT> resultWarpingProcessed0;
        BLUtils::AddValues(&resultWarpingProcessed0, (BL_FLOAT)-1.0);

        // Sum
        BLUtils::AddValues(&sumWarpingProcessed0, sumWarpingProcessed0,
                           resultWarpingProcessed0);

        // Re-center on 1
        BLUtils::AddValues(&sumWarpingProcessed0, (BL_FLOAT)1.0);
    }
    result->SetWarpingProcessed(sumWarpingProcessed);
    
    // Noise envelope
    BL_FLOAT noiseFactor = ComputeNoiseFactor(weights, index);
    
    WDL_TypedBuf<BL_FLOAT> noise;
    frame.GetNoiseEnvelope(&noise, true);
    BLUtils::MultValues(&noise, noiseFactor);

    WDL_TypedBuf<BL_FLOAT> sumNoise = noise;

    if (!firstFrame)
    {
        WDL_TypedBuf<BL_FLOAT> resultNoise;
        result->GetNoiseEnvelope(&resultNoise, true);

        BLUtils::AddValues(&sumNoise, sumNoise, resultNoise);
    }
    result->SetNoiseEnvelope(sumNoise);
            
    // Denorm noise envelope
    WDL_TypedBuf<BL_FLOAT> noiseDenorm;
    frame.GetDenormNoiseEnvelope(&noiseDenorm, true);
    BLUtils::MultValues(&noiseDenorm, noiseFactor);

    WDL_TypedBuf<BL_FLOAT> sumNoiseDenorm = noiseDenorm;

    if (!firstFrame)
    {
        WDL_TypedBuf<BL_FLOAT> resultNoiseDenorm;
        result->GetDenormNoiseEnvelope(&resultNoiseDenorm, true);

        BLUtils::AddValues(&sumNoiseDenorm, sumNoiseDenorm, resultNoiseDenorm);
    }
    result->SetDenormNoiseEnvelope(sumNoiseDenorm);
            
    // Partials

    // Must apply amp, to apply it source by source (with corresponding source amp)
    //MorphoFrame7::ApplyAmpFactorPartials(&frame); // ?
    MorphoFrame7::ApplyAmplitudePartials(&frame);
    
    // Apply pitch
    MorphoFrame7::ApplyFreqFactorPartials(&frame);
    
    // Add partials
    MorphoFrame7::AddAllPartials(result, frame);
}

BL_FLOAT
MorphoMixer::ComputeAmpFactor(const vector<BL_FLOAT> &weights, int index)
{
    // Set necessary weights to 0, re-normalize, then return the the current weight
    vector<BL_FLOAT> weights0 = weights;
    for (int i = 0; i < weights0.size(); i++)
    {
        bool muted = IsAmpMuted(i + 1);
        if (muted)
            weights0[i] = 0.0;
    }

    return weights0[index - 1];
}

BL_FLOAT
MorphoMixer::ComputePitchFactor(const vector<BL_FLOAT> &weights, int index)
{
    // Set necessary weights to 0, re-normalize, then return the the current weight
    vector<BL_FLOAT> weights0 = weights;
    for (int i = 0; i < weights0.size(); i++)
    {
        bool muted = IsPitchMuted(i + 1);
        if (muted)
            weights0[i] = 0.0;
    }

    return weights0[index - 1];
}

BL_FLOAT
MorphoMixer::ComputeColorFactor(const vector<BL_FLOAT> &weights, int index)
{
    // Set necessary weights to 0, re-normalize, then return the the current weight
    vector<BL_FLOAT> weights0 = weights;
    for (int i = 0; i < weights0.size(); i++)
    {
        bool muted = IsColorMuted(i + 1);
        if (muted)
            weights0[i] = 0.0;
    }

    return weights0[index - 1];
}

BL_FLOAT
MorphoMixer::ComputeWarpingFactor(const vector<BL_FLOAT> &weights, int index)
{
    // Set necessary weights to 0, re-normalize, then return the the current weight
    vector<BL_FLOAT> weights0 = weights;
    for (int i = 0; i < weights0.size(); i++)
    {
        bool muted = IsWarpingMuted(i + 1);
        if (muted)
            weights0[i] = 0.0;
    }

    return weights0[index - 1];
}

BL_FLOAT
MorphoMixer::ComputeNoiseFactor(const vector<BL_FLOAT> &weights, int index)
{
    // Set necessary weights to 0, re-normalize, then return the the current weight
    vector<BL_FLOAT> weights0 = weights;
    for (int i = 0; i < weights0.size(); i++)
    {
        bool muted = IsNoiseMuted(i + 1);
        if (muted)
            weights0[i] = 0.0;
    }

    return weights0[index - 1];
}

bool
MorphoMixer::IsSourceMuted(int index) const
{
    SySource *source = mSySourceManager->GetSource(index);
    if (source == NULL)
        return true;

    if (source->GetSourceSolo())
        return false;
    
    if (source->GetSourceMute() && !source->GetSourceSolo())
        return true;

    // Else check if another source is solo
    bool otherSolo = false;
    for (int i = 1; i < mSySourceManager->GetNumSources(); i++)
    {
        if (i == index)
            // Do not self-check source
            continue;

        SySource *source = mSySourceManager->GetSource(i);
        if (source == NULL)
            continue;

        if (source->GetSourceSolo())
            return true;
    }

    return false;
}

bool
MorphoMixer::IsAmpMuted(int index) const
{
    SySource *source = mSySourceManager->GetSource(index);
    if (source == NULL)
        return true;

    if (source->GetAmpSolo())
        return false;
    
    if (source->GetAmpMute() && !source->GetAmpSolo())
        return true;

    // Else check if another source amp is solo
    bool otherSolo = false;
    for (int i = 1; i < mSySourceManager->GetNumSources(); i++)
    {
        if (i == index)
            // Do not self-check source
            continue;

        SySource *source = mSySourceManager->GetSource(i);
        if (source == NULL)
            continue;

        if (source->GetAmpSolo())
            return true;
    }

    return false;
}

bool
MorphoMixer::IsPitchMuted(int index) const
{
    SySource *source = mSySourceManager->GetSource(index);
    if (source == NULL)
        return true;

    if (source->GetPitchSolo())
        return false;
    
    if (source->GetPitchMute() && !source->GetPitchSolo())
        return true;

    // Else check if another source pitch is solo
    bool otherSolo = false;
    for (int i = 1; i < mSySourceManager->GetNumSources(); i++)
    {
        if (i == index)
            // Do not self-check source
            continue;

        SySource *source = mSySourceManager->GetSource(i);
        if (source == NULL)
            continue;

        if (source->GetPitchSolo())
            return true;
    }

    return false;
}

// TODO later: make macros here to avoid code duplicate!
bool
MorphoMixer::IsColorMuted(int index) const
{
    SySource *source = mSySourceManager->GetSource(index);
    if (source == NULL)
        return true;

    if (source->GetColorSolo())
        return false;
    
    if (source->GetColorMute() && !source->GetColorSolo())
        return true;

    // Else check if another color is solo
    bool otherSolo = false;
    for (int i = 1; i < mSySourceManager->GetNumSources(); i++)
    {
        if (i == index)
            // Do not self-check source
            continue;

        SySource *source = mSySourceManager->GetSource(i);
        if (source == NULL)
            continue;

        if (source->GetColorSolo())
            return true;
    }

    return false;
}

bool
MorphoMixer::IsWarpingMuted(int index) const
{
    SySource *source = mSySourceManager->GetSource(index);
    if (source == NULL)
        return true;

    if (source->GetWarpingSolo())
        return false;
    
    if (source->GetWarpingMute() && !source->GetWarpingSolo())
        return true;

    // Else check if another warping is solo
    bool otherSolo = false;
    for (int i = 1; i < mSySourceManager->GetNumSources(); i++)
    {
        if (i == index)
            // Do not self-check source
            continue;

        SySource *source = mSySourceManager->GetSource(i);
        if (source == NULL)
            continue;

        if (source->GetWarpingSolo())
            return true;
    }

    return false;
}

bool
MorphoMixer::IsNoiseMuted(int index) const
{
    SySource *source = mSySourceManager->GetSource(index);
    if (source == NULL)
        return true;

    if (source->GetNoiseSolo())
        return false;
    
    if (source->GetNoiseMute() && !source->GetNoiseSolo())
        return true;

    // Else check if another warping is solo
    bool otherSolo = false;
    for (int i = 1; i < mSySourceManager->GetNumSources(); i++)
    {
        if (i == index)
            // Do not self-check source
            continue;

        SySource *source = mSySourceManager->GetSource(i);
        if (source == NULL)
            continue;

        if (source->GetNoiseSolo())
            return true;
    }

    return false;
}

void
MorphoMixer::UpdateSelection()
{
    for (int i = 0; i < mSoSourceManager->GetNumSources(); i++)
    {
        SoSource *soSource = mSoSourceManager->GetSource(i);
        
        SoSource::Type type = soSource->GetType();
        if (type != SoSource::FILE)
            continue;

        BL_FLOAT normX0;
        BL_FLOAT normX1;
        soSource->GetNormSelection(&normX0, &normX1);

        SySource *sySource = mSySourceManager->GetSource(i + 1); // mix source is 0
        sySource->SetNormSelection(normX0, normX1);
    }
}
