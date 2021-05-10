#pragma once

//#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <functional>
#include "BlockCircularBuffer.h"

#ifndef M_PI
#define M_PI 3.1415926535897932384
#endif

#ifndef TWO_PI
#define TWO_PI 6.28318530717958647692
#endif

namespace stekyne
{
    template <typename FloatType>
    static void WindowFunctionHann(FloatType *buffer, int size)
    {
        for (int i = 0; i < size; i++)
            buffer[i] = 0.5 * (1.0 - std::cos((FloatType)(2.0*M_PI * ((FloatType)i) / (size - 1))));
}

    int NextPowerOfTwo(int value)
    {
        int result = 1;
    
        while(result < value)
            result *= 2;
    
        return result;
    }

#include "../../WDL/fft.h"
    
    template <typename FloatType>
    static void performRealOnlyForwardTransform (FloatType *buf, int bufSize);
    {
        // Real parts are all stacked on the first half of the buffer
        // Second half of the buffer is filled with zeros

        // TODO: manage the case when the data is all zero

        FloatType *tmpBuf = malloc(bufSize*sizeof(FloatType));
                                   
        // Normalization for WDL
        BL_FLOAT coeff = 1.0;
        if (bufSize > 0)
            coeff = 1.0/bufSize;

        for (int i = 0; i < bufSize/2; i++)
        {
            tmpBuf[i*2] = buf[i]*coeff;
            tmpBuf[i*2 + 1] = buf[i*2];
        }
        
        WDL_fft((WDL_FFT_COMPLEX *)tmpBuf, bufSize/2, false);

        for (int i = 0; i < bufSize/2; i++)
        {
            int k = WDL_fft_permute(bufSize/2, i);
   
            buf[i*2] = tmpBuf[k*2];
            buf[i*2 + 1] = tmpBuf[k*2 + 1];
        }
        
        free(tmpBuf);
    }
    
    static void performRealOnlyInverseTransform (FloatType *buf, int bufSize)
    {
        // The buffer contained re/im interleaved complex values

        FloatType *tmpBuf = malloc(bufSize*sizeof(FloatType));

        for (int i = 0; i < bufSize/2; i++)
        {
            int k = WDL_fft_permute(bufSize/2, i);
        
            tmpBuf[k*2] = buf[i*2];
            tmpBuf[k*2 + 1] = buf[i*2 + 1];
        }

        WDL_fft((WDL_FFT_COMPLEX *)tmpBuf, bufSize/2, true);

        for (int i = 0; i < bufSize/2; i++)
        {
            buf[i] = tmpBuf[i*2];
            buf[i*2] = tmpBuf[i*2 + 1];
        }
        
        free(tmpBuf);
    }
    
    
// Resample a signal to a new size using linear interpolation
// The 'originalSize' is the max size of the original signal
// The 'newSignalSize' is the size to resample to. The 'newSignal' must be at least as big as this size.
static void linearResample (const float* const originalSignal, int originalSize,
	float* const newSignal, int newSignalSize)
{
	const auto lerp = [&](float v0, float v1, float t)
	{
		return (1.f - t) * v0 + t * v1;
	};

	// If the original signal is bigger than the new size, condense the signal to fit the new buffer
	// otherwise expand the signal to fit the new buffer
	const auto scale = originalSize / (float)newSignalSize;

	float index = 0.f;

	for (int i = 0; i < newSignalSize; ++i)
	{
		const auto wholeIndex = (int)floor (index);
		const auto fractionIndex = index - wholeIndex;
		const auto sampleA = originalSignal[wholeIndex];
		const auto sampleB = originalSignal[wholeIndex + 1];
		newSignal[i] = lerp (sampleA, sampleB, fractionIndex);
		index += scale;
	}
}

template <typename FloatType = float>
class PhaseVocoder
{
public: 
	static constexpr int MinOverlapAmount = 4;
	static constexpr float MaxPitchRatio = 2.0f;
	static constexpr float MinPitchRatio = 0.5f;

public:
	// Default settings
	PhaseVocoder (int windowLength = 2048, int fftSize = 2048) :
		samplesTilNextProcess (windowLength),
		analysisHopSize (windowLength / MinOverlapAmount),
		synthesisHopSize (windowLength / MinOverlapAmount),
		windowSize (windowLength),
		resampleSize (windowLength),
		spectralBufferSize (windowLength * 2),
		//fft (std::make_unique<juce::dsp::FFT> (nearestPower2 (fftSize))),
		analysisBuffer (windowLength),
		synthesisBuffer (windowLength * 3),
		windowBuffer (new FloatType[windowLength])
	{
		// TODO make the window more configurable
		//juce::dsp::WindowingFunction<FloatType>::fillWindowingTables (windowBuffer, windowSize,
		//	juce::dsp::WindowingFunction<FloatType>::hann, false);
        WindowFunctionHann(windowBuffer, windowSize);
    
		// Processing reuses the spectral buffer to resize the output grain
		// It must be the at least the size of the min pitch ratio
		// TODO FFT size must be big enough also
		spectralBufferSize = windowLength * (1 / MinPitchRatio) < spectralBufferSize ? 
			(int)ceil (windowLength * (1 / MinPitchRatio)) : spectralBufferSize;

		spectralBuffer = new FloatType[spectralBufferSize];
		std::fill (spectralBuffer, spectralBuffer + spectralBufferSize, 0.f);

		// Calculate maximium size resample signal can be
		const auto maxResampleSize = (int)std::ceil (std::max (this->windowSize * MaxPitchRatio,
			this->windowSize / MinPitchRatio));

		resampleBuffer = new FloatType[maxResampleSize];
		std::fill (resampleBuffer, resampleBuffer + maxResampleSize, 0.f);
	}

	~PhaseVocoder()
	{
		if (spectralBuffer != nullptr) {
			delete[] spectralBuffer;
			spectralBuffer = nullptr;
		}

		if (windowBuffer != nullptr) {
			delete[] windowBuffer;
			windowBuffer = nullptr;
		}

		if (resampleBuffer != nullptr) {
			delete[] resampleBuffer;
			resampleBuffer = nullptr;
		}
		
	}
	
	int getLatencyInSamples () const
	{
		return windowSize;
	}

	// The main process function corresponds to the following high level algorithm
	// Note: The processing is split up internally to avoid extra memory usage
	// 1. Read incoming samples into the internal analysis buffer
	// 2. If there are enough samples to begin processing, read a block from the analysis buffer
	// 3. Perform an FFT of on the block of samples
	// 4. Do some processing with the spectral data
	// 5. Perform an iFFT back into the time domain
	// 6. Write the block of samples back into the internal synthesis buffer
	// 7. Read a block of samples from the synthesis buffer
	void process (FloatType* const incomingBuffer, const int incomingBufferSize)
	{
		//const juce::SpinLock::ScopedLockType lock(paramLock);
		//juce::ScopedNoDenormals noDenormals;

		//static int callbackCount = 0;
		/*DBG (" ");
          DBG ("Callback: " << ++callbackCount << ", SampleCount: " << incomingSampleCount << 
          ", (+ incoming): " << incomingBufferSize);*/

		// Only write enough samples into the analysis buffer to complete a processing
		// frame. Likewise, only write enough into the synthesis buffer to generate the 
		// next output audio frame. 
		for (auto internalOffset = 0, internalBufferSize = 0; 
			internalOffset < incomingBufferSize; 
			internalOffset += internalBufferSize)
		{
			const auto remainingIncomingSamples = (incomingBufferSize - internalOffset);
			internalBufferSize = incomingSampleCount + remainingIncomingSamples >= samplesTilNextProcess ?
				samplesTilNextProcess - incomingSampleCount : remainingIncomingSamples;
			
			//DBG ("Internal buffer: Offset: " << internalOffset << ", Size: " << internalBufferSize);
			//jassert (internalBufferSize <= incomingBufferSize);

			// Write the incoming samples into the internal buffer
			// Once there are enough samples, perform spectral processing
			const auto previousAnalysisWriteIndex = analysisBuffer.getReadIndex ();
			analysisBuffer.write (incomingBuffer + internalOffset, internalBufferSize);
			//DBG ("Analysis Write Index: " << previousAnalysisWriteIndex << " -> " << analysisBuffer.getWriteIndex ());

			incomingSampleCount += internalBufferSize;

			// Collected enough samples, do processing
			if (incomingSampleCount >= samplesTilNextProcess)
			{
				isProcessing = true;
				incomingSampleCount -= samplesTilNextProcess;
				//DBG (" ");
				//DBG ("Process: SampleCount: " << incomingSampleCount);

				// After first processing, do another process every analysisHopSize samples
				samplesTilNextProcess = analysisHopSize;
				
				//jassert (spectralBufferSize > windowSize);
				analysisBuffer.setReadHopSize (analysisHopSize);
				analysisBuffer.read (spectralBuffer, windowSize);
				//DBG ("Analysis Read Index: " << analysisBuffer.getReadIndex ());

				// Apply window to signal
				//juce::FloatVectorOperations::multiply (spectralBuffer, windowBuffer, windowSize);
                for (int k = 0; k < windowSize; k++)
                    spectralBuffer[k] *= windowBuffer[k];

				// Rotate signal 180 degrees, move the first half to the back and back to the front
				std::rotate (spectralBuffer, spectralBuffer + (windowSize / 2), spectralBuffer + windowSize);
				
				// Perform FFT, process and inverse FFT
				performRealOnlyForwardTransform (spectralBuffer, windowSize);
				processImpl (spectralBuffer, spectralBufferSize);
				performRealOnlyInverseTransform (spectralBuffer, windowSize);

				// Undo signal back to original rotation
				std::rotate (spectralBuffer, spectralBuffer + (windowSize / 2), spectralBuffer + windowSize);

				// Apply window to signal
				//juce::FloatVectorOperations::multiply (spectralBuffer, windowBuffer, windowSize);
                for (int k = 0; k < windowSize; k++)
                    spectralBuffer[k] *= windowBuffer[k];

				// Resample output grain to N * (hop size analysis / hop size synthesis)
				linearResample (spectralBuffer, windowSize, resampleBuffer, resampleSize);

				synthesisBuffer.setWriteHopSize (synthesisHopSize);
				synthesisBuffer.overlapWrite (resampleBuffer, resampleSize);
				//DBG ("Synthesis Write Index: " << synthesisBuffer.getWriteIndex ());
			}

			// Emit silence until we start producing output
			if (!isProcessing)
			{
				std::fill (incomingBuffer + internalOffset, incomingBuffer + internalOffset +
					internalBufferSize, 0.f);
				
				//DBG ("Zeroed output: " << internalOffset << " -> " << internalBufferSize);
				continue;
			}

			const auto previousSynthesisReadIndex = synthesisBuffer.getReadIndex ();
			synthesisBuffer.read (incomingBuffer + internalOffset, internalBufferSize);
			//DBG ("Synthesis Read Index: " << previousSynthesisReadIndex << " -> " << synthesisBuffer.getReadIndex ());
		}

		// Rescale output
		//juce::FloatVectorOperations::multiply (incomingBuffer, 1.f / rescalingFactor, incomingBufferSize);
        for (int k = 0; k < incomingBufferSize; k++)
            incomingBuffer[k] *= 1.f / rescalingFactor;
	}

	// Principal argument - Unwrap a phase argument to between [-PI, PI]
	static float principalArgument (float arg)
	{
		return std::fmod (arg + M_PI, -TWO_PI) + M_PI;
	}

	// Returns the 2^x exponent for a given power of two value
	// If the value given is not a power of two, the nearest power 2 will be used
	static int nearestPower2 (int value)
	{
		return (int)log2 (NextPowerOfTwo (value));
	}

private:
	virtual void processImpl (FloatType* const, const int) = 0;

private:	
	//std::unique_ptr<juce::dsp::FFT> fft;

	// Buffers
	BlockCircularBuffer<FloatType> analysisBuffer;
	BlockCircularBuffer<FloatType> synthesisBuffer;
	FloatType* spectralBuffer = nullptr;
	FloatType* resampleBuffer = nullptr;

	// Misc state
	long incomingSampleCount = 0;
	int spectralBufferSize = 0;
	int samplesTilNextProcess = 0;
	bool isProcessing = false;

protected:
	//juce::SpinLock paramLock;
	FloatType* windowBuffer = nullptr;
	float rescalingFactor = 1.f;
	int analysisHopSize = 0;
	int synthesisHopSize = 0;
	int windowSize = 0;
	int resampleSize = 0;
};

}
