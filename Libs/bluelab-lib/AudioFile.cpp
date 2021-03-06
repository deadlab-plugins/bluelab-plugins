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
//  AudioFile.cpp
//  BL-Ghost
//
//  Created by Pan on 31/05/18.
//
//

#include <BLTypes.h>

#include <BLUtils.h>
#include <BLUtilsFile.h>

#include "Resampler2.h"
#include "AudioFile.h"

#define	BLOCK_SIZE 4096


AudioFile::AudioFile(int numChannels, BL_FLOAT sampleRate, int format,
                     vector<WDL_TypedBuf<BL_FLOAT> > *data)
{
    mNumChannels = numChannels;
    
    if (data != NULL)
    {
        // Improved behaviour, to avoid having the data 2 times in memory
        // The data pointer is provided from outside the function
        mData = data;
        mDataExtRef = true;
    }
    else
    {
        mData = new vector<WDL_TypedBuf<BL_FLOAT> >();
        mDataExtRef = false;
    }
    
    mData->resize(numChannels);
    
    mSampleRate = sampleRate;
    
    mInternalFormat = format;
}

AudioFile::~AudioFile()
{
    if (!mDataExtRef)
    {
        delete mData;
    }
}

// NOTE: Would crash when opening very large files
// under the debugger, and using Scheme -> Guard Edge/ Guard Malloc
AudioFile *
AudioFile::Load(const char *fileName, vector<WDL_TypedBuf<BL_FLOAT> > *data)
{
	AudioFile *result = NULL;

#if !DISABLE_LIBSNDFILE
    SF_INFO	sfinfo;
    memset(&sfinfo, 0, sizeof (sfinfo)) ;
    
    SNDFILE	*file = sf_open(fileName, SFM_READ, &sfinfo);
    if (file == NULL)
        return NULL;
    
    // Keep the loaded format, in case we would like to save to the same format
    result = new AudioFile(sfinfo.channels,
                           (BL_FLOAT)sfinfo.samplerate, sfinfo.format, data);
    
    // Avoid duplicating data
    //vector<WDL_TypedBuf<BL_FLOAT> > channels;
    //channels.resize(sfinfo.channels);
    
    // Use directly mData, without temporary buffer
    result->mData->resize(sfinfo.channels);
    
    //if (!channels.empty())
    if (!result->mData->empty())
    {
        double buf[BLOCK_SIZE];
        sf_count_t frames = BLOCK_SIZE / sfinfo.channels;
    
        int readcount;
        while ((readcount = sf_readf_double(file, buf, frames)) > 0)
        {
            //int prevSize = channels[0].GetSize();
            int prevSize = (*result->mData)[0].GetSize();
            for (int m = 0; m < sfinfo.channels; m++)
                //channels[m].Resize(prevSize + readcount);
                (*result->mData)[m].Resize(prevSize + readcount);
        
            for (int k = 0 ; k < readcount ; k++)
            {
                for (int m = 0; m < sfinfo.channels; m++)
                {
                    double val = buf[k*sfinfo.channels + m];
                    //channels[m].Add(val); // Crashes
                
                    //channels[m].Get()[prevSize + k] = val;
                    (*result->mData)[m].Get()[prevSize + k] = val;
                }
            }
        }
    }
    
    //for (int i = 0; i < result->mData->size(); i++)
    //    (*result->mData)[i] = channels[i];
    
    sf_close(file);
#endif

    return result;
}

bool
AudioFile::Save(const char *fileName)
{
#if !DISABLE_LIBSNDFILE

#define USE_DOUBLE 0
	SF_INFO	sfinfo;
    
    memset(&sfinfo, 0, sizeof (sfinfo));
    
	sfinfo.samplerate = (int)mSampleRate;
	sfinfo.frames = (mData->size() > 0) ? (*mData)[0].GetSize() : 0;
	sfinfo.channels	= mNumChannels;
    
    char *ext = BLUtilsFile::GetFileExtension(fileName);
    
    if (ext == NULL)
        ext = "wav";
    
    // Set the format to a default format
    
#if USE_DOUBLE // For 64 bit (not supported everywhere)
    if (strcmp(ext, "wav") == 0)
    {
        sfinfo.format = (SF_FORMAT_WAV | SF_FORMAT_BL_FLOAT/*SF_FORMAT_PCM_24*/);

        if ((mInternalFormat & SF_FORMAT_WAV) != SF_FORMAT_WAV)
            // We use a different save extension than the one when loaded
            // => do not reuse loading format
            mInternalFormat = -1;
    }
    
    if ((strcmp(ext, "aif") == 0) || (strcmp(ext, "aiff") == 0))
    {
        sfinfo.format = (SF_FORMAT_AIFF | SF_FORMAT_BL_FLOAT/*SF_FORMAT_PCM_24*/);

        if ((mInternalFormat & SF_FORMAT_AIFF) != SF_FORMAT_AIFF)
            // We use a different save extension than the one when loaded
            // => do not reuse loading format
            mInternalFormat = -1;
    }
    
#if AUDIOFILE_USE_FLAC
    if (strcmp(ext, "flac") == 0)
    {
        sfinfo.format = (SF_FORMAT_FLAC | SF_FORMAT_BL_FLOAT/*SF_FORMAT_PCM_24*/);

        if ((mInternalFormat & SF_FORMAT_FLAC) != SF_FORMAT_FLAC)
            // We use a different save extension than the one when loaded
            // => do not reuse loading format
            mInternalFormat = -1;
    }
#endif
    
#else // More standard bit depth
    if (strcmp(ext, "wav") == 0)
    {
        sfinfo.format = (SF_FORMAT_WAV | SF_FORMAT_FLOAT/*SF_FORMAT_PCM_24*/);

        if ((mInternalFormat & SF_FORMAT_WAV) != SF_FORMAT_WAV)
            // We use a different save extension than the one when loaded
            // => do not reuse loading format
            mInternalFormat = -1;
    }
    
    if ((strcmp(ext, "aif") == 0) || (strcmp(ext, "aiff") == 0))
    {
        sfinfo.format = (SF_FORMAT_AIFF | SF_FORMAT_FLOAT/*SF_FORMAT_PCM_24*/);

        if ((mInternalFormat & SF_FORMAT_AIFF) != SF_FORMAT_AIFF)
            // We use a different save extension than the one when loaded
            // => do not reuse loading format
            mInternalFormat = -1;
    }
    
#if AUDIOFILE_USE_FLAC
    if (strcmp(ext, "flac") == 0)
    {
        //sfinfo.format = (SF_FORMAT_FLAC | SF_FORMAT_FLOAT/*SF_FORMAT_PCM_24*/);
        sfinfo.format = (SF_FORMAT_FLAC | SF_FORMAT_PCM_24);

        if ((mInternalFormat & SF_FORMAT_FLAC) != SF_FORMAT_FLAC)
            // We use a different save extension than the one when loaded
            // => do not reuse loading format
            mInternalFormat = -1;
    }
#endif
    
#endif
    
    if (mInternalFormat != -1)
        // Internal format is defined, and we are saving the same format
        // as the one that was loaded => overwrite the default with it
        sfinfo.format = mInternalFormat;
    
    SNDFILE *file = sf_open(fileName, SFM_WRITE, &sfinfo);
    if (file == NULL)
        return false;
    
    int numData = (mData->size() > 0) ? mNumChannels*(*mData)[0].GetSize() : 0;
    
#if USE_DOUBLE
    BL_FLOAT *buffer = (BL_FLOAT *)malloc(numData*sizeof(BL_FLOAT));
#else
    float *buffer = (float *)malloc(numData*sizeof(float));
#endif
    
    if (mData->size() > 0)
    {
        for (int j = 0; j < (*mData)[0].GetSize(); j++)
        {
            for (int i = 0; i < mNumChannels; i++)
            {
                buffer[j*mNumChannels + i] = (*mData)[i].Get()[j];
            }
        }
    
#if USE_DOUBLE
        sf_write_double(file, buffer, numData);
#else
        sf_write_float(file, buffer, numData);
#endif
        
    }
    
    sf_close(file);
                  
    free(buffer);
#endif

    return true;
}

void
AudioFile::Resample(BL_FLOAT newSampleRate)
{
    if (newSampleRate == mSampleRate)
    {
        // Nothing to do
        // And will save having the audio buffer 2 times in memory
        return;
    }
    
    for (int i = 0; i < mNumChannels; i++)
    {
        WDL_TypedBuf<BL_FLOAT> &chan = (*mData)[i];
        WDL_TypedBuf<BL_FLOAT> resamp;
        
        Resampler2::Resample(&chan,
                             &resamp,
                             mSampleRate, newSampleRate);
        
        chan = resamp;
    }

    mSampleRate = newSampleRate;
}

void
AudioFile::Resample2(BL_FLOAT newSampleRate)
{
    if (newSampleRate == mSampleRate)
    {
        // Nothing to do
        // And will save having the audio buffer 2 times in memory
        return;
    }
    
    for (int i = 0; i < mNumChannels; i++)
    {
        WDL_TypedBuf<BL_FLOAT> &chan = (*mData)[i];
        WDL_TypedBuf<BL_FLOAT> resamp;
        
        Resampler2::Resample2(&chan,
                              &resamp,
                              mSampleRate, newSampleRate);
        
        chan = resamp;
    }
    
    mSampleRate = newSampleRate;
}

int
AudioFile::GetNumChannels()
{
    return mNumChannels;
}

BL_FLOAT
AudioFile::GetSampleRate()
{
    return mSampleRate;
}

int
AudioFile::GetInternalFormat()
{
    return mInternalFormat;
}

void
AudioFile::GetData(int channelNum, WDL_TypedBuf<BL_FLOAT> **data)
{
    if (channelNum >= mData->size())
        return;
    
    *data = &((*mData)[channelNum]);
}

void
AudioFile::SetData(int channelNum, const WDL_TypedBuf<BL_FLOAT> &data, long dataSize)
{
    if (channelNum >= mData->size())
        return;
    
    (*mData)[channelNum] = data;
    
    // dataSize provided: we don't want to save all the data
    if (dataSize > 0)
        BLUtils::ResizeFillZeros(&(*mData)[channelNum], dataSize);
}

void
AudioFile::FixDataBounds()
{
    BL_FLOAT maxSamp = 0.0;

    for (int i = 0; i < mData->size(); i++)
    {
        const WDL_TypedBuf<BL_FLOAT> &chan = (*mData)[i];

        for (int j = 0; j < chan.GetSize(); j++)
        {
            BL_FLOAT val = chan.Get()[j];
            if (val > maxSamp)
                maxSamp = val;
            else if (-val > maxSamp)
                maxSamp = -val;
        }
    }

    if (maxSamp > 1.0)
    {
        BL_FLOAT coeff = 1.0/maxSamp;
        
        for (int i = 0; i < mData->size(); i++)
        {
            WDL_TypedBuf<BL_FLOAT> &chan = (*mData)[i];

            BLUtils::MultValues(&chan, coeff);
        }
    }
}
