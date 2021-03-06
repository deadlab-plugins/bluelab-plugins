#pragma once

//#include <juce_core/juce_core.h>
#include <memory>
#include <cassert>

namespace stekyne
{
    
template<typename ElementType = float>
struct BlockCircularBuffer final
{
    //BlockCircularBuffer () = default;
    BlockCircularBuffer ()
    {
        block = NULL;
    }

    BlockCircularBuffer (long newSize)
    {
        block = NULL;
        
        setSize (newSize, true);
    }

    virtual ~BlockCircularBuffer ()
    {
        if (block != NULL)
            free(block);
    }
    
	void setReadHopSize (int hopSize)
	{
		readHopSize = hopSize;
	}

	auto getReadHopSize () const 
	{
		return readHopSize;
	}

	void setWriteHopSize (int hopSize)
	{
		writeHopSize = hopSize;
	}

	auto getWriteHopSize () const
	{
		return writeHopSize;
	}

	auto getReadIndex () const 
	{
		return readIndex;
	}

	auto getWriteIndex () const 
	{
		return writeIndex;
	}

    void setSize (long newSize, bool shouldClear = false)
    {
        if (newSize == length)
        {
            if (shouldClear)
                reset ();

            return;
        }

        //block.allocate (newSize, shouldClear);
        if (block != NULL)
            free(block);
        block = (ElementType *)malloc(newSize*sizeof(ElementType));
        if (shouldClear)
            memset(block, 0, newSize*sizeof(ElementType));
        
        length = newSize;
        writeIndex = readIndex = 0;
    }

	/*void setEnableLogging (const char * const bufferName, bool enabled)
      {
      name = bufferName;
      shouldLog = enabled;
      }*/

    void reset ()
    {
        //block.clear (length);
        memset(block, 0, length*sizeof(ElementType));
        
        writeIndex = readIndex = 0;
    }

    // Read samples from the internal buffer into the 'destBuffer'
    // perform a wrap of the read if near the internal buffer boundaries
    void read (ElementType* const destBuffer, const long destLength)
    {
        const auto firstReadAmount = readIndex + destLength >= length ?
			length - readIndex : destLength;

		assert (destLength <= length);
		assert (firstReadAmount <= destLength);

		//const auto internalBuffer = block.getData ();
        const ElementType *internalBuffer = block;
		assert (internalBuffer != destBuffer);

		memcpy (destBuffer, internalBuffer + readIndex, sizeof (ElementType) * firstReadAmount);

		if (firstReadAmount < destLength)
		{
			memcpy (destBuffer + firstReadAmount, internalBuffer, sizeof (ElementType) * 
				(static_cast<unsigned long long>(destLength) - firstReadAmount));
		}

		readIndex += readHopSize != 0 ? readHopSize : destLength;
		readIndex %= length;

		//if (shouldLog) printState ();
    }

    // Write all samples from the 'sourceBuffer' into the internal buffer
    // Perform any wrapping required
    void write (const ElementType* sourceBuffer, const long sourceLength)
    {
		sampleCount += sourceLength;

		const auto firstWriteAmount = writeIndex + sourceLength >= length ?
			length - writeIndex : sourceLength;

		//auto internalBuffer = block.getData ();
        const ElementType *internalBuffer = block;
		assert (internalBuffer != sourceBuffer);
		memcpy ((void *)(internalBuffer + writeIndex), sourceBuffer,
                sizeof (ElementType) * firstWriteAmount);

		if (firstWriteAmount < sourceLength)
		{
			memcpy ((void *)internalBuffer,
                    sourceBuffer + firstWriteAmount, sizeof (ElementType) * 
				(static_cast<unsigned long long>(sourceLength) - firstWriteAmount));
		}

		writeIndex += writeHopSize != 0 ? writeHopSize : sourceLength;
		writeIndex %= length;

		//if (shouldLog) printState ();
    }

    // The first 'overlapAmount' of 'sourceBuffer' samples are added to the existing buffer
    // The remainder of samples are set in the buffer (overwrite)
    void overlapWrite (ElementType* sourceBuffer, const long sourceLength)
    {
		const auto overlapAmount = sourceLength - writeHopSize;
        //auto internalBuffer = block.getData ();
        ElementType *internalBuffer = block;
		auto tempWriteIndex = writeIndex;
		auto firstWriteAmount = writeIndex + overlapAmount > length ?
			length - writeIndex : overlapAmount;

		//juce::FloatVectorOperations::add (internalBuffer + writeIndex, sourceBuffer, firstWriteAmount);
        for (int k = 0; k < firstWriteAmount; k++)
            internalBuffer[writeIndex + k] += sourceBuffer[k];
                 
		if (firstWriteAmount < overlapAmount)
		{
			//juce::FloatVectorOperations::add (internalBuffer, sourceBuffer + firstWriteAmount, overlapAmount - firstWriteAmount);
            for (int k = 0; k < overlapAmount - firstWriteAmount; k++)
                internalBuffer[k] += sourceBuffer[firstWriteAmount + k];
		}

		tempWriteIndex += overlapAmount;
		tempWriteIndex %= length;

		const auto remainingElements = sourceLength - overlapAmount;
		firstWriteAmount = tempWriteIndex + remainingElements > length ?
			length - tempWriteIndex : remainingElements;

		memcpy (internalBuffer + tempWriteIndex, sourceBuffer + overlapAmount, sizeof (ElementType) *
			firstWriteAmount);

		if (firstWriteAmount < remainingElements)
		{
			memcpy (internalBuffer, sourceBuffer + overlapAmount + firstWriteAmount, sizeof (ElementType) *
				(remainingElements - static_cast<unsigned long long>(firstWriteAmount)));
		}

		writeIndex += writeHopSize;
		writeIndex %= length;

		//if (shouldLog) printState ();
    }

	/*void printState ()
      {
      #ifdef DEBUG
      DBG ("Name: " << name << juce::String::formatted (", Read Indx: %d, Write Indx: %d, Length: %d",
      readIndex, writeIndex, length));
      #endif
      }*/

private:
    //juce::HeapBlock<ElementType> block;
    ElementType *block;
    
    long writeIndex = 0;
    long readIndex = 0;
    long length = 0;
	long sampleCount = 0;
    int writeHopSize = 0;
    int readHopSize = 0;
	bool shouldLog = false;

    /*#ifdef DEBUG
      const char* name = "";
      #endif*/
};

}
