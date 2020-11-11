//
//  BLUtils.cpp
//  Denoiser
//
//  Created by Apple m'a Tuer on 06/05/17.
//
//

#include <math.h>

#include <cmath>

#include <vector>
#include <algorithm>
using namespace std;

#ifdef WIN32
#include <Windows.h>
#endif

#if 0
// Mel / Mfcc
extern "C" {
#include <libmfcc.h>
}
#endif

#ifndef WIN32
#include <sys/time.h>
#else
#include "GetTimeOfDay.h"
#endif

#include "CMA2Smoother.h"

// For AmpToDB
#include "IPlug_include_in_plug_hdr.h"
//#include "../../WDL/IPlug/Containers.h"

#include <IPlugPaths.h>

#include "BLUtils.h"

using namespace iplug;

#define TWO_PI 6.28318530717959

// Optimizations
// - avoid "GetSize()" in loops
// - avoid "buf.Get()" in loops


// TODO: must check if it is used
/* BL_FLOAT
BLUtils::ampToDB(BL_FLOAT amp, BL_FLOAT minDB)
{
    if (amp <= 0.0)
        return minDB;
    
    BL_FLOAT db = 20. * std::log10(amp);
    
    return db;
} */

#define FIND_VALUE_INDEX_EXPE 0

// Simd
//
// With AVX, he gain is not exceptional with SoundMetaViewer
// and may not support computers before 2011
//
// NOTE: AVX apperad in 2008, and was integrated in 2011
// (this could be risky to use it in plugins)
//
// TODO: make unit tests SIMD for SIMSD (not tested well)
// See class "TestSimd"

// NOTE: before re-enabling this, must check for all templates with float
// Use simdpp::float32 if possible...
#define USE_SIMD 0 //1

// For dispatch see: https://p12tic.github.io/libsimdpp/v2.1/libsimdpp/w/arch/dispatch.html
// For operations see: http://p12tic.github.io/libsimdpp/v2.2-dev/libsimdpp/w/

// For dynamic dispatch, define for the preprocessor:
// - SIMDPP_EMIT_DISPATCHER
// - SIMDPP_DISPATCH_NONE_NULL ?
// - SIMDPP_ARCH_X86_AVX
// Some others...
//
// NOTE: for the moment, did not succeed (need to compile several times the same cpp ?)
// => for the moment, crashes
//
#if USE_SIMD

// Configs
//

// None: 73%

// 66%
#define SIMDPP_ARCH_X86_AVX 1
#define SIMD_PACK_SIZE 4 // Native
//#define SIMD_PACK_SIZE 8 //64

//69%
//#define SIMDPP_ARCH_X86_SSE2 1
//#define SIMD_PACK_SIZE 2

// Not supported
//#define SIMDPP_ARCH_X86_AVX512F 1
//#define SIMD_PACK_SIZE 8


#include <simd.h>

#include <simdpp/dispatch/get_arch_gcc_builtin_cpu_supports.h>
#include <simdpp/dispatch/get_arch_raw_cpuid.h>
#include <simdpp/dispatch/get_arch_linux_cpuinfo.h>

#if SIMDPP_HAS_GET_ARCH_RAW_CPUID
#define SIMDPP_USER_ARCH_INFO ::simdpp::get_arch_raw_cpuid()
#elif SIMDPP_HAS_GET_ARCH_GCC_BUILTIN_CPU_SUPPORTS
#define SIMDPP_USER_ARCH_INFO ::simdpp::get_arch_gcc_builtin_cpu_supports()
#elif SIMDPP_HAS_GET_ARCH_LINUX_CPUINFO
#define SIMDPP_USER_ARCH_INFO ::simdpp::get_arch_linux_cpuinfo()
#else
#error "Unsupported platform"
#endif

#endif

// Additional optilizations added duging the implementation of USE_SIMD
#define USE_SIMD_OPTIM 1

#define TRY_FIX_SIDE_CHAIN_AU 1

#define INF 1e15

bool _useSimd = false;

void
BLUtils::SetUseSimdFlag(bool flag)
{
#if USE_SIMD
    _useSimd = flag;
#endif
}

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeRMSAvg(const FLOAT_TYPE *values, int nFrames)
{
#if !USE_SIMD
    FLOAT_TYPE avg = 0.0;
    for (int i = 0; i < nFrames; i++)
    {
        FLOAT_TYPE value = values[i];
        avg += value*value;
    }
#else
    FLOAT_TYPE avg = ComputeSquareSum(values, nFrames);
#endif
    
    avg = std::sqrt(avg/nFrames);
    
    //avg = std::sqrt(avg)/nFrames;
    
    return avg;
}
template float BLUtils::ComputeRMSAvg(const float *values, int nFrames);
template double BLUtils::ComputeRMSAvg(const double *values, int nFrames);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeSquareSum(const FLOAT_TYPE *values, int nFrames)
{
    FLOAT_TYPE sum2 = 0.0;
    
#if USE_SIMD
    if (_useSimd && (nFrames % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < nFrames; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(values);
            simdpp::float64<SIMD_PACK_SIZE> s = v0*v0;
            
            FLOAT_TYPE r = simdpp::reduce_add(s);
            
            sum2 += r;
            
            values += SIMD_PACK_SIZE;
        }
        
        // Finished
        return sum2;
    }
#endif
    
    for (int i = 0; i < nFrames; i++)
    {
        FLOAT_TYPE value = values[i];
        sum2 += value*value;
    }
    
    return sum2;
}
template float BLUtils::ComputeSquareSum(const float *values, int nFrames);
template double BLUtils::ComputeSquareSum(const double *values, int nFrames);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeRMSAvg(const WDL_TypedBuf<FLOAT_TYPE> &values)
{
    return ComputeRMSAvg(values.Get(), values.GetSize());
}
template float BLUtils::ComputeRMSAvg(const WDL_TypedBuf<float> &values);
template double BLUtils::ComputeRMSAvg(const WDL_TypedBuf<double> &values);

// Does not give good results with UST
// (this looks false)
template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeRMSAvg2(const FLOAT_TYPE *values, int nFrames)
{
#if !USE_SIMD_OPTIM
    FLOAT_TYPE avg = 0.0;
    for (int i = 0; i < nFrames; i++)
    {
        FLOAT_TYPE value = values[i];
        avg += value*value;
    }
#else
    FLOAT_TYPE avg = ComputeSquareSum(values, nFrames);
#endif
    
    avg = std::sqrt(avg)/nFrames;
    
    return avg;
}
template float BLUtils::ComputeRMSAvg2(const float *values, int nFrames);
template double BLUtils::ComputeRMSAvg2(const double *values, int nFrames);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeRMSAvg2(const WDL_TypedBuf<FLOAT_TYPE> &buf)
{
    return ComputeRMSAvg2(buf.Get(), buf.GetSize());
}
template float BLUtils::ComputeRMSAvg2(const WDL_TypedBuf<float> &buf);
template double BLUtils::ComputeRMSAvg2(const WDL_TypedBuf<double> &buf);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeAvg(const FLOAT_TYPE *buf, int nFrames)
{
#if !USE_SIMD_OPTIM
    FLOAT_TYPE avg = 0.0;
    for (int i = 0; i < nFrames; i++)
    {
        FLOAT_TYPE value = buf[i];
        avg += value;
    }
#else
    FLOAT_TYPE avg = ComputeSum(buf, nFrames);
#endif
    
    if (nFrames > 0)
        avg = avg/nFrames;
    
    return avg;
}
template float BLUtils::ComputeAvg(const float *buf, int nFrames);
template double BLUtils::ComputeAvg(const double *buf, int nFrames);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeAvg(const vector<FLOAT_TYPE> &data)
{
    if (data.empty())
        return 0.0;
    
    FLOAT_TYPE sum = 0.0;
    for (int i = 0; i < data.size(); i++)
    {
        sum += data[i];
    }
    
    FLOAT_TYPE avg = sum/data.size();
    
    return avg;
}
template float BLUtils::ComputeAvg(const vector<float> &data);
template double BLUtils::ComputeAvg(const vector<double> &data);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeAvg(const WDL_TypedBuf<FLOAT_TYPE> &buf)
{
    return ComputeAvg(buf.Get(), buf.GetSize());
}
template float BLUtils::ComputeAvg(const WDL_TypedBuf<float> &buf);
template double BLUtils::ComputeAvg(const WDL_TypedBuf<double> &buf);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeAvg(const WDL_TypedBuf<FLOAT_TYPE> &buf, int startIndex, int endIndex)
{
    FLOAT_TYPE sum = 0.0;
    int numValues = 0;
    
    int bufSize = buf.GetSize();
    FLOAT_TYPE *bufData = buf.Get();
    for (int i = startIndex; i <= endIndex ; i++)
    {
        if (i < 0)
            continue;
        if (i >= bufSize)
            break;
        
        FLOAT_TYPE value = bufData[i];
        sum += value;
        numValues++;
    }
    
    FLOAT_TYPE avg = 0.0;
    if (numValues > 0)
        avg = sum/numValues;
    
    return avg;
}
template float BLUtils::ComputeAvg(const WDL_TypedBuf<float> &buf, int startIndex, int endIndex);
template double BLUtils::ComputeAvg(const WDL_TypedBuf<double> &buf, int startIndex, int endIndex);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeAvg(const vector<WDL_TypedBuf<FLOAT_TYPE> > &image)
{
    FLOAT_TYPE sum = 0.0;
    int numValues = 0;
    
    for (int i = 0; i < image.size(); i++)
    {
#if !USE_SIMD_OPTIM
        for (int j = 0; j < image[i].GetSize(); j++)
        {
            FLOAT_TYPE val = image[i].Get()[j];
        
            sum += val;
            numValues++;
        }
#else
        sum += ComputeAvg(image[i]);
        numValues += image[i].GetSize();
#endif
    }
    
    FLOAT_TYPE avg = sum;
    if (numValues > 0)
        avg /= numValues;
    
    return avg;
}
template float BLUtils::ComputeAvg(const vector<WDL_TypedBuf<float> > &image);
template double BLUtils::ComputeAvg(const vector<WDL_TypedBuf<double> > &image);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeAvgSquare(const FLOAT_TYPE *buf, int nFrames)
{
#if !USE_SIMD_OPTIM
    FLOAT_TYPE avg = 0.0;
    for (int i = 0; i < nFrames; i++)
    {
        FLOAT_TYPE value = buf[i];
        avg += value*value;
    }
#else
    FLOAT_TYPE avg = ComputeSquareSum(buf, nFrames);
#endif
    
    if (nFrames > 0)
        avg = std::sqrt(avg)/nFrames;
    
    return avg;
}
template float BLUtils::ComputeAvgSquare(const float *buf, int nFrames);
template double BLUtils::ComputeAvgSquare(const double *buf, int nFrames);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeAvgSquare(const WDL_TypedBuf<FLOAT_TYPE> &buf)
{
    return ComputeAvgSquare(buf.Get(), buf.GetSize());
}
template float BLUtils::ComputeAvgSquare(const WDL_TypedBuf<float> &buf);
template double BLUtils::ComputeAvgSquare(const WDL_TypedBuf<double> &buf);

template <typename FLOAT_TYPE>
void
BLUtils::ComputeSquare(WDL_TypedBuf<FLOAT_TYPE> *buf)
{
    int bufSize = buf->GetSize();
    FLOAT_TYPE *bufData = buf->Get();
    
#if USE_SIMD
    if (_useSimd && (bufSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < bufSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(bufData);
            simdpp::float64<SIMD_PACK_SIZE> r = v0*v0;
            
            simdpp::store(bufData, r);
            
            bufData += SIMD_PACK_SIZE;
        }
        
        // Finished
        return;
    }
#endif
    
    for (int i = 0; i < bufSize; i++)
    {
        FLOAT_TYPE val = bufData[i];

        val = val*val;
        
        bufData[i] = val;
    }
}
template void BLUtils::ComputeSquare(WDL_TypedBuf<float> *buf);
template void BLUtils::ComputeSquare(WDL_TypedBuf<double> *buf);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeAbsAvg(const FLOAT_TYPE *buf, int nFrames)
{
    FLOAT_TYPE avg = ComputeAbsSum(buf, nFrames);
    
    if (nFrames > 0)
        avg = avg/nFrames;
    
    return avg;
}
template float BLUtils::ComputeAbsAvg(const float *buf, int nFrames);
template double BLUtils::ComputeAbsAvg(const double *buf, int nFrames);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeAbsAvg(const WDL_TypedBuf<FLOAT_TYPE> &buf)
{
    return ComputeAbsAvg(buf.Get(), buf.GetSize());
}
template float BLUtils::ComputeAbsAvg(const WDL_TypedBuf<float> &buf);
template double BLUtils::ComputeAbsAvg(const WDL_TypedBuf<double> &buf);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeAbsAvg(const WDL_TypedBuf<FLOAT_TYPE> &buf, int startIndex, int endIndex)
{
    FLOAT_TYPE sum = 0.0;
    int numValues = 0;
    
    int bufSize = buf.GetSize();
    FLOAT_TYPE *bufData = buf.Get();
    for (int i = startIndex; i <= endIndex ; i++)
    {
        if (i < 0)
            continue;
        if (i >= bufSize)
            break;
        
        FLOAT_TYPE value = bufData[i];
        value = std::fabs(value);
        
        sum += value;
        numValues++;
    }
    
    FLOAT_TYPE avg = 0.0;
    if (numValues > 0)
        avg = sum/numValues;
    
    return avg;
}
template float BLUtils::ComputeAbsAvg(const WDL_TypedBuf<float> &buf, int startIndex, int endIndex);
template double BLUtils::ComputeAbsAvg(const WDL_TypedBuf<double> &buf, int startIndex, int endIndex);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeMax(const WDL_TypedBuf<FLOAT_TYPE> &buf)
{
    return ComputeMax(buf.Get(), buf.GetSize());
}
template float BLUtils::ComputeMax(const WDL_TypedBuf<float> &buf);
template double BLUtils::ComputeMax(const WDL_TypedBuf<double> &buf);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeMax(const FLOAT_TYPE *output, int nFrames)
{
    FLOAT_TYPE maxVal = -1e16;
    
#if USE_SIMD
    if (_useSimd && (nFrames % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < nFrames; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(output);
            
            FLOAT_TYPE r = simdpp::reduce_max(v0);
            
            if (r > maxVal)
                maxVal = r;
            
            output += SIMD_PACK_SIZE;
        }
        
        // Finished
        return maxVal;
    }
#endif
    
    for (int i = 0; i < nFrames; i++)
    {
        FLOAT_TYPE value = output[i];
        
        if (value > maxVal)
            maxVal = value;
    }
    
    return maxVal;
}
template float BLUtils::ComputeMax(const float *output, int nFrames);
template double BLUtils::ComputeMax(const double *output, int nFrames);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeMax(const vector<WDL_TypedBuf<FLOAT_TYPE> > &values)
{
    // SIMD
    FLOAT_TYPE maxValue = -1e15;
    
    for (int i = 0; i < values.size(); i++)
    {
#if !USE_SIMD_OPTIM
        for (int j = 0; j < values[i].GetSize(); j++)
        {
            FLOAT_TYPE val = values[i].Get()[j];
            if (val > maxValue)
                maxValue = val;
        }
#else
        FLOAT_TYPE max0 = ComputeMax(values[i].Get(), values[i].GetSize());
        if (max0 > maxValue)
            maxValue = max0;
#endif
    }
    
    return maxValue;
}
template float BLUtils::ComputeMax(const vector<WDL_TypedBuf<float> > &values);
template double BLUtils::ComputeMax(const vector<WDL_TypedBuf<double> > &values);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeMaxAbs(const FLOAT_TYPE *buf, int nFrames)
{
    FLOAT_TYPE maxVal = 0.0;
    
#if USE_SIMD
    if (_useSimd && (nFrames % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < nFrames; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(buf);
            
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::abs(v0);
            
            FLOAT_TYPE r = simdpp::reduce_max(v1);
            
            if (r > maxVal)
                maxVal = r;
            
            buf += SIMD_PACK_SIZE;
        }
        
        // Finished
        return maxVal;
    }
#endif
    
    for (int i = 0; i < nFrames; i++)
    {
        FLOAT_TYPE value = buf[i];
        
        value = std::fabs(value);
        
        if (value > maxVal)
            maxVal = value;
    }
    
    return maxVal;
}
template float BLUtils::ComputeMaxAbs(const float *buf, int nFrames);
template double BLUtils::ComputeMaxAbs(const double *buf, int nFrames);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeMaxAbs(const WDL_TypedBuf<FLOAT_TYPE> &buf)
{
#if !USE_SIMD_OPTIM
    FLOAT_TYPE maxVal = 0.0;
    int bufSize = buf.GetSize();
    FLOAT_TYPE *bufData = buf.Get();
    
    for (int i = 0; i < bufSize; i++)
    {
        FLOAT_TYPE value = bufData[i];
        
        value = std::fabs(value);
        
        if (value > maxVal)
            maxVal = value;
    }
    
    return maxVal;
#else
    FLOAT_TYPE maxVal = ComputeMaxAbs(buf.Get(), buf.GetSize());
    
    return maxVal;
#endif
}
template float BLUtils::ComputeMaxAbs(const WDL_TypedBuf<float> &buf);
template double BLUtils::ComputeMaxAbs(const WDL_TypedBuf<double> &buf);

template <typename FLOAT_TYPE>
void
BLUtils::ComputeMax(WDL_TypedBuf<FLOAT_TYPE> *max, const WDL_TypedBuf<FLOAT_TYPE> &buf)
{
    if (max->GetSize() != buf.GetSize())
        max->Resize(buf.GetSize());
    
    int maxSize = max->GetSize();
    FLOAT_TYPE *bufData = buf.Get();
    FLOAT_TYPE *maxData = max->Get();
    
#if USE_SIMD
    if (_useSimd && (maxSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < maxSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(bufData);
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::load(maxData);
            
            simdpp::float64<SIMD_PACK_SIZE> r = simdpp::max(v0, v1);
            
            simdpp::store(maxData, r);
            
            bufData += SIMD_PACK_SIZE;
            maxData += SIMD_PACK_SIZE;
        }
    }
    
    return;
#endif
    
    for (int i = 0; i < maxSize; i++)
    {
        FLOAT_TYPE val = bufData[i];
        FLOAT_TYPE m = maxData[i];
        
        if (val > m)
            maxData[i] = val;
    }
}
template void BLUtils::ComputeMax(WDL_TypedBuf<float> *max, const WDL_TypedBuf<float> &buf);
template void BLUtils::ComputeMax(WDL_TypedBuf<double> *max, const WDL_TypedBuf<double> &buf);

template <typename FLOAT_TYPE>
void
BLUtils::ComputeMax(WDL_TypedBuf<FLOAT_TYPE> *maxBuf, const FLOAT_TYPE *buf)
{
    int maxSize = maxBuf->GetSize();
    FLOAT_TYPE *maxData = maxBuf->Get();
    
#if USE_SIMD
    if (_useSimd && (maxSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < maxSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(buf);
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::load(maxData);
            
            simdpp::float64<SIMD_PACK_SIZE> r = simdpp::max(v0, v1);
            
            simdpp::store(maxData, r);
            
            buf += SIMD_PACK_SIZE;
            maxData += SIMD_PACK_SIZE;
        }
    }
    
    return;
#endif
    
    for (int i = 0; i < maxSize; i++)
    {
        FLOAT_TYPE val = buf[i];
        FLOAT_TYPE m = maxData[i];
        
        if (val > m)
            maxData[i] = val;
    }
}
template void BLUtils::ComputeMax(WDL_TypedBuf<float> *maxBuf, const float *buf);
template void BLUtils::ComputeMax(WDL_TypedBuf<double> *maxBuf, const double *buf);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeMin(const WDL_TypedBuf<FLOAT_TYPE> &buf)
{
    int bufSize = buf.GetSize();
    FLOAT_TYPE *bufData = buf.Get();
    
    if (bufSize == 0)
        return 0.0;
    
    FLOAT_TYPE minVal = bufData[0];
    
#if USE_SIMD
    if (_useSimd && (bufSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < bufSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(bufData);
            
            FLOAT_TYPE r = simdpp::reduce_min(v0);
            
            if (r < minVal)
                minVal = r;
            
            bufData += SIMD_PACK_SIZE;
        }
        
        // Finished
        return minVal;
    }
#endif
    
    for (int i = 0; i < bufSize; i++)
    {
        FLOAT_TYPE val = bufData[i];
        
        if (val < minVal)
            minVal = val;
    }
    
    return minVal;
}
template float BLUtils::ComputeMin(const WDL_TypedBuf<float> &buf);
template double BLUtils::ComputeMin(const WDL_TypedBuf<double> &buf);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeMin(const vector<WDL_TypedBuf<FLOAT_TYPE> > &values)
{
    FLOAT_TYPE minValue = 1e15;
    
    for (int i = 0; i < values.size(); i++)
    {
#if !USE_SIMD_OPTIM
        for (int j = 0; j < values[i].GetSize(); j++)
        {
            FLOAT_TYPE val = values[i].Get()[j];
            if (val < minValue)
                minValue = val;
        }
#else
        FLOAT_TYPE val = ComputeMin(values[i]);
        if (val < minValue)
            minValue = val;
#endif
    }
    
    return minValue;
}
template float BLUtils::ComputeMin(const vector<WDL_TypedBuf<float> > &values);
template double BLUtils::ComputeMin(const vector<WDL_TypedBuf<double> > &values);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeSum(const WDL_TypedBuf<FLOAT_TYPE> &buf)
{
    
    int bufSize = buf.GetSize();
    FLOAT_TYPE *bufData = buf.Get();
    
    FLOAT_TYPE result = 0.0;
    
#if USE_SIMD
    if (_useSimd && (bufSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < bufSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(bufData);
            
            FLOAT_TYPE r = simdpp::reduce_add(v0);
            
            result += r;
            
            bufData += SIMD_PACK_SIZE;
        }
        
        // Finished
        return result;
    }
#endif
    
    for (int i = 0; i < bufSize; i++)
    {
        FLOAT_TYPE val = bufData[i];
        
        result += val;
    }
    
    return result;
}
template float BLUtils::ComputeSum(const WDL_TypedBuf<float> &buf);
template double BLUtils::ComputeSum(const WDL_TypedBuf<double> &buf);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeSum(const FLOAT_TYPE *buf, int nFrames)
{
    FLOAT_TYPE result = 0.0;
    
#if USE_SIMD
    if (_useSimd && (nFrames % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < nFrames; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(buf);
            
            FLOAT_TYPE r = simdpp::reduce_add(v0);
            
            result += r;
            
            buf += SIMD_PACK_SIZE;
        }
        
        // Finished
        return result;
    }
#endif
    
    for (int i = 0; i < nFrames; i++)
    {
        FLOAT_TYPE val = buf[i];
        
        result += val;
    }
    
    return result;
}
template float BLUtils::ComputeSum(const float *buf, int nFrames);
template double BLUtils::ComputeSum(const double *buf, int nFrames);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeAbsSum(const WDL_TypedBuf<FLOAT_TYPE> &buf)
{
    FLOAT_TYPE result = 0.0;
    
    int bufSize = buf.GetSize();
    FLOAT_TYPE *bufData = buf.Get();
    
#if USE_SIMD
    if (_useSimd && (bufSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < bufSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(bufData);
            
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::abs(v0);
            
            FLOAT_TYPE r = simdpp::reduce_add(v1);
            
            result += r;
            
            bufData += SIMD_PACK_SIZE;
        }
        
        // Finished
        return result;
    }
#endif
    
    for (int i = 0; i < bufSize; i++)
    {
        FLOAT_TYPE val = bufData[i];
        
        result += std::fabs(val);
    }
    
    return result;
}
template float BLUtils::ComputeAbsSum(const WDL_TypedBuf<float> &buf);
template double BLUtils::ComputeAbsSum(const WDL_TypedBuf<double> &buf);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeAbsSum(const FLOAT_TYPE *buf, int nFrames)
{
    FLOAT_TYPE result = 0.0;
    
    int bufSize = nFrames;
    const FLOAT_TYPE *bufData = buf;
    
#if USE_SIMD
    if (_useSimd && (bufSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < bufSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(bufData);
            
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::abs(v0);
            
            FLOAT_TYPE r = simdpp::reduce_add(v1);
            
            result += r;
            
            bufData += SIMD_PACK_SIZE;
        }
        
        // Finished
        return result;
    }
#endif
    
    for (int i = 0; i < bufSize; i++)
    {
        FLOAT_TYPE val = bufData[i];
        
        result += std::fabs(val);
    }
    
    return result;
}
template float BLUtils::ComputeAbsSum(const float *buf, int nFrames);
template double BLUtils::ComputeAbsSum(const double *buf, int nFrames);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeClipSum(const WDL_TypedBuf<FLOAT_TYPE> &buf)
{
    FLOAT_TYPE result = 0.0;
    
    int bufSize = buf.GetSize();
    FLOAT_TYPE *bufData = buf.Get();
    
#if USE_SIMD
    if (_useSimd && (bufSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < bufSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(bufData);
            
            FLOAT_TYPE z = 0.0;
            simdpp::float64<SIMD_PACK_SIZE> z0 = simdpp::load_splat(&z);
            
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::max(v0, z0);
            
            FLOAT_TYPE r = simdpp::reduce_add(v1);
            
            result += r;
            
            bufData += SIMD_PACK_SIZE;
        }
        
        // Finished
        return result;
    }
#endif
    
    for (int i = 0; i < bufSize; i++)
    {
        FLOAT_TYPE val = bufData[i];
        
        // Clip if necessary
        if (val < 0.0)
            val = 0.0;
        
        result += val;
    }
    
    return result;
}
template float BLUtils::ComputeClipSum(const WDL_TypedBuf<float> &buf);
template double BLUtils::ComputeClipSum(const WDL_TypedBuf<double> &buf);

template <typename FLOAT_TYPE>
void
BLUtils::ComputeSum(const WDL_TypedBuf<FLOAT_TYPE> &buf0,
                  const WDL_TypedBuf<FLOAT_TYPE> &buf1,
                  WDL_TypedBuf<FLOAT_TYPE> *result)
{
    if (buf0.GetSize() != buf1.GetSize())
        return;
    
    result->Resize(buf0.GetSize());
    
    int resultSize = result->GetSize();
    FLOAT_TYPE *resultData = result->Get();
    FLOAT_TYPE *buf0Data = buf0.Get();
    FLOAT_TYPE *buf1Data = buf1.Get();
    
#if USE_SIMD
    0 //if (_useSimd && (resultSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < resultSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(buf0Data);
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::load(buf1Data);
            
            simdpp::float64<SIMD_PACK_SIZE> r = v0 + v1;
            
            simdpp::store(resultData, r);
            
            
            buf0Data += SIMD_PACK_SIZE;
            buf1Data += SIMD_PACK_SIZE;
            
            resultData += SIMD_PACK_SIZE;
        }
        
        // Finished
        return;
    }
#endif
    
    for (int i = 0; i < resultSize; i++)
    {
        FLOAT_TYPE val0 = buf0Data[i];
        FLOAT_TYPE val1 = buf1Data[i];
        
        FLOAT_TYPE sum = val0 + val1;
        
        resultData[i] = sum;
    }
}
template void BLUtils::ComputeSum(const WDL_TypedBuf<float> &buf0,
                                const WDL_TypedBuf<float> &buf1,
                                WDL_TypedBuf<float> *result);
template void BLUtils::ComputeSum(const WDL_TypedBuf<double> &buf0,
                                const WDL_TypedBuf<double> &buf1,
                                WDL_TypedBuf<double> *result);

template <typename FLOAT_TYPE>
void
BLUtils::ComputeProduct(const WDL_TypedBuf<FLOAT_TYPE> &buf0,
                      const WDL_TypedBuf<FLOAT_TYPE> &buf1,
                      WDL_TypedBuf<FLOAT_TYPE> *result)
{
    if (buf0.GetSize() != buf1.GetSize())
        return;
    
    result->Resize(buf0.GetSize());
    
    int resultSize = result->GetSize();
    FLOAT_TYPE *resultData = result->Get();
    
    const FLOAT_TYPE *buf0Data = buf0.Get();
    const FLOAT_TYPE *buf1Data = buf1.Get();
    
#if USE_SIMD
    if (_useSimd && (resultSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < resultSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(buf0Data);
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::load(buf1Data);
            
            simdpp::float64<SIMD_PACK_SIZE> r = v0 * v1;
            
            simdpp::store(resultData, r);
            
            
            buf0Data += SIMD_PACK_SIZE;
            buf1Data += SIMD_PACK_SIZE;
            
            resultData += SIMD_PACK_SIZE;
        }
        
        // Finished
        return;
    }
#endif
    
    for (int i = 0; i < resultSize; i++)
    {
        // SIMD
        //FLOAT_TYPE val0 = buf0.Get()[i];
        //FLOAT_TYPE val1 = buf1.Get()[i];
        
        FLOAT_TYPE val0 = buf0Data[i];
        FLOAT_TYPE val1 = buf1Data[i];
        
        FLOAT_TYPE prod = val0 * val1;
        
        resultData[i] = prod;
    }
}
template void BLUtils::ComputeProduct(const WDL_TypedBuf<float> &buf0,
                           const WDL_TypedBuf<float> &buf1,
                           WDL_TypedBuf<float> *result);
template void BLUtils::ComputeProduct(const WDL_TypedBuf<double> &buf0,
                           const WDL_TypedBuf<double> &buf1,
                           WDL_TypedBuf<double> *result);


template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::NormalizedXTodB(FLOAT_TYPE x, FLOAT_TYPE mindB, FLOAT_TYPE maxdB)
{
    x = x*(maxdB - mindB) + mindB;
    
    if (x > 0.0)
        x = BLUtils::AmpToDB(x);
        
    FLOAT_TYPE lMin = BLUtils::AmpToDB(mindB);
    FLOAT_TYPE lMax = BLUtils::AmpToDB(maxdB);
        
    x = (x - lMin)/(lMax - lMin);
    
    return x;
}
template float BLUtils::NormalizedXTodB(float x, float mindB, float maxdB);
template double BLUtils::NormalizedXTodB(double x, double mindB, double maxdB);

// Same as NormalizedYTodBInv
template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::NormalizedXTodBInv(FLOAT_TYPE x, FLOAT_TYPE mindB, FLOAT_TYPE maxdB)
{
    FLOAT_TYPE lMin = BLUtils::AmpToDB(mindB);
    FLOAT_TYPE lMax = BLUtils::AmpToDB(maxdB);
    
    FLOAT_TYPE result = x*(lMax - lMin) + lMin;
    
    if (result > 0.0)
        result = BLUtils::DBToAmp(result);
    
    result = (result - mindB)/(maxdB - mindB);
    
    return result;
}
template float BLUtils::NormalizedXTodBInv(float x, float mindB, float maxdB);
template double BLUtils::NormalizedXTodBInv(double x, double mindB, double maxdB);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::NormalizedYTodB(FLOAT_TYPE y, FLOAT_TYPE mindB, FLOAT_TYPE maxdB)
{
    if (std::fabs(y) < BL_EPS)
        y = mindB;
    else
        y = BLUtils::AmpToDB(y);
    
    y = (y - mindB)/(maxdB - mindB);
    
    return y;
}
template float BLUtils::NormalizedYTodB(float y, float mindB, float maxdB);
template double BLUtils::NormalizedYTodB(double y, double mindB, double maxdB);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::NormalizedYTodB2(FLOAT_TYPE y, FLOAT_TYPE mindB, FLOAT_TYPE maxdB)
{
    y = BLUtils::AmpToDB(y);
    
    y = (y - mindB)/(maxdB - mindB);
    
    return y;
}
template float BLUtils::NormalizedYTodB2(float y, float mindB, float maxdB);
template double BLUtils::NormalizedYTodB2(double y, double mindB, double maxdB);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::NormalizedYTodB3(FLOAT_TYPE y, FLOAT_TYPE mindB, FLOAT_TYPE maxdB)
{
    y = y*(maxdB - mindB) + mindB;
    
    if (y > 0.0)
        y = BLUtils::AmpToDB(y);
    
    FLOAT_TYPE lMin = BLUtils::AmpToDB(mindB);
    FLOAT_TYPE lMax = BLUtils::AmpToDB(maxdB);
    
    y = (y - lMin)/(lMax - lMin);
    
    return y;
}
template float BLUtils::NormalizedYTodB3(float y, float mindB, float maxdB);
template double BLUtils::NormalizedYTodB3(double y, double mindB, double maxdB);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::NormalizedYTodBInv(FLOAT_TYPE y, FLOAT_TYPE mindB, FLOAT_TYPE maxdB)
{
    FLOAT_TYPE result = y*(maxdB - mindB) + mindB;
    
    result = BLUtils::DBToAmp(result);
    
    return result;
}
template float BLUtils::NormalizedYTodBInv(float y, float mindB, float maxdB);
template double BLUtils::NormalizedYTodBInv(double y, double mindB, double maxdB);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::AverageYDB(FLOAT_TYPE y0, FLOAT_TYPE y1, FLOAT_TYPE mindB, FLOAT_TYPE maxdB)
{
    FLOAT_TYPE y0Norm = NormalizedYTodB(y0, mindB, maxdB);
    FLOAT_TYPE y1Norm = NormalizedYTodB(y1, mindB, maxdB);
    
    FLOAT_TYPE avg = (y0Norm + y1Norm)/2.0;
    
    FLOAT_TYPE result = NormalizedYTodBInv(avg, mindB, maxdB);
    
    return result;
}
template float BLUtils::AverageYDB(float y0, float y1, float mindB, float maxdB);
template double BLUtils::AverageYDB(double y0, double y1, double mindB, double maxdB);

template <typename FLOAT_TYPE>
void
BLUtils::ComputeAvg(WDL_TypedBuf<FLOAT_TYPE> *avg,
                  const WDL_TypedBuf<FLOAT_TYPE> &values0,
                  const WDL_TypedBuf<FLOAT_TYPE> &values1)
{
    avg->Resize(values0.GetSize());
    
    int values0Size = values0.GetSize();
    FLOAT_TYPE *values0Data = values0.Get();
    FLOAT_TYPE *values1Data = values1.Get();
    FLOAT_TYPE *avgData = avg->Get();
    
#if USE_SIMD
    if (_useSimd && (values0Size % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < values0Size; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(values0Data);
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::load(values1Data);
            
            FLOAT_TYPE c = 0.5;
            simdpp::float64<SIMD_PACK_SIZE> c0 = simdpp::load_splat(&c);
            
            simdpp::float64<SIMD_PACK_SIZE> r = (v0 + v1)*c0;
            
            simdpp::store(avgData, r);
            
            
            values0Data += SIMD_PACK_SIZE;
            values1Data += SIMD_PACK_SIZE;
            
            avgData += SIMD_PACK_SIZE;
        }
        
        // Finished
        return;
    }
#endif
    
    for (int i = 0; i < values0Size; i++)
    {
        FLOAT_TYPE val0 = values0Data[i];
        FLOAT_TYPE val1 = values1Data[i];
        
        FLOAT_TYPE res = (val0 + val1)*0.5;
        
        avgData[i] = res;
    }
}
template void BLUtils::ComputeAvg(WDL_TypedBuf<float> *avg,
                                const WDL_TypedBuf<float> &values0,
                                const WDL_TypedBuf<float> &values1);
template void BLUtils::ComputeAvg(WDL_TypedBuf<double> *avg,
                                const WDL_TypedBuf<double> &values0,
                                const WDL_TypedBuf<double> &values1);

template <typename FLOAT_TYPE>
void
BLUtils::ComputeAvg(FLOAT_TYPE *avg,
                  const FLOAT_TYPE *values0, const FLOAT_TYPE *values1,
                  int nFrames)
{
#if USE_SIMD
    if (_useSimd && (nFrames % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < nFrames; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(values0);
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::load(values1);
            
            FLOAT_TYPE c = 0.5;
            simdpp::float64<SIMD_PACK_SIZE> c0 = simdpp::load_splat(&c);
            
            simdpp::float64<SIMD_PACK_SIZE> r = (v0 + v1)*c0;
            
            simdpp::store(avg, r);
            
            
            values0 += SIMD_PACK_SIZE;
            values1 += SIMD_PACK_SIZE;
            
            avg += SIMD_PACK_SIZE;
        }
        
        // Finished
        return;
    }
#endif
    
    for (int i = 0; i < nFrames; i++)
    {
        FLOAT_TYPE val0 = values0[i];
        FLOAT_TYPE val1 = values1[i];
        
        FLOAT_TYPE res = (val0 + val1)*0.5;
        
        avg[i] = res;
    }
}
template void BLUtils::ComputeAvg(float *avg,
                                const float *values0, const float *values1,
                                int nFrames);
template void BLUtils::ComputeAvg(double *avg,
                                const double *values0, const double *values1,
                                int nFrames);

template <typename FLOAT_TYPE>
void
BLUtils::ComplexSum(WDL_TypedBuf<FLOAT_TYPE> *ioMagns,
                  WDL_TypedBuf<FLOAT_TYPE> *ioPhases,
                  const WDL_TypedBuf<FLOAT_TYPE> &magns,
                  WDL_TypedBuf<FLOAT_TYPE> &phases)
{
    FLOAT_TYPE ioMagnsSize = ioMagns->GetSize();
    FLOAT_TYPE *ioMagnsData = ioMagns->Get();
    FLOAT_TYPE *ioPhasesData = ioPhases->Get();
    FLOAT_TYPE *magnsData = magns.Get();
    FLOAT_TYPE *phasesData = phases.Get();
    
    for (int i = 0; i < ioMagnsSize; i++)
    {
        FLOAT_TYPE magn0 = ioMagnsData[i];
        FLOAT_TYPE phase0 = ioPhasesData[i];
        
        WDL_FFT_COMPLEX comp0;
        BLUtils::MagnPhaseToComplex(&comp0, magn0, phase0);
        
        FLOAT_TYPE magn1 = magnsData[i];
        FLOAT_TYPE phase1 = phasesData[i];
        
        WDL_FFT_COMPLEX comp1;
        BLUtils::MagnPhaseToComplex(&comp1, magn1, phase1);
        
        comp0.re += comp1.re;
        comp0.im += comp1.im;
        
        BLUtils::ComplexToMagnPhase(comp0, &magn0, &phase0);
        
        ioMagnsData[i] = magn0;
        ioPhasesData[i] = phase0;
    }
}
template void BLUtils::ComplexSum(WDL_TypedBuf<float> *ioMagns,
                       WDL_TypedBuf<float> *ioPhases,
                       const WDL_TypedBuf<float> &magns,
                       WDL_TypedBuf<float> &phases);
template void BLUtils::ComplexSum(WDL_TypedBuf<double> *ioMagns,
                       WDL_TypedBuf<double> *ioPhases,
                       const WDL_TypedBuf<double> &magns,
                       WDL_TypedBuf<double> &phases);


template <typename FLOAT_TYPE>
bool
BLUtils::IsAllZero(const WDL_TypedBuf<FLOAT_TYPE> &buffer)
{
    return IsAllZero(buffer.Get(), buffer.GetSize());
}
template bool BLUtils::IsAllZero(const WDL_TypedBuf<float> &buffer);
template bool BLUtils::IsAllZero(const WDL_TypedBuf<double> &buffer);

template <typename FLOAT_TYPE>
bool
BLUtils::IsAllZero(const FLOAT_TYPE *buffer, int nFrames)
{
    if (buffer == NULL)
        return true;

#if USE_SIMD
    if (_useSimd && (nFrames % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < nFrames; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(buffer);
            
            simdpp::float64<SIMD_PACK_SIZE> a = simdpp::abs(v0);
            
            FLOAT_TYPE r = simdpp::reduce_max(a);
            if (r > BL_EPS)
                return false;
            
            buffer += SIMD_PACK_SIZE;
        }
        
        // Finished
        return true;
    }
#endif
    
    for (int i = 0; i < nFrames; i++)
    {
        FLOAT_TYPE val = std::fabs(buffer[i]);
        if (val > BL_EPS)
            return false;
    }
            
    return true;
}
template bool BLUtils::IsAllZero(const float *buffer, int nFrames);
template bool BLUtils::IsAllZero(const double *buffer, int nFrames);

bool
BLUtils::IsAllZeroComp(const WDL_TypedBuf<WDL_FFT_COMPLEX> &buffer)
{
#if !USE_SIMD_OPTIM
    int bufferSize = buffer.GetSize();
    WDL_FFT_COMPLEX *bufferData = buffer.Get();
    
    for (int i = 0; i < bufferSize; i++)
    {
        if (std::fabs(bufferData[i].re) > BL_EPS)
            return false;
        
        if (std::fabs(bufferData[i].im) > BL_EPS)
            return false;
    }
    
    return true;
#else
    int bufSize = buffer.GetSize()/**2*/;
    bool res = IsAllZeroComp(buffer.Get(), bufSize);
    
    return res;
#endif
}

bool
BLUtils::IsAllZeroComp(const WDL_FFT_COMPLEX *buffer, int bufLen)
{
    for (int i = 0; i < bufLen; i++)
    {
        if (std::fabs(buffer[i].re) > BL_EPS)
            return false;
        if (std::fabs(buffer[i].im) > BL_EPS)
            return false;
    }
    
    return true;
}

template <typename FLOAT_TYPE>
bool
BLUtils::IsAllSmallerEps(const WDL_TypedBuf<FLOAT_TYPE> &buffer, FLOAT_TYPE eps)
{
    if (buffer.GetSize() == 0)
        return true;
    
    int bufferSize = buffer.GetSize();
    FLOAT_TYPE *bufferData = buffer.Get();
    
#if USE_SIMD
    if (_useSimd && (bufferSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < bufferSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(bufferData);
            
            simdpp::float64<SIMD_PACK_SIZE> a = simdpp::abs(v0);
            
            FLOAT_TYPE r = simdpp::reduce_max(a);
            if (r > eps)
                return false;
            
            bufferData += SIMD_PACK_SIZE;
        }
        
        // Finished
        return true;
    }
#endif
    
    for (int i = 0; i < bufferSize; i++)
    {
        FLOAT_TYPE val = std::fabs(bufferData[i]);
        if (val > eps)
            return false;
    }
    
    return true;
}
template bool BLUtils::IsAllSmallerEps(const WDL_TypedBuf<float> &buffer, float eps);
template bool BLUtils::IsAllSmallerEps(const WDL_TypedBuf<double> &buffer, double eps);

// OPTIM PROF Infra
#if 0 // ORIGIN VERSION
void
BLUtils::FillAllZero(WDL_TypedBuf<FLOAT_TYPE> *ioBuf)
{
    int bufferSize = ioBuf->GetSize();
    FLOAT_TYPE *bufferData = ioBuf->Get();
    
    for (int i = 0; i < bufferSize; i++)
        bufferData[i] = 0.0;
}
#else // OPTIMIZED
template <typename FLOAT_TYPE>
void
BLUtils::FillAllZero(WDL_TypedBuf<FLOAT_TYPE> *ioBuf)
{
    memset(ioBuf->Get(), 0, ioBuf->GetSize()*sizeof(FLOAT_TYPE));
}
template void BLUtils::FillAllZero(WDL_TypedBuf<float> *ioBuf);
template void BLUtils::FillAllZero(WDL_TypedBuf<double> *ioBuf);
#endif

template <typename FLOAT_TYPE>
void
BLUtils::FillZero(WDL_TypedBuf<FLOAT_TYPE> *ioBuf, int numZeros)
{
    if (numZeros > ioBuf->GetSize())
        numZeros = ioBuf->GetSize();
    
    memset(ioBuf->Get(), 0, numZeros*sizeof(FLOAT_TYPE));
}
template void BLUtils::FillZero(WDL_TypedBuf<float> *ioBuf, int numZeros);
template void BLUtils::FillZero(WDL_TypedBuf<double> *ioBuf, int numZeros);

// OPTIM PROF Infra
#if 0 // ORIGIN
void
BLUtils::FillAllZero(WDL_TypedBuf<int> *ioBuf)
{
    int bufferSize = ioBuf->GetSize();
    int *bufferData = ioBuf->Get();
    
    for (int i = 0; i < bufferSize; i++)
        bufferData[i] = 0;
}
#else // OPTIMIZED
void
BLUtils::FillAllZero(WDL_TypedBuf<int> *ioBuf)
{
    memset(ioBuf->Get(), 0, ioBuf->GetSize()*sizeof(int));
}
#endif

// OPTIM PROF Infra
#if 0 // ORIGIN
void
BLUtils::FillAllZero(WDL_TypedBuf<WDL_FFT_COMPLEX> *ioBuf)
{
    int bufferSize = ioBuf->GetSize();
    WDL_FFT_COMPLEX *bufferData = ioBuf->Get();
    
	for (int i = 0; i < bufferSize; i++)
	{
		bufferData[i].re = 0.0;
		bufferData[i].im = 0.0;
	}
}
#else // OPTIMIZED
void
BLUtils::FillAllZero(WDL_TypedBuf<WDL_FFT_COMPLEX> *ioBuf)
{
    memset(ioBuf->Get(), 0, ioBuf->GetSize()*sizeof(WDL_FFT_COMPLEX));
}
#endif

template <typename FLOAT_TYPE>
void
BLUtils::FillAllZero(FLOAT_TYPE *ioBuf, int size)
{
    memset(ioBuf, 0, size*sizeof(FLOAT_TYPE));
}
template void BLUtils::FillAllZero(float *ioBuf, int size);
template void BLUtils::FillAllZero(double *ioBuf, int size);

template <typename FLOAT_TYPE>
void
BLUtils::FillAllZero(vector<WDL_TypedBuf<FLOAT_TYPE> > *samples)
{
    for (int i = 0; i < samples->size(); i++)
    {
        FillAllZero(&(*samples)[i]);
    }
}
template void BLUtils::FillAllZero(vector<WDL_TypedBuf<float> > *samples);
template void BLUtils::FillAllZero(vector<WDL_TypedBuf<double> > *samples);

template <typename FLOAT_TYPE>
void
BLUtils::FillAllValue(WDL_TypedBuf<FLOAT_TYPE> *ioBuf, FLOAT_TYPE val)
{
    int bufferSize = ioBuf->GetSize();
    FLOAT_TYPE *bufData = ioBuf->Get();
    
#if USE_SIMD
    if (_useSimd && (bufferSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < bufferSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> r = simdpp::splat<simdpp::float64<SIMD_PACK_SIZE> >(val);
            
            simdpp::store(bufData, r);
            
            bufData += SIMD_PACK_SIZE;
        }
        
        // Finished
        return;
    }
#endif
    
    for (int i = 0; i < bufferSize; i++)
        bufData[i] = val;
}
template void BLUtils::FillAllValue(WDL_TypedBuf<float> *ioBuf, float val);
template void BLUtils::FillAllValue(WDL_TypedBuf<double> *ioBuf, double val);

template <typename FLOAT_TYPE>
void
BLUtils::AddZeros(WDL_TypedBuf<FLOAT_TYPE> *ioBuf, int size)
{
    int prevSize = ioBuf->GetSize();
    int newSize = prevSize + size;
    
    ioBuf->Resize(newSize);
    
    FLOAT_TYPE *bufData = ioBuf->Get();
    
    for (int i = prevSize; i < newSize; i++)
        bufData[i] = 0.0;
}
template void BLUtils::AddZeros(WDL_TypedBuf<float> *ioBuf, int size);
template void BLUtils::AddZeros(WDL_TypedBuf<double> *ioBuf, int size);

template <typename FLOAT_TYPE>
void
BLUtils::StereoToMono(WDL_TypedBuf<FLOAT_TYPE> *monoResult,
                    const FLOAT_TYPE *in0, const FLOAT_TYPE *in1, int nFrames)
{
    if ((in0 == NULL) && (in1 == NULL))
        return;
    
    monoResult->Resize(nFrames);
    
    // Crashes with App mode, when using sidechain
    if ((in0 != NULL) && (in1 != NULL))
    {
        FLOAT_TYPE *monoResultBuf = monoResult->Get();
        
#if !USE_SIMD_OPTIM
        for (int i = 0; i < nFrames; i++)
            monoResultBuf[i] = (in0[i] + in1[i])/2.0;
#else
        BLUtils::ComputeAvg(monoResultBuf, in0, in1, nFrames);
#endif
    }
    else
    {
        FLOAT_TYPE *monoResultBuf = monoResult->Get();
    
#if !USE_SIMD_OPTIM
        for (int i = 0; i < nFrames; i++)
            monoResultBuf[i] = (in0 != NULL) ? in0[i] : in1[i];
#else
        if (in0 != NULL)
            memcpy(monoResultBuf, in0, nFrames*sizeof(FLOAT_TYPE));
        else
        {
            if (in1 != NULL)
                memcpy(monoResultBuf, in1, nFrames*sizeof(FLOAT_TYPE));
        }
#endif
    }
}
template void BLUtils::StereoToMono(WDL_TypedBuf<float> *monoResult,
                                  const float *in0, const float *in1, int nFrames);
template void BLUtils::StereoToMono(WDL_TypedBuf<double> *monoResult,
                                  const double *in0, const double *in1, int nFrames);


template <typename FLOAT_TYPE>
void
BLUtils::StereoToMono(WDL_TypedBuf<FLOAT_TYPE> *monoResult,
                    const WDL_TypedBuf<FLOAT_TYPE> &in0,
                    const WDL_TypedBuf<FLOAT_TYPE> &in1)
{
    if ((in0.GetSize() == 0) && (in1.GetSize() == 0))
        return;
    
    monoResult->Resize(in0.GetSize());
    
    // Crashes with App mode, when using sidechain
    if ((in0.GetSize() > 0) && (in1.GetSize() > 0))
    {
        FLOAT_TYPE *monoResultBuf = monoResult->Get();
        int in0Size = in0.GetSize();
        FLOAT_TYPE *in0Data = in0.Get();
        FLOAT_TYPE *in1Data = in1.Get();
        
#if !USE_SIMD_OPTIM
        for (int i = 0; i < in0Size; i++)
            monoResultBuf[i] = (in0Data[i] + in1Data[i])*0.5;
#else
        BLUtils::ComputeAvg(monoResultBuf, in0Data, in1Data, in0Size);
#endif
    }
    else
    {
        FLOAT_TYPE *monoResultBuf = monoResult->Get();
        int in0Size = in0.GetSize();
        FLOAT_TYPE *in0Data = in0.Get();
        FLOAT_TYPE *in1Data = in1.Get();
    
#if !USE_SIMD_OPTIM
        for (int i = 0; i < in0Size; i++)
            monoResultBuf[i] = (in0Size > 0) ? in0Data[i] : in1Data[i];
#else
        if (in0Size > 0)
            memcpy(monoResultBuf, in0Data, in0Size*sizeof(FLOAT_TYPE));
        else
            memcpy(monoResultBuf, in1Data, in0Size*sizeof(FLOAT_TYPE));
#endif
    }
}
template void BLUtils::StereoToMono(WDL_TypedBuf<float> *monoResult,
                                  const WDL_TypedBuf<float> &in0,
                                  const WDL_TypedBuf<float> &in1);
template void BLUtils::StereoToMono(WDL_TypedBuf<double> *monoResult,
                                  const WDL_TypedBuf<double> &in0,
                                  const WDL_TypedBuf<double> &in1);


template <typename FLOAT_TYPE>
void
BLUtils::StereoToMono(WDL_TypedBuf<FLOAT_TYPE> *monoResult,
                    const vector< WDL_TypedBuf<FLOAT_TYPE> > &in0)
{
    if (in0.empty())
        return;
    
    if (in0.size() == 1)
        *monoResult = in0[0];
    
    if (in0.size() == 2)
    {
        StereoToMono(monoResult, in0[0], in0[1]);
    }
}
template void BLUtils::StereoToMono(WDL_TypedBuf<float> *monoResult,
                                  const vector< WDL_TypedBuf<float> > &in0);
template void BLUtils::StereoToMono(WDL_TypedBuf<double> *monoResult,
                                  const vector< WDL_TypedBuf<double> > &in0);


template <typename FLOAT_TYPE>
void
BLUtils::StereoToMono(vector<WDL_TypedBuf<FLOAT_TYPE> > *samplesVec)
{
    // First, set to bi-mono channels
    if (samplesVec->size() == 1)
    {
        // Duplicate mono channel
        samplesVec->push_back((*samplesVec)[0]);
    }
    else if (samplesVec->size() == 2)
    {
        // Set to mono
        WDL_TypedBuf<FLOAT_TYPE> mono;
        BLUtils::StereoToMono(&mono, (*samplesVec)[0], (*samplesVec)[1]);
        
        (*samplesVec)[0] = mono;
        (*samplesVec)[1] = mono;
    }
}
template void BLUtils::StereoToMono(vector<WDL_TypedBuf<float> > *samplesVec);
template void BLUtils::StereoToMono(vector<WDL_TypedBuf<double> > *samplesVec);

void
BLUtils::StereoToMono(WDL_TypedBuf<WDL_FFT_COMPLEX> *monoResult,
                      const vector< WDL_TypedBuf<WDL_FFT_COMPLEX> > &in0)
{
    if (in0.empty())
        return;
    
    if (in0.size() == 1)
    {
        *monoResult = in0[0];
        
        return;
    }
    
    monoResult->Resize(in0[0].GetSize());
    for (int i = 0; i < in0[0].GetSize(); i++)
    {
        monoResult->Get()[i].re = 0.5*(in0[0].Get()[i].re + in0[1].Get()[i].re);
        monoResult->Get()[i].im = 0.5*(in0[0].Get()[i].im + in0[1].Get()[i].im);
    }
}

// Compute the similarity between two normalized curves
template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeCurveMatchCoeff(const FLOAT_TYPE *curve0, const FLOAT_TYPE *curve1, int nFrames)
{
    // Use POW_COEFF, to make more difference between matching and out of matching
    // (added for EQHack)
#define POW_COEFF 4.0
    
    FLOAT_TYPE area = 0.0;
    for (int i = 0; i < nFrames; i++)
    {
        FLOAT_TYPE val0 = curve0[i];
        FLOAT_TYPE val1 = curve1[i];
        
        FLOAT_TYPE a = std::fabs(val0 - val1);
        
        a = std::pow(a, (FLOAT_TYPE)POW_COEFF);
        
        area += a;
    }
    
    FLOAT_TYPE coeff = area / nFrames;
    
    //
    coeff = std::pow(coeff, (FLOAT_TYPE)(1.0/POW_COEFF));
    
    // Should be Normalized
    
    // Clip, just in case.
    if (coeff < 0.0)
        coeff = 0.0;

    if (coeff > 1.0)
        coeff = 1.0;
    
    // If coeff is 1, this is a perfect match
    // so reverse
    coeff = 1.0 - coeff;
    
    return coeff;
}
template float BLUtils::ComputeCurveMatchCoeff(const float *curve0, const float *curve1, int nFrames);
template double BLUtils::ComputeCurveMatchCoeff(const double *curve0, const double *curve1, int nFrames);

template <typename FLOAT_TYPE>
void
BLUtils::AntiClipping(WDL_TypedBuf<FLOAT_TYPE> *values, FLOAT_TYPE maxValue)
{
    int valuesSize = values->GetSize();
    FLOAT_TYPE *valuesBuf = values->Get();
    
#if USE_SIMD
    if (_useSimd && (valuesSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < valuesSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(valuesBuf);
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::load_splat(&maxValue);
            
            simdpp::float64<SIMD_PACK_SIZE> r = simdpp::min(v0, v1);
            
            simdpp::store(valuesBuf, r);
            
            valuesBuf += SIMD_PACK_SIZE;
        }
        
        // Finished
        return;
    }
#endif
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE val = valuesBuf[i];
        
        if (val > maxValue)
            val = maxValue;
        
        valuesBuf[i] = val;
    }
}
template void BLUtils::AntiClipping(WDL_TypedBuf<float> *values, float maxValue);
template void BLUtils::AntiClipping(WDL_TypedBuf<double> *values, double maxValue);

template <typename FLOAT_TYPE>
void
BLUtils::SamplesAntiClipping(WDL_TypedBuf<FLOAT_TYPE> *samples, FLOAT_TYPE maxValue)
{
    int samplesSize = samples->GetSize();
    FLOAT_TYPE *samplesData = samples->Get();
    
#if USE_SIMD
    if (_useSimd && (samplesSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < samplesSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(samplesData);
            simdpp::float64<SIMD_PACK_SIZE> m0 = simdpp::load_splat(&maxValue);
            
            FLOAT_TYPE minValue = -maxValue;
            simdpp::float64<SIMD_PACK_SIZE> m1 = simdpp::load_splat(&minValue);
            
            simdpp::float64<SIMD_PACK_SIZE> r0 = simdpp::min(v0, m0);
            simdpp::float64<SIMD_PACK_SIZE> r = simdpp::max(r0, m1);
            
            simdpp::store(samplesData, r);
            
            samplesData += SIMD_PACK_SIZE;
        }
        
        // Finished
        return;
    }
#endif
    
    for (int i = 0; i < samplesSize; i++)
    {
        FLOAT_TYPE val = samplesData[i];
        
        if (val > maxValue)
            val = maxValue;
        
        if (val < -maxValue)
            val = -maxValue;
        
        samplesData[i] = val;
    }
}
template void BLUtils::SamplesAntiClipping(WDL_TypedBuf<float> *samples, float maxValue);
template void BLUtils::SamplesAntiClipping(WDL_TypedBuf<double> *samples, double maxValue);

void
BLUtils::BypassPlug(double **inputs, double **outputs, int nFrames)
{
    if ((inputs[0] != NULL) && (outputs[0] != NULL))
        memcpy(outputs[0], inputs[0], nFrames*sizeof(double));

    if ((inputs[1] != NULL) && (outputs[1] != NULL))
        memcpy(outputs[1], inputs[1], nFrames*sizeof(double));
}

// #bl-iplug2: ok
#if 0
void
BLUtils::GetPlugIOBuffers(Plugin *plug, double **inputs, double **outputs,
                          double *in[2], double *scIn[2], double *out[2])
{
    int numInChannels = plug->NInChannels();
    int numInScChannels = plug->NInScChannels();
    
#ifdef AAX_API
    // Protools only takes one channel for side chain
    
    // force it to 1, just in case
    if (numInScChannels > 1)
        numInScChannels = 1;
#endif
    
    bool isInConnected[4] = { false, false, false, false };
    
    for (int i = 0; i < numInChannels; i++)
    {
        isInConnected[i] = plug->IsInChannelConnected(i);
    }
    
    // in
    in[0] = ((numInChannels - numInScChannels > 0) &&
	     isInConnected[0]) ? inputs[0] : NULL;
    in[1] = ((numInChannels - numInScChannels >  1) &&
	     isInConnected[1]) ? inputs[1] : NULL;
    
#ifndef APP_API
    // scin
    scIn[0] = ((numInScChannels > 0) && isInConnected[numInChannels - numInScChannels]) ?
                inputs[numInChannels - numInScChannels] : NULL;
    scIn[1] = ((numInScChannels > 1) && isInConnected[numInChannels - numInScChannels + 1]) ?
                inputs[numInChannels - numInScChannels + 1] : NULL;
#else
    // When in application mode, must deactivate sidechains
    // BUG: otherwise it crashes if we try to get sidechains
    scIn[0] = NULL;
    scIn[1] = NULL;
#endif
    
    // out
    out[0] = plug->IsOutChannelConnected(0) ? outputs[0] : NULL;
    out[1] = plug->IsOutChannelConnected(1) ? outputs[1] : NULL;
}
#endif

//#bl-iplug2
void
BLUtils::GetPlugIOBuffers(Plugin *plug, double **inputs, double **outputs,
                          double *in[2], double *scIn[2], double *out[2])
{
    // TODO: manage sidechanin
    // And also chacke AAX_API
    for (int i = 0; i < 2; i++)
        scIn[i] = NULL;
    
    // Inputs
    for (int i = 0; i < 2; i++)
    {
        in[i] = NULL;
        if (plug->IsChannelConnected(kInput, i) &&
             (inputs[i] != NULL))
        {
            in[i] = inputs[i];
        }
    }
            
    // Outputs
    for (int i = 0; i < 2; i++)
    {
        out[i] = NULL;
        
        if (plug->IsChannelConnected(kOutput, i) &&
            (outputs[i] != NULL))
        {
            out[i] = outputs[i];
        }
    }
}

//#bl-iplug2
void
BLUtils::GetPlugIOBuffers(Plugin *plug,
                          double **inputs, double **outputs, int nFrames,
                          vector<WDL_TypedBuf<BL_FLOAT> > *inp,
                          vector<WDL_TypedBuf<BL_FLOAT> > *scIn,
                          vector<WDL_TypedBuf<BL_FLOAT> > *outp)
{
    // TODO: manage sidechain
    // And also check side chain AAX_API
    scIn->resize(0);
    
    // Inputs
    int numInChannelsConnected = plug->NInChansConnected();
    
    // #bl-iplug2 HACK
    if (numInChannelsConnected > 2)
        numInChannelsConnected = 2;
    
    inp->resize(numInChannelsConnected);
    for (int i = 0; i < inp->size(); i++)
    {
        if (plug->IsChannelConnected(kInput, i))
        {
            WDL_TypedBuf<BL_FLOAT> buf;
#if !BL_TYPE_FLOAT
            buf.Add(inputs[i], nFrames);
            
#else
            //BLUtils::ConvertToFloatType(&buf, inputs[i]);
            buf.Resize(nFrames);
            for (int j = 0; j < nFrames; j++)
                buf.Get()[j] = inputs[i][j];
#endif
            
            (*inp)[i] = buf;
        }
    }
    
    // Outputs
    int numOutChannelsConnected = plug->NOutChansConnected();
    outp->resize(numOutChannelsConnected);
    for (int i = 0; i < outp->size(); i++)
    {
        if (plug->IsChannelConnected(kOutput, i))
        {
            WDL_TypedBuf<BL_FLOAT> buf;
            
#if !BL_TYPE_FLOAT
            buf.Add(outputs[i], nFrames);
#else
            buf.Resize(nFrames);
            for (int j = 0; j < nFrames; j++)
                buf.Get()[j] = outputs[i][j];
#endif
            
            (*outp)[i] = buf;
        }
    }
}

// #bl-iplug2: ok
#if 0
void
BLUtils::GetPlugIOBuffers(Plugin *plug,
                        double **inputs, double **outputs, int nFrames,
                        vector<WDL_TypedBuf<BL_FLOAT> > *inp,
                        vector<WDL_TypedBuf<BL_FLOAT> > *scIn,
                        vector<WDL_TypedBuf<BL_FLOAT> > *outp)
{
#define FIX_NUMEROUS_SC_CHANNELS 1
#define MAX_IN_CHANNELS 256
    
    int numInChannels = plug->NInChannels();
    int numInScChannels = plug->NInScChannels();
    
#if FIX_NUMEROUS_SC_CHANNELS
    // Avoid a crach
    if (numInChannels > MAX_IN_CHANNELS)
        return;
#endif
    
#ifdef AAX_API
    // Protools only takes one channel for side chain
    
    // force it to 1, just in case
    if (numInScChannels > 1)
        numInScChannels = 1;
#endif
    
#if !FIX_NUMEROUS_SC_CHANNELS
    bool isInConnected[4] = { false, false, false, false };
#else
    bool isInConnected[MAX_IN_CHANNELS];
    for (int i = 0; i < MAX_IN_CHANNELS; i++)
    {
        isInConnected[i] = false;
    }
#endif
    
    for (int i = 0; i < numInChannels; i++)
    {
        isInConnected[i] = plug->IsInChannelConnected(i);
    }
    
    // in
    int numInputs = numInChannels - numInScChannels;
    
    if ((numInputs > 0) && isInConnected[0])
    {
        // FIX: Logic High Sierra crash when start playing (for plugs with sidechain)
        if (inputs[0] != NULL)
        {
            WDL_TypedBuf<BL_FLOAT> input;
            input.Resize(nFrames);
            
            if (sizeof(BL_FLOAT) == sizeof(double))
                input.Set(((BL_FLOAT **)inputs)[0], nFrames);
            else
            {
                for (int i = 0; i < nFrames; i++)
                    input.Get()[i] = inputs[0][i];
            }
            
            inp->push_back(input);
        }
    }
    
    if ((numInputs > 1) && isInConnected[1])
    {
        // FIX: Logic High Sierra crash when start playing (for plugs with sidechain)
        if (inputs[1] != NULL)
        {
            WDL_TypedBuf<BL_FLOAT> input;
            input.Resize(nFrames);
            
            if (sizeof(BL_FLOAT) == sizeof(double))
                input.Set(((BL_FLOAT **)inputs)[1], nFrames);
            else
            {
                for (int i = 0; i < nFrames; i++)
                    input.Get()[i] = inputs[1][i];
            }
            
            inp->push_back(input);
        }
    }

    // When in application mode, must deactivate sidechains
    // BUG: otherwise it crashes if we try to get sidechains
#ifndef APP_API
    // scin
#if !FIX_NUMEROUS_SC_CHANNELS
    if ((numInScChannels > 0) && isInConnected[numInChannels - numInScChannels])
    {
        // FIX: Logic High Sierra crash when start playing (for plugs with sidechain)
        if (inputs[numInChannels - numInScChannels] != NULL)
        {
            WDL_TypedBuf<BL_FLOAT> sc;
            sc.Resize(nFrames);
            
            if (sizeof(BL_FLOAT) == sizeof(double))
                sc.Set(inputs[numInChannels - numInScChannels], nFrames);
            {
                for (int i = 0; i < nFrames; i++)
                    sc.Get()[i] = inputs[numInChannels - numInScChannels][i];
            }
            
            scIn->push_back(sc);
        }
    }
    
    if ((numInScChannels > 1) && isInConnected[numInChannels - numInScChannels + 1])
    {
        // FIX: Logic High Sierra crash when start playing (for plugs with sidechain)
        if (inputs[numInChannels - numInScChannels + 1] != NULL)
        {
            WDL_TypedBuf<BL_FLOAT> sc;
            sc.Resize(nFrames);
 
            if (sizeof(BL_FLOAT) == sizeof(double))
                sc.Set(inputs[numInChannels - numInScChannels + 1], nFrames);
            else
            {
                for (int i = 0; i < nFrames; i++)
                    sc.Get()[i] = inputs[numInChannels - numInScChannels + 1][i];
            }
            
            scIn->push_back(sc);
        }
    }
#else
    for (int i = 0; i < MAX_IN_CHANNELS; i++)
    {
        if (numInChannels - numInScChannels + i >= MAX_IN_CHANNELS)
            break;
        
        if ((numInScChannels > i) && isInConnected[numInChannels - numInScChannels + i])
        {
            // FIX: Logic High Sierra crash when start playing (for plugs with sidechain)
            if (inputs[numInChannels - numInScChannels + i] != NULL)
            {
                WDL_TypedBuf<BL_FLOAT> sc;
                sc.Resize(nFrames);
                
                if (sizeof(BL_FLOAT) == sizeof(double))
                    sc.Set(((BL_FLOAT **)inputs)[numInChannels - numInScChannels + i], nFrames);
                else
                {
                    for (int j = 0; j < nFrames; j++)
                        sc.Get()[j] = inputs[numInChannels - numInScChannels + i][j];
                }
                
                scIn->push_back(sc);
            }
        }
    }
#endif
    
#endif
    
    // #bl-iplug2
#if 0
    
    // out
    if (plug->IsOutChannelConnected(0))
    {
        // FIX: Logic High Sierra crash when start playing (for plugs with sidechain)
        if (outputs[0] != NULL)
        {
            WDL_TypedBuf<BL_FLOAT> out;
            out.Resize(nFrames);
 
            //out.Set(outputs[0], nFrames);
            for (int i = 0; i < nFrames; i++)
                out.Get()[i] = outputs[0][i];
            
            outp->push_back(out);
        }
    }
    
    if (plug->IsOutChannelConnected(1))
    {
        // FIX: Logic High Sierra crash when start playing (for plugs with sidechain)
        if (outputs[1] != NULL)
        {
            WDL_TypedBuf<BL_FLOAT> out;
            out.Resize(nFrames);
            
            if (sizeof(BL_FLOAT) == sizeof(double))
                out.Set(((BL_FLOAT **)outputs)[1], nFrames);
            else
            {
                for (int i = 0; i < nFrames; i++)
                    out.Get()[i] = outputs[1][i];
            }
            
            outp->push_back(out);
        }
    }
#endif
    
#if !TRY_FIX_SIDE_CHAIN_AU
    // Set inputs and outputs to NULL if necessary
    // (will avoid later crashes)
    if (inp->size() == 1)
        inputs[1] = NULL;
    
    if (outp->size() == 1)
        outputs[1] = NULL;
#else
    // Try to make something more smart...
    
    for (int i = 0; i < numInChannels; i++)
    {
        if (inp->size() <= i)
            inputs[i] = NULL;
    }
    
    for (int i = 0; i < numInChannels; i++)
    {
        if (scIn->size() <= i)
            inputs[numInChannels + i] = NULL;
    }
   
    // #bl-iplug2
#if 0
    
    int numOutChannels = plug->NOutChannels();
    for (int i = 0; i < numOutChannels; i++)
    {
        if (outp->size() <= i)
            outputs[i] = NULL;
    }
#endif
    
#endif
}
#endif

bool
BLUtils::GetIOBuffers(int index, double *in[2], double *out[2],
                    double **inBuf, double **outBuf)
{
    *inBuf = NULL;
    *outBuf = NULL;
    
    if (out[index] != NULL)
        // We want to ouput
    {
        *outBuf = out[index];
        
        *inBuf = in[index];
        if (*inBuf == NULL)
        {
            // We have only one input
            // So take it, this is the first one
            *inBuf = in[0];
        }
        
        if (*inBuf != NULL)
            // We have both buffers
            return true;
    }
    
    // We have either no buffer, or only one out of two
    return false;
}

bool
BLUtils::GetIOBuffers(int index,
                    vector<WDL_TypedBuf<double> > &in,
                    vector<WDL_TypedBuf<double> > &out,
                    double **inBuf, double **outBuf)
{
    *inBuf = NULL;
    *outBuf = NULL;
    
    if (out.size() > index)
        // We want to ouput
    {
        *outBuf = out[index].Get();
        
        *inBuf = NULL;
        if (in.size() > index)
            *inBuf = in[index].Get();
        else
            *inBuf = in[0].Get();
        
        if (*inBuf != NULL)
            // We have both buffers
            return true;
    }
    
    // We have either no buffer, or only one out of two
    return false;
}

bool
BLUtils::GetIOBuffers(int index,
                    vector<WDL_TypedBuf<double> > &in,
                    vector<WDL_TypedBuf<double> > &out,
                    WDL_TypedBuf<double> **inBuf,
                    WDL_TypedBuf<double> **outBuf)
{
    *inBuf = NULL;
    *outBuf = NULL;
    
    if (out.size() > index)
        // We want to ouput
    {
        *outBuf = &out[index];
        
        *inBuf = NULL;
        if (in.size() > index)
            *inBuf = &in[index];
        else
            *inBuf = &in[0];
        
        if (*inBuf != NULL)
            // We have both buffers
            return true;
    }
    
    // We have either no buffer, or only one out of two
    return false;
}

bool
BLUtils::PlugIOAllZero(double *inputs[2], double *outputs[2], int nFrames)
{
    bool allZero0 = false;
    bool channelDefined0 = ((inputs[0] != NULL) && (outputs[0] != NULL));
    if (channelDefined0)
    {
        allZero0 = (BLUtils::IsAllZero(inputs[0], nFrames) &&
                    BLUtils::IsAllZero(outputs[0], nFrames));
    }
    
    bool allZero1 = false;
    bool channelDefined1 = ((inputs[1] != NULL) && (outputs[1] != NULL));
    if (channelDefined1)
    {
        allZero1 = (BLUtils::IsAllZero(inputs[1], nFrames) &&
                    BLUtils::IsAllZero(outputs[1], nFrames));
    }
    
    if (!channelDefined1 && allZero0)
        return true;
    
    if (channelDefined1 && allZero0 && allZero1)
        return true;
    
    return false;
}

bool
BLUtils::PlugIOAllZero(const vector<WDL_TypedBuf<BL_FLOAT> > &inputs,
                     const vector<WDL_TypedBuf<BL_FLOAT> > &outputs)
{
    bool allZero0 = false;
    bool channelDefined0 = ((inputs.size() > 0) && (outputs.size() > 0));
    if (channelDefined0)
    {
        allZero0 = (BLUtils::IsAllZero(inputs[0].Get(), inputs[0].GetSize()) &&
                    BLUtils::IsAllZero(outputs[0].Get(), outputs[0].GetSize()));
    }
    
    bool allZero1 = false;
    bool channelDefined1 = ((inputs.size() > 1) && (outputs.size() > 1));
    if (channelDefined1)
    {
        allZero1 = (BLUtils::IsAllZero(inputs[1].Get(), inputs[1].GetSize()) &&
                    BLUtils::IsAllZero(outputs[1].Get(), outputs[1].GetSize()));
    }
    
    if (!channelDefined1 && allZero0)
        return true;
    
    if (channelDefined1 && allZero0 && allZero1)
        return true;
    
    return false;
}

void
BLUtils::PlugCopyOutputs(const vector<WDL_TypedBuf<BL_FLOAT> > &outp,
                         double **outputs, int nFrames)
{
    for (int i = 0; i < outp.size(); i++)
    {
        if (outputs[i] == NULL)
            continue;
     
        const WDL_TypedBuf<BL_FLOAT> &out = outp[i];
        
        if (out.GetSize() == nFrames)
        {
            if (sizeof(BL_FLOAT) == sizeof(double))
                memcpy(outputs[i], out.Get(), nFrames*sizeof(double));
            else
            {
                for (int j = 0; j < nFrames; j++)
                    outputs[i][j] = out.Get()[j];
            }
        }
    }
}

int
BLUtils::PlugComputeBufferSize(int bufferSize, BL_FLOAT sampleRate)
{
    double ratio = sampleRate/44100.0;
    ratio = bl_round(ratio);
    
    // FIX: Logic Auval checks for 11025 sample rate
    // So ratio would be 0.
    if (ratio < 1.0)
        ratio = 1.0;
    
    int result = bufferSize*ratio;
    
    return result;
}

// Fails sometimes...
int
BLUtils::PlugComputeLatency(Plugin *plug,
                          int nativeBufferSize, int nativeLatency,
                          BL_FLOAT sampleRate)
{
#define NATIVE_SAMPLE_RATE 44100.0
    
    // How many blocks for filling BUFFER_SIZE ?
    int blockSize = plug->GetBlockSize();
    BL_FLOAT coeff = sampleRate/NATIVE_SAMPLE_RATE;
    
    // FIX: for 48KHz and multiples
    coeff = bl_round(coeff);
    
    BL_FLOAT numBuffers = coeff*((BL_FLOAT)nativeBufferSize)/blockSize;
    if (numBuffers > (int)numBuffers)
        numBuffers = (int)numBuffers + 1;
    
    // GOOD !
    // Compute remaining, in order to compensate for
    // remaining compensation in FftProcessObj15
    BL_FLOAT remaining = numBuffers*blockSize/coeff - nativeBufferSize;
    
    BL_FLOAT newLatency = numBuffers*blockSize - (int)remaining;
    
    return newLatency;
}

// Fails sometimes...
void
BLUtils::PlugUpdateLatency(Plugin *plug,
                         int nativeBufferSize, int nativeLatency,
                         BL_FLOAT sampleRate)
{
    if (std::fabs((BL_FLOAT)(sampleRate - NATIVE_SAMPLE_RATE)) < BL_EPS)
        // We are in the native state, no need to tweek latency
    {
        plug->SetLatency(nativeLatency);
        
        return;
    }

    // Fails sometimes...
    int newLatency = BLUtils::PlugComputeLatency(plug,
                                               nativeBufferSize, nativeLatency,
                                               sampleRate);

    // Set latency dynamically
    // (not sure it works for all hosts)
    plug->SetLatency(newLatency);
}

BL_FLOAT
BLUtils::GetBufferSizeCoeff(Plugin *plug, int nativeBufferSize)
{
    BL_FLOAT sampleRate = plug->GetSampleRate();
    int bufferSize = BLUtils::PlugComputeBufferSize(nativeBufferSize, sampleRate);
    BL_FLOAT bufferSizeCoeff = ((BL_FLOAT)bufferSize)/nativeBufferSize;
    
    return bufferSizeCoeff;
}

bool
BLUtils::ChannelAllZero(const vector<WDL_TypedBuf<BL_FLOAT> > &channel)
{
    for (int i = 0; i < channel.size(); i++)
    {
        bool allZero = BLUtils::IsAllZero(channel[i]);
        if (!allZero)
            return false;
    }
    
    return true;
}

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::FftBinToFreq(int binNum, int numBins, FLOAT_TYPE sampleRate)
{
    if (binNum > numBins/2)
        // Second half => not relevant
        return -1.0;
    
    // Problem here ?
    return binNum*sampleRate/(numBins /*2.0*/); // Modif for Zarlino
}
template float BLUtils::FftBinToFreq(int binNum, int numBins, float sampleRate);
template double BLUtils::FftBinToFreq(int binNum, int numBins, double sampleRate);

// Fixed version
// In the case we want to fill a BUFFER_SIZE/2 array, as it should be
// (for stereo phase correction)
template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::FftBinToFreq2(int binNum, int numBins, FLOAT_TYPE sampleRate)
{
    if (binNum > numBins)
        // Second half => not relevant
        return -1.0;
    
    // Problem here ?
    return binNum*sampleRate/(numBins*2.0);
}
template float BLUtils::FftBinToFreq2(int binNum, int numBins, float sampleRate);
template double BLUtils::FftBinToFreq2(int binNum, int numBins, double sampleRate);

// This version may be false
template <typename FLOAT_TYPE>
int
BLUtils::FreqToFftBin(FLOAT_TYPE freq, int numBins, FLOAT_TYPE sampleRate, FLOAT_TYPE *t)
{
    FLOAT_TYPE fftBin = (freq*numBins/*/2.0*/)/sampleRate; // Modif for Zarlino
    
    // Round with 1e-10 precision
    // This is necessary otherwise we will take the wrong
    // bin when there are rounding errors like "80.999999999"
    fftBin = BLUtils::Round(fftBin, 10);
    
    if (t != NULL)
    {
        FLOAT_TYPE freq0 = FftBinToFreq(fftBin, numBins, sampleRate);
        FLOAT_TYPE freq1 = FftBinToFreq(fftBin + 1, numBins, sampleRate);
        
        *t = (freq - freq0)/(freq1 - freq0);
    }
    
    return fftBin;
}
template int BLUtils::FreqToFftBin(float freq, int numBins, float sampleRate, float *t);
template int BLUtils::FreqToFftBin(double freq, int numBins, double sampleRate, double *t);

template <typename FLOAT_TYPE>
void
BLUtils::FftFreqs(WDL_TypedBuf<FLOAT_TYPE> *freqs, int numBins, FLOAT_TYPE sampleRate)
{
    freqs->Resize(numBins);
    FLOAT_TYPE *freqsData = freqs->Get();
    
    for (int i = 0; i < numBins; i++)
    {
        FLOAT_TYPE freq = FftBinToFreq2(i, numBins, sampleRate);
        
        freqsData[i] = freq;
    }
}
template void BLUtils::FftFreqs(WDL_TypedBuf<float> *freqs, int numBins, float sampleRate);
template void BLUtils::FftFreqs(WDL_TypedBuf<double> *freqs, int numBins, double sampleRate);

template <typename FLOAT_TYPE>
void
BLUtils::MinMaxFftBinFreq(FLOAT_TYPE *minFreq, FLOAT_TYPE *maxFreq, int numBins, FLOAT_TYPE sampleRate)
{
    *minFreq = sampleRate/(numBins/2.0);
    *maxFreq = ((FLOAT_TYPE)(numBins/2.0 - 1.0)*sampleRate)/numBins;
}
template void BLUtils::MinMaxFftBinFreq(float *minFreq, float *maxFreq, int numBins, float sampleRate);
template void BLUtils::MinMaxFftBinFreq(double *minFreq, double *maxFreq, int numBins, double sampleRate);

template <typename FLOAT_TYPE>
void
BLUtils::ComplexToMagnPhase(WDL_FFT_COMPLEX comp, FLOAT_TYPE *outMagn, FLOAT_TYPE *outPhase)
{
    *outMagn = COMP_MAGN(comp);
    
    *outPhase = std::atan2(comp.im, comp.re);
}
template void BLUtils::ComplexToMagnPhase(WDL_FFT_COMPLEX comp, float *outMagn, float *outPhase);
template void BLUtils::ComplexToMagnPhase(WDL_FFT_COMPLEX comp, double *outMagn, double *outPhase);

template <typename FLOAT_TYPE>
void
BLUtils::MagnPhaseToComplex(WDL_FFT_COMPLEX *outComp, FLOAT_TYPE magn, FLOAT_TYPE phase)
{
    WDL_FFT_COMPLEX comp;
    comp.re = magn*std::cos(phase);
    comp.im = magn*std::sin(phase);
    
    *outComp = comp;
}
template void BLUtils::MagnPhaseToComplex(WDL_FFT_COMPLEX *outComp, float magn, float phase);
template void BLUtils::MagnPhaseToComplex(WDL_FFT_COMPLEX *outComp, double magn, double phase);

template <typename FLOAT_TYPE>
void
BLUtils::ComplexToMagn(WDL_TypedBuf<FLOAT_TYPE> *result, const WDL_TypedBuf<WDL_FFT_COMPLEX> &complexBuf)
{
    result->Resize(complexBuf.GetSize());
    
    int complexBufSize = complexBuf.GetSize();
    WDL_FFT_COMPLEX *complexBufData = complexBuf.Get();
    FLOAT_TYPE *resultData = result->Get();
    
    for (int i = 0; i < complexBufSize; i++)
    {
        FLOAT_TYPE magn = COMP_MAGN(complexBufData[i]);
        resultData[i] = magn;
    }
}
template void BLUtils::ComplexToMagn(WDL_TypedBuf<float> *result, const WDL_TypedBuf<WDL_FFT_COMPLEX> &complexBuf);
template void BLUtils::ComplexToMagn(WDL_TypedBuf<double> *result, const WDL_TypedBuf<WDL_FFT_COMPLEX> &complexBuf);

template <typename FLOAT_TYPE>
void
BLUtils::ComplexToPhase(WDL_TypedBuf<FLOAT_TYPE> *result,
                      const WDL_TypedBuf<WDL_FFT_COMPLEX> &complexBuf)
{
    result->Resize(complexBuf.GetSize());
    
    int complexBufSize = complexBuf.GetSize();
    WDL_FFT_COMPLEX *complexBufData = complexBuf.Get();
    FLOAT_TYPE *resultData = result->Get();
    
    for (int i = 0; i < complexBufSize; i++)
    {
        FLOAT_TYPE phase = COMP_PHASE(complexBufData[i]);
        resultData[i] = phase;
    }
}
template void BLUtils::ComplexToPhase(WDL_TypedBuf<float> *result,
                           const WDL_TypedBuf<WDL_FFT_COMPLEX> &complexBuf);
template void BLUtils::ComplexToPhase(WDL_TypedBuf<double> *result,
                           const WDL_TypedBuf<WDL_FFT_COMPLEX> &complexBuf);

template <typename FLOAT_TYPE>
void
BLUtils::ComplexToMagnPhase(WDL_TypedBuf<FLOAT_TYPE> *resultMagn,
                          WDL_TypedBuf<FLOAT_TYPE> *resultPhase,
                          const WDL_TypedBuf<WDL_FFT_COMPLEX> &complexBuf)
{
    resultMagn->Resize(complexBuf.GetSize());
    resultPhase->Resize(complexBuf.GetSize());
    
    int complexBufSize = complexBuf.GetSize();
    WDL_FFT_COMPLEX *complexBufData = complexBuf.Get();
    FLOAT_TYPE *resultMagnData = resultMagn->Get();
    FLOAT_TYPE *resultPhaseData = resultPhase->Get();
    
    for (int i = 0; i < complexBufSize; i++)
    {
        FLOAT_TYPE magn = COMP_MAGN(complexBufData[i]);
        resultMagnData[i] = magn;
        
#if 1
        FLOAT_TYPE phase = std::atan2(complexBufData[i].im, complexBufData[i].re);
#endif
        
#if 0 // Make some leaks with diracs
        FLOAT_TYPE phase = DomainAtan2(complexBufData[i].im, complexBufData[i].re);
#endif
        resultPhaseData[i] = phase;
    }
}
template void BLUtils::ComplexToMagnPhase(WDL_TypedBuf<float> *resultMagn,
                               WDL_TypedBuf<float> *resultPhase,
                               const WDL_TypedBuf<WDL_FFT_COMPLEX> &complexBuf);
template void BLUtils::ComplexToMagnPhase(WDL_TypedBuf<double> *resultMagn,
                               WDL_TypedBuf<double> *resultPhase,
                               const WDL_TypedBuf<WDL_FFT_COMPLEX> &complexBuf);


template <typename FLOAT_TYPE>
void
BLUtils::MagnPhaseToComplex(WDL_TypedBuf<WDL_FFT_COMPLEX> *complexBuf,
                          const WDL_TypedBuf<FLOAT_TYPE> &magns,
                          const WDL_TypedBuf<FLOAT_TYPE> &phases)
{
    //complexBuf->Resize(0);
    
    if (magns.GetSize() != phases.GetSize())
        // Error
        return;
    
    complexBuf->Resize(magns.GetSize());
    
    int magnsSize = magns.GetSize();
    FLOAT_TYPE *magnsData = magns.Get();
    FLOAT_TYPE *phasesData = phases.Get();
    WDL_FFT_COMPLEX *complexBufData = complexBuf->Get();
    
    for (int i = 0; i < magnsSize; i++)
    {
        FLOAT_TYPE magn = magnsData[i];
        FLOAT_TYPE phase = phasesData[i];
        
        WDL_FFT_COMPLEX res;
        res.re = magn*std::cos(phase);
        res.im = magn*std::sin(phase);
        
        complexBufData[i] = res;
    }
}
template void BLUtils::MagnPhaseToComplex(WDL_TypedBuf<WDL_FFT_COMPLEX> *complexBuf,
                               const WDL_TypedBuf<float> &magns,
                               const WDL_TypedBuf<float> &phases);
template void BLUtils::MagnPhaseToComplex(WDL_TypedBuf<WDL_FFT_COMPLEX> *complexBuf,
                               const WDL_TypedBuf<double> &magns,
                               const WDL_TypedBuf<double> &phases);


template <typename FLOAT_TYPE>
void
BLUtils::NormalizeFftValues(WDL_TypedBuf<FLOAT_TYPE> *magns)
{
    FLOAT_TYPE sum = 0.0;
    
    int magnsSize = magns->GetSize();
    FLOAT_TYPE *magnsData = magns->Get();
    
#if !USE_SIMD_OPTIM
    // Not test "/2"
    for (int i = 1; i < magnsSize/*/2*/; i++)
    {
        FLOAT_TYPE magn = magnsData[i];
        
        sum += magn;
    }
#else
    sum = BLUtils::ComputeSum(magnsData, magnsSize);
    if (magnsSize > 0)
        sum -= magnsData[0];
#endif
    
    sum /= magns->GetSize()/*/2*/ - 1;
    
    magns->Get()[0] = sum;
    magns->Get()[magns->GetSize() - 1] = sum;
}
template void BLUtils::NormalizeFftValues(WDL_TypedBuf<float> *magns);
template void BLUtils::NormalizeFftValues(WDL_TypedBuf<double> *magns);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::Round(FLOAT_TYPE val, int precision)
{
    val = val*std::pow((FLOAT_TYPE)10.0, precision);
    val = bl_round(val);
    //val *= std::pow(10, -precision);
    val *= std::pow((FLOAT_TYPE)10.0, -precision);
    
    return val;
}
template float BLUtils::Round(float val, int precision);
template double BLUtils::Round(double val, int precision);

template <typename FLOAT_TYPE>
void
BLUtils::Round(FLOAT_TYPE *buf, int nFrames, int precision)
{
    for (int i = 0; i < nFrames; i++)
    {
        FLOAT_TYPE val = buf[i];
        FLOAT_TYPE res = BLUtils::Round(val, precision);
        
        buf[i] = res;
    }
}
template void BLUtils::Round(float *buf, int nFrames, int precision);
template void BLUtils::Round(double *buf, int nFrames, int precision);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::DomainAtan2(FLOAT_TYPE x, FLOAT_TYPE y)
{
    FLOAT_TYPE signx;
    if (x > 0.) signx = 1.;
    else signx = -1.;
    
    if (x == 0.) return 0.;
    if (y == 0.) return signx * M_PI / 2.;
    
    return std::atan2(x, y);
}
template float BLUtils::DomainAtan2(float x, float y);
template double BLUtils::DomainAtan2(double x, double y);

template <typename FLOAT_TYPE>
void
BLUtils::AppendValues(WDL_TypedBuf<FLOAT_TYPE> *ioBuffer, const WDL_TypedBuf<FLOAT_TYPE> &values)
{
    if (ioBuffer->GetSize() == 0)
    {
        *ioBuffer = values;
        return;
    }
    
    ioBuffer->Add(values.Get(), values.GetSize());
}
template void BLUtils::AppendValues(WDL_TypedBuf<float> *ioBuffer, const WDL_TypedBuf<float> &values);
template void BLUtils::AppendValues(WDL_TypedBuf<double> *ioBuffer, const WDL_TypedBuf<double> &values);

#if 0 // ORIG
template <typename FLOAT_TYPE>
void
BLUtils::ConsumeLeft(WDL_TypedBuf<FLOAT_TYPE> *ioBuffer, int numToConsume)
{
    int newSize = ioBuffer->GetSize() - numToConsume;
    if (newSize <= 0)
    {
        ioBuffer->Resize(0);
        
        return;
    }
    
    // Resize down, skipping left
    WDL_TypedBuf<FLOAT_TYPE> tmpChunk;
    tmpChunk.Add(&ioBuffer->Get()[numToConsume], newSize);
    *ioBuffer = tmpChunk;
}
template void BLUtils::ConsumeLeft(WDL_TypedBuf<float> *ioBuffer, int numToConsume);
template void BLUtils::ConsumeLeft(WDL_TypedBuf<double> *ioBuffer, int numToConsume);
#endif

#if 1 // OPTIM
template <typename FLOAT_TYPE>
void
BLUtils::ConsumeLeft(WDL_TypedBuf<FLOAT_TYPE> *ioBuffer, int numToConsume)
{
    int newSize = ioBuffer->GetSize() - numToConsume;
    if (newSize <= 0)
    {
        ioBuffer->Resize(0);
        
        return;
    }
    
    memcpy(ioBuffer->Get(), &ioBuffer->Get()[numToConsume], newSize*sizeof(FLOAT_TYPE));
    ioBuffer->Resize(newSize);
}
template void BLUtils::ConsumeLeft(WDL_TypedBuf<float> *ioBuffer, int numToConsume);
template void BLUtils::ConsumeLeft(WDL_TypedBuf<double> *ioBuffer, int numToConsume);
#endif

template <typename FLOAT_TYPE>
void
BLUtils::ConsumeRight(WDL_TypedBuf<FLOAT_TYPE> *ioBuffer, int numToConsume)
{
    int size = ioBuffer->GetSize();
    
    int newSize = size - numToConsume;
    if (newSize < 0)
        newSize = 0;
    
    ioBuffer->Resize(newSize);
}
template void BLUtils::ConsumeRight(WDL_TypedBuf<float> *ioBuffer, int numToConsume);
template void BLUtils::ConsumeRight(WDL_TypedBuf<double> *ioBuffer, int numToConsume);

template <typename FLOAT_TYPE>
void
BLUtils::ConsumeLeft(vector<WDL_TypedBuf<FLOAT_TYPE> > *ioBuffer)
{
    if (ioBuffer->empty())
        return;
    
    vector<WDL_TypedBuf<FLOAT_TYPE> > copy = *ioBuffer;
    
    ioBuffer->resize(0);
    for (int i = 1; i < copy.size(); i++)
    {
        WDL_TypedBuf<FLOAT_TYPE> &buf = copy[i];
        ioBuffer->push_back(buf);
    }
}
template void BLUtils::ConsumeLeft(vector<WDL_TypedBuf<float> > *ioBuffer);
template void BLUtils::ConsumeLeft(vector<WDL_TypedBuf<double> > *ioBuffer);

template <typename FLOAT_TYPE>
void
BLUtils::TakeHalf(WDL_TypedBuf<FLOAT_TYPE> *buf)
{
    int halfSize = buf->GetSize() / 2;
    
    buf->Resize(halfSize);
}
template void BLUtils::TakeHalf(WDL_TypedBuf<float> *buf);
template void BLUtils::TakeHalf(WDL_TypedBuf<double> *buf);

void
BLUtils::TakeHalf(WDL_TypedBuf<WDL_FFT_COMPLEX> *buf)
{
    int halfSize = buf->GetSize() / 2;
    
    buf->Resize(halfSize);
}

void
BLUtils::TakeHalf(WDL_TypedBuf<WDL_FFT_COMPLEX> *res, const WDL_TypedBuf<WDL_FFT_COMPLEX> &buf)
{
    int halfSize = buf.GetSize() / 2;
    
    res->Resize(0);
    res->Add(buf.Get(), halfSize);
}

// OPTIM PROF Infra
#if 0 // ORIGIN
void
BLUtils::ResizeFillZeros(WDL_TypedBuf<FLOAT_TYPE> *buf, int newSize)
{
    int prevSize = buf->GetSize();
    buf->Resize(newSize);
    
    if (newSize <= prevSize)
        return;
    
    for (int i = prevSize; i < newSize; i++)
    {
        if (i >= buf->GetSize())
            break;
        
        buf->Get()[i] = 0.0;
    }
}
#else // Optimized
template <typename FLOAT_TYPE>
void
BLUtils::ResizeFillZeros(WDL_TypedBuf<FLOAT_TYPE> *buf, int newSize)
{
    int prevSize = buf->GetSize();
    buf->Resize(newSize);
    
    if (newSize <= prevSize)
        return;
    
    memset(&buf->Get()[prevSize], 0, (newSize - prevSize)*sizeof(FLOAT_TYPE));
}
template void BLUtils::ResizeFillZeros(WDL_TypedBuf<float> *buf, int newSize);
template void BLUtils::ResizeFillZeros(WDL_TypedBuf<double> *buf, int newSize);
#endif

template <typename FLOAT_TYPE>
void
BLUtils::ResizeFillAllZeros(WDL_TypedBuf<FLOAT_TYPE> *buf, int newSize)
{
    int prevSize = buf->GetSize();
    buf->Resize(newSize);
    
    if (newSize <= prevSize)
        return;
    
    memset(buf->Get(), 0, buf->GetSize()*sizeof(FLOAT_TYPE));
}
template void BLUtils::ResizeFillAllZeros(WDL_TypedBuf<float> *buf, int newSize);
template void BLUtils::ResizeFillAllZeros(WDL_TypedBuf<double> *buf, int newSize);

template <typename FLOAT_TYPE>
void
BLUtils::ResizeFillValue(WDL_TypedBuf<FLOAT_TYPE> *buf, int newSize, FLOAT_TYPE value)
{
    buf->Resize(newSize);
    FillAllValue(buf, value);
}
template void BLUtils::ResizeFillValue(WDL_TypedBuf<float> *buf, int newSize, float value);
template void BLUtils::ResizeFillValue(WDL_TypedBuf<double> *buf, int newSize, double value);

template <typename FLOAT_TYPE>
void
BLUtils::ResizeFillValue2(WDL_TypedBuf<FLOAT_TYPE> *buf, int newSize, FLOAT_TYPE value)
{
    int prevSize = buf->GetSize();
    buf->Resize(newSize);
    
    FLOAT_TYPE *bufData = buf->Get();
    
    for (int i = prevSize; i < newSize; i++)
        bufData[i] = value;
}
template void BLUtils::ResizeFillValue2(WDL_TypedBuf<float> *buf, int newSize, float value);
template void BLUtils::ResizeFillValue2(WDL_TypedBuf<double> *buf, int newSize, double value);

template <typename FLOAT_TYPE>
void
BLUtils::ResizeFillRandom(WDL_TypedBuf<FLOAT_TYPE> *buf, int newSize, FLOAT_TYPE coeff)
{
    buf->Resize(newSize);
    
    int bufSize = buf->GetSize();
    FLOAT_TYPE *bufData = buf->Get();
    
    for (int i = 0; i < bufSize; i++)
    {
        FLOAT_TYPE r = ((FLOAT_TYPE)std::rand())/RAND_MAX;
        
        FLOAT_TYPE newVal = r*coeff;
        
        bufData[i] = newVal;
    }
}
template void BLUtils::ResizeFillRandom(WDL_TypedBuf<float> *buf, int newSize, float coeff);
template void BLUtils::ResizeFillRandom(WDL_TypedBuf<double> *buf, int newSize, double coeff);

void
BLUtils::ResizeFillZeros(WDL_TypedBuf<WDL_FFT_COMPLEX> *buf, int newSize)
{
    int prevSize = buf->GetSize();
    buf->Resize(newSize);
    
    if (newSize <= prevSize)
        return;
    
    WDL_FFT_COMPLEX *bufData = buf->Get();
    
    for (int i = prevSize; i < newSize; i++)
    {
        bufData[i].re = 0.0;
        bufData[i].im = 0.0;
    }
}

template <typename FLOAT_TYPE>
void
BLUtils::GrowFillZeros(WDL_TypedBuf<FLOAT_TYPE> *buf, int numGrow)
{
    int newSize = buf->GetSize() + numGrow;
    
    ResizeFillZeros(buf, newSize);
}
template void BLUtils::GrowFillZeros(WDL_TypedBuf<float> *buf, int numGrow);
template void BLUtils::GrowFillZeros(WDL_TypedBuf<double> *buf, int numGrow);

template <typename FLOAT_TYPE>
void
BLUtils::InsertZeros(WDL_TypedBuf<FLOAT_TYPE> *buf, int index, int numZeros)
{
    WDL_TypedBuf<FLOAT_TYPE> result;
    result.Resize(buf->GetSize() + numZeros);
    BLUtils::FillAllZero(&result);
    
    //
    if (buf->GetSize() < index)
    {
        *buf = result;
        
        return;
    }
        
    // Before zeros
    memcpy(result.Get(), buf->Get(), index*sizeof(FLOAT_TYPE));
    
    // After zeros
    memcpy(&result.Get()[index + numZeros], &buf->Get()[index],
           (buf->GetSize() - index)*sizeof(FLOAT_TYPE));
    
    *buf = result;
}
template void BLUtils::InsertZeros(WDL_TypedBuf<float> *buf, int index, int numZeros);
template void BLUtils::InsertZeros(WDL_TypedBuf<double> *buf, int index, int numZeros);

template <typename FLOAT_TYPE>
void
BLUtils::InsertValues(WDL_TypedBuf<FLOAT_TYPE> *buf, int index,
                    int numValues, FLOAT_TYPE value)
{
    for (int i = 0; i < numValues; i++)
        buf->Insert(value, index);
}
template void BLUtils::InsertValues(WDL_TypedBuf<float> *buf, int index,
                         int numValues, float value);
template void BLUtils::InsertValues(WDL_TypedBuf<double> *buf, int index,
                         int numValues, double value);

// BUGGY
template <typename FLOAT_TYPE>
void
BLUtils::RemoveValuesCyclic(WDL_TypedBuf<FLOAT_TYPE> *buf, int index, int numValues)
{
    // Remove too many => empty the result
    if (numValues >= buf->GetSize())
    {
        buf->Resize(0);
        
        return;
    }
    
    // Prepare the result with the new size
    WDL_TypedBuf<FLOAT_TYPE> result;
    result.Resize(buf->GetSize() - numValues);
    BLUtils::FillAllZero(&result);
    
    // Copy cyclicly
    int resultSize = result.GetSize();
    FLOAT_TYPE *resultData = result.Get();
    
    int bufSize = buf->GetSize();
    FLOAT_TYPE *bufData = buf->Get();
    
    for (int i = 0; i < resultSize; i++)
    {
        int idx = index + i;
        idx = idx % bufSize;
        
        FLOAT_TYPE val = bufData[idx];
        resultData[i] = val;
    }
    
    *buf = result;
}
template void BLUtils::RemoveValuesCyclic(WDL_TypedBuf<float> *buf, int index, int numValues);
template void BLUtils::RemoveValuesCyclic(WDL_TypedBuf<double> *buf, int index, int numValues);

// Remove before and until index
template <typename FLOAT_TYPE>
void
BLUtils::RemoveValuesCyclic2(WDL_TypedBuf<FLOAT_TYPE> *buf, int index, int numValues)
{
    // Remove too many => empty the result
    if (numValues >= buf->GetSize())
    {
        buf->Resize(0);
        
        return;
    }
    
    // Manage negative index
    if (index < 0)
        index += buf->GetSize();
    
    // Prepare the result with the new size
    WDL_TypedBuf<FLOAT_TYPE> result;
    result.Resize(buf->GetSize() - numValues);
    BLUtils::FillAllZero(&result); // Just in case
    
    // Copy cyclicly
    int bufPos = index + 1;
    int resultPos = index + 1 - numValues;
    if (resultPos < 0)
        resultPos += result.GetSize();
    
    int resultSize = result.GetSize();
    FLOAT_TYPE *resultData = result.Get();
    int bufSize = buf->GetSize();
    FLOAT_TYPE *bufData = buf->Get();
    
    for (int i = 0; i < resultSize; i++)
    {
        bufPos = bufPos % bufSize;
        resultPos = resultPos % resultSize;
        
        FLOAT_TYPE val = bufData[bufPos];
        resultData[resultPos] = val;
        
        bufPos++;
        resultPos++;
    }
    
    *buf = result;
}
template void BLUtils::RemoveValuesCyclic2(WDL_TypedBuf<float> *buf, int index, int numValues);
template void BLUtils::RemoveValuesCyclic2(WDL_TypedBuf<double> *buf, int index, int numValues);

template <typename FLOAT_TYPE>
void
BLUtils::AddValues(WDL_TypedBuf<FLOAT_TYPE> *buf, FLOAT_TYPE value)
{
    int bufSize = buf->GetSize();
    FLOAT_TYPE *bufData = buf->Get();
    
#if USE_SIMD
    if (_useSimd && (bufSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < bufSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(bufData);
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::load_splat(&value);
            
            simdpp::float64<SIMD_PACK_SIZE> r = v0 + v1;
            
            simdpp::store(bufData, r);
            
            bufData += SIMD_PACK_SIZE;
        }
        
        // Finished
        return;
    }
#endif
    
    for (int i = 0; i < bufSize; i++)
    {
        bufData[i] += value;
    }
}
template void BLUtils::AddValues(WDL_TypedBuf<float> *buf, float value);
template void BLUtils::AddValues(WDL_TypedBuf<double> *buf, double value);

template <typename FLOAT_TYPE>
void
BLUtils::AddValues(WDL_TypedBuf<FLOAT_TYPE> *result,
                 const WDL_TypedBuf<FLOAT_TYPE> &buf0,
                 const WDL_TypedBuf<FLOAT_TYPE> &buf1)
{
    result->Resize(buf0.GetSize());
    
    int resultSize = result->GetSize();
    FLOAT_TYPE *resultData = result->Get();
    FLOAT_TYPE *buf0Data = buf0.Get();
    FLOAT_TYPE *buf1Data = buf1.Get();
    
#if USE_SIMD
    if (_useSimd && (resultSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < resultSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(buf0Data);
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::load(buf1Data);
            
            simdpp::float64<SIMD_PACK_SIZE> r = v0 + v1;
            
            simdpp::store(resultData, r);
            
            buf0Data += SIMD_PACK_SIZE;
            buf1Data += SIMD_PACK_SIZE;
            resultData += SIMD_PACK_SIZE;
        }
        
        // Finished
        return;
    }
#endif
    
    for (int i = 0; i < resultSize; i++)
    {
        FLOAT_TYPE a = buf0Data[i];
        FLOAT_TYPE b = buf1Data[i];
        
        FLOAT_TYPE sum = a + b;
        
        resultData[i] = sum;
    }
}
template void BLUtils::AddValues(WDL_TypedBuf<float> *result,
                               const WDL_TypedBuf<float> &buf0,
                               const WDL_TypedBuf<float> &buf1);
template void BLUtils::AddValues(WDL_TypedBuf<double> *result,
                               const WDL_TypedBuf<double> &buf0,
                               const WDL_TypedBuf<double> &buf1);

template <typename FLOAT_TYPE>
void
BLUtils::AddValues(WDL_TypedBuf<FLOAT_TYPE> *ioBuf, const FLOAT_TYPE *addBuf)
{
    int bufSize = ioBuf->GetSize();
    FLOAT_TYPE *bufData = ioBuf->Get();
    
#if USE_SIMD
    if (_useSimd && (bufSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < bufSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(bufData);
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::load(addBuf);
            
            simdpp::float64<SIMD_PACK_SIZE> r = v0 + v1;
            
            simdpp::store(bufData, r);
            
            bufData += SIMD_PACK_SIZE;
            addBuf += SIMD_PACK_SIZE;
        }
        
        // Finished
        return;
    }
#endif
    
    for (int i = 0; i < bufSize; i++)
    {
        bufData[i] += addBuf[i];
    }
}
template void BLUtils::AddValues(WDL_TypedBuf<float> *ioBuf, const float *addBuf);
template void BLUtils::AddValues(WDL_TypedBuf<double> *ioBuf, const double *addBuf);

template <typename FLOAT_TYPE>
void
BLUtils::AddValues(FLOAT_TYPE *ioBuf, const FLOAT_TYPE *addBuf, int bufSize)
{
#if USE_SIMD
    if (_useSimd && (bufSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < bufSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(ioBuf);
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::load(addBuf);
            
            simdpp::float64<SIMD_PACK_SIZE> r = v0 + v1;
            
            simdpp::store(ioBuf, r);
            
            ioBuf += SIMD_PACK_SIZE;
            addBuf += SIMD_PACK_SIZE;
        }
        
        // Finished
        return;
    }
#endif
    
    for (int i = 0; i < bufSize; i++)
    {
        ioBuf[i] += addBuf[i];
    }
}
template void BLUtils::AddValues(float *ioBuf, const float *addBuf, int bufSize);
template void BLUtils::AddValues(double *ioBuf, const double *addBuf, int bufSize);

template <typename FLOAT_TYPE>
void
BLUtils::MultValues(WDL_TypedBuf<FLOAT_TYPE> *buf, FLOAT_TYPE value)
{
    int bufSize = buf->GetSize();
    FLOAT_TYPE *bufData = buf->Get();
    
#if USE_SIMD
    if (_useSimd && (bufSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < bufSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(bufData);
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::load_splat(&value);
        
            simdpp::float64<SIMD_PACK_SIZE> r = v0*v1;
        
            simdpp::store(bufData, r);
            
            bufData += SIMD_PACK_SIZE;
        }
        
        // Finished
        return;
    }
#endif
    
    // Normal version
    for (int i = 0; i < bufSize; i++)
    {
        FLOAT_TYPE v = bufData[i];
        v *= value;
        bufData[i] = v;
    }
}
template void BLUtils::MultValues(WDL_TypedBuf<float> *buf, float value);
template void BLUtils::MultValues(WDL_TypedBuf<double> *buf, double value);

#if !USE_SIMD_OPTIM
void
BLUtils::MultValues(vector<WDL_TypedBuf<FLOAT_TYPE> > *buf, FLOAT_TYPE value)
{
    for (int i = 0; i < buf->size(); i++)
    {
        for (int j = 0; j < (*buf)[i].GetSize(); j++)
        {
            FLOAT_TYPE v = (*buf)[i].Get()[j];
            
            v *= value;
            
            (*buf)[i].Get()[j] = v;
        }
    }
}
#else
template <typename FLOAT_TYPE>
void
BLUtils::MultValues(vector<WDL_TypedBuf<FLOAT_TYPE> > *buf, FLOAT_TYPE value)
{
    for (int i = 0; i < buf->size(); i++)
    {
        BLUtils::MultValues(&(*buf)[i], value);
    }
}
template void BLUtils::MultValues(vector<WDL_TypedBuf<float> > *buf, float value);
template void BLUtils::MultValues(vector<WDL_TypedBuf<double> > *buf, double value);
#endif

// OPTIM PROF Infra
#if 0 // ORIGIN
void
BLUtils::MultValuesRamp(WDL_TypedBuf<FLOAT_TYPE> *buf, FLOAT_TYPE value0, FLOAT_TYPE value1)
{
    for (int i = 0; i < buf->GetSize(); i++)
    {
        FLOAT_TYPE t = ((FLOAT_TYPE)i)/(buf->GetSize() - 1);
        FLOAT_TYPE value = (1.0 - t)*value0 + t*value1;
        
        FLOAT_TYPE v = buf->Get()[i];
        v *= value;
        buf->Get()[i] = v;
    }
}
#else // OPTIMIZED
template <typename FLOAT_TYPE>
void
BLUtils::MultValuesRamp(WDL_TypedBuf<FLOAT_TYPE> *buf, FLOAT_TYPE value0, FLOAT_TYPE value1)
{
    //FLOAT_TYPE step = 0.0;
    //if (buf->GetSize() >= 2)
    //    step = 1.0/(buf->GetSize() - 1);
    //FLOAT_TYPE t = 0.0;
    FLOAT_TYPE value = value0;
    FLOAT_TYPE step = 0.0;
    if (buf->GetSize() >= 2)
        step = (value1 - value0)/(buf->GetSize() - 1);
    
    int bufSize = buf->GetSize();
    FLOAT_TYPE *bufData = buf->Get();
    
    for (int i = 0; i < bufSize; i++)
    {
        FLOAT_TYPE v = bufData[i];
        v *= value;
        bufData[i] = v;
        
        value += step;
    }
}
template void BLUtils::MultValuesRamp(WDL_TypedBuf<float> *buf, float value0, float value1);
template void BLUtils::MultValuesRamp(WDL_TypedBuf<double> *buf, double value0, double value1);
#endif

template <typename FLOAT_TYPE>
void
BLUtils::MultValues(FLOAT_TYPE *buf, int size, FLOAT_TYPE value)
{
#if USE_SIMD
    if (_useSimd && (size % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < size; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(buf);
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::load_splat(&value);
            
            simdpp::float64<SIMD_PACK_SIZE> r = v0*v1;
            
            simdpp::store(buf, r);
            
            buf += SIMD_PACK_SIZE;
        }
        
        // Finished
        return;
    }
#endif
    
    for (int i = 0; i < size; i++)
    {
        FLOAT_TYPE v = buf[i];
        v *= value;
        buf[i] = v;
    }
}
template void BLUtils::MultValues(float *buf, int size, float value);
template void BLUtils::MultValues(double *buf, int size, double value);

template <typename FLOAT_TYPE>
void
BLUtils::MultValues(WDL_TypedBuf<WDL_FFT_COMPLEX> *buf, FLOAT_TYPE value)
{
    int bufSize = buf->GetSize();
    WDL_FFT_COMPLEX *bufData = buf->Get();
    
#if USE_SIMD
    if (_useSimd && (bufSize % SIMD_PACK_SIZE/2 == 0))
    {
        for (int i = 0; i < bufSize; i += SIMD_PACK_SIZE/2)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(bufData);
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::load_splat(&value);
            
            simdpp::float64<SIMD_PACK_SIZE> r = v0*v1;
            
            simdpp::store(bufData, r);
            
            bufData += SIMD_PACK_SIZE/2;
        }
        
        // Finished
        return;
    }
#endif
    
    for (int i = 0; i < bufSize; i++)
    {
        WDL_FFT_COMPLEX &v = bufData[i];
        v.re *= value;
        v.im *= value;
    }
}
template void BLUtils::MultValues(WDL_TypedBuf<WDL_FFT_COMPLEX> *buf, float value);
template void BLUtils::MultValues(WDL_TypedBuf<WDL_FFT_COMPLEX> *buf, double value);

template <typename FLOAT_TYPE>
void
BLUtils::MultValues(WDL_TypedBuf<WDL_FFT_COMPLEX> *buf,
                  const WDL_TypedBuf<FLOAT_TYPE> &values)
{
    if (buf->GetSize() != values.GetSize())
        return;
    
    int bufSize = buf->GetSize();
    WDL_FFT_COMPLEX *bufData = buf->Get();
    
    for (int i = 0; i < bufSize; i++)
    {
        FLOAT_TYPE value = values.Get()[i];
        
        WDL_FFT_COMPLEX &v = bufData[i];
        v.re *= value;
        v.im *= value;
    }
}
template void BLUtils::MultValues(WDL_TypedBuf<WDL_FFT_COMPLEX> *buf,
                       const WDL_TypedBuf<float> &values);
template void BLUtils::MultValues(WDL_TypedBuf<WDL_FFT_COMPLEX> *buf,
                       const WDL_TypedBuf<double> &values);

void
BLUtils::MultValues(WDL_TypedBuf<WDL_FFT_COMPLEX> *buf,
                  const WDL_TypedBuf<WDL_FFT_COMPLEX> &values)
{
    if (buf->GetSize() != values.GetSize())
        return;
    
    int bufSize = buf->GetSize();
    WDL_FFT_COMPLEX *bufData = buf->Get();
    
    for (int i = 0; i < bufSize; i++)
    {
        WDL_FFT_COMPLEX &bv = bufData[i];
        
        WDL_FFT_COMPLEX value = values.Get()[i];

        WDL_FFT_COMPLEX res;
        COMP_MULT(bv, value, res);
        
        bv = res;
    }
}

template <typename FLOAT_TYPE>
void
BLUtils::ApplyPow(WDL_TypedBuf<FLOAT_TYPE> *values, FLOAT_TYPE exp)
{
    int valuesSize = values->GetSize();
    FLOAT_TYPE *valuesData = values->Get();
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE val = valuesData[i];

        val = std::pow(val, exp);
        
        valuesData[i] = val;
    }
}
template void BLUtils::ApplyPow(WDL_TypedBuf<float> *values, float exp);
template void BLUtils::ApplyPow(WDL_TypedBuf<double> *values, double exp);

template <typename FLOAT_TYPE>
void
BLUtils::ApplyExp(WDL_TypedBuf<FLOAT_TYPE> *values)
{
    int valuesSize = values->GetSize();
    FLOAT_TYPE *valuesData = values->Get();
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE val = valuesData[i];
        
        val = std::exp(val);
        
        valuesData[i] = val;
    }
}
template void BLUtils::ApplyExp(WDL_TypedBuf<float> *values);
template void BLUtils::ApplyExp(WDL_TypedBuf<double> *values);

template <typename FLOAT_TYPE>
void
BLUtils::MultValues(WDL_TypedBuf<FLOAT_TYPE> *buf, const WDL_TypedBuf<FLOAT_TYPE> &values)
{
    int bufSize = buf->GetSize();
    FLOAT_TYPE *bufData = buf->Get();
    FLOAT_TYPE *valuesData = values.Get();
 
#if USE_SIMD
    if (_useSimd && (bufSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < bufSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(bufData);
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::load(valuesData);
            
            simdpp::float64<SIMD_PACK_SIZE> r = v0*v1;
            
            simdpp::store(bufData, r);
            
            bufData += SIMD_PACK_SIZE;
            valuesData += SIMD_PACK_SIZE;
        }
        
        // Finished
        return;
    }
#endif
    
    for (int i = 0; i < bufSize; i++)
    {
        FLOAT_TYPE val = valuesData[i];
        bufData[i] *= val;
    }
}
template void BLUtils::MultValues(WDL_TypedBuf<float> *buf, const WDL_TypedBuf<float> &values);
template void BLUtils::MultValues(WDL_TypedBuf<double> *buf, const WDL_TypedBuf<double> &values);

template <typename FLOAT_TYPE>
void
BLUtils::PadZerosLeft(WDL_TypedBuf<FLOAT_TYPE> *buf, int padSize)
{
    if (padSize == 0)
        return;
    
    WDL_TypedBuf<FLOAT_TYPE> newBuf;
    newBuf.Resize(buf->GetSize() + padSize);
    
    memset(newBuf.Get(), 0, padSize*sizeof(FLOAT_TYPE));
    
    memcpy(&newBuf.Get()[padSize], buf->Get(), buf->GetSize()*sizeof(FLOAT_TYPE));
    
    *buf = newBuf;
}
template void BLUtils::PadZerosLeft(WDL_TypedBuf<float> *buf, int padSize);
template void BLUtils::PadZerosLeft(WDL_TypedBuf<double> *buf, int padSize);

template <typename FLOAT_TYPE>
void
BLUtils::PadZerosRight(WDL_TypedBuf<FLOAT_TYPE> *buf, int padSize)
{
    if (padSize == 0)
        return;
    
    long prevSize = buf->GetSize();
    buf->Resize(prevSize + padSize);
    
    memset(&buf->Get()[prevSize], 0, padSize*sizeof(FLOAT_TYPE));
}
template void BLUtils::PadZerosRight(WDL_TypedBuf<float> *buf, int padSize);
template void BLUtils::PadZerosRight(WDL_TypedBuf<double> *buf, int padSize);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::Interp(FLOAT_TYPE val0, FLOAT_TYPE val1, FLOAT_TYPE t)
{
    FLOAT_TYPE res = (1.0 - t)*val0 + t*val1;
    
    return res;
}
template float BLUtils::Interp(float val0, float val1, float t);
template double BLUtils::Interp(double val0, double val1, double t);

template <typename FLOAT_TYPE>
void
BLUtils::Interp(WDL_TypedBuf<FLOAT_TYPE> *result,
              const WDL_TypedBuf<FLOAT_TYPE> *buf0,
              const WDL_TypedBuf<FLOAT_TYPE> *buf1,
              FLOAT_TYPE t)
{
    result->Resize(buf0->GetSize());
    
    int resultSize = result->GetSize();
    FLOAT_TYPE *resultData = result->Get();
    FLOAT_TYPE *buf0Data = buf0->Get();
    FLOAT_TYPE *buf1Data = buf1->Get();
    
    for (int i = 0; i < resultSize; i++)
    {
        FLOAT_TYPE val0 = buf0Data[i];
        FLOAT_TYPE val1 = buf1Data[i];
        
        FLOAT_TYPE res = (1.0 - t)*val0 + t*val1;
        
        resultData[i] = res;
    }
}
template void BLUtils::Interp(WDL_TypedBuf<float> *result,
                   const WDL_TypedBuf<float> *buf0,
                   const WDL_TypedBuf<float> *buf1,
                   float t);
template void BLUtils::Interp(WDL_TypedBuf<double> *result,
                   const WDL_TypedBuf<double> *buf0,
                   const WDL_TypedBuf<double> *buf1,
                   double t);


template <typename FLOAT_TYPE>
void
BLUtils::Interp(WDL_TypedBuf<WDL_FFT_COMPLEX> *result,
              const WDL_TypedBuf<WDL_FFT_COMPLEX> *buf0,
              const WDL_TypedBuf<WDL_FFT_COMPLEX> *buf1,
              FLOAT_TYPE t)
{
    result->Resize(buf0->GetSize());
    
    int resultSize = result->GetSize();
    WDL_FFT_COMPLEX *resultData = result->Get();
    WDL_FFT_COMPLEX *buf0Data = buf0->Get();
    WDL_FFT_COMPLEX *buf1Data = buf1->Get();
    
    for (int i = 0; i < resultSize; i++)
    {
        WDL_FFT_COMPLEX val0 = buf0Data[i];
        WDL_FFT_COMPLEX val1 = buf1Data[i];
        
        WDL_FFT_COMPLEX res;
        
        res.re = (1.0 - t)*val0.re + t*val1.re;
        res.im = (1.0 - t)*val0.im + t*val1.im;
        
        resultData[i] = res;
    }
}
template void BLUtils::Interp(WDL_TypedBuf<WDL_FFT_COMPLEX> *result,
                   const WDL_TypedBuf<WDL_FFT_COMPLEX> *buf0,
                   const WDL_TypedBuf<WDL_FFT_COMPLEX> *buf1,
                   float t);
template void BLUtils::Interp(WDL_TypedBuf<WDL_FFT_COMPLEX> *result,
                   const WDL_TypedBuf<WDL_FFT_COMPLEX> *buf0,
                   const WDL_TypedBuf<WDL_FFT_COMPLEX> *buf1,
                   double t);

template <typename FLOAT_TYPE>
void
BLUtils::Interp2D(WDL_TypedBuf<FLOAT_TYPE> *result,
                const WDL_TypedBuf<FLOAT_TYPE> bufs[2][2], FLOAT_TYPE u, FLOAT_TYPE v)
{
    WDL_TypedBuf<FLOAT_TYPE> bufv0;
    Interp(&bufv0, &bufs[0][0], &bufs[1][0], u);
    
    WDL_TypedBuf<FLOAT_TYPE> bufv1;
    Interp(&bufv1, &bufs[0][1], &bufs[1][1], u);
    
    Interp(result, &bufv0, &bufv1, v);
}
template void BLUtils::Interp2D(WDL_TypedBuf<float> *result,
                     const WDL_TypedBuf<float> bufs[2][2], float u, float v);
template void BLUtils::Interp2D(WDL_TypedBuf<double> *result,
                     const WDL_TypedBuf<double> bufs[2][2], double u, double v);

template <typename FLOAT_TYPE>
void
BLUtils::ComputeAvg(WDL_TypedBuf<FLOAT_TYPE> *result, const vector<WDL_TypedBuf<FLOAT_TYPE> > &bufs)
{
    if (bufs.empty())
        return;
    
    result->Resize(bufs[0].GetSize());
    
#if !USE_SIMD_OPTIM
    int resultSize = result->GetSize();
    FLOAT_TYPE *resultData = result->Get();
    
    for (int i = 0; i < resultSize; i++)
    {
        FLOAT_TYPE val = 0.0;
        for (int j = 0; j < bufs.size(); j++)
        {
            val += bufs[j].Get()[i];
        }
        
        val /= bufs.size();
        
        resultData[i] = val;
    }
#else
    BLUtils::FillAllZero(result);
    for (int j = 0; j < bufs.size(); j++)
    {
        BLUtils::AddValues(result, bufs[j]);
    }
    
    FLOAT_TYPE coeff = 1.0/bufs.size();
    BLUtils::MultValues(result, coeff);
#endif
}
template void BLUtils::ComputeAvg(WDL_TypedBuf<float> *result, const vector<WDL_TypedBuf<float> > &bufs);
template void BLUtils::ComputeAvg(WDL_TypedBuf<double> *result, const vector<WDL_TypedBuf<double> > &bufs);

template <typename FLOAT_TYPE>
void
BLUtils::Mix(FLOAT_TYPE *output, FLOAT_TYPE *buf0, FLOAT_TYPE *buf1, int nFrames, FLOAT_TYPE mix)
{
#if USE_SIMD
    if (_useSimd && (nFrames % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < nFrames; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(buf0);
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::load(buf1);
            
            FLOAT_TYPE c0 = (1.0 - mix);
            FLOAT_TYPE c1 = mix;
            
            simdpp::float64<SIMD_PACK_SIZE> cc0 = simdpp::load_splat(&c0);
            simdpp::float64<SIMD_PACK_SIZE> cc1 = simdpp::load_splat(&c1);
            
            simdpp::float64<SIMD_PACK_SIZE> r = cc0*v0 + cc1*v1;
            
            simdpp::store(output, r);
            
            buf0 += SIMD_PACK_SIZE;
            buf1 += SIMD_PACK_SIZE;
            output += SIMD_PACK_SIZE;
        }
        
        // Finished
        return;
    }
#endif
    
    for (int i = 0; i < nFrames; i++)
    {
        FLOAT_TYPE val = (1.0 - mix)*buf0[i] + mix*buf1[i];
        
        output[i] = val;
    }
}
template void BLUtils::Mix(float *output, float *buf0, float *buf1, int nFrames, float mix);
template void BLUtils::Mix(double *output, double *buf0, double *buf1, int nFrames, double mix);

template <typename FLOAT_TYPE>
void
BLUtils::Fade(const WDL_TypedBuf<FLOAT_TYPE> &buf0,
            const WDL_TypedBuf<FLOAT_TYPE> &buf1,
            FLOAT_TYPE *resultBuf, FLOAT_TYPE fadeStart, FLOAT_TYPE fadeEnd)
{
    int buf0Size = buf0.GetSize() - 1;
    FLOAT_TYPE *buf0Data = buf0.Get();
    FLOAT_TYPE *buf1Data = buf1.Get();
    
    for (int i = 0; i < buf0Size; i++)
    {
        FLOAT_TYPE prevVal = buf0Data[i];
        FLOAT_TYPE newVal = buf1Data[i];
        
        // Fades only on the part of the frame
        FLOAT_TYPE t = 0.0;
        if ((i >= buf0Size*fadeStart) &&
            (i < buf0Size*fadeEnd))
        {
            t = (i - buf0Size*fadeStart)/(buf0Size*(fadeEnd - fadeStart));
        }
        
        if (i >= buf0Size*fadeEnd)
            t = 1.0;
        
        FLOAT_TYPE result = (1.0 - t)*prevVal + t*newVal;
        
        resultBuf[i] = result;
    }
}
template void BLUtils::Fade(const WDL_TypedBuf<float> &buf0,
                 const WDL_TypedBuf<float> &buf1,
                 float *resultBuf, float fadeStart, float fadeEnd);
template void BLUtils::Fade(const WDL_TypedBuf<double> &buf0,
                 const WDL_TypedBuf<double> &buf1,
                 double *resultBuf, double fadeStart, double fadeEnd);

template <typename FLOAT_TYPE>
void
BLUtils::Fade(WDL_TypedBuf<FLOAT_TYPE> *buf, FLOAT_TYPE fadeStart, FLOAT_TYPE fadeEnd, bool fadeIn)
{
    Fade(buf->Get(), buf->GetSize(), fadeStart, fadeEnd, fadeIn);
}
template void BLUtils::Fade(WDL_TypedBuf<float> *buf, float fadeStart, float fadeEnd, bool fadeIn);
template void BLUtils::Fade(WDL_TypedBuf<double> *buf, double fadeStart, double fadeEnd, bool fadeIn);

template <typename FLOAT_TYPE>
void
BLUtils::Fade(FLOAT_TYPE *buf, int origBufSize, FLOAT_TYPE fadeStart, FLOAT_TYPE fadeEnd, bool fadeIn)
{
    int bufSize = origBufSize - 1;
    
    for (int i = 0; i < origBufSize; i++)
    {
        FLOAT_TYPE val = buf[i];
        
        // Fades only on the part of the frame
        FLOAT_TYPE t = 0.0;
        if ((i >= bufSize*fadeStart) &&
            (i < bufSize*fadeEnd))
        {
            t = (i - bufSize*fadeStart)/(bufSize*(fadeEnd - fadeStart));
        }
        
        if (i >= bufSize*fadeEnd)
            t = 1.0;
        
        FLOAT_TYPE result;
        
        if (fadeIn)
            result = t*val;
        else
            result = (1.0 - t)*val;
        
        buf[i] = result;
    }
}
template void BLUtils::Fade(float *buf, int origBufSize, float fadeStart, float fadeEnd, bool fadeIn);
template void BLUtils::Fade(double *buf, int origBufSize, double fadeStart, double fadeEnd, bool fadeIn);

// BUG: regression Spatializer5.0.8 => Spatialize 5.0.9 (clicks)
#define FIX_REGRESSION_SPATIALIZER 1
#if !FIX_REGRESSION_SPATIALIZER
void
BLUtils::Fade(const WDL_TypedBuf<FLOAT_TYPE> &buf0,
            const WDL_TypedBuf<FLOAT_TYPE> &buf1,
            FLOAT_TYPE *resultBuf,
            FLOAT_TYPE fadeStart, FLOAT_TYPE fadeEnd,
            FLOAT_TYPE startT, FLOAT_TYPE endT)
{
    int buf0Size = buf0.GetSize() - 1;
    FLOAT_TYPE *buf0Data = buf0.Get();
    FLOAT_TYPE *buf1Data = buf1.Get();
    
    for (int i = 0; i < buf0Size; i++)
    {
        FLOAT_TYPE prevVal = buf0Data[i];
        FLOAT_TYPE newVal = buf1Data[i];
        
        // Fades only on the part of the frame
        FLOAT_TYPE u = 0.0;
        if ((i >= buf0Size*fadeStart) &&
            (i < buf0Size*fadeEnd))
        {
            u = (i - buf0Size*fadeStart)/(buf0Size*(fadeEnd - fadeStart));
        }
        
        if (i >= buf0Size*fadeEnd)
            u = 1.0;
        
        FLOAT_TYPE t = startT + u*(endT - startT);
        
        FLOAT_TYPE result = (1.0 - t)*prevVal + t*newVal;
        
        resultBuf[i] = result;
    }
}
#else // Fixed version
template <typename FLOAT_TYPE>
void
BLUtils::Fade(const WDL_TypedBuf<FLOAT_TYPE> &buf0,
            const WDL_TypedBuf<FLOAT_TYPE> &buf1,
            FLOAT_TYPE *resultBuf,
            FLOAT_TYPE fadeStart, FLOAT_TYPE fadeEnd,
            FLOAT_TYPE startT, FLOAT_TYPE endT)
{
    int bufSize = buf0.GetSize() - 1;
    
    int buf0Size = buf0.GetSize();
    FLOAT_TYPE *buf0Data = buf0.Get();
    FLOAT_TYPE *buf1Data = buf1.Get();
    
    for (int i = 0; i < buf0Size; i++)
    {
        FLOAT_TYPE prevVal = buf0Data[i];
        FLOAT_TYPE newVal = buf1Data[i];
        
        // Fades only on the part of the frame
        FLOAT_TYPE u = 0.0;
        if ((i >= buf0Size*fadeStart) &&
            (i < buf0Size*fadeEnd))
        {
            u = (i - buf0Size*fadeStart)/(buf0Size*(fadeEnd - fadeStart));
        }
        
        if (i >= buf0Size*fadeEnd)
            u = 1.0;
        
        FLOAT_TYPE t = startT + u*(endT - startT);
        
        FLOAT_TYPE result = (1.0 - t)*prevVal + t*newVal;
        
        resultBuf[i] = result;
    }
}
template void BLUtils::Fade(const WDL_TypedBuf<float> &buf0,
                 const WDL_TypedBuf<float> &buf1,
                 float *resultBuf,
                 float fadeStart, float fadeEnd,
                 float startT, float endT);
template void BLUtils::Fade(const WDL_TypedBuf<double> &buf0,
                 const WDL_TypedBuf<double> &buf1,
                 double *resultBuf,
                 double fadeStart, double fadeEnd,
                 double startT, double endT);
#endif

template <typename FLOAT_TYPE>
void
BLUtils::Fade(WDL_TypedBuf<FLOAT_TYPE> *ioBuf0,
            const WDL_TypedBuf<FLOAT_TYPE> &buf1,
            FLOAT_TYPE fadeStart, FLOAT_TYPE fadeEnd,
            bool fadeIn,
            FLOAT_TYPE startPos, FLOAT_TYPE endPos)
{
    // NEW: check for bounds !
    // Added for Ghost-X and FftProcessObj15 (latency fix)
    if (startPos < 0.0)
        startPos = 0.0;
    if (startPos > 1.0)
        startPos = 1.0;
    
    if (endPos < 0.0)
        endPos = 0.0;
    if (endPos > 1.0)
        endPos = 1.0;
    
    long startIdx = startPos*ioBuf0->GetSize();
    long endIdx = endPos*ioBuf0->GetSize();
 
    int buf0Size = ioBuf0->GetSize() - 1;
    FLOAT_TYPE *buf0Data = ioBuf0->Get();
    FLOAT_TYPE *buf1Data = buf1.Get();
    
    for (int i = startIdx; i < endIdx; i++)
    {
        FLOAT_TYPE prevVal = buf0Data[i];
        FLOAT_TYPE newVal = buf1Data[i];
        
        // Fades only on the part of the frame
        FLOAT_TYPE t = 0.0;
        if ((i >= buf0Size*fadeStart) &&
            (i < buf0Size*fadeEnd))
        {
            t = (i - buf0Size*fadeStart)/(buf0Size*(fadeEnd - fadeStart));
        }
        
        if (i >= buf0Size*fadeEnd)
            t = 1.0;
        
        FLOAT_TYPE result;
        if (fadeIn)
            result = (1.0 - t)*prevVal + t*newVal;
        else
            result = t*prevVal + (1.0 - t)*newVal;
        
        buf0Data[i] = result;
    }
}
template void BLUtils::Fade(WDL_TypedBuf<float> *ioBuf0,
                 const WDL_TypedBuf<float> &buf1,
                 float fadeStart, float fadeEnd,
                 bool fadeIn,
                 float startPos, float endPos);
template void BLUtils::Fade(WDL_TypedBuf<double> *ioBuf0,
                 const WDL_TypedBuf<double> &buf1,
                 double fadeStart, double fadeEnd,
                 bool fadeIn,
                 double startPos, double endPos);

//
template <typename FLOAT_TYPE>
void
BLUtils::FadeOut(WDL_TypedBuf<FLOAT_TYPE> *ioBuf,
              int startSampleId, int endSampleId)
{
    if (ioBuf->GetSize() == 0)
        return;
    
    if (startSampleId < 0)
        startSampleId = 0;
    if (endSampleId >= ioBuf->GetSize())
        endSampleId = ioBuf->GetSize() - 1;
    
    if (startSampleId == endSampleId)
        return;
    
    for (int i = startSampleId; i <= endSampleId; i++)
    {
        FLOAT_TYPE t = ((FLOAT_TYPE)(i - startSampleId))/(endSampleId - startSampleId);
        
        t = 1.0 - t;
        
        FLOAT_TYPE val = ioBuf->Get()[i];
        val *= t;
        ioBuf->Get()[i] = val;
    }
}
template void BLUtils::FadeOut(WDL_TypedBuf<float> *ioBuf,
                    int startSampleId, int endSampleId);
template void BLUtils::FadeOut(WDL_TypedBuf<double> *ioBuf,
                    int startSampleId, int endSampleId);


template <typename FLOAT_TYPE>
void
BLUtils::Fade(WDL_TypedBuf<FLOAT_TYPE> *ioBuf0,
            const WDL_TypedBuf<FLOAT_TYPE> &buf1,
            FLOAT_TYPE fadeStart, FLOAT_TYPE fadeEnd)
{
    Fade(ioBuf0, buf1, fadeStart, fadeEnd, true, (FLOAT_TYPE)0.0, (FLOAT_TYPE)0.5);
    Fade(ioBuf0, buf1, (FLOAT_TYPE)1.0 - fadeEnd, (FLOAT_TYPE)1.0 - fadeStart,
         false, (FLOAT_TYPE)0.5, (FLOAT_TYPE)1.0);
}
template void BLUtils::Fade(WDL_TypedBuf<float> *ioBuf0,
                 const WDL_TypedBuf<float> &buf1,
                 float fadeStart, float fadeEnd);
template void BLUtils::Fade(WDL_TypedBuf<double> *ioBuf0,
                 const WDL_TypedBuf<double> &buf1,
                 double fadeStart, double fadeEnd);

template <typename FLOAT_TYPE>
void
BLUtils::Fade(FLOAT_TYPE *ioBuf0Data,
            const FLOAT_TYPE *buf1Data,
            int bufSize,
            FLOAT_TYPE fadeStart, FLOAT_TYPE fadeEnd)
{
    WDL_TypedBuf<FLOAT_TYPE> buf0;
    buf0.Resize(bufSize);
    memcpy(buf0.Get(), ioBuf0Data, bufSize*sizeof(FLOAT_TYPE));
    
    WDL_TypedBuf<FLOAT_TYPE> buf1;
    buf1.Resize(bufSize);
    memcpy(buf1.Get(), buf1Data, bufSize*sizeof(FLOAT_TYPE));
    
    Fade(&buf0, buf1, fadeStart, fadeEnd, true, (FLOAT_TYPE)0.0, (FLOAT_TYPE)0.5);
    Fade(&buf0, buf1, (FLOAT_TYPE)1.0 - fadeEnd, (FLOAT_TYPE)1.0 - fadeStart,
         false, (FLOAT_TYPE)0.5, (FLOAT_TYPE)1.0);
    
    memcpy(ioBuf0Data, buf0.Get(), bufSize*sizeof(FLOAT_TYPE));
}
template void BLUtils::Fade(float *ioBuf0Data,
                 const float *buf1Data,
                 int bufSize,
                 float fadeStart, float fadeEnd);
template void BLUtils::Fade(double *ioBuf0Data,
                 const double *buf1Data,
                 int bufSize,
                 double fadeStart, double fadeEnd);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::AmpToDB(FLOAT_TYPE sampleVal, FLOAT_TYPE eps, FLOAT_TYPE minDB)
{
    FLOAT_TYPE result = minDB;
    FLOAT_TYPE absSample = std::fabs(sampleVal);
    if (absSample > eps/*EPS*/)
    {
        result = BLUtils::AmpToDB(absSample);
    }
    
    return result;
}
template float BLUtils::AmpToDB(float sampleVal, float eps, float minDB);
template double BLUtils::AmpToDB(double sampleVal, double eps, double minDB);

template <typename FLOAT_TYPE>
void
BLUtils::AmpToDB(WDL_TypedBuf<FLOAT_TYPE> *dBBuf,
               const WDL_TypedBuf<FLOAT_TYPE> &ampBuf,
               FLOAT_TYPE eps, FLOAT_TYPE minDB)
{
    dBBuf->Resize(ampBuf.GetSize());
    
    int ampBufSize = ampBuf.GetSize();
    FLOAT_TYPE *ampBufData = ampBuf.Get();
    FLOAT_TYPE *dBBufData = dBBuf->Get();
    
    for (int i = 0; i < ampBufSize; i++)
    {
        FLOAT_TYPE amp = ampBufData[i];
        FLOAT_TYPE dbAmp = AmpToDB(amp, eps, minDB);
        
        dBBufData[i] = dbAmp;
    }
}
template void BLUtils::AmpToDB(WDL_TypedBuf<float> *dBBuf,
                    const WDL_TypedBuf<float> &ampBuf,
                    float eps, float minDB);
template void BLUtils::AmpToDB(WDL_TypedBuf<double> *dBBuf,
                    const WDL_TypedBuf<double> &ampBuf,
                    double eps, double minDB);

template <typename FLOAT_TYPE>
void
BLUtils::AmpToDB(WDL_TypedBuf<FLOAT_TYPE> *dBBuf,
               const WDL_TypedBuf<FLOAT_TYPE> &ampBuf)
{
    *dBBuf = ampBuf;
    
    int dBBufSize = dBBuf->GetSize();
    FLOAT_TYPE *dBBufData = dBBuf->Get();
    
    for (int i = 0; i < dBBufSize; i++)
    {
        FLOAT_TYPE amp = dBBufData[i];
        FLOAT_TYPE dbAmp = BLUtils::AmpToDB(amp);
        
        dBBufData[i] = dbAmp;
    }
}
template void BLUtils::AmpToDB(WDL_TypedBuf<float> *dBBuf,
                    const WDL_TypedBuf<float> &ampBuf);
template void BLUtils::AmpToDB(WDL_TypedBuf<double> *dBBuf,
                    const WDL_TypedBuf<double> &ampBuf);

// OPTIM PROF Infra
// (compute in place)
template <typename FLOAT_TYPE>
void
BLUtils::AmpToDB(WDL_TypedBuf<FLOAT_TYPE> *ioBuf,
               FLOAT_TYPE eps, FLOAT_TYPE minDB)
{
    int ioBufSize = ioBuf->GetSize();
    FLOAT_TYPE *ioBufData = ioBuf->Get();
    
    for (int i = 0; i < ioBufSize; i++)
    {
        FLOAT_TYPE amp = ioBufData[i];
        FLOAT_TYPE dbAmp = AmpToDB(amp, eps, minDB);
        
        ioBufData[i] = dbAmp;
    }
}
template void BLUtils::AmpToDB(WDL_TypedBuf<float> *ioBuf,
                    float eps, float minDB);
template void BLUtils::AmpToDB(WDL_TypedBuf<double> *ioBuf,
                    double eps, double minDB);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::AmpToDBClip(FLOAT_TYPE sampleVal, FLOAT_TYPE eps, FLOAT_TYPE minDB)
{
    FLOAT_TYPE result = minDB;
    FLOAT_TYPE absSample = std::fabs(sampleVal);
    if (absSample > BL_EPS)
    {
        result = BLUtils::AmpToDB(absSample);
    }
 
    // Avoid very low negative dB values, which are not significant
    if (result < minDB)
        result = minDB;
    
    return result;
}
template float BLUtils::AmpToDBClip(float sampleVal, float eps, float minDB);
template double BLUtils::AmpToDBClip(double sampleVal, double eps, double minDB);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::AmpToDBNorm(FLOAT_TYPE sampleVal, FLOAT_TYPE eps, FLOAT_TYPE minDB)
{
    FLOAT_TYPE result = minDB;
    FLOAT_TYPE absSample = std::fabs(sampleVal);
    // EPS is 1e-8 here...
    // So fix it as it should be!
    //if (absSample > EPS) // (AMP_DB_CRITICAL_FIX)
    if (absSample > eps)
    {
        result = BLUtils::AmpToDB(absSample);
    }
    
    // Avoid very low negative dB values, which are not significant
    if (result < minDB)
        result = minDB;
    
    result += -minDB;
    result /= -minDB;
    
    return result;
}
template float BLUtils::AmpToDBNorm(float sampleVal, float eps, float minDB);
template double BLUtils::AmpToDBNorm(double sampleVal, double eps, double minDB);

#if !USE_SIMD_OPTIM
void
BLUtils::AmpToDBNorm(WDL_TypedBuf<FLOAT_TYPE> *dBBufNorm,
                   const WDL_TypedBuf<FLOAT_TYPE> &ampBuf,
                   FLOAT_TYPE eps, FLOAT_TYPE minDB)
{
    dBBufNorm->Resize(ampBuf.GetSize());
    
    int ampBufSize = ampBuf.GetSize();
    FLOAT_TYPE *ampBufData = ampBuf.Get();
    FLOAT_TYPE *dBBufNormData = dBBufNorm->Get();
    
    for (int i = 0; i < ampBufSize; i++)
    {
        FLOAT_TYPE amp = ampBufData[i];
        FLOAT_TYPE dbAmpNorm = AmpToDBNorm(amp, eps, minDB);
        
        dBBufNormData[i] = dbAmpNorm;
    }
}
#else
template <typename FLOAT_TYPE>
void
BLUtils::AmpToDBNorm(WDL_TypedBuf<FLOAT_TYPE> *dBBufNorm,
                   const WDL_TypedBuf<FLOAT_TYPE> &ampBuf,
                   FLOAT_TYPE eps, FLOAT_TYPE minDB)
{
    dBBufNorm->Resize(ampBuf.GetSize());
    
    int ampBufSize = ampBuf.GetSize();
    FLOAT_TYPE *ampBufData = ampBuf.Get();
    FLOAT_TYPE *dBBufNormData = dBBufNorm->Get();
    
    FLOAT_TYPE minDbInv = -1.0/minDB; //
    for (int i = 0; i < ampBufSize; i++)
    {
        FLOAT_TYPE amp = ampBufData[i];
        
        ///
        FLOAT_TYPE dbAmpNorm = minDB;
        FLOAT_TYPE absSample = std::fabs(amp);
        
        // EPS is 1e-8 here...
        // So fix it as it should be!
        //if (absSample > EPS) // (AMP_DB_CRITICAL_FIX)
        if (absSample > eps)
        {
            dbAmpNorm = BLUtils::AmpToDB(absSample);
        }
        
        // Avoid very low negative dB values, which are not significant
        if (dbAmpNorm < minDB)
            dbAmpNorm = minDB;
        
        dbAmpNorm += -minDB;
        dbAmpNorm *= minDbInv;
        ///
        
        dBBufNormData[i] = dbAmpNorm;
    }
}
template void BLUtils::AmpToDBNorm(WDL_TypedBuf<float> *dBBufNorm,
                        const WDL_TypedBuf<float> &ampBuf,
                        float eps, float minDB);
template void BLUtils::AmpToDBNorm(WDL_TypedBuf<double> *dBBufNorm,
                        const WDL_TypedBuf<double> &ampBuf,
                        double eps, double minDB);
#endif

static const double BL_AMP_DB = 8.685889638065036553;
// Magic number for dB to gain conversion.
// Approximates 10^(x/20)
static const double BL_IAMP_DB = 0.11512925464970;

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::DBToAmp(FLOAT_TYPE dB)
{
    return std::exp(((FLOAT_TYPE)BL_IAMP_DB) * dB);
}
template float BLUtils::DBToAmp(float dB);
template double BLUtils::DBToAmp(double dB);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::AmpToDB(FLOAT_TYPE dB)
{
    return ((FLOAT_TYPE)BL_AMP_DB) * std::log(std::fabs(dB));
}
template float BLUtils::AmpToDB(float dB);
template double BLUtils::AmpToDB(double dB);

template <typename FLOAT_TYPE>
void
BLUtils::AmpToDBNorm(WDL_TypedBuf<FLOAT_TYPE> *ioBuf,
                   FLOAT_TYPE eps, FLOAT_TYPE minDB)
{
    WDL_TypedBuf<FLOAT_TYPE> ampBuf = *ioBuf;
    
    AmpToDBNorm(ioBuf, ampBuf, eps, minDB);
}
template void BLUtils::AmpToDBNorm(WDL_TypedBuf<float> *ioBuf,
                        float eps, float minDB);
template void BLUtils::AmpToDBNorm(WDL_TypedBuf<double> *ioBuf,
                        double eps, double minDB);

template <typename FLOAT_TYPE>
void
BLUtils::DBToAmp(WDL_TypedBuf<FLOAT_TYPE> *ioBuf)
{
    int ioBufSize = ioBuf->GetSize();
    FLOAT_TYPE *ioBufData = ioBuf->Get();
    
    for (int i = 0; i < ioBufSize; i++)
    {
        FLOAT_TYPE db = ioBufData[i];
        
        FLOAT_TYPE amp = BLUtils::DBToAmp(db);
        
        ioBufData[i] = amp;
    }
}
template void BLUtils::DBToAmp(WDL_TypedBuf<float> *ioBuf);
template void BLUtils::DBToAmp(WDL_TypedBuf<double> *ioBuf);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::DBToAmpNorm(FLOAT_TYPE sampleVal, FLOAT_TYPE eps, FLOAT_TYPE minDB)
{
    FLOAT_TYPE db = sampleVal;
    if (db < minDB)
        db = minDB;
    
    db *= -minDB;
    db += minDB;
    
    FLOAT_TYPE result = BLUtils::DBToAmp(db);
    
    return result;
}
template float BLUtils::DBToAmpNorm(float sampleVal, float eps, float minDB);
template double BLUtils::DBToAmpNorm(double sampleVal, double eps, double minDB);

template <typename FLOAT_TYPE>
void
BLUtils::DBToAmpNorm(WDL_TypedBuf<FLOAT_TYPE> *ioBuf,
                   FLOAT_TYPE eps, FLOAT_TYPE minDB)
{
    int bufSize = ioBuf->GetSize();
    FLOAT_TYPE *bufData = ioBuf->Get();
    
    for (int i = 0; i < bufSize; i++)
    {
        FLOAT_TYPE val = bufData[i];
        val = DBToAmpNorm(val, eps, minDB);
        bufData[i] = val;
    }
}
template void BLUtils::DBToAmpNorm(WDL_TypedBuf<float> *ioBuf,
                        float eps, float minDB);
template void BLUtils::DBToAmpNorm(WDL_TypedBuf<double> *ioBuf,
                        double eps, double minDB);

int
BLUtils::NextPowerOfTwo(int value)
{
    int result = 1;
    
    while(result < value)
        result *= 2;
    
    return result;
}

template <typename FLOAT_TYPE>
void
BLUtils::AddValues(WDL_TypedBuf<FLOAT_TYPE> *ioBuf, const WDL_TypedBuf<FLOAT_TYPE> &addBuf)
{
    int ioBufSize = ioBuf->GetSize();
    FLOAT_TYPE *ioBufData = ioBuf->Get();
    int addBufSize = addBuf.GetSize();
    FLOAT_TYPE *addBufData = addBuf.Get();
    
#if USE_SIMD_OPTIM
    if (ioBufSize == addBufSize)
    {
        AddValues(ioBuf, addBuf.Get());
        
        return;
    }
#endif
    
    for (int i = 0; i < ioBufSize; i++)
    {
        if (i > addBufSize - 1)
            break;
        
        FLOAT_TYPE val = ioBufData[i];
        FLOAT_TYPE add = addBufData[i];
        
        val += add;
        
        ioBufData[i] = val;
    }
}
template void BLUtils::AddValues(WDL_TypedBuf<float> *ioBuf, const WDL_TypedBuf<float> &addBuf);
template void BLUtils::AddValues(WDL_TypedBuf<double> *ioBuf, const WDL_TypedBuf<double> &addBuf);

void
BLUtils::AddValues(WDL_TypedBuf<WDL_FFT_COMPLEX> *ioBuf,
                 const WDL_TypedBuf<WDL_FFT_COMPLEX> &addBuf)
{
    int ioBufSize = ioBuf->GetSize();
    WDL_FFT_COMPLEX *ioBufData = ioBuf->Get();
    int addBufSize = addBuf.GetSize();
    WDL_FFT_COMPLEX *addBufData = addBuf.Get();
    
#if 0 //USE_SIMD_OPTIM
    if (ioBufSize == addBufSize)
    {
        FLOAT_TYPE *ioBufData = (FLOAT_TYPE *)ioBuf->Get();
        const FLOAT_TYPE *addBufData = (const FLOAT_TYPE *)addBuf.Get();
        int bufSize = ioBuf->GetSize()*2;
        
        AddValues(ioBufData, addBufData, bufSize);
        
        return;
    }
#endif
    
    for (int i = 0; i < ioBufSize; i++)
    {
        if (i > addBufSize - 1)
            break;
        
        WDL_FFT_COMPLEX val = ioBufData[i];
        WDL_FFT_COMPLEX add = addBufData[i];
        
        val.re += add.re;
        val.im += add.im;
        
        ioBufData[i] = val;
    }
}

template <typename FLOAT_TYPE>
void
BLUtils::SubstractValues(WDL_TypedBuf<FLOAT_TYPE> *ioBuf, const WDL_TypedBuf<FLOAT_TYPE> &subBuf)
{
    int ioBufSize = ioBuf->GetSize();
    FLOAT_TYPE *ioBufData = ioBuf->Get();
    FLOAT_TYPE *subBufData = subBuf.Get();
    
#if USE_SIMD
    if (_useSimd && (ioBufSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < ioBufSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(ioBufData);
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::load(subBufData);
            
            simdpp::float64<SIMD_PACK_SIZE> r = v0 - v1;
            
            simdpp::store(ioBufData, r);
            
            ioBufData += SIMD_PACK_SIZE;
            subBufData += SIMD_PACK_SIZE;
        }
        
        // Finished
        return;
    }
#endif
    
    for (int i = 0; i < ioBufSize; i++)
    {
        FLOAT_TYPE val = ioBufData[i];
        FLOAT_TYPE sub = subBufData[i];
        
        val -= sub;
        
        ioBufData[i] = val;
    }
}
template void BLUtils::SubstractValues(WDL_TypedBuf<float> *ioBuf, const WDL_TypedBuf<float> &subBuf);
template void BLUtils::SubstractValues(WDL_TypedBuf<double> *ioBuf, const WDL_TypedBuf<double> &subBuf);

void
BLUtils::SubstractValues(WDL_TypedBuf<WDL_FFT_COMPLEX> *ioBuf,
                       const WDL_TypedBuf<WDL_FFT_COMPLEX> &subBuf)
{
    int ioBufSize = ioBuf->GetSize();
    WDL_FFT_COMPLEX *ioBufData = ioBuf->Get();
    WDL_FFT_COMPLEX *subBufData = subBuf.Get();
    
    for (int i = 0; i < ioBufSize; i++)
    {
        WDL_FFT_COMPLEX val = ioBufData[i];
        WDL_FFT_COMPLEX sub = subBufData[i];
        
        val.re -= sub.re;
        val.im -= sub.im;
        
        ioBufData[i] = val;
    }
}

template <typename FLOAT_TYPE>
void
BLUtils::ComputeDiff(WDL_TypedBuf<FLOAT_TYPE> *resultDiff,
                   const WDL_TypedBuf<FLOAT_TYPE> &buf0,
                   const WDL_TypedBuf<FLOAT_TYPE> &buf1)
{
    resultDiff->Resize(buf0.GetSize());
    
    int resultDiffSize = resultDiff->GetSize();
    FLOAT_TYPE *resultDiffData = resultDiff->Get();
    FLOAT_TYPE *buf0Data = buf0.Get();
    FLOAT_TYPE *buf1Data = buf1.Get();
    
#if USE_SIMD
    if (_useSimd && (resultDiffSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < resultDiffSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(buf0Data);
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::load(buf1Data);
            
            simdpp::float64<SIMD_PACK_SIZE> r = v1 - v0;
            
            simdpp::store(resultDiffData, r);
            
            buf0Data += SIMD_PACK_SIZE;
            buf1Data += SIMD_PACK_SIZE;
            resultDiffData += SIMD_PACK_SIZE;
        }
        
        // Finished
        return;
    }
#endif
    
    for (int i = 0; i < resultDiffSize; i++)
    {
        FLOAT_TYPE val0 = buf0Data[i];
        FLOAT_TYPE val1 = buf1Data[i];
        
        FLOAT_TYPE diff = val1 - val0;
        
        resultDiffData[i] = diff;
    }
}
template void BLUtils::ComputeDiff(WDL_TypedBuf<float> *resultDiff,
                        const WDL_TypedBuf<float> &buf0,
                        const WDL_TypedBuf<float> &buf1);
template void BLUtils::ComputeDiff(WDL_TypedBuf<double> *resultDiff,
                        const WDL_TypedBuf<double> &buf0,
                        const WDL_TypedBuf<double> &buf1);

template <typename FLOAT_TYPE>
void
BLUtils::ComputeDiff(FLOAT_TYPE *resultDiff,
                   const FLOAT_TYPE* buf0, const FLOAT_TYPE* buf1,
                   int bufSize)
{
#if USE_SIMD
    if (_useSimd && (bufSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < bufSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(buf0);
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::load(buf1);
            
            simdpp::float64<SIMD_PACK_SIZE> r = v1 - v0;
            
            simdpp::store(resultDiff, r);
            
            buf0 += SIMD_PACK_SIZE;
            buf1 += SIMD_PACK_SIZE;
            resultDiff += SIMD_PACK_SIZE;
        }
        
        // Finished
        return;
    }
#endif
    
    for (int i = 0; i < bufSize; i++)
    {
        FLOAT_TYPE val0 = buf0[i];
        FLOAT_TYPE val1 = buf1[i];
        
        FLOAT_TYPE diff = val1 - val0;
        
        resultDiff[i] = diff;
    }
}
template void BLUtils::ComputeDiff(float *resultDiff,
                        const float *buf0, const float *buf1,
                        int bufSize);
template void BLUtils::ComputeDiff(double *resultDiff,
                        const double *buf0, const double *buf1,
                        int bufSize);
void
BLUtils::ComputeDiff(WDL_TypedBuf<WDL_FFT_COMPLEX> *resultDiff,
                   const WDL_TypedBuf<WDL_FFT_COMPLEX> &buf0,
                   const WDL_TypedBuf<WDL_FFT_COMPLEX> &buf1)
{
    resultDiff->Resize(buf0.GetSize());
    
    int resultDiffSize = resultDiff->GetSize();
    WDL_FFT_COMPLEX *resultDiffData = resultDiff->Get();
    WDL_FFT_COMPLEX *buf0Data = buf0.Get();
    WDL_FFT_COMPLEX *buf1Data = buf1.Get();
    
#if 0 //USE_SIMD_OPTIM
    FLOAT_TYPE *resD = (FLOAT_TYPE *)resultDiffData;
    FLOAT_TYPE *buf0D = (FLOAT_TYPE *)buf0Data;
    FLOAT_TYPE *buf1D = (FLOAT_TYPE *)buf1Data;
    
    int bufSizeD = resultDiffSize*2;
    
    ComputeDiff(resD, buf0D, buf1D, bufSizeD);
    
    return;
#endif
    
    for (int i = 0; i < resultDiffSize; i++)
    {
        WDL_FFT_COMPLEX val0 = buf0Data[i];
        WDL_FFT_COMPLEX val1 = buf1Data[i];
        
        WDL_FFT_COMPLEX diff;
        diff.re = val1.re - val0.re;
        diff.im = val1.im - val0.im;
        
        resultDiffData[i] = diff;
    }
}

void
BLUtils::ComputeDiff(WDL_TypedBuf<WDL_FFT_COMPLEX> *resultDiff,
                   const vector<WDL_FFT_COMPLEX> &buf0,
                   const vector<WDL_FFT_COMPLEX> &buf1)
{
    resultDiff->Resize(buf0.size());
    
    int resultDiffSize = resultDiff->GetSize();
    WDL_FFT_COMPLEX *resultDiffData = resultDiff->Get();
    
    for (int i = 0; i < resultDiffSize; i++)
    {
        WDL_FFT_COMPLEX val0 = buf0[i];
        WDL_FFT_COMPLEX val1 = buf1[i];
        
        WDL_FFT_COMPLEX diff;
        diff.re = val1.re - val0.re;
        diff.im = val1.im - val0.im;
        
        resultDiffData[i] = diff;
    }
}

template <typename FLOAT_TYPE>
void
BLUtils::Permute(WDL_TypedBuf<FLOAT_TYPE> *values,
               const WDL_TypedBuf<int> &indices,
               bool forward)
{
    if (values->GetSize() != indices.GetSize())
        return;
    
    WDL_TypedBuf<FLOAT_TYPE> origValues = *values;
    
    int valuesSize = values->GetSize();
    FLOAT_TYPE *valuesData = values->Get();
    int *indicesData = indices.Get();
    FLOAT_TYPE *origValuesData = origValues.Get();
    
    for (int i = 0; i < valuesSize; i++)
    {
        int idx = indicesData[i];
        if (idx >= valuesSize)
            // Error
            return;
        
        if (forward)
            valuesData[idx] = origValuesData[i];
        else
            valuesData[i] = origValuesData[idx];
    }
}
template void BLUtils::Permute(WDL_TypedBuf<float> *values,
                    const WDL_TypedBuf<int> &indices,
                    bool forward);
template void BLUtils::Permute(WDL_TypedBuf<double> *values,
                    const WDL_TypedBuf<int> &indices,
                    bool forward);

void
BLUtils::Permute(vector< vector< int > > *values,
               const WDL_TypedBuf<int> &indices,
               bool forward)
{
    if (values->size() != indices.GetSize())
        return;
    
    vector<vector<int> > origValues = *values;
    
    for (int i = 0; i < values->size(); i++)
    {
        int idx = indices.Get()[i];
        if (idx >= values->size())
            // Error
            return;
        
        if (forward)
            (*values)[idx] = origValues[i];
        else
            (*values)[i] = origValues[idx];
    }
}

template <typename FLOAT_TYPE>
void
BLUtils::ClipMin(WDL_TypedBuf<FLOAT_TYPE> *values, FLOAT_TYPE minVal)
{
    int valuesSize = values->GetSize();
    FLOAT_TYPE *valuesData = values->Get();
    
#if USE_SIMD
    if (_useSimd && (valuesSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < valuesSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(valuesData);
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::load_splat(&minVal);
            
            simdpp::float64<SIMD_PACK_SIZE> r = simdpp::max(v0, v1);
            
            simdpp::store(valuesData, r);
            
            valuesData += SIMD_PACK_SIZE;
        }
        
        // Finished
        return;
    }
#endif
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE val = valuesData[i];
        if (val < minVal)
            val = minVal;
        
        valuesData[i] = val;
    }
}
template void BLUtils::ClipMin(WDL_TypedBuf<float> *values, float minVal);
template void BLUtils::ClipMin(WDL_TypedBuf<double> *values, double minVal);

template <typename FLOAT_TYPE>
void
BLUtils::ClipMin2(WDL_TypedBuf<FLOAT_TYPE> *values, FLOAT_TYPE clipVal, FLOAT_TYPE minVal)
{
    int valuesSize = values->GetSize();
    FLOAT_TYPE *valuesData = values->Get();
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE val = valuesData[i];
        if (val < clipVal)
            val = minVal;
        
        valuesData[i] = val;
    }
}
template void BLUtils::ClipMin2(WDL_TypedBuf<float> *values, float clipVal, float minVal);
template void BLUtils::ClipMin2(WDL_TypedBuf<double> *values, double clipVal, double minVal);

template <typename FLOAT_TYPE>
void
BLUtils::ClipMax(WDL_TypedBuf<FLOAT_TYPE> *values, FLOAT_TYPE maxVal)
{
    int valuesSize = values->GetSize();
    FLOAT_TYPE *valuesData = values->Get();
    
#if USE_SIMD
    if (_useSimd && (valuesSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < valuesSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(valuesData);
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::load_splat(&maxVal);
            
            simdpp::float64<SIMD_PACK_SIZE> r = simdpp::min(v0, v1);
            
            simdpp::store(valuesData, r);
            
            valuesData += SIMD_PACK_SIZE;
        }
        
        // Finished
        return;
    }
#endif
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE val = valuesData[i];
        if (val > maxVal)
            val = maxVal;
        
        valuesData[i] = val;
    }
}
template void BLUtils::ClipMax(WDL_TypedBuf<float> *values, float maxVal);
template void BLUtils::ClipMax(WDL_TypedBuf<double> *values, double maxVal);

template <typename FLOAT_TYPE>
void
BLUtils::ClipMinMax(FLOAT_TYPE *val, FLOAT_TYPE min, FLOAT_TYPE max)
{
    if (*val < min)
        *val = min;
    if (*val > max)
        *val = max;
}
template void BLUtils::ClipMinMax(float *val, float min, float max);
template void BLUtils::ClipMinMax(double *val, double min, double max);

template <typename FLOAT_TYPE>
void
BLUtils::ClipMinMax(WDL_TypedBuf<FLOAT_TYPE> *values, FLOAT_TYPE minVal, FLOAT_TYPE maxVal)
{
    int valuesSize = values->GetSize();
    FLOAT_TYPE *valuesData = values->Get();
    
#if USE_SIMD
    if (_useSimd && (valuesSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < valuesSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(valuesData);
            simdpp::float64<SIMD_PACK_SIZE> m0 = simdpp::load_splat(&minVal);
            simdpp::float64<SIMD_PACK_SIZE> m1 = simdpp::load_splat(&maxVal);
            
            simdpp::float64<SIMD_PACK_SIZE> r0 = simdpp::max(v0, m0);
            simdpp::float64<SIMD_PACK_SIZE> r = simdpp::min(r0, m1);
            
            simdpp::store(valuesData, r);
            
            valuesData += SIMD_PACK_SIZE;
        }
        
        // Finished
        return;
    }
#endif
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE val = valuesData[i];
        if (val < minVal)
            val = minVal;
        if (val > maxVal)
            val = maxVal;
        
        valuesData[i] = val;
    }
}
template void BLUtils::ClipMinMax(WDL_TypedBuf<float> *values, float minVal, float maxVal);
template void BLUtils::ClipMinMax(WDL_TypedBuf<double> *values, double minVal, double maxVal);

template <typename FLOAT_TYPE>
void
BLUtils::ThresholdMin(WDL_TypedBuf<FLOAT_TYPE> *values, FLOAT_TYPE min)
{    
    int valuesSize = values->GetSize();
    FLOAT_TYPE *valuesData = values->Get();
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE val = valuesData[i];
        if (val < min)
            val = 0.0;
        
        valuesData[i] = val;
    }
}
template void BLUtils::ThresholdMin(WDL_TypedBuf<float> *values, float min);
template void BLUtils::ThresholdMin(WDL_TypedBuf<double> *values, double min);

template <typename FLOAT_TYPE>
void
BLUtils::ThresholdMinRel(WDL_TypedBuf<FLOAT_TYPE> *values, FLOAT_TYPE min)
{
    FLOAT_TYPE maxValue = BLUtils::FindMaxValue(*values);
    min *= maxValue;
    
    int valuesSize = values->GetSize();
    FLOAT_TYPE *valuesData = values->Get();
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE val = valuesData[i];
        if (val < min)
            val = 0.0;
        
        valuesData[i] = val;
    }
}
template void BLUtils::ThresholdMinRel(WDL_TypedBuf<float> *values, float min);
template void BLUtils::ThresholdMinRel(WDL_TypedBuf<double> *values, double min);

template <typename FLOAT_TYPE>
void
BLUtils::Diff(WDL_TypedBuf<FLOAT_TYPE> *diff,
            const WDL_TypedBuf<FLOAT_TYPE> &prevValues,
            const WDL_TypedBuf<FLOAT_TYPE> &values)
{
#if USE_SIMD_OPTIM
    // Diff and ComputeDiff are the same function
    ComputeDiff(diff, prevValues, values);
    
    return;
#endif
    
    diff->Resize(values.GetSize());
    
    int valuesSize = values.GetSize();
    FLOAT_TYPE *valuesData = values.Get();
    FLOAT_TYPE *prevValuesData = prevValues.Get();
    FLOAT_TYPE *diffData = diff->Get();
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE val = valuesData[i];
        FLOAT_TYPE prevVal = prevValuesData[i];
        
        FLOAT_TYPE d = val - prevVal;
        
        diffData[i] = d;
    }
}
template void BLUtils::Diff(WDL_TypedBuf<float> *diff,
                 const WDL_TypedBuf<float> &prevValues,
                 const WDL_TypedBuf<float> &values);
template void BLUtils::Diff(WDL_TypedBuf<double> *diff,
                 const WDL_TypedBuf<double> &prevValues,
                 const WDL_TypedBuf<double> &values);

template <typename FLOAT_TYPE>
void
BLUtils::ApplyDiff(WDL_TypedBuf<FLOAT_TYPE> *values,
                 const WDL_TypedBuf<FLOAT_TYPE> &diff)
{
#if USE_SIMD_OPTIM
    AddValues(values, diff);
    
    return;
#endif
    
    int valuesSize = values->GetSize();
    FLOAT_TYPE *valuesData = values->Get();
    FLOAT_TYPE *diffData = diff.Get();
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE val = valuesData[i];
        
        FLOAT_TYPE d = diffData[i];
        
        val += d;
        
        valuesData[i] = val;
    }
}
template void BLUtils::ApplyDiff(WDL_TypedBuf<float> *values,
                      const WDL_TypedBuf<float> &diff);
template void BLUtils::ApplyDiff(WDL_TypedBuf<double> *values,
                      const WDL_TypedBuf<double> &diff);


template <typename FLOAT_TYPE>
bool
BLUtils::IsEqual(const WDL_TypedBuf<FLOAT_TYPE> &values0,
               const WDL_TypedBuf<FLOAT_TYPE> &values1)
{
    if (values0.GetSize() != values1.GetSize())
        return false;
    
    int values0Size = values0.GetSize();
    FLOAT_TYPE *values0Data = values0.Get();
    FLOAT_TYPE *values1Data = values1.Get();
    
#if USE_SIMD
    if (_useSimd && (values0Size % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < values0Size; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(values0Data);
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::load(values1Data);
            
            simdpp::float64<SIMD_PACK_SIZE> d = v1 - v0;
            
            simdpp::float64<SIMD_PACK_SIZE> a = simdpp::abs(d);
            
            FLOAT_TYPE maxVal = simdpp::reduce_max(a);
            
            if (maxVal > BL_EPS)
                return false;
            
            values0Data += SIMD_PACK_SIZE;
            values1Data += SIMD_PACK_SIZE;
        }
    }
    
    return true;
#endif
    
    for (int i = 0; i < values0Size; i++)
    {
        FLOAT_TYPE val0 = values0Data[i];
        FLOAT_TYPE val1 = values1Data[i];
        
        if (std::fabs(val0 - val1) > BL_EPS)
            return false;
    }
    
    return true;
}
template bool BLUtils::IsEqual(const WDL_TypedBuf<float> &values0,
                    const WDL_TypedBuf<float> &values1);
template bool BLUtils::IsEqual(const WDL_TypedBuf<double> &values0,
                    const WDL_TypedBuf<double> &values1);

template <typename FLOAT_TYPE>
void
BLUtils::ReplaceValue(WDL_TypedBuf<FLOAT_TYPE> *values, FLOAT_TYPE srcValue, FLOAT_TYPE dstValue)
{
    int valuesSize = values->GetSize();
    FLOAT_TYPE *valuesData = values->Get();
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE val = valuesData[i];
        
        if (std::fabs(val - srcValue) < BL_EPS)
            val = dstValue;
        
        valuesData[i] = val;
    }
}
template void BLUtils::ReplaceValue(WDL_TypedBuf<float> *values, float srcValue, float dstValue);
template void BLUtils::ReplaceValue(WDL_TypedBuf<double> *values, double srcValue, double dstValue);

template <typename FLOAT_TYPE>
void
BLUtils::MakeSymmetry(WDL_TypedBuf<FLOAT_TYPE> *symBuf, const WDL_TypedBuf<FLOAT_TYPE> &buf)
{
    symBuf->Resize(buf.GetSize()*2);
    
    int bufSize = buf.GetSize();
    FLOAT_TYPE *bufData = buf.Get();
    FLOAT_TYPE *symBufData = symBuf->Get();
    
    for (int i = 0; i < bufSize; i++)
    {
        symBufData[i] = bufData[i];
        symBufData[bufSize*2 - i - 1] = bufData[i];
    }
}
template void BLUtils::MakeSymmetry(WDL_TypedBuf<float> *symBuf, const WDL_TypedBuf<float> &buf);
template void BLUtils::MakeSymmetry(WDL_TypedBuf<double> *symBuf, const WDL_TypedBuf<double> &buf);

void
BLUtils::Reverse(WDL_TypedBuf<int> *values)
{
    WDL_TypedBuf<int> origValues = *values;
    
    int valuesSize = values->GetSize();
    int *valuesData = values->Get();
    int *origValuesData = origValues.Get();
    
    for (int i = 0; i < valuesSize; i++)
    {
        int val = origValuesData[i];
        
        int idx = valuesSize - i - 1;
        
        valuesData[idx] = val;
    }
}

template <typename FLOAT_TYPE>
void
BLUtils::Reverse(WDL_TypedBuf<FLOAT_TYPE> *values)
{
    WDL_TypedBuf<FLOAT_TYPE> origValues = *values;
    
    int valuesSize = values->GetSize();
    FLOAT_TYPE *valuesData = values->Get();
    FLOAT_TYPE *origValuesData = origValues.Get();
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE val = origValuesData[i];
        
        int idx = valuesSize - i - 1;
        
        valuesData[idx] = val;
    }
}
template void BLUtils::Reverse(WDL_TypedBuf<float> *values);
template void BLUtils::Reverse(WDL_TypedBuf<double> *values);

template <typename FLOAT_TYPE>
void
BLUtils::ApplySqrt(WDL_TypedBuf<FLOAT_TYPE> *values)
{
    int valuesSize = values->GetSize();
    FLOAT_TYPE *valuesData = values->Get();
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE val = valuesData[i];
        
        if (val > 0.0)
            val = std::sqrt(val);
        
        valuesData[i] = val;
    }
}
template void BLUtils::ApplySqrt(WDL_TypedBuf<float> *values);
template void BLUtils::ApplySqrt(WDL_TypedBuf<double> *values);

template <typename FLOAT_TYPE>
void
BLUtils::DecimateValues(WDL_TypedBuf<FLOAT_TYPE> *result,
                      const WDL_TypedBuf<FLOAT_TYPE> &buf,
                      FLOAT_TYPE decFactor)
{
    if (buf.GetSize() == 0)
        return;
    
    if (decFactor >= 1.0)
    {
        *result = buf;
        
        return;
    }
    
    BLUtils::ResizeFillZeros(result, buf.GetSize()*decFactor);
    
    int resultIdx = 0;
    
    // Keep the maxima when decimating
    FLOAT_TYPE count = 0.0;
    FLOAT_TYPE maxSample = 0.0;
    
    int bufSize = buf.GetSize();
    FLOAT_TYPE *bufData = buf.Get();
    int resultSize = result->GetSize();
    FLOAT_TYPE *resultData = result->Get();
    
    for (int i = 0; i < bufSize; i++)
    {
        FLOAT_TYPE samp = bufData[i];
        if (std::fabs(samp) > std::fabs(maxSample))
            maxSample = samp;
        
        // Fix for spectrograms
        //count += 1.0/decFactor;
        count += decFactor;
        
        if (count >= 1.0)
        {
            resultData[resultIdx++] = maxSample;
            
            maxSample = 0.0;
            
            count -= 1.0;
        }
        
        if (resultIdx >=  resultSize)
            break;
    }
}
template void BLUtils::DecimateValues(WDL_TypedBuf<float> *result,
                                    const WDL_TypedBuf<float> &buf,
                                    float decFactor);
template void BLUtils::DecimateValues(WDL_TypedBuf<double> *result,
                                    const WDL_TypedBuf<double> &buf,
                                    double decFactor);


template <typename FLOAT_TYPE>
void
BLUtils::DecimateValues(WDL_TypedBuf<FLOAT_TYPE> *ioValues,
                      FLOAT_TYPE decFactor)
{
    WDL_TypedBuf<FLOAT_TYPE> origSamples = *ioValues;
    DecimateValues(ioValues, origSamples, decFactor);
}
template void BLUtils::DecimateValues(WDL_TypedBuf<float> *ioValues,
                                    float decFactor);
template void BLUtils::DecimateValues(WDL_TypedBuf<double> *ioValues,
                                    double decFactor);


template <typename FLOAT_TYPE>
void
BLUtils::DecimateValuesDb(WDL_TypedBuf<FLOAT_TYPE> *result,
                        const WDL_TypedBuf<FLOAT_TYPE> &buf,
                        FLOAT_TYPE decFactor, FLOAT_TYPE minValueDb)
{
    if (buf.GetSize() == 0)
        return;
    
    if (decFactor >= 1.0)
    {
        *result = buf;
        
        return;
    }
    
    BLUtils::ResizeFillZeros(result, buf.GetSize()*decFactor);
    
    int resultIdx = 0;
    
    // Keep the maxima when decimating
    FLOAT_TYPE count = 0.0;
    FLOAT_TYPE maxSample = minValueDb;
    
    int bufSize = buf.GetSize();
    FLOAT_TYPE *bufData = buf.Get();
    int resultSize = result->GetSize();
    FLOAT_TYPE *resultData = result->Get();
    
    for (int i = 0; i < bufSize; i++)
    {
        FLOAT_TYPE samp = bufData[i];
        //if (std::fabs(samp) > std::fabs(maxSample))
        if (samp > maxSample)
            maxSample = samp;
        
        // Fix for spectrograms
        //count += 1.0/decFactor;
        count += decFactor;
        
        if (count >= 1.0)
        {
            resultData[resultIdx++] = maxSample;
            
            maxSample = minValueDb;
            
            count -= 1.0;
        }
        
        if (resultIdx >= resultSize)
            break;
    }
}
template void BLUtils::DecimateValuesDb(WDL_TypedBuf<float> *result,
                                      const WDL_TypedBuf<float> &buf,
                                      float decFactor, float minValueDb);
template void BLUtils::DecimateValuesDb(WDL_TypedBuf<double> *result,
                                      const WDL_TypedBuf<double> &buf,
                                      double decFactor, double minValueDb);


// FIX: to avoid long series of positive values not looking like waveforms
// FIX2: improved initial fix: really avoid loosing interesting min and max
template <typename FLOAT_TYPE>
void
BLUtils::DecimateSamples(WDL_TypedBuf<FLOAT_TYPE> *result,
                       const WDL_TypedBuf<FLOAT_TYPE> &buf,
                       FLOAT_TYPE decFactor)
{
    if (buf.GetSize() == 0)
        return;
    
    if (decFactor >= 1.0)
    {
        *result = buf;
        
        return;
    }
    
    BLUtils::ResizeFillZeros(result, buf.GetSize()*decFactor);
    
    int resultIdx = 0;
    
    // Keep the maxima when decimating
    FLOAT_TYPE count = 0.0;
    
    // When set to the first value instead of 0,
    // it avoid a first line from 0 to the first value at the beginning
    //FLOAT_TYPE minSample = 0.0;
    //FLOAT_TYPE maxSample = 0.0;
    //FLOAT_TYPE prevSample = 0.0;
    
    FLOAT_TYPE minSample = buf.Get()[0];
    FLOAT_TYPE maxSample = buf.Get()[0];
    FLOAT_TYPE prevSample = buf.Get()[0];
    
    // When set to true, avoid flat beginning when the first values are negative
    //bool zeroCrossed = false;
    bool zeroCrossed = true;
    
    FLOAT_TYPE prevSampleUsed = 0.0;
    
    int bufSize = buf.GetSize();
    FLOAT_TYPE *bufData = buf.Get();
    int resultSize = result->GetSize();
    FLOAT_TYPE *resultData = result->Get();
    
    for (int i = 0; i < bufSize; i++)
    {
        FLOAT_TYPE samp = bufData[i];
        //if (std::fabs(samp) > std::fabs(maxSample))
        //    maxSample = samp;
        if (samp > maxSample)
            maxSample = samp;
        
        if (samp < minSample)
            minSample = samp;
        
        // Optimize by removing the multiplication
        // (sometimes we run through millions of samples,
        // so it could be worth it to optimize this)
        
        //if (samp*prevSample < 0.0)
        if ((samp > 0.0 && prevSample < 0.0) ||
            (samp < 0.0 && prevSample > 0.0))
            zeroCrossed = true;
        
        prevSample = samp;
        
        // Fix for spectrograms
        //count += 1.0/decFactor;
        count += decFactor;
        
        if (count >= 1.0)
        {
            // Take care, if we crossed zero,
            // we take alternately positive and negative samples
            // (otherwise, we could have very long series of positive samples
            // for example. And this won't look like a waveform anymore...
            FLOAT_TYPE sampleToUse;
            if (!zeroCrossed)
            {
                // Prefer reseting only min or max, not both, to avoid loosing
                // interesting values
                if (prevSampleUsed >= 0.0)
                {
                    sampleToUse = maxSample;
                    
                    // FIX: avoid segments stuck at 0 during several samples
                    maxSample = samp; //0.0;
                }
                else
                {
                    sampleToUse = minSample;
                    minSample = samp; //0.0;
                }
            } else
            {
                if (prevSampleUsed >= 0.0)
                {
                    sampleToUse = minSample;
                    minSample = samp; //0.0;
                }
                else
                {
                    sampleToUse = maxSample;
                    maxSample = samp; //0.0;
                }
            }
            
            resultData[resultIdx++] = sampleToUse;
            
            // Prefer reseting only min or max, not both, to avoid loosing
            // interesting values
            
            //minSample = 0.0;
            //maxSample = 0.0;
            
            count -= 1.0;
            
            prevSampleUsed = sampleToUse;
            zeroCrossed = false;
        }
        
        if (resultIdx >=  resultSize)
            break;
    }
}
template void BLUtils::DecimateSamples(WDL_TypedBuf<float> *result,
                                     const WDL_TypedBuf<float> &buf,
                                     float decFactor);
template void BLUtils::DecimateSamples(WDL_TypedBuf<double> *result,
                                     const WDL_TypedBuf<double> &buf,
                                     double decFactor);


// DOESN'T WORK...
// Incremental version
// Try to fix long sections of 0 values
template <typename FLOAT_TYPE>
void
BLUtils::DecimateSamples2(WDL_TypedBuf<FLOAT_TYPE> *result,
                        const WDL_TypedBuf<FLOAT_TYPE> &buf,
                        FLOAT_TYPE decFactor)
{
    FLOAT_TYPE factor = 0.5;
    
    // Decimate progressively
    WDL_TypedBuf<FLOAT_TYPE> tmp = buf;
    while(tmp.GetSize() > buf.GetSize()*decFactor*2.0)
    {
        DecimateSamples(&tmp, factor);
    }
    
    // Last step
    DecimateSamples(&tmp, decFactor);
    
    *result = tmp;
}
template void BLUtils::DecimateSamples2(WDL_TypedBuf<float> *result,
                                      const WDL_TypedBuf<float> &buf,
                                      float decFactor);
template void BLUtils::DecimateSamples2(WDL_TypedBuf<double> *result,
                                      const WDL_TypedBuf<double> &buf,
                                      double decFactor);


template <typename FLOAT_TYPE>
void
BLUtils::DecimateSamples(WDL_TypedBuf<FLOAT_TYPE> *ioSamples,
                       FLOAT_TYPE decFactor)
{
    WDL_TypedBuf<FLOAT_TYPE> origSamples = *ioSamples;
    DecimateSamples(ioSamples, origSamples, decFactor);
}
template void BLUtils::DecimateSamples(WDL_TypedBuf<float> *ioSamples,
                                     float decFactor);
template void BLUtils::DecimateSamples(WDL_TypedBuf<double> *ioSamples,
                                     double decFactor);

// Simply take some samples and throw out the others
// ("sparkling" when zooming in Ghost)
template <typename FLOAT_TYPE>
void
BLUtils::DecimateSamplesFast(WDL_TypedBuf<FLOAT_TYPE> *result,
                           const WDL_TypedBuf<FLOAT_TYPE> &buf,
                           FLOAT_TYPE decFactor)
{
    BLUtils::ResizeFillZeros(result, buf.GetSize()*decFactor);
    
    int step = (decFactor > 0) ? 1.0/decFactor : buf.GetSize();
    int resId = 0;
    
    int bufSize = buf.GetSize();
    FLOAT_TYPE *bufData = buf.Get();
    int resultSize = result->GetSize();
    FLOAT_TYPE *resultData = result->Get();
    
    for (int i = 0; i < bufSize; i+= step)
    {
        FLOAT_TYPE val = bufData[i];
        
        if (resId >= resultSize)
            break;
        
        resultData[resId++] = val;
    }
}
template void BLUtils::DecimateSamplesFast(WDL_TypedBuf<float> *result,
                                const WDL_TypedBuf<float> &buf,
                                float decFactor);
template void BLUtils::DecimateSamplesFast(WDL_TypedBuf<double> *result,
                                const WDL_TypedBuf<double> &buf,
                                double decFactor);

template <typename FLOAT_TYPE>
void
BLUtils::DecimateStep(WDL_TypedBuf<FLOAT_TYPE> *ioSamples, int step)
{
    int numSamples = ioSamples->GetSize();
    
    WDL_TypedBuf<FLOAT_TYPE> samplesCopy = *ioSamples;
    const FLOAT_TYPE *copyBuf = samplesCopy.Get();
    
    ioSamples->Resize(ioSamples->GetSize()/step);
    int numResultSamples = ioSamples->GetSize();
    FLOAT_TYPE *resultBuf = ioSamples->Get();
    
    int resPos = 0;
    for (int i = 0; i < numSamples; i += step)
    {
        if (resPos < numResultSamples)
        {
            FLOAT_TYPE val = copyBuf[i];
            resultBuf[resPos++] = val;
        }
    }
}
template void BLUtils::DecimateStep(WDL_TypedBuf<float> *ioSamples, int step);
template void BLUtils::DecimateStep(WDL_TypedBuf<double> *ioSamples, int step);

template <typename FLOAT_TYPE>
void
BLUtils::DecimateSamplesFast(WDL_TypedBuf<FLOAT_TYPE> *ioSamples,
                           FLOAT_TYPE decFactor)
{
    WDL_TypedBuf<FLOAT_TYPE> buf = *ioSamples;
    DecimateSamplesFast(ioSamples, buf, decFactor);
}
template void BLUtils::DecimateSamplesFast(WDL_TypedBuf<float> *ioSamples,
                                float decFactor);
template void BLUtils::DecimateSamplesFast(WDL_TypedBuf<double> *ioSamples,
                                double decFactor);

template <typename FLOAT_TYPE>
int
BLUtils::SecondOrderEqSolve(FLOAT_TYPE a, FLOAT_TYPE b, FLOAT_TYPE c, FLOAT_TYPE res[2])
{
    // See: http://math.lyceedebaudre.net/premiere-sti2d/second-degre/resoudre-une-equation-du-second-degre
    //
    FLOAT_TYPE delta = b*b - 4.0*a*c;
    
    if (delta > 0.0)
    {
        res[0] = (-b - std::sqrt(delta))/(2.0*a);
        res[1] = (-b + std::sqrt(delta))/(2.0*a);
        
        return 2;
    }
    
    if (std::fabs(delta) < BL_EPS)
    {
        res[0] = -b/(2.0*a);
        
        return 1;
    }
    
    return 0;
}
template int BLUtils::SecondOrderEqSolve(float a, float b, float c, float res[2]);
template int BLUtils::SecondOrderEqSolve(double a, double b, double c, double res[2]);

void
BLUtils::FillSecondFftHalf(WDL_TypedBuf<WDL_FFT_COMPLEX> *ioBuffer)
{
    if (ioBuffer->GetSize() < 2)
        return;
    
    // It is important that the "middle value" (ie index 1023) is duplicated
    // to index 1024. So we have twice the center value
    
    int ioBufferSize2 = ioBuffer->GetSize()/2;
    WDL_FFT_COMPLEX *ioBufferData = ioBuffer->Get();
    
    for (int i = 1; i < ioBufferSize2; i++)
    {
        int id0 = i + ioBufferSize2;
        
#if 1 // ORIG
        // Orig, bug...
        // doesn't fill exactly the symetry (the last value differs)
        // but WDL generates ffts like that
        int id1 = ioBufferSize2 - i;
#endif
        
#if 1 // FIX: fill the value at the middle
      // (which was not filled, and could be undefined if not filled outside the function)
      //
      // NOTE: added for Rebalance, to fix a bug:
      // - waveform values like 1e+250
      //
      // NOTE: quick fix, better solution could be found, by
      // comparing with WDL fft
      //
      // NOTE: could fix many plugins, like for example StereoViz
      //
      ioBufferData[ioBufferSize2].re = 0.0;
      ioBufferData[ioBufferSize2].im = 0.0;
#endif
        
#if 0 // Bug fix (but strange WDL behaviour)
        // Really symetric version
        // with correct last value
        // But if we apply to just generate WDL fft, the behaviour becomes different
        int id1 = ioBufferSize2 - i - 1;
#endif
        
        ioBufferData[id0].re = ioBufferData[id1].re;
        
        // Complex conjugate
        ioBufferData[id0].im = -ioBufferData[id1].im;
    }
}

template <typename FLOAT_TYPE>
void
BLUtils::FillSecondFftHalf(WDL_TypedBuf<FLOAT_TYPE> *ioMagns)
{
    if (ioMagns->GetSize() < 2)
        return;

    int ioMagnsSize2 = ioMagns->GetSize()/2;
    FLOAT_TYPE *ioMagnsData = ioMagns->Get();
    
    for (int i = 1; i < ioMagnsSize2; i++)
    {
        int id0 = i + ioMagnsSize2;
        
        // WARNING: doesn't fill exactly the symetry (the last value differs)
        // but WDL generates ffts like that
        int id1 = ioMagnsSize2 - i;
        
        // FIX: fill the value at the middle
        ioMagnsData[ioMagnsSize2] = 0.0;
        
        ioMagnsData[id0] = ioMagnsData[id1];
    }
}
template void BLUtils::FillSecondFftHalf(WDL_TypedBuf<float> *ioMagns);
template void BLUtils::FillSecondFftHalf(WDL_TypedBuf<double> *ioMagns);

template <typename FLOAT_TYPE>
void
BLUtils::CopyBuf(WDL_TypedBuf<FLOAT_TYPE> *toBuf, const WDL_TypedBuf<FLOAT_TYPE> &fromBuf)
{
    int toBufSize = toBuf->GetSize();
    FLOAT_TYPE *toBufData = toBuf->Get();
    FLOAT_TYPE *fromBufData = fromBuf.Get();
    
    int fromBufSize = fromBuf.GetSize();
    
#if USE_SIMD_OPTIM
    int size = toBufSize;
    if (fromBufSize < size)
        size = fromBufSize;
    memcpy(toBufData, fromBufData, size*sizeof(FLOAT_TYPE));
    
    return;
#endif
    
    for (int i = 0; i < toBufSize; i++)
    {
        if (i >= fromBufSize)
            break;
    
        FLOAT_TYPE val = fromBufData[i];
        toBufData[i] = val;
    }
}
template void BLUtils::CopyBuf(WDL_TypedBuf<float> *toBuf, const WDL_TypedBuf<float> &fromBuf);
template void BLUtils::CopyBuf(WDL_TypedBuf<double> *toBuf, const WDL_TypedBuf<double> &fromBuf);

template <typename FLOAT_TYPE>
void
BLUtils::CopyBuf(WDL_TypedBuf<FLOAT_TYPE> *toBuf, const FLOAT_TYPE *fromData, int fromSize)
{
    toBuf->Resize(fromSize);
    memcpy(toBuf->Get(), fromData, fromSize*sizeof(FLOAT_TYPE));
}
template void BLUtils::CopyBuf(WDL_TypedBuf<float> *toBuf, const float *fromData, int fromSize);
template void BLUtils::CopyBuf(WDL_TypedBuf<double> *toBuf, const double *fromData, int fromSize);

template <typename FLOAT_TYPE>
void
BLUtils::CopyBuf(FLOAT_TYPE *toBuf, const FLOAT_TYPE *fromData, int fromSize)
{
    memcpy(toBuf, fromData, fromSize*sizeof(FLOAT_TYPE));
}
template void BLUtils::CopyBuf(float *toBuf, const float *fromData, int fromSize);
template void BLUtils::CopyBuf(double *toBuf, const double *fromData, int fromSize);

template <typename FLOAT_TYPE>
void
BLUtils::CopyBuf(FLOAT_TYPE *toData, const WDL_TypedBuf<FLOAT_TYPE> &fromBuf)
{
    memcpy(toData, fromBuf.Get(), fromBuf.GetSize()*sizeof(FLOAT_TYPE));
}
template void BLUtils::CopyBuf(float *toData, const WDL_TypedBuf<float> &fromBuf);
template void BLUtils::CopyBuf(double *toData, const WDL_TypedBuf<double> &fromBuf);

template <typename FLOAT_TYPE>
void
BLUtils::Replace(WDL_TypedBuf<FLOAT_TYPE> *dst, int startIdx, const WDL_TypedBuf<FLOAT_TYPE> &src)
{
    int srcSize = src.GetSize();
    FLOAT_TYPE *srcData = src.Get();
    int dstSize = dst->GetSize();
    FLOAT_TYPE *dstData = dst->Get();
    
#if USE_SIMD_OPTIM
    int size = srcSize;
    if (srcSize + startIdx > dstSize)
        size = dstSize - startIdx;
    
    memcpy(&dstData[startIdx], srcData, size*sizeof(FLOAT_TYPE));
    
    return;
#endif
    
    for (int i = 0; i < srcSize; i++)
    {
        if (startIdx + i >= dstSize)
            break;
        
        FLOAT_TYPE val = srcData[i];
        
        dstData[startIdx + i] = val;
    }
}
template void BLUtils::Replace(WDL_TypedBuf<float> *dst, int startIdx, const WDL_TypedBuf<float> &src);
template void BLUtils::Replace(WDL_TypedBuf<double> *dst, int startIdx, const WDL_TypedBuf<double> &src);

#if !FIND_VALUE_INDEX_EXPE
// Current version
template <typename FLOAT_TYPE>
int
BLUtils::FindValueIndex(FLOAT_TYPE val, const WDL_TypedBuf<FLOAT_TYPE> &values, FLOAT_TYPE *outT)
{
    if (outT != NULL)
        *outT = 0.0;
    
    int valuesSize = values.GetSize();
    FLOAT_TYPE *valuesData = values.Get();
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE v = valuesData[i];
        
        if (v > val)
        {
            int idx0 = i - 1;
            if (idx0 < 0)
                idx0 = 0;
            
            if (outT != NULL)
            {
                int idx1 = i;
            
                FLOAT_TYPE val0 = valuesData[idx0];
                FLOAT_TYPE val1 = valuesData[idx1];
                
                if (std::fabs(val1 - val0) > BL_EPS)
                    *outT = (val - val0)/(val1 - val0);
            }
            
            return idx0;
        }
    }
    
    return -1;
}
template int BLUtils::FindValueIndex(float val, const WDL_TypedBuf<float> &values, float *outT);
template int BLUtils::FindValueIndex(double val, const WDL_TypedBuf<double> &values, double *outT);

#else
// New version
// NOT very well tested yet (but should be better - or same -)
int
BLUtils::FindValueIndex(FLOAT_TYPE val, const WDL_TypedBuf<FLOAT_TYPE> &values, FLOAT_TYPE *outT)
{
    *outT = 0.0;
    
    int valuesSize = values.GetSize();
    FLOAT_TYPE *valuesData = values.Get();
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE v = valuesData[i];
        
        if (v == val)
            // Same value
        {
            *outT = 0.0;
            
            return i;
        }
        
        if (v > val)
        {
            int idx0 = i - 1;
            if (idx0 < 0)
                // We have not found the value in the array
                // because the first value of the array is already grater than
                // the value we are testing
            {
                idx0 = 0;
                *outT = 0.0;
                
                return idx0;
            }
            
            // We are between values
            if (outT != NULL)
            {
                int idx1 = idx0 + 1;
                
                FLOAT_TYPE val0 = valuesData[idx0];
                FLOAT_TYPE val1 = valuesData[idx1];
                
                if (std::fabs(val1 - val0) > BL_EPS)
                    *outT = (val - val0)/(val1 - val0);
            }
            
            return idx0;
        }
    }
    
    return -1;
}
#endif

// Find the matching index from srcVal and the src list,
// and return the corresponding value from the dstValues
//
// The src values do not need to be sorted !
// (as opposite to FindValueIndex())
//
// Used to sort without loosing the consistency
template <typename FLOAT_TYPE>
class Value
{
public:
    static bool IsGreater(const Value& v0, const Value& v1)
        { return v0.mSrcValue < v1.mSrcValue; }
    
    FLOAT_TYPE mSrcValue;
    FLOAT_TYPE mDstValue;
};

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::FindMatchingValue(FLOAT_TYPE srcVal,
                         const WDL_TypedBuf<FLOAT_TYPE> &srcValues,
                         const WDL_TypedBuf<FLOAT_TYPE> &dstValues)
{
    // Fill the vector
    vector<Value<FLOAT_TYPE> > values;
    values.resize(srcValues.GetSize());
    
    int srcValuesSize = srcValues.GetSize();
    FLOAT_TYPE *srcValuesData = srcValues.Get();
    FLOAT_TYPE *dstValuesData = dstValues.Get();
    
    for (int i = 0; i < srcValuesSize; i++)
    {
        FLOAT_TYPE srcValue = srcValuesData[i];
        FLOAT_TYPE dstValue = dstValuesData[i];
        
        Value<FLOAT_TYPE> val;
        val.mSrcValue = srcValue;
        val.mDstValue = dstValue;
        
        values[i] = val;
    }
    
    // Sort the vector
    sort(values.begin(), values.end(), Value<FLOAT_TYPE>::IsGreater);
    
    // Re-create the WDL list
    WDL_TypedBuf<FLOAT_TYPE> sortedSrcValues;
    sortedSrcValues.Resize(values.size());
    
    FLOAT_TYPE *sortedSrcValuesData = sortedSrcValues.Get();
    
    for (int i = 0; i < values.size(); i++)
    {
        FLOAT_TYPE val = values[i].mSrcValue;
        sortedSrcValuesData[i] = val;
    }
    
    // Find the index and the t parameter
    FLOAT_TYPE t;
    int idx = FindValueIndex(srcVal, sortedSrcValues, &t);
    if (idx < 0)
        idx = 0;
    
    // Find the new matching value
    FLOAT_TYPE dstVal0 = dstValues.Get()[idx];
    if (idx + 1 >= dstValues.GetSize())
        // LAst value
        return dstVal0;
    
    FLOAT_TYPE dstVal1 = dstValues.Get()[idx + 1];
    
    FLOAT_TYPE res = Interp(dstVal0, dstVal1, t);
    
    return res;
}
template float BLUtils::FindMatchingValue(float srcVal,
                               const WDL_TypedBuf<float> &srcValues,
                               const WDL_TypedBuf<float> &dstValues);
template double BLUtils::FindMatchingValue(double srcVal,
                                const WDL_TypedBuf<double> &srcValues,
                                const WDL_TypedBuf<double> &dstValues);

template <typename FLOAT_TYPE>
void
BLUtils::PrepareMatchingValueSorted(WDL_TypedBuf<FLOAT_TYPE> *srcValues,
                                  WDL_TypedBuf<FLOAT_TYPE> *dstValues)
{
    // Fill the vector
    vector<Value<FLOAT_TYPE> > values;
    values.resize(srcValues->GetSize());
    
    int srcValuesSize = srcValues->GetSize();
    FLOAT_TYPE *srcValuesData = srcValues->Get();
    FLOAT_TYPE *dstValuesData = dstValues->Get();
    
    for (int i = 0; i < srcValuesSize; i++)
    {
        FLOAT_TYPE srcValue = srcValuesData[i];

        FLOAT_TYPE dstValue = 0.0;
        if (dstValues != NULL)
            dstValue = dstValuesData[i];
        
        Value<FLOAT_TYPE> val;
        val.mSrcValue = srcValue;
        val.mDstValue = dstValue;
        
        values[i] = val;
    }
    
    // Sort the vector
    sort(values.begin(), values.end(), Value<FLOAT_TYPE>::IsGreater);
    
    // Re-create the WDL lists
    
    // Src values
    for (int i = 0; i < values.size(); i++)
    {
        FLOAT_TYPE val = values[i].mSrcValue;
        srcValuesData[i] = val;
    }
    
    // Dst values
    if (dstValues != NULL)
    {
        WDL_TypedBuf<FLOAT_TYPE> sortedDstValues;
        sortedDstValues.Resize(values.size());
        
        FLOAT_TYPE *dstValuesData = dstValues->Get();
        
        for (int i = 0; i < values.size(); i++)
        {
            FLOAT_TYPE val = values[i].mDstValue;
            dstValuesData[i] = val;
        }
    }
}
template void BLUtils::PrepareMatchingValueSorted(WDL_TypedBuf<float> *srcValues,
                                       WDL_TypedBuf<float> *dstValues);
template void BLUtils::PrepareMatchingValueSorted(WDL_TypedBuf<double> *srcValues,
                                       WDL_TypedBuf<double> *dstValues);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::FindMatchingValueSorted(FLOAT_TYPE srcVal,
                               const WDL_TypedBuf<FLOAT_TYPE> &sortedSrcValues,
                               const WDL_TypedBuf<FLOAT_TYPE> &sortedDstValues)
{
    // Find the index and the t parameter
    FLOAT_TYPE t;
    int idx = FindValueIndex(srcVal, sortedSrcValues, &t);
    if (idx < 0)
        idx = 0;
        
    // Find the new matching value
    FLOAT_TYPE dstVal0 = sortedDstValues.Get()[idx];
    if (idx + 1 >= sortedDstValues.GetSize())
        // Last value
        return dstVal0;
    
    FLOAT_TYPE dstVal1 = sortedDstValues.Get()[idx + 1];
    
    FLOAT_TYPE res = Interp(dstVal0, dstVal1, t);
    
    return res;
}
template float BLUtils::FindMatchingValueSorted(float srcVal,
                                     const WDL_TypedBuf<float> &sortedSrcValues,
                                     const WDL_TypedBuf<float> &sortedDstValues);
template double BLUtils::FindMatchingValueSorted(double srcVal,
                                      const WDL_TypedBuf<double> &sortedSrcValues,
                                      const WDL_TypedBuf<double> &sortedDstValues);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::FactorToDivFactor(FLOAT_TYPE val, FLOAT_TYPE coeff)
{
    FLOAT_TYPE res = std::pow(coeff, val);
    
    return res;
}
template float BLUtils::FactorToDivFactor(float val, float coeff);
template double BLUtils::FactorToDivFactor(double val, double coeff);

template <typename FLOAT_TYPE>
void
BLUtils::ShiftSamples(const WDL_TypedBuf<FLOAT_TYPE> *ioSamples, int shiftSize)
{
    if (shiftSize < 0)
        shiftSize += ioSamples->GetSize();
    
    WDL_TypedBuf<FLOAT_TYPE> copySamples = *ioSamples;
    
    int ioSamplesSize = ioSamples->GetSize();
    FLOAT_TYPE *ioSamplesData = ioSamples->Get();
    FLOAT_TYPE *copySamplesData = copySamples.Get();
    
    for (int i = 0; i < ioSamplesSize; i++)
    {
        int srcIndex = i;
        int dstIndex = (srcIndex + shiftSize) % ioSamplesSize;
        
        ioSamplesData[dstIndex] = copySamplesData[srcIndex];
    }
}
template void BLUtils::ShiftSamples(const WDL_TypedBuf<float> *ioSamples, int shiftSize);
template void BLUtils::ShiftSamples(const WDL_TypedBuf<double> *ioSamples, int shiftSize);

template <typename FLOAT_TYPE>
void
BLUtils::ComputeEnvelope(const WDL_TypedBuf<FLOAT_TYPE> &samples,
                       WDL_TypedBuf<FLOAT_TYPE> *envelope,
                       bool extendBoundsValues)
{
    WDL_TypedBuf<FLOAT_TYPE> maxValues;
    maxValues.Resize(samples.GetSize());
    BLUtils::FillAllZero(&maxValues);
    
    // First step: put the maxima in the array
    FLOAT_TYPE prevSamples[3] = { 0.0, 0.0, 0.0 };
    bool zeroWasCrossed = false;
    FLOAT_TYPE prevValue = 0.0;
    
    int samplesSize = samples.GetSize();
    FLOAT_TYPE *samplesData = samples.Get();
    int maxValuesSize = maxValues.GetSize();
    FLOAT_TYPE *maxValuesData = maxValues.Get();
    
    for (int i = 0; i < samplesSize; i++)
    {
        FLOAT_TYPE sample = samplesData[i];
        
        // Wait for crossing the zero line a first time
        if (i == 0)
        {
            prevValue = sample;
            
            continue;
        }
        
        if (!zeroWasCrossed)
        {
            if (prevValue*sample < 0.0)
            {
                zeroWasCrossed = true;
            }
            
            prevValue = sample;
            
            // Before first zero cross, we don't take the maximum
            continue;
        }
        
        sample = std::fabs(sample);
        
        prevSamples[0] = prevSamples[1];
        prevSamples[1] = prevSamples[2];
        prevSamples[2] = sample;
        
        if ((prevSamples[1] >= prevSamples[0]) &&
           (prevSamples[1] >= prevSamples[2]))
           // Local maximum
        {
            int idx = i - 1;
            if (idx < 0)
                idx = 0;
            maxValuesData[idx] = prevSamples[1];
        }
    }
    
    // Suppress the last maximum until zero is crossed
    // (avoids finding maxima from edges of truncated periods)
    FLOAT_TYPE prevValue2 = 0.0;
    for (int i = samplesSize - 1; i > 0; i--)
    {
        FLOAT_TYPE sample = samplesData[i];
        if (prevValue2*sample < 0.0)
            // Zero is crossed !
        {
            break;
        }
        
        prevValue2 = sample;
        
        // Suppress potential false maxima
        maxValuesData[i] = 0.0;
    }
    
    // Should be defined to 1 !
#if 0 // TODO: check it and validate it (code factoring :) ) !
    *envelope = maxValues;
    
    FillMissingValues(envelope, extendBoundsValues);
    
#else
    if (extendBoundsValues)
        // Extend the last maximum to the end
    {
        // Find the last max
        int lastMaxIndex = samplesSize - 1;
        FLOAT_TYPE lastMax = 0.0;
        for (int i = samplesSize - 1; i > 0; i--)
        {
            FLOAT_TYPE val = maxValuesData[i];
            if (val > 0.0)
            {
                lastMax = val;
                lastMaxIndex = i;
            
                break;
            }
        }
    
        // Fill the last values with last max
        for (int i = samplesSize - 1; i > lastMaxIndex; i--)
        {
            maxValuesData[i] = lastMax;
        }
    }
    
    // Second step: fill the holes by linear interpolation
    //envelope->Resize(samples.GetSize());
    //BLUtils::FillAllZero(envelope);
    *envelope = maxValues;
    
    FLOAT_TYPE startVal = 0.0;
    
    // First, find start val
    for (int i = 0; i < maxValuesSize; i++)
    {
        FLOAT_TYPE val = maxValuesData[i];
        if (val > 0.0)
        {
            startVal = val;
        }
    }
    
    int loopIdx = 0;
    int startIndex = 0;
    //FLOAT_TYPE lastValidVal = 0.0;
    
    // Then start the main loop
    
    int envelopeSize = envelope->GetSize();
    FLOAT_TYPE *envelopeData = envelope->Get();
    
    while(loopIdx < envelopeSize)
    {
        FLOAT_TYPE val = maxValuesData[loopIdx];
        
        if (val > 0.0)
            // Defined
        {
            startVal = val;
            startIndex = loopIdx;
            
            loopIdx++;
        }
        else
            // Undefined
        {
            // Start at 0
            if (!extendBoundsValues &&
                (loopIdx == 0))
                startVal = 0.0;
            
            // Find how many missing values we have
            int endIndex = startIndex + 1;
            FLOAT_TYPE endVal = 0.0;
            bool defined = false;
            
            while(endIndex < maxValuesSize)
            {
                if (endIndex < maxValuesSize)
                    endVal = maxValuesData[endIndex];
                
                defined = (endVal > 0.0);
                if (defined)
                    break;
                
                endIndex++;
            }
    
#if 0 // Make problems with envelopes ending with zeros
            if (defined)
            {
                lastValidVal = endVal;
            }
            else
                // Not found at the end
            {
                endVal = lastValidVal;
            }
#endif
            
            // Fill the missing values with lerp
            for (int i = startIndex; i < endIndex; i++)
            {
                FLOAT_TYPE t = ((FLOAT_TYPE)(i - startIndex))/(endIndex - startIndex - 1);
                
                FLOAT_TYPE newVal = (1.0 - t)*startVal + t*endVal;
                envelopeData[i] = newVal;
            }
            
            startIndex = endIndex;
            loopIdx = endIndex;
        }
    }
#endif
}
template void BLUtils::ComputeEnvelope(const WDL_TypedBuf<float> &samples,
                            WDL_TypedBuf<float> *envelope,
                            bool extendBoundsValues);
template void BLUtils::ComputeEnvelope(const WDL_TypedBuf<double> &samples,
                            WDL_TypedBuf<double> *envelope,
                            bool extendBoundsValues);

// GOOD: makes good linerp !
// And fixed NaN
template <typename FLOAT_TYPE>
void
BLUtils::FillMissingValues(WDL_TypedBuf<FLOAT_TYPE> *values,
                         bool extendBounds, FLOAT_TYPE undefinedValue)
{
    if (extendBounds)
    // Extend the last value to the end
    {
        // Find the last max
        int lastIndex = values->GetSize() - 1;
        FLOAT_TYPE lastValue = 0.0;
        
        int valuesSize = values->GetSize();
        FLOAT_TYPE *valuesData = values->Get();
        
        for (int i = valuesSize - 1; i > 0; i--)
        {
            FLOAT_TYPE val = valuesData[i];
            if (val > undefinedValue)
            {
                lastValue = val;
                lastIndex = i;
                
                break;
            }
        }
        
        // Fill the last values with last max
        for (int i = valuesSize - 1; i > lastIndex; i--)
        {
            valuesData[i] = lastValue;
        }
    }
    
    // Fill the holes by linear interpolation
    FLOAT_TYPE startVal = 0.0;
    
    // First, find start val
    
    int valuesSize = values->GetSize();
    FLOAT_TYPE *valuesData = values->Get();
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE val = valuesData[i];
        if (val > undefinedValue)
        {
            startVal = val;
        }
    }
    
    int loopIdx = 0;
    int startIndex = 0;
    //FLOAT_TYPE lastValidVal = 0.0;
    
    // Then start the main loop
    while(loopIdx < valuesSize)
    {
        FLOAT_TYPE val = valuesData[loopIdx];
        
        if (val > undefinedValue)
            // Defined
        {
            startVal = val;
            startIndex = loopIdx;
            
            loopIdx++;
        }
        else
            // Undefined
        {
            // Start at 0
            if (!extendBounds &&
                (loopIdx == 0))
                startVal = 0.0;
            
            // Find how many missing values we have
            int endIndex = startIndex + 1;
            FLOAT_TYPE endVal = 0.0;
            bool defined = false;
            
            while(endIndex < valuesSize)
            {
                if (endIndex < valuesSize)
                    endVal = valuesData[endIndex];
                
                defined = (endVal > undefinedValue);
                if (defined)
                    break;
                
                endIndex++;
            }
            
#if 0 // Make problems with series ending with zeros
            if (defined)
            {
                lastValidVal = endVal;
            }
            else
                // Not found at the end
            {
                endVal = lastValidVal;
            }
#endif
            
            // Fill the missing values with lerp
            for (int i = startIndex; i < endIndex; i++)
            {
                // FIX "+1": avoid NaN, and better linerp !
                FLOAT_TYPE t = ((FLOAT_TYPE)(i - startIndex))/(endIndex - startIndex /*+ 1*/);
                
                FLOAT_TYPE newVal = (1.0 - t)*startVal + t*endVal;
                    
                valuesData[i] = newVal;
            }
            
            startIndex = endIndex;
            loopIdx = endIndex;
        }
    }
}
template void BLUtils::FillMissingValues(WDL_TypedBuf<float> *values,
                              bool extendBounds, float undefinedValue);
template void BLUtils::FillMissingValues(WDL_TypedBuf<double> *values,
                              bool extendBounds, double undefinedValue);

template <typename FLOAT_TYPE>
void
BLUtils::FillMissingValues2(WDL_TypedBuf<FLOAT_TYPE> *values,
                          bool extendBounds, FLOAT_TYPE undefinedValue)
{
    if (extendBounds)
        // Extend the last value to the end
    {
        // Find the last max
        int lastIndex = values->GetSize() - 1;
        FLOAT_TYPE lastValue = undefinedValue;
        
        int valuesSize = values->GetSize();
        FLOAT_TYPE *valuesData = values->Get();
        
        for (int i = valuesSize - 1; i > 0; i--)
        {
            FLOAT_TYPE val = valuesData[i];
            if (val > undefinedValue)
            {
                lastValue = val;
                lastIndex = i;
                
                break;
            }
        }
        
        // Fill the last values with last max
        for (int i = valuesSize - 1; i > lastIndex; i--)
        {
            valuesData[i] = lastValue;
        }
    }
    
    // Fill the holes by linear interpolation
    FLOAT_TYPE startVal = undefinedValue;
    
    // First, find start val
    
    int valuesSize = values->GetSize();
    FLOAT_TYPE *valuesData = values->Get();
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE val = valuesData[i];
        if (val > undefinedValue)
        {
            startVal = val;
        }
    }
    
    int loopIdx = 0;
    int startIndex = 0;
    //FLOAT_TYPE lastValidVal = 0.0;
    
    // Then start the main loop
    while(loopIdx < valuesSize)
    {
        FLOAT_TYPE val = valuesData[loopIdx];
        
        if (val > undefinedValue)
            // Defined
        {
            startVal = val;
            startIndex = loopIdx;
            
            loopIdx++;
        }
        else
            // Undefined
        {
            // Start at 0
            if (!extendBounds &&
                (loopIdx == 0))
                startVal = undefinedValue;
            
            // Find how many missing values we have
            int endIndex = startIndex + 1;
            FLOAT_TYPE endVal = undefinedValue;
            bool defined = false;
            
            while(endIndex < valuesSize)
            {
                if (endIndex < valuesSize)
                    endVal = valuesData[endIndex];
                
                defined = (endVal > undefinedValue);
                if (defined)
                    break;
                
                endIndex++;
            }
            
#if 0 // Make problems with series ending with zeros
            if (defined)
            {
                lastValidVal = endVal;
            }
            else
                // Not found at the end
            {
                endVal = lastValidVal;
            }
#endif
            
            // Fill the missing values with lerp
            for (int i = startIndex; i < endIndex; i++)
            {
                // FIX "+1": avoid NaN, and better linerp !
                FLOAT_TYPE t = ((FLOAT_TYPE)(i - startIndex))/(endIndex - startIndex /*+ 1*/);
                
                FLOAT_TYPE newVal = (1.0 - t)*startVal + t*endVal;
                
                valuesData[i] = newVal;
            }
            
            startIndex = endIndex;
            loopIdx = endIndex;
        }
    }
}
template void BLUtils::FillMissingValues2(WDL_TypedBuf<float> *values,
                               bool extendBounds, float undefinedValue);
template void BLUtils::FillMissingValues2(WDL_TypedBuf<double> *values,
                               bool extendBounds, double undefinedValue);

template <typename FLOAT_TYPE>
void
BLUtils::FillMissingValues3(WDL_TypedBuf<FLOAT_TYPE> *values,
                          bool extendBounds, FLOAT_TYPE undefinedValue)
{
    if (extendBounds)
        // Extend the last value to the end
    {
        // Find the last max
        int lastIndex = values->GetSize() - 1;
        FLOAT_TYPE lastValue = undefinedValue;
        
        int valuesSize = values->GetSize();
        FLOAT_TYPE *valuesData = values->Get();
        
        for (int i = valuesSize - 1; i > 0; i--)
        {
            FLOAT_TYPE val = valuesData[i];
            if (val > undefinedValue)
            {
                lastValue = val;
                lastIndex = i;
                
                break;
            }
        }
        
        // Fill the last values with last max
        for (int i = valuesSize - 1; i > lastIndex; i--)
        {
            valuesData[i] = lastValue;
        }
    }
    
    // Fill the holes by linear interpolation
    FLOAT_TYPE startVal = undefinedValue;
    
    // First, find start val
    
    int valuesSize = values->GetSize();
    FLOAT_TYPE *valuesData = values->Get();
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE val = valuesData[i];
        if (val > undefinedValue)
        {
            startVal = val;
            
            break; // NEW
        }
    }
    
    int loopIdx = 0;
    int startIndex = 0;
    //FLOAT_TYPE lastValidVal = 0.0;
    
    // Then start the main loop
    while(loopIdx < valuesSize)
    {
        FLOAT_TYPE val = valuesData[loopIdx];
        
        if (val > undefinedValue)
            // Defined
        {
            startVal = val;
            startIndex = loopIdx;
            
            loopIdx++;
        }
        else
            // Undefined
        {
            // Start at 0
            if (!extendBounds &&
                (loopIdx == 0))
                startVal = undefinedValue;
            
            // Find how many missing values we have
            int endIndex = startIndex + 1;
            FLOAT_TYPE endVal = undefinedValue;
            bool defined = false;
            
            while(endIndex < valuesSize)
            {
                if (endIndex < valuesSize)
                    endVal = valuesData[endIndex];
                
                defined = (endVal > undefinedValue);
                if (defined)
                    break;
                
                endIndex++;
            }
            
#if 0 // Make problems with series ending with zeros
            if (defined)
            {
                lastValidVal = endVal;
            }
            else
                // Not found at the end
            {
                endVal = lastValidVal;
            }
#endif
            
            // Fill the missing values with lerp
            for (int i = startIndex; i < endIndex; i++)
            {
                // FIX "+1": avoid NaN, and better linerp !
                FLOAT_TYPE t = ((FLOAT_TYPE)(i - startIndex))/(endIndex - startIndex /*+ 1*/);
                
                FLOAT_TYPE newVal = (1.0 - t)*startVal + t*endVal;
                
                valuesData[i] = newVal;
            }
            
            startIndex = endIndex;
            loopIdx = endIndex;
        }
    }
}
template void BLUtils::FillMissingValues3(WDL_TypedBuf<float> *values,
                               bool extendBounds, float undefinedValue);
template void BLUtils::FillMissingValues3(WDL_TypedBuf<double> *values,
                               bool extendBounds, double undefinedValue);

// Smooth, then compute envelope
template <typename FLOAT_TYPE>
void
BLUtils::ComputeEnvelopeSmooth(const WDL_TypedBuf<FLOAT_TYPE> &samples,
                             WDL_TypedBuf<FLOAT_TYPE> *envelope,
                             FLOAT_TYPE smoothCoeff,
                             bool extendBoundsValues)
{
    WDL_TypedBuf<FLOAT_TYPE> smoothedSamples;
    smoothedSamples.Resize(samples.GetSize());
                           
    FLOAT_TYPE cmaCoeff = smoothCoeff*samples.GetSize();
    
    WDL_TypedBuf<FLOAT_TYPE> samplesAbs = samples;
    BLUtils::ComputeAbs(&samplesAbs);
    
    CMA2Smoother::ProcessOne(samplesAbs.Get(), smoothedSamples.Get(),
                             samplesAbs.GetSize(), cmaCoeff);
    
    
    // Restore the sign, for envelope computation
    
    int samplesSize = samples.GetSize();
    FLOAT_TYPE *samplesData = samples.Get();
    FLOAT_TYPE *smoothedSamplesData = smoothedSamples.Get();
    
    for (int i = 0; i < samplesSize; i++)
    {
        FLOAT_TYPE sample = samplesData[i];
        
        if (sample < 0.0)
            smoothedSamplesData[i] *= -1.0;
    }
    
    ComputeEnvelope(smoothedSamples, envelope, extendBoundsValues);
}
template void BLUtils::ComputeEnvelopeSmooth(const WDL_TypedBuf<float> &samples,
                                  WDL_TypedBuf<float> *envelope,
                                  float smoothCoeff,
                                  bool extendBoundsValues);
template void BLUtils::ComputeEnvelopeSmooth(const WDL_TypedBuf<double> &samples,
                                  WDL_TypedBuf<double> *envelope,
                                  double smoothCoeff,
                                  bool extendBoundsValues);

// Compute an envelope by only smoothing
template <typename FLOAT_TYPE>
void
BLUtils::ComputeEnvelopeSmooth2(const WDL_TypedBuf<FLOAT_TYPE> &samples,
                              WDL_TypedBuf<FLOAT_TYPE> *envelope,
                              FLOAT_TYPE smoothCoeff)
{
    envelope->Resize(samples.GetSize());
    
    FLOAT_TYPE cmaCoeff = smoothCoeff*samples.GetSize();
    
    WDL_TypedBuf<FLOAT_TYPE> samplesAbs = samples;
    BLUtils::ComputeAbs(&samplesAbs);
    
    CMA2Smoother::ProcessOne(samplesAbs.Get(), envelope->Get(),
                             samplesAbs.GetSize(), cmaCoeff);
    
    // Normalize
    // Because CMA2Smoother reduce the values
    
    FLOAT_TYPE maxSamples = BLUtils::ComputeMax(samples.Get(), samples.GetSize());
    FLOAT_TYPE maxEnvelope = BLUtils::ComputeMax(envelope->Get(), envelope->GetSize());
    
    if (maxEnvelope > BL_EPS)
    {
        FLOAT_TYPE coeff = maxSamples/maxEnvelope;
        BLUtils::MultValues(envelope, coeff);
    }
}
template void BLUtils::ComputeEnvelopeSmooth2(const WDL_TypedBuf<float> &samples,
                                   WDL_TypedBuf<float> *envelope,
                                   float smoothCoeff);
template void BLUtils::ComputeEnvelopeSmooth2(const WDL_TypedBuf<double> &samples,
                                   WDL_TypedBuf<double> *envelope,
                                   double smoothCoeff);

template <typename FLOAT_TYPE>
void
BLUtils::ZeroBoundEnvelope(WDL_TypedBuf<FLOAT_TYPE> *envelope)
{
    if (envelope->GetSize() == 0)
        return;
    
    envelope->Get()[0] = 0.0;
    envelope->Get()[envelope->GetSize() - 1] = 0.0;
}
template void BLUtils::ZeroBoundEnvelope(WDL_TypedBuf<float> *envelope);
template void BLUtils::ZeroBoundEnvelope(WDL_TypedBuf<double> *envelope);

template <typename FLOAT_TYPE>
void
BLUtils::ScaleNearest(WDL_TypedBuf<FLOAT_TYPE> *values, int factor)
{
    WDL_TypedBuf<FLOAT_TYPE> newValues;
    newValues.Resize(values->GetSize()*factor);
    
    int newValuesSize = newValues.GetSize();
    FLOAT_TYPE *newValuesData = newValues.Get();
    FLOAT_TYPE *valuesData = values->Get();
    
    for (int i = 0; i < newValuesSize; i++)
    {
        FLOAT_TYPE val = valuesData[i/factor];
        newValuesData[i] = val;
    }
    
    *values = newValues;
}
template void BLUtils::ScaleNearest(WDL_TypedBuf<float> *values, int factor);
template void BLUtils::ScaleNearest(WDL_TypedBuf<double> *values, int factor);

template <typename FLOAT_TYPE>
int
BLUtils::FindMaxIndex(const WDL_TypedBuf<FLOAT_TYPE> &values)
{
    int maxIndex = -1;
    FLOAT_TYPE maxValue = -1e15;
    
    int valuesSize = values.GetSize();
    FLOAT_TYPE *valuesData = values.Get();
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE value = valuesData[i];
        
        if (value > maxValue)
        {
            maxValue = value;
            maxIndex = i;
        }
    }
    
    return maxIndex;
}
template int BLUtils::FindMaxIndex(const WDL_TypedBuf<float> &values);
template int BLUtils::FindMaxIndex(const WDL_TypedBuf<double> &values);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::FindMaxValue(const WDL_TypedBuf<FLOAT_TYPE> &values)
{
#if USE_SIMD_OPTIM
    // This is the same function...
    FLOAT_TYPE maxValue0 = ComputeMax(values);
    
    return maxValue0;
#endif
    
    FLOAT_TYPE maxValue = -1e15;
    
    int valuesSize = values.GetSize();
    FLOAT_TYPE *valuesData = values.Get();
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE value = valuesData[i];
        
        if (value > maxValue)
        {
            maxValue = value;
        }
    }
    
    return maxValue;
}
template float BLUtils::FindMaxValue(const WDL_TypedBuf<float> &values);
template double BLUtils::FindMaxValue(const WDL_TypedBuf<double> &values);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::FindMaxValue(const vector<WDL_TypedBuf<FLOAT_TYPE> > &values)
{
    FLOAT_TYPE maxValue = -1e15;
    
    for (int i = 0; i < values.size(); i++)
    {
#if !USE_SIMD_OPTIM
        for (int j = 0; j < values[i].GetSize(); j++)
        {
            FLOAT_TYPE value = values[i].Get()[j];
        
            if (value > maxValue)
            {
                maxValue = value;
            }
        }
#else
        FLOAT_TYPE maxValue0 = ComputeMax(values[i]);
        if (maxValue0 > maxValue)
            maxValue = maxValue0;
#endif
    }
    
    return maxValue;
}
template float BLUtils::FindMaxValue(const vector<WDL_TypedBuf<float> > &values);
template double BLUtils::FindMaxValue(const vector<WDL_TypedBuf<double> > &values);

template <typename FLOAT_TYPE>
void
BLUtils::ComputeAbs(WDL_TypedBuf<FLOAT_TYPE> *values)
{
    int valuesSize = values->GetSize();
    FLOAT_TYPE *valuesData = values->Get();
    
#if USE_SIMD
    if (_useSimd && (valuesSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < valuesSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(valuesData);
            
            simdpp::float64<SIMD_PACK_SIZE> r = simdpp::abs(v0);
            
            simdpp::store(valuesData, r);
            
            valuesData += SIMD_PACK_SIZE;
        }
        
        // Finished
        return;
    }
#endif
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE value = valuesData[i];
        
        value = std::fabs(value);
        
        valuesData[i] = value;
    }
}
template void BLUtils::ComputeAbs(WDL_TypedBuf<float> *values);
template void BLUtils::ComputeAbs(WDL_TypedBuf<double> *values);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::LogScaleNormInv(FLOAT_TYPE value, FLOAT_TYPE maxValue, FLOAT_TYPE factor)
{
    FLOAT_TYPE result = value/maxValue;
    
    //result = LogScale(result, factor);
    result *= std::exp(factor) - 1.0;
    result += 1.0;
    result = std::log(result);
    result /= factor;
    
    result *= maxValue;
    
    return result;
}
template float BLUtils::LogScaleNormInv(float value, float maxValue, float factor);
template double BLUtils::LogScaleNormInv(double value, double maxValue, double factor);

// values should be already normalized before calling the function
template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::LogScaleNorm2(FLOAT_TYPE value, FLOAT_TYPE factor)
{
    FLOAT_TYPE result = std::log((BL_FLOAT)(1.0 + value*factor))/std::log((BL_FLOAT)(1.0 + factor));
    
    return result;
}
template float BLUtils::LogScaleNorm2(float value, float factor);
template double BLUtils::LogScaleNorm2(double value, double factor);

// values should be already normalized before calling the function
template <typename FLOAT_TYPE>
void
BLUtils::LogScaleNorm2(WDL_TypedBuf<FLOAT_TYPE> *values, FLOAT_TYPE factor)
{
    WDL_TypedBuf<FLOAT_TYPE> resultValues;
    resultValues.Resize(values->GetSize());
    
    for (int i = 0; i < values->GetSize(); i++)
    {
        FLOAT_TYPE val = values->Get()[i];
        
        val = std::log((BL_FLOAT)(1.0 + val*factor))/std::log((BL_FLOAT)(1.0 + factor));
        
        resultValues.Get()[i] = val;
    }
    
    *values = resultValues;
}
template void BLUtils::LogScaleNorm2(WDL_TypedBuf<float> *values, float factor);
template void BLUtils::LogScaleNorm2(WDL_TypedBuf<double> *values, double factor);

// NOTE: not tested (done specifically for MetaSoundViewer, no other use for the moment)
// values should be already normalized before calling the function
template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::LogScaleNormInv2(FLOAT_TYPE value, FLOAT_TYPE factor)
{
    FLOAT_TYPE result = std::exp(value*factor)/std::exp(factor) - 1.0;
        
    return result;
}
template float BLUtils::LogScaleNormInv2(float value, float factor);
template double BLUtils::LogScaleNormInv2(double value, double factor);

// NOTE: not tested (done specifically for MetaSoundViewer, no other use for the moment)
// values should be already normalized before calling the function
template <typename FLOAT_TYPE>
void
BLUtils::LogScaleNormInv2(WDL_TypedBuf<FLOAT_TYPE> *values, FLOAT_TYPE factor)
{
    WDL_TypedBuf<FLOAT_TYPE> resultValues;
    resultValues.Resize(values->GetSize());
    
    for (int i = 0; i < values->GetSize(); i++)
    {
        FLOAT_TYPE val = values->Get()[i];
        
        val = std::exp(val*factor)/std::exp(factor) - 1.0;
        
        resultValues.Get()[i] = val;
    }
    
    *values = resultValues;
}
template void BLUtils::LogScaleNormInv2(WDL_TypedBuf<float> *values, float factor);
template void BLUtils::LogScaleNormInv2(WDL_TypedBuf<double> *values, double factor);

template <typename FLOAT_TYPE>
void
BLUtils::FreqsToLogNorm(WDL_TypedBuf<FLOAT_TYPE> *resultMagns,
                      const WDL_TypedBuf<FLOAT_TYPE> &magns,
                      FLOAT_TYPE hzPerBin)
{
    BLUtils::ResizeFillZeros(resultMagns, magns.GetSize());
    
    FLOAT_TYPE maxFreq = hzPerBin*(magns.GetSize() - 1);
    FLOAT_TYPE maxLog = std::log10(maxFreq);
    
    int resultMagnsSize = resultMagns->GetSize();
    FLOAT_TYPE *resultMagnsData = resultMagns->Get();
    
    int magnsSize = magns.GetSize();
    FLOAT_TYPE *magnsData = magns.Get();
    
    for (int i = 0; i < resultMagnsSize; i++)
    {
        FLOAT_TYPE logVal = i*maxLog/resultMagnsSize;
        FLOAT_TYPE freq = std::pow((FLOAT_TYPE)10.0, logVal);
        
        if (maxFreq < BL_EPS)
            return;
        
        FLOAT_TYPE id0 = (freq/maxFreq) * resultMagnsSize;
        FLOAT_TYPE t = id0 - (int)(id0);
        
        if ((int)id0 >= magnsSize)
            continue;
        
        int id1 = id0 + 1;
        if (id1 >= magnsSize)
            continue;
        
        FLOAT_TYPE magn0 = magnsData[(int)id0];
        FLOAT_TYPE magn1 = magnsData[id1];
        
        FLOAT_TYPE magn = (1.0 - t)*magn0 + t*magn1;
        
        resultMagnsData[i] = magn;
    }
}
template void BLUtils::FreqsToLogNorm(WDL_TypedBuf<float> *resultMagns,
                           const WDL_TypedBuf<float> &magns,
                           float hzPerBin);
template void BLUtils::FreqsToLogNorm(WDL_TypedBuf<double> *resultMagns,
                           const WDL_TypedBuf<double> &magns,
                           double hzPerBin);


template <typename FLOAT_TYPE>
void
BLUtils::LogToFreqsNorm(WDL_TypedBuf<FLOAT_TYPE> *resultMagns,
                      const WDL_TypedBuf<FLOAT_TYPE> &magns,
                      FLOAT_TYPE hzPerBin)
{
    BLUtils::ResizeFillZeros(resultMagns, magns.GetSize());
    
    FLOAT_TYPE maxFreq = hzPerBin*(magns.GetSize() - 1);
    FLOAT_TYPE maxLog = std::log10(maxFreq);
    
    int resultMagnsSize = resultMagns->GetSize();
    FLOAT_TYPE *resultMagnsData = resultMagns->Get();
    int magnsSize = magns.GetSize();
    FLOAT_TYPE *magnsData = magns.Get();
    
    for (int i = 0; i < resultMagnsSize; i++)
    {
        FLOAT_TYPE freq = hzPerBin*i;
        FLOAT_TYPE logVal = 0.0;
        // Check for log(0) => -inf
        if (freq > 0)
            logVal = std::log10(freq);
        
        FLOAT_TYPE id0 = (logVal/maxLog) * resultMagnsSize;
        
        if ((int)id0 >= magnsSize)
            continue;
        
        // Linear
        FLOAT_TYPE t = id0 - (int)(id0);
        
        int id1 = id0 + 1;
        if (id1 >= magnsSize)
            continue;
        
        FLOAT_TYPE magn0 = magnsData[(int)id0];
        FLOAT_TYPE magn1 = magnsData[id1];
        
        FLOAT_TYPE magn = (1.0 - t)*magn0 + t*magn1;
        
        // Nearest
        //FLOAT_TYPE magn = magns.Get()[(int)id0];
        
        resultMagnsData[i] = magn;
    }
}
template void BLUtils::LogToFreqsNorm(WDL_TypedBuf<float> *resultMagns,
                           const WDL_TypedBuf<float> &magns,
                           float hzPerBin);
template void BLUtils::LogToFreqsNorm(WDL_TypedBuf<double> *resultMagns,
                           const WDL_TypedBuf<double> &magns,
                           double hzPerBin);

template <typename FLOAT_TYPE>
void
BLUtils::FreqsToDbNorm(WDL_TypedBuf<FLOAT_TYPE> *resultMagns,
                     const WDL_TypedBuf<FLOAT_TYPE> &magns,
                     FLOAT_TYPE hzPerBin,
                     FLOAT_TYPE minValue, FLOAT_TYPE maxValue)
{
    BLUtils::ResizeFillZeros(resultMagns, magns.GetSize());
    
    //FLOAT_TYPE maxFreq = hzPerBin*(magns.GetSize() - 1);
    //FLOAT_TYPE maxDb = BLUtils::NormalizedXTodB(1.0, minValue, maxValue);
    
    // We work in normalized coordinates
    FLOAT_TYPE maxFreq = 1.0;
    
    int resultMagnsSize = resultMagns->GetSize();
    FLOAT_TYPE *resultMagnsData = resultMagns->Get();
    int magnsSize = magns.GetSize();
    FLOAT_TYPE *magnsData = magns.Get();
    
    for (int i = 0; i < resultMagnsSize; i++)
    {
        FLOAT_TYPE dbVal = ((FLOAT_TYPE)i)/resultMagnsSize;
        FLOAT_TYPE freq = BLUtils::NormalizedXTodBInv(dbVal, minValue, maxValue);
        
        if (maxFreq < BL_EPS)
            return;
        
        FLOAT_TYPE id0 = (freq/maxFreq) * resultMagnsSize;
        FLOAT_TYPE t = id0 - (int)(id0);
        
        if ((int)id0 >= magnsSize)
            continue;
        
        int id1 = id0 + 1;
        if (id1 >= magnsSize)
            continue;
        
        FLOAT_TYPE magn0 = magnsData[(int)id0];
        FLOAT_TYPE magn1 = magnsData[id1];
        
        FLOAT_TYPE magn = (1.0 - t)*magn0 + t*magn1;
        
        resultMagnsData[i] = magn;
    }
}
template void BLUtils::FreqsToDbNorm(WDL_TypedBuf<float> *resultMagns,
                                   const WDL_TypedBuf<float> &magns,
                                   float hzPerBin,
                                   float minValue, float maxValue);
template void BLUtils::FreqsToDbNorm(WDL_TypedBuf<double> *resultMagns,
                                   const WDL_TypedBuf<double> &magns,
                                   double hzPerBin,
                                   double minValue, double maxValue);

// TODO: need to check this
template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::LogNormToFreq(int idx, FLOAT_TYPE hzPerBin, int bufferSize)
{
    FLOAT_TYPE maxFreq = hzPerBin*(bufferSize - 1);
    FLOAT_TYPE maxLog = std::log10(maxFreq);
    
    FLOAT_TYPE freq = hzPerBin*idx;
    FLOAT_TYPE logVal = std::log10(freq);
    
    FLOAT_TYPE id0 = (logVal/maxLog) * bufferSize;
    
    FLOAT_TYPE result = id0*hzPerBin;
    
    return result;
}
template float BLUtils::LogNormToFreq(int idx, float hzPerBin, int bufferSize);
template double BLUtils::LogNormToFreq(int idx, double hzPerBin, int bufferSize);

// GOOD !
template <typename FLOAT_TYPE>
int
BLUtils::FreqIdToLogNormId(int idx, FLOAT_TYPE hzPerBin, int bufferSize)
{
    FLOAT_TYPE maxFreq = hzPerBin*(bufferSize/2);
    FLOAT_TYPE maxLog = std::log10(maxFreq);
    
    FLOAT_TYPE freq = hzPerBin*idx;
    FLOAT_TYPE logVal = std::log10(freq);
        
    FLOAT_TYPE resultId = (logVal/maxLog)*(bufferSize/2);
    
    resultId = bl_round(resultId);
    if (resultId < 0)
        resultId = 0;
    if (resultId > bufferSize/2 - 1)
        resultId = bufferSize/2;
    
    return resultId;
}
template int BLUtils::FreqIdToLogNormId(int idx, float hzPerBin, int bufferSize);
template int BLUtils::FreqIdToLogNormId(int idx, double hzPerBin, int bufferSize);

template <typename FLOAT_TYPE>
void
BLUtils::ApplyWindow(WDL_TypedBuf<FLOAT_TYPE> *values,
                   const WDL_TypedBuf<FLOAT_TYPE> &window)
{
    if (values->GetSize() != window.GetSize())
        return;
    
#if USE_SIMD_OPTIM
    MultValues(values, window);
    
    return;
#endif
    
    int valuesSize = values->GetSize();
    FLOAT_TYPE *valuesData = values->Get();
    FLOAT_TYPE *windowData = window.Get();
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE val = valuesData[i];
        FLOAT_TYPE w = windowData[i];
        
        val *= w;
        
        valuesData[i] = val;
    }
}
template void BLUtils::ApplyWindow(WDL_TypedBuf<float> *values,
                        const WDL_TypedBuf<float> &window);
template void BLUtils::ApplyWindow(WDL_TypedBuf<double> *values,
                        const WDL_TypedBuf<double> &window);


template <typename FLOAT_TYPE>
void
BLUtils::ApplyWindowRescale(WDL_TypedBuf<FLOAT_TYPE> *values,
                          const WDL_TypedBuf<FLOAT_TYPE> &window)
{
    FLOAT_TYPE coeff = ((FLOAT_TYPE)window.GetSize())/values->GetSize();
    
    int valuesSize = values->GetSize();
    FLOAT_TYPE *valuesData = values->Get();
    int windowSize = window.GetSize();
    FLOAT_TYPE *windowData = window.Get();
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE val = valuesData[i];
        
        int winIdx = i*coeff;
        if (winIdx >= windowSize)
            continue;
        
        FLOAT_TYPE w = windowData[winIdx];
        
        val *= w;
        
        valuesData[i] = val;
    }
}
template void BLUtils::ApplyWindowRescale(WDL_TypedBuf<float> *values,
                               const WDL_TypedBuf<float> &window);
template void BLUtils::ApplyWindowRescale(WDL_TypedBuf<double> *values,
                               const WDL_TypedBuf<double> &window);

template <typename FLOAT_TYPE>
void
BLUtils::ApplyWindowFft(WDL_TypedBuf<FLOAT_TYPE> *ioMagns,
                      const WDL_TypedBuf<FLOAT_TYPE> &phases,
                      const WDL_TypedBuf<FLOAT_TYPE> &window)
{
    WDL_TypedBuf<int> samplesIds;
    BLUtils::FftIdsToSamplesIds(phases, &samplesIds);
    
    WDL_TypedBuf<FLOAT_TYPE> sampleMagns = *ioMagns;
    
    BLUtils::Permute(&sampleMagns, samplesIds, true);
    
    BLUtils::ApplyWindow(&sampleMagns, window);
    
    BLUtils::Permute(&sampleMagns, samplesIds, false);
    
#if 1
    // Must multiply by 2... why ?
    //
    // Maybe due to hanning window normalization of windows
    //
    BLUtils::MultValues(&sampleMagns, (FLOAT_TYPE)2.0);
#endif
    
    *ioMagns = sampleMagns;
}
template void BLUtils::ApplyWindowFft(WDL_TypedBuf<float> *ioMagns,
                           const WDL_TypedBuf<float> &phases,
                           const WDL_TypedBuf<float> &window);
template void BLUtils::ApplyWindowFft(WDL_TypedBuf<double> *ioMagns,
                           const WDL_TypedBuf<double> &phases,
                           const WDL_TypedBuf<double> &window);

// boundSize is used to not divide by the extremities, which are often zero
template <typename FLOAT_TYPE>
void
BLUtils::UnapplyWindow(WDL_TypedBuf<FLOAT_TYPE> *values, const WDL_TypedBuf<FLOAT_TYPE> &window,
                     int boundSize)
{
#define EPS 1e-6
    
    if (values->GetSize() != window.GetSize())
        return;
    
    FLOAT_TYPE max0 = BLUtils::ComputeMaxAbs(*values);
    
    // First, apply be the invers window
    int start = boundSize;
    
    int valuesSize = values->GetSize();
    FLOAT_TYPE *valuesData = values->Get();
    FLOAT_TYPE *windowData = window.Get();
    
    for (int i = boundSize; i < valuesSize - boundSize; i++)
    {
        FLOAT_TYPE val = valuesData[i];
        FLOAT_TYPE w = windowData[i];
        
        if (w > EPS)
        {
            val /= w;
            valuesData[i] = val;
        }
        else
        {
            int newStart = i;
            if (newStart > valuesSize/2)
                newStart = valuesSize - i - 1;
            
            if (newStart > start)
                start = newStart;
        }
    }
    
    if (start >= valuesSize/2)
        // Error
        return;
    
    // Then fill the missing values
    
    // Start
    FLOAT_TYPE startVal = valuesData[start];
    for (int i = 0; i < start; i++)
    {
        valuesData[i] = startVal;
    }
    
    // End
    int end = valuesSize - start - 1;
    FLOAT_TYPE endVal = valuesData[end];
    for (int i = valuesSize - 1; i > end; i--)
    {
        valuesData[i] = endVal;
    }
    
    FLOAT_TYPE max1 = BLUtils::ComputeMaxAbs(*values);
    
    // Normalize
    if (max1 > 0.0)
    {
        FLOAT_TYPE coeff = max0/max1;
        
        BLUtils::MultValues(values, coeff);
    }
}
template void BLUtils::UnapplyWindow(WDL_TypedBuf<float> *values,
                          const WDL_TypedBuf<float> &window,
                          int boundSize);
template void BLUtils::UnapplyWindow(WDL_TypedBuf<double> *values,
                          const WDL_TypedBuf<double> &window,
                          int boundSize);

template <typename FLOAT_TYPE>
void
BLUtils::UnapplyWindowFft(WDL_TypedBuf<FLOAT_TYPE> *ioMagns,
                        const WDL_TypedBuf<FLOAT_TYPE> &phases,
                        const WDL_TypedBuf<FLOAT_TYPE> &window,
                        int boundSize)
{
    WDL_TypedBuf<int> samplesIds;
    BLUtils::FftIdsToSamplesIds(phases, &samplesIds);
    
    WDL_TypedBuf<FLOAT_TYPE> sampleMagns = *ioMagns;
    
    BLUtils::Permute(&sampleMagns, samplesIds, true);
    
    BLUtils::UnapplyWindow(&sampleMagns, window, boundSize);
    
    BLUtils::Permute(&sampleMagns, samplesIds, false);
    
#if 1
    // Must multiply by 8... why ?
    //
    // Maybe due to hanning window normalization of windows
    //
    BLUtils::MultValues(&sampleMagns, (FLOAT_TYPE)8.0);
#endif
    
    *ioMagns = sampleMagns;
}
template void BLUtils::UnapplyWindowFft(WDL_TypedBuf<float> *ioMagns,
                             const WDL_TypedBuf<float> &phases,
                             const WDL_TypedBuf<float> &window,
                             int boundSize);
template void BLUtils::UnapplyWindowFft(WDL_TypedBuf<double> *ioMagns,
                             const WDL_TypedBuf<double> &phases,
                             const WDL_TypedBuf<double> &window,
                             int boundSize);

// See: http://werner.yellowcouch.org/Papers/transients12/index.html
template <typename FLOAT_TYPE>
void
BLUtils::FftIdsToSamplesIds(const WDL_TypedBuf<FLOAT_TYPE> &phases,
                          WDL_TypedBuf<int> *samplesIds)
{
    samplesIds->Resize(phases.GetSize());
    BLUtils::FillAllZero(samplesIds);
    
    int bufSize = phases.GetSize();
    FLOAT_TYPE *phasesData = phases.Get();
    int *samplesIdsData = samplesIds->Get();
    
    FLOAT_TYPE prev = 0.0;
    for (int i = 0; i < bufSize; i++)
    {
        FLOAT_TYPE phase = phasesData[i];
        
        FLOAT_TYPE phaseDiff = phase - prev;
        prev = phase;
        
        // Niko: Avoid having a big phase diff due to prev == 0
        if (i == 0)
            continue;
        
        // TODO: optimize this !
        while(phaseDiff < 0.0)
            phaseDiff += 2.0*M_PI;
        
        FLOAT_TYPE samplePos = ((FLOAT_TYPE)bufSize)*phaseDiff/(2.0*M_PI);
        
        samplesIdsData[i] = (int)samplePos;
    }
    
    // NOT SURE AT ALL !
    // Just like that, seems inverted
    // So we reverse back !
    //BLUtils::Reverse(samplesIds);
}
template void BLUtils::FftIdsToSamplesIds(const WDL_TypedBuf<float> &phases,
                               WDL_TypedBuf<int> *samplesIds);
template void BLUtils::FftIdsToSamplesIds(const WDL_TypedBuf<double> &phases,
                               WDL_TypedBuf<int> *samplesIds);

// See: http://werner.yellowcouch.org/Papers/transients12/index.html
template <typename FLOAT_TYPE>
void
BLUtils::FftIdsToSamplesIdsFloat(const WDL_TypedBuf<FLOAT_TYPE> &phases,
                               WDL_TypedBuf<FLOAT_TYPE> *samplesIds)
{
    samplesIds->Resize(phases.GetSize());
    BLUtils::FillAllZero(samplesIds);
    
    int bufSize = phases.GetSize();
    FLOAT_TYPE *phasesData = phases.Get();
    FLOAT_TYPE *samplesIdsData = samplesIds->Get();
    
    FLOAT_TYPE prev = 0.0;
    for (int i = 0; i < bufSize; i++)
    {
        FLOAT_TYPE phase = phasesData[i];
        
        FLOAT_TYPE phaseDiff = phase - prev;
        prev = phase;
        
        // Niko: Avoid having a big phase diff due to prev == 0
        if (i == 0)
            continue;
        
        // TODO: optimize this !
        while(phaseDiff < 0.0)
            phaseDiff += 2.0*M_PI;
        
        FLOAT_TYPE samplePos = ((FLOAT_TYPE)bufSize)*phaseDiff/(2.0*M_PI);
        
        samplesIdsData[i] = samplePos;
    }
    
    // NOT SURE AT ALL !
    // Just like that, seems inverted
    // So we reverse back !
    //BLUtils::Reverse(samplesIds);
}
template void BLUtils::FftIdsToSamplesIdsFloat(const WDL_TypedBuf<float> &phases,
                                    WDL_TypedBuf<float> *samplesIds);
template void BLUtils::FftIdsToSamplesIdsFloat(const WDL_TypedBuf<double> &phases,
                                    WDL_TypedBuf<double> *samplesIds);

template <typename FLOAT_TYPE>
void
BLUtils::FftIdsToSamplesIdsSym(const WDL_TypedBuf<FLOAT_TYPE> &phases,
                             WDL_TypedBuf<int> *samplesIds)
{
    samplesIds->Resize(phases.GetSize());
    BLUtils::FillAllZero(samplesIds);
    
    int bufSize = phases.GetSize();
    FLOAT_TYPE *phasesData = phases.Get();
    int samplesIdsSize = samplesIds->GetSize();
    int *samplesIdsData = samplesIds->Get();
    
    FLOAT_TYPE prev = 0.0;
    for (int i = 0; i < bufSize; i++)
    {
        FLOAT_TYPE phase = phasesData[i];
        
        FLOAT_TYPE phaseDiff = phase - prev;
        prev = phase;
        
        // Niko: Avoid having a big phase diff due to prev == 0
        if (i == 0)
            continue;
        
        while(phaseDiff < 0.0)
            phaseDiff += 2.0*M_PI;
        
        FLOAT_TYPE samplePos = ((FLOAT_TYPE)bufSize)*phaseDiff/(2.0*M_PI);
        
        // For sym...
        samplePos *= 2.0;
        
        samplePos = fmod(samplePos, samplesIdsSize);
        
        samplesIdsData[i] = (int)samplePos;
    }
}
template void BLUtils::FftIdsToSamplesIdsSym(const WDL_TypedBuf<float> &phases,
                                  WDL_TypedBuf<int> *samplesIds);
template void BLUtils::FftIdsToSamplesIdsSym(const WDL_TypedBuf<double> &phases,
                                  WDL_TypedBuf<int> *samplesIds);

template <typename FLOAT_TYPE>
void
BLUtils::SamplesIdsToFftIds(const WDL_TypedBuf<FLOAT_TYPE> &phases,
                          WDL_TypedBuf<int> *fftIds)
{
    fftIds->Resize(phases.GetSize());
    BLUtils::FillAllZero(fftIds);
    
    int bufSize = phases.GetSize();
    FLOAT_TYPE *phasesData = phases.Get();
    int fftIdsSize = fftIds->GetSize();
    int *fftIdsData = fftIds->Get();
    
    FLOAT_TYPE prev = 0.0;
    for (int i = 0; i < bufSize; i++)
    {
        FLOAT_TYPE phase = phasesData[i];
        
        FLOAT_TYPE phaseDiff = phase - prev;
        prev = phase;
        
        // Niko: Avoid having a big phase diff due to prev == 0
        if (i == 0)
            continue;
        
        while(phaseDiff < 0.0)
            phaseDiff += 2.0*M_PI;
        
        FLOAT_TYPE samplePos = ((FLOAT_TYPE)bufSize)*phaseDiff/(2.0*M_PI);
        
        int samplePosI = (int)samplePos;
        
        if ((samplePosI > 0) && (samplePosI < fftIdsSize))
            fftIdsData[samplePosI] = i;
    }
}
template void BLUtils::SamplesIdsToFftIds(const WDL_TypedBuf<float> &phases,
                               WDL_TypedBuf<int> *fftIds);
template void BLUtils::SamplesIdsToFftIds(const WDL_TypedBuf<double> &phases,
                               WDL_TypedBuf<int> *fftIds);

// See: https://gist.github.com/arrai/451426
template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::fmod_negative(FLOAT_TYPE x, FLOAT_TYPE y)
{
    // Move input to range 0.. 2*pi
    if(x < 0.0)
    {
        // fmod only supports positive numbers. Thus we have
        // to emulate negative numbers
        FLOAT_TYPE modulus = x * -1.0;
        modulus = std::fmod(modulus, y);
        modulus = -modulus + y;
        
        return modulus;
    }
    return std::fmod(x, y);
}
template float BLUtils::fmod_negative(float x, float y);
template double BLUtils::fmod_negative(double x, double y);

template <typename FLOAT_TYPE>
void
BLUtils::FindNextPhase(FLOAT_TYPE *phase, FLOAT_TYPE refPhase)
{
#if 0
    while(*phase < refPhase)
        *phase += 2.0*M_PI;
#endif
    
#if 0 // Optim (does not optimize a lot)
    while(*phase < refPhase)
        *phase += TWO_PI;
#endif
    
#if 1 // Optim2: very efficient !
      // Optim for SoundMetaViewer: gain about 10%
    if (*phase >= refPhase)
        return;
    
    FLOAT_TYPE refMod = fmod_negative(refPhase, (FLOAT_TYPE)TWO_PI);
    FLOAT_TYPE pMod = fmod_negative(*phase, (FLOAT_TYPE)TWO_PI);
    
    FLOAT_TYPE resPhase = (refPhase - refMod) + pMod;
    if (resPhase < refPhase)
        resPhase += TWO_PI;
    
    *phase = resPhase;
#endif
}
template void BLUtils::FindNextPhase(float *phase, float refPhase);
template void BLUtils::FindNextPhase(double *phase, double refPhase);

template <typename FLOAT_TYPE>
void
BLUtils::ComputeTimeDelays(WDL_TypedBuf<FLOAT_TYPE> *timeDelays,
                         const WDL_TypedBuf<FLOAT_TYPE> &phasesL,
                         const WDL_TypedBuf<FLOAT_TYPE> &phasesR,
                         FLOAT_TYPE sampleRate)
{
    if (phasesL.GetSize() != phasesR.GetSize())
        // R can be empty if we are in mono
        return;
    
    timeDelays->Resize(phasesL.GetSize());
    
    WDL_TypedBuf<FLOAT_TYPE> samplesIdsL;
    BLUtils::FftIdsToSamplesIdsFloat(phasesL, &samplesIdsL);
    
    WDL_TypedBuf<FLOAT_TYPE> samplesIdsR;
    BLUtils::FftIdsToSamplesIdsFloat(phasesR, &samplesIdsR);
   
#if !USE_SIMD_OPTIM
    int timeDelaysSize = timeDelays->GetSize();
    FLOAT_TYPE *timeDelaysData = timeDelays->Get();
    FLOAT_TYPE *sampleIdsLData = samplesIdsL.Get();
    FLOAT_TYPE *sampleIdsRData = samplesIdsR.Get();
    
    for (int i = 0; i < timeDelaysSize; i++)
    {
        FLOAT_TYPE sampIdL = sampleIdsLData[i];
        FLOAT_TYPE sampIdR = sampleIdsRData[i];
        
        FLOAT_TYPE diff = sampIdR - sampIdL;
        
        FLOAT_TYPE delay = diff/sampleRate;
        
        timeDelaysData[i] = delay;
    }
#else
    *timeDelays = samplesIdsR;
    SubstractValues(timeDelays, samplesIdsL);
    FLOAT_TYPE coeff = 1.0/sampleRate;
    MultValues(timeDelays, coeff);
#endif
}
template void BLUtils::ComputeTimeDelays(WDL_TypedBuf<float> *timeDelays,
                              const WDL_TypedBuf<float> &phasesL,
                              const WDL_TypedBuf<float> &phasesR,
                              float sampleRate);
template void BLUtils::ComputeTimeDelays(WDL_TypedBuf<double> *timeDelays,
                              const WDL_TypedBuf<double> &phasesL,
                              const WDL_TypedBuf<double> &phasesR,
                              double sampleRate);

#if 0 // Quite costly version !
void
BLUtils::UnwrapPhases(WDL_TypedBuf<FLOAT_TYPE> *phases)
{
    FLOAT_TYPE prevPhase = phases->Get()[0];
    
    FindNextPhase(&prevPhase, 0.0);
    
    int phasesSize = phases->GetSize();
    FLOAT_TYPE *phasesData = phases->Get();
    
    for (int i = 0; i < phasesSize; i++)
    {
        FLOAT_TYPE phase = phasesData[i];
        
        FindNextPhase(&phase, prevPhase);
        
        phasesData[i] = phase;
        
        prevPhase = phase;
    }
}
#endif

#if !USE_SIMD_OPTIM
// Optimized version
void
BLUtils::UnwrapPhases(WDL_TypedBuf<FLOAT_TYPE> *phases)
{
    if (phases->GetSize() == 0)
        // Empty phases
        return;
    
    FLOAT_TYPE prevPhase = phases->Get()[0];
    FindNextPhase(&prevPhase, 0.0);
    
    FLOAT_TYPE sum = 0.0;
    
    int phasesSize = phases->GetSize();
    FLOAT_TYPE *phasesData = phases->Get();
    
    for (int i = 0; i < phasesSize; i++)
    {
        FLOAT_TYPE phase = phasesData[i];
        phase += sum;
        
        while(phase < prevPhase)
        {
            phase += 2.0*M_PI;
            
            sum += 2.0*M_PI;
        }
        
        phasesData[i] = phase;
        
        prevPhase = phase;
    }
}
#else
// Optimized version 2
template <typename FLOAT_TYPE>
void
BLUtils::UnwrapPhases(WDL_TypedBuf<FLOAT_TYPE> *phases)
{
    if (phases->GetSize() == 0)
        // Empty phases
        return;
    
    FLOAT_TYPE prevPhase = phases->Get()[0];
    FindNextPhase(&prevPhase, (FLOAT_TYPE)0.0);
    
    //FLOAT_TYPE sum = 0.0;
    
    int phasesSize = phases->GetSize();
    FLOAT_TYPE *phasesData = phases->Get();
    for (int i = 0; i < phasesSize; i++)
    {
        FLOAT_TYPE phase = phasesData[i];
        //phase += sum;
        
        //while(phase < prevPhase)
        //{
        //    phase += 2.0*M_PI;
        //    sum += 2.0*M_PI;
        //}
        
        FindNextPhase(&phase, prevPhase);
        
        phasesData[i] = phase;
        
        prevPhase = phase;
    }
}
template void BLUtils::UnwrapPhases(WDL_TypedBuf<float> *phases);
template void BLUtils::UnwrapPhases(WDL_TypedBuf<double> *phases);
#endif

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::MapToPi(FLOAT_TYPE val)
{
    /* Map delta phase into +/- Pi interval */
    val =  std::fmod(val, (FLOAT_TYPE)(2.0*M_PI));
    if (val <= -M_PI)
        val += 2.0*M_PI;
    if (val > M_PI)
        val -= 2.0*M_PI;
    
    return val;
}
template float BLUtils::MapToPi(float val);
template double BLUtils::MapToPi(double val);

template <typename FLOAT_TYPE>
void
BLUtils::MapToPi(WDL_TypedBuf<FLOAT_TYPE> *values)
{
    int valuesSize = values->GetSize();
    FLOAT_TYPE *valuesData = values->Get();
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE val = valuesData[i];
        
        val = MapToPi(val);
        
        valuesData[i] = val;
    }
}
template void BLUtils::MapToPi(WDL_TypedBuf<float> *values);
template void BLUtils::MapToPi(WDL_TypedBuf<double> *values);

template <typename FLOAT_TYPE>
void
BLUtils::PolarToCartesian(const WDL_TypedBuf<FLOAT_TYPE> &Rs,
                        const WDL_TypedBuf<FLOAT_TYPE> &thetas,
                        WDL_TypedBuf<FLOAT_TYPE> *xValues,
                        WDL_TypedBuf<FLOAT_TYPE> *yValues)
{
    xValues->Resize(thetas.GetSize());
    yValues->Resize(thetas.GetSize());
    
    int thetasSize = thetas.GetSize();
    FLOAT_TYPE *thetasData = thetas.Get();
    FLOAT_TYPE *RsData = Rs.Get();
    FLOAT_TYPE *xValuesData = xValues->Get();
    FLOAT_TYPE *yValuesData = yValues->Get();
    
    for (int i = 0; i < thetasSize; i++)
    {
        FLOAT_TYPE theta = thetasData[i];
        
        FLOAT_TYPE r = RsData[i];
        
        FLOAT_TYPE x = r*std::cos(theta);
        FLOAT_TYPE y = r*std::sin(theta);
        
        xValuesData[i] = x;
        yValuesData[i] = y;
    }
}
template void BLUtils::PolarToCartesian(const WDL_TypedBuf<float> &Rs,
                             const WDL_TypedBuf<float> &thetas,
                             WDL_TypedBuf<float> *xValues,
                             WDL_TypedBuf<float> *yValues);
template void BLUtils::PolarToCartesian(const WDL_TypedBuf<double> &Rs,
                             const WDL_TypedBuf<double> &thetas,
                             WDL_TypedBuf<double> *xValues,
                             WDL_TypedBuf<double> *yValues);

template <typename FLOAT_TYPE>
void
BLUtils::PhasesPolarToCartesian(const WDL_TypedBuf<FLOAT_TYPE> &phasesDiff,
                              const WDL_TypedBuf<FLOAT_TYPE> *magns,
                              WDL_TypedBuf<FLOAT_TYPE> *xValues,
                              WDL_TypedBuf<FLOAT_TYPE> *yValues)
{
    xValues->Resize(phasesDiff.GetSize());
    yValues->Resize(phasesDiff.GetSize());
    
    int phaseDiffSize = phasesDiff.GetSize();
    FLOAT_TYPE *phaseDiffData = phasesDiff.Get();
    int magnsSize = magns->GetSize();
    FLOAT_TYPE *magnsData = magns->Get();
    FLOAT_TYPE *xValuesData = xValues->Get();
    FLOAT_TYPE *yValuesData = yValues->Get();
    
    for (int i = 0; i < phaseDiffSize; i++)
    {
        FLOAT_TYPE phaseDiff = phaseDiffData[i];
        
        // TODO: check this
        phaseDiff = MapToPi(phaseDiff);
        
        FLOAT_TYPE magn = 1.0;
        if ((magns != NULL) && (magnsSize > 0))
            magn = magnsData[i];
        
        FLOAT_TYPE x = magn*std::cos(phaseDiff);
        FLOAT_TYPE y = magn*std::sin(phaseDiff);
        
        xValuesData[i] = x;
        yValuesData[i] = y;
    }
}
template void BLUtils::PhasesPolarToCartesian(const WDL_TypedBuf<float> &phasesDiff,
                                   const WDL_TypedBuf<float> *magns,
                                   WDL_TypedBuf<float> *xValues,
                                   WDL_TypedBuf<float> *yValues);
template void BLUtils::PhasesPolarToCartesian(const WDL_TypedBuf<double> &phasesDiff,
                                   const WDL_TypedBuf<double> *magns,
                                   WDL_TypedBuf<double> *xValues,
                                   WDL_TypedBuf<double> *yValues);

// From (angle, distance) to (normalized angle on x, hight distance on y)
template <typename FLOAT_TYPE>
void
BLUtils::CartesianToPolarFlat(WDL_TypedBuf<FLOAT_TYPE> *xVector, WDL_TypedBuf<FLOAT_TYPE> *yVector)
{
    int xVectorSize = xVector->GetSize();
    FLOAT_TYPE *xVectorData = xVector->Get();
    FLOAT_TYPE *yVectorData = yVector->Get();
    
    for (int i = 0; i < xVectorSize; i++)
    {
        FLOAT_TYPE x0 = xVectorData[i];
        FLOAT_TYPE y0 = yVectorData[i];
        
        FLOAT_TYPE angle = std::atan2(y0, x0);
        
        // Normalize x
        FLOAT_TYPE x = (angle/M_PI - 0.5)*2.0;
        
        // Keep y as it is ?
        //FLOAT_TYPE y = y0;
        
        // or change it ?
        FLOAT_TYPE y = std::sqrt(x0*x0 + y0*y0);
        
        xVectorData[i] = x;
        yVectorData[i] = y;
    }
}
template void BLUtils::CartesianToPolarFlat(WDL_TypedBuf<float> *xVector, WDL_TypedBuf<float> *yVector);
template void BLUtils::CartesianToPolarFlat(WDL_TypedBuf<double> *xVector, WDL_TypedBuf<double> *yVector);

template <typename FLOAT_TYPE>
void
BLUtils::PolarToCartesianFlat(WDL_TypedBuf<FLOAT_TYPE> *xVector, WDL_TypedBuf<FLOAT_TYPE> *yVector)
{
    int xVectorSize = xVector->GetSize();
    FLOAT_TYPE *xVectorData = xVector->Get();
    FLOAT_TYPE *yVectorData = yVector->Get();
    
    for (int i = 0; i < xVectorSize; i++)
    {
        FLOAT_TYPE theta = xVectorData[i];
        FLOAT_TYPE r = yVectorData[i];
        
        theta = ((theta + 1.0)*0.5)*M_PI;
        
        FLOAT_TYPE x = r*std::cos(theta);
        FLOAT_TYPE y = r*std::sin(theta);
        
        xVectorData[i] = x;
        yVectorData[i] = y;
    }
}
template void BLUtils::PolarToCartesianFlat(WDL_TypedBuf<float> *xVector, WDL_TypedBuf<float> *yVector);
template void BLUtils::PolarToCartesianFlat(WDL_TypedBuf<double> *xVector, WDL_TypedBuf<double> *yVector);

template <typename FLOAT_TYPE>
void
BLUtils::ApplyInverseWindow(WDL_TypedBuf<WDL_FFT_COMPLEX> *fftSamples,
                          const WDL_TypedBuf<FLOAT_TYPE> &window,
                          const WDL_TypedBuf<FLOAT_TYPE> *originEnvelope)
{
    WDL_TypedBuf<FLOAT_TYPE> magns;
    WDL_TypedBuf<FLOAT_TYPE> phases;
    BLUtils::ComplexToMagnPhase(&magns, &phases, *fftSamples);
    
    ApplyInverseWindow(&magns, phases, window, originEnvelope);
    
    BLUtils::MagnPhaseToComplex(fftSamples, magns, phases);
}
template void BLUtils::ApplyInverseWindow(WDL_TypedBuf<WDL_FFT_COMPLEX> *fftSamples,
                               const WDL_TypedBuf<float> &window,
                               const WDL_TypedBuf<float> *originEnvelope);
template void BLUtils::ApplyInverseWindow(WDL_TypedBuf<WDL_FFT_COMPLEX> *fftSamples,
                               const WDL_TypedBuf<double> &window,
                               const WDL_TypedBuf<double> *originEnvelope);

template <typename FLOAT_TYPE>
void
BLUtils::ApplyInverseWindow(WDL_TypedBuf<FLOAT_TYPE> *magns,
                          const WDL_TypedBuf<FLOAT_TYPE> &phases,
                          const WDL_TypedBuf<FLOAT_TYPE> &window,
                          const WDL_TypedBuf<FLOAT_TYPE> *originEnvelope)
{
#define WIN_EPS 1e-3
    
    // Suppresses the amplification of noise at the border of the wndow
#define MAGN_EPS 1e-6
    
    WDL_TypedBuf<int> samplesIds;
    BLUtils::FftIdsToSamplesIds(phases, &samplesIds);
    
    const WDL_TypedBuf<FLOAT_TYPE> origMagns = *magns;
    
    int magnsSize = magns->GetSize();
    FLOAT_TYPE *magnsData = magns->Get();
    int *samplesIdsData = samplesIds.Get();
    FLOAT_TYPE *origMagnsData = origMagns.Get();
    FLOAT_TYPE *windowData = window.Get();
    FLOAT_TYPE *originEnvelopeData = originEnvelope->Get();
    
    for (int i = 0; i < magnsSize; i++)
        //for (int i = 1; i < magns->GetSize() - 1; i++)
    {
        int sampleIdx = samplesIdsData[i];
        
        FLOAT_TYPE magn = origMagnsData[i];
        FLOAT_TYPE win1 = windowData[sampleIdx];
        
        FLOAT_TYPE coeff = 0.0;
        
        if (win1 > WIN_EPS)
            coeff = 1.0/win1;
        
        //coeff = win1;
        
        // Better with
        if (originEnvelope != NULL)
        {
            FLOAT_TYPE originSample = originEnvelopeData[sampleIdx];
            coeff *= std::fabs(originSample);
        }
        
        if (magn > MAGN_EPS) // TEST
            magn *= coeff;
        
#if 0
        // Just in case
        if (magn > 1.0)
            magn = 1.0;
        if (magn < -1.0)
            magn = -1.0;
#endif
        
        magnsData[i] = magn;
    }
}
template void BLUtils::ApplyInverseWindow(WDL_TypedBuf<float> *magns,
                               const WDL_TypedBuf<float> &phases,
                               const WDL_TypedBuf<float> &window,
                               const WDL_TypedBuf<float> *originEnvelope);
template void BLUtils::ApplyInverseWindow(WDL_TypedBuf<double> *magns,
                               const WDL_TypedBuf<double> &phases,
                               const WDL_TypedBuf<double> &window,
                               const WDL_TypedBuf<double> *originEnvelope);

template <typename FLOAT_TYPE>
void
BLUtils::CorrectEnvelope(WDL_TypedBuf<FLOAT_TYPE> *samples,
                       const WDL_TypedBuf<FLOAT_TYPE> &envelope0,
                       const WDL_TypedBuf<FLOAT_TYPE> &envelope1)
{
    int samplesSize = samples->GetSize();
    FLOAT_TYPE *samplesData = samples->Get();
    FLOAT_TYPE *envelope0Data = envelope0.Get();
    FLOAT_TYPE *envelope1Data = envelope1.Get();
    
    for (int i = 0; i < samplesSize; i++)
    {
        FLOAT_TYPE sample = samplesData[i];
        
        FLOAT_TYPE env0 = envelope0Data[i];
        FLOAT_TYPE env1 = envelope1Data[i];
        
        FLOAT_TYPE coeff = 0.0; //
        
        if ((env0 > BL_EPS) && (env1 > BL_EPS))
            coeff = env0/env1;
        
        sample *= coeff;
        
        // Just in case
        if (sample > 1.0)
            sample = 1.0;
        if (sample < -1.0)
            sample = -1.0;
        
        samplesData[i] = sample;
    }
}
template void BLUtils::CorrectEnvelope(WDL_TypedBuf<float> *samples,
                            const WDL_TypedBuf<float> &envelope0,
                            const WDL_TypedBuf<float> &envelope1);
template void BLUtils::CorrectEnvelope(WDL_TypedBuf<double> *samples,
                            const WDL_TypedBuf<double> &envelope0,
                            const WDL_TypedBuf<double> &envelope1);

template <typename FLOAT_TYPE>
int
BLUtils::GetEnvelopeShift(const WDL_TypedBuf<FLOAT_TYPE> &envelope0,
                        const WDL_TypedBuf<FLOAT_TYPE> &envelope1,
                        int precision)
{
    int max0 = BLUtils::FindMaxIndex(envelope0);
    int max1 = BLUtils::FindMaxIndex(envelope1);
    
    int shift = max1 - max0;
    
    if (precision > 1)
    {
        FLOAT_TYPE newShift = ((FLOAT_TYPE)shift)/precision;
        newShift = bl_round(newShift);
        
        newShift *= precision;
        
        shift = newShift;
    }
    
    return shift;
}
template int BLUtils::GetEnvelopeShift(const WDL_TypedBuf<float> &envelope0,
                            const WDL_TypedBuf<float> &envelope1,
                            int precision);
template int BLUtils::GetEnvelopeShift(const WDL_TypedBuf<double> &envelope0,
                            const WDL_TypedBuf<double> &envelope1,
                            int precision);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ApplyParamShape(FLOAT_TYPE normVal, FLOAT_TYPE shape)
{
    return std::pow(normVal, (FLOAT_TYPE)(1.0/shape));
}
template float BLUtils::ApplyParamShape(float normVal, float shape);
template double BLUtils::ApplyParamShape(double normVal, double shape);

template <typename FLOAT_TYPE>
void
BLUtils::ApplyParamShape(WDL_TypedBuf<FLOAT_TYPE> *normVals, FLOAT_TYPE shape)
{
    for (int i = 0; i < normVals->GetSize(); i++)
    {
        FLOAT_TYPE normVal = normVals->Get()[i];
        
        normVal = std::pow(normVal, (FLOAT_TYPE)(1.0/shape));
        
        normVals->Get()[i] = normVal;
    }
}
template void BLUtils::ApplyParamShape(WDL_TypedBuf<float> *normVals, float shape);
template void BLUtils::ApplyParamShape(WDL_TypedBuf<double> *normVals, double shape);

template <typename FLOAT_TYPE>
void
BLUtils::ApplyParamShapeWaveform(WDL_TypedBuf<FLOAT_TYPE> *normVals, FLOAT_TYPE shape)
{
    for (int i = 0; i < normVals->GetSize(); i++)
    {
        FLOAT_TYPE normVal = normVals->Get()[i];
        
        bool neg = (normVal < 0.0);
        if(neg)
            normVal = -normVal;
        
        normVal = std::pow(normVal, (FLOAT_TYPE)(1.0/shape));
        
        if(neg)
            normVal = -normVal;
        
        normVals->Get()[i] = normVal;
    }
}
template void BLUtils::ApplyParamShapeWaveform(WDL_TypedBuf<float> *normVals, float shape);
template void BLUtils::ApplyParamShapeWaveform(WDL_TypedBuf<double> *normVals, double shape);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeShapeForCenter0(FLOAT_TYPE minKnobValue, FLOAT_TYPE maxKnobValue)
{
    // Normalized position of the zero
    FLOAT_TYPE normZero = -minKnobValue/(maxKnobValue - minKnobValue);
    
    FLOAT_TYPE shape = std::log((BL_FLOAT)0.5)/std::log(normZero);
    shape = 1.0/shape;
    
    return shape;
}
template float BLUtils::ComputeShapeForCenter0(float minKnobValue, float maxKnobValue);
template double BLUtils::ComputeShapeForCenter0(double minKnobValue, double maxKnobValue);

char *
BLUtils::GetFileExtension(const char *fileName)
{
	char *ext = (char *)strrchr(fileName, '.');
    
    // Here, we have for example ".wav"
    if (ext != NULL)
    {
        if (strlen(ext) > 0)
            // Skip the dot
            ext = &ext[1];
    }
        
    return ext;
}

char *
BLUtils::GetFileName(const char *path)
{
	char *fileName = (char *)strrchr(path, '/');

	// Here, we have for example "/file.wav"
	if (fileName != NULL)
	{
		if (strlen(fileName) > 0)
			// Skip the dot
			fileName = &fileName[1];
	}
	else
	{
		// There were no "/" in the path,
		// we already had the correct file name
		return (char *)path;
	}

	return fileName;
}

template <typename FLOAT_TYPE>
void
BLUtils::AppendValuesFile(const char *fileName, const WDL_TypedBuf<FLOAT_TYPE> &values, char delim)
{
    // Compute the file size
    FILE *fileSz = fopen(fileName, "a+");
    if (fileSz == NULL)
        return;
    
    fseek(fileSz, 0L, SEEK_END);
    long size = ftell(fileSz);
    
    fseek(fileSz, 0L, SEEK_SET);
    fclose(fileSz);
    
    // Write
    FILE *file = fopen(fileName, "a+");
    
    for (int i = 0; i < values.GetSize(); i++)
    {
        if ((i == 0) && (size == 0))
            fprintf(file, "%g", values.Get()[i]);
        else
            fprintf(file, "%c%g", delim, values.Get()[i]);
    }
    
    //fprintf(file, "\n");
           
    fclose(file);
}
template void BLUtils::AppendValuesFile(const char *fileName, const WDL_TypedBuf<float> &values, char delim);
template void BLUtils::AppendValuesFile(const char *fileName, const WDL_TypedBuf<double> &values, char delim);

template <typename FLOAT_TYPE>
void
BLUtils::AppendValuesFileBin(const char *fileName, const WDL_TypedBuf<FLOAT_TYPE> &values)
{
    // Write
    FILE *file = fopen(fileName, "ab+");
    
    fwrite(values.Get(), sizeof(FLOAT_TYPE), values.GetSize(), file);
    
    fclose(file);
}
template void BLUtils::AppendValuesFileBin(const char *fileName, const WDL_TypedBuf<float> &values);
template void BLUtils::AppendValuesFileBin(const char *fileName, const WDL_TypedBuf<double> &values);

template <typename FLOAT_TYPE>
void
BLUtils::AppendValuesFileBinFloat(const char *fileName, const WDL_TypedBuf<FLOAT_TYPE> &values)
{
    WDL_TypedBuf<float> floatBuf;
    floatBuf.Resize(values.GetSize());
    for (int i = 0; i < floatBuf.GetSize(); i++)
    {
        FLOAT_TYPE val = values.Get()[i];
        floatBuf.Get()[i] = val;
    }
    
    // Write
    FILE *file = fopen(fileName, "ab+");
    
    fwrite(floatBuf.Get(), sizeof(float), floatBuf.GetSize(), file);
    
    fclose(file);
}
template void BLUtils::AppendValuesFileBinFloat(const char *fileName, const WDL_TypedBuf<float> &values);
template void BLUtils::AppendValuesFileBinFloat(const char *fileName, const WDL_TypedBuf<double> &values);

void *
BLUtils::AppendValuesFileBinFloatInit(const char *fileName)
{
    // Write
    FILE *file = fopen(fileName, "ab+");
 
    return file;
}

template <typename FLOAT_TYPE>
void
BLUtils::AppendValuesFileBinFloat(void *cookie, const WDL_TypedBuf<FLOAT_TYPE> &values)
{
    // Write
    FILE *file = (FILE *)cookie;
    
    WDL_TypedBuf<float> floatBuf;
    floatBuf.Resize(values.GetSize());
    for (int i = 0; i < floatBuf.GetSize(); i++)
    {
        FLOAT_TYPE val = values.Get()[i];
        floatBuf.Get()[i] = val;
    }
    
    fwrite(floatBuf.Get(), sizeof(float), floatBuf.GetSize(), file);
    
    // TEST
    fflush(file);
}
template void BLUtils::AppendValuesFileBinFloat(void *cookie, const WDL_TypedBuf<float> &values);
template void BLUtils::AppendValuesFileBinFloat(void *cookie, const WDL_TypedBuf<double> &values);

void
BLUtils::AppendValuesFileBinFloatShutdown(void *cookie)
{
    fclose((FILE *)cookie);
}

template <typename FLOAT_TYPE>
void
BLUtils::ResizeLinear(WDL_TypedBuf<FLOAT_TYPE> *ioBuffer,
                    int newSize)
{
    FLOAT_TYPE *data = ioBuffer->Get();
    int size = ioBuffer->GetSize();
    
    WDL_TypedBuf<FLOAT_TYPE> result;
    BLUtils::ResizeFillZeros(&result, newSize);
    
    FLOAT_TYPE ratio = ((FLOAT_TYPE)(size - 1))/newSize;
    
    FLOAT_TYPE  *resultData = result.Get();
    
    FLOAT_TYPE pos = 0.0;
    for (int i = 0; i < newSize; i++)
    {
        // Optim
        //int x = (int)(ratio * i);
        //FLOAT_TYPE diff = (ratio * i) - x;
        
        int x = (int)pos;
        FLOAT_TYPE diff = pos - x;
        pos += ratio;
        
        if (x >= size - 1)
            continue;
        
        FLOAT_TYPE a = data[x];
        FLOAT_TYPE b = data[x + 1];
        
        FLOAT_TYPE val = a*(1.0 - diff) + b*diff;
        
        resultData[i] = val;
    }
    
    *ioBuffer = result;
}
template void BLUtils::ResizeLinear(WDL_TypedBuf<double> *ioBuffer, int newSize);
template void BLUtils::ResizeLinear(WDL_TypedBuf<float> *ioBuffer, int newSize);

template <typename FLOAT_TYPE>
void
BLUtils::ResizeLinear(WDL_TypedBuf<FLOAT_TYPE> *rescaledBuf,
                    const WDL_TypedBuf<FLOAT_TYPE> &buf,
                    int newSize)
{
    FLOAT_TYPE *data = buf.Get();
    int size = buf.GetSize();
    
    BLUtils::ResizeFillZeros(rescaledBuf, newSize);
    
    FLOAT_TYPE ratio = ((FLOAT_TYPE)(size - 1))/newSize;
    
    FLOAT_TYPE *rescaledBufData = rescaledBuf->Get();
    
    FLOAT_TYPE pos = 0.0;
    for (int i = 0; i < newSize; i++)
    {
        // Optim
        //int x = (int)(ratio * i);
        //FLOAT_TYPE diff = (ratio * i) - x;
        
        int x = (int)pos;
        FLOAT_TYPE diff = pos - x;
        pos += ratio;
        
        if (x >= size - 1)
            continue;
        
        FLOAT_TYPE a = data[x];
        FLOAT_TYPE b = data[x + 1];
        
        FLOAT_TYPE val = a*(1.0 - diff) + b*diff;
        
        rescaledBufData[i] = val;
    }
}
template void BLUtils::ResizeLinear(WDL_TypedBuf<float> *rescaledBuf,
                         const WDL_TypedBuf<float> &buf,
                         int newSize);
template void BLUtils::ResizeLinear(WDL_TypedBuf<double> *rescaledBuf,
                         const WDL_TypedBuf<double> &buf,
                         int newSize);

template <typename FLOAT_TYPE>
void
BLUtils::ResizeLinear2(WDL_TypedBuf<FLOAT_TYPE> *ioBuffer,
                    int newSize)
{
    if (ioBuffer->GetSize() == 0)
        return;
    
    if (newSize == 0)
        return;
    
    // Fix last value
    FLOAT_TYPE lastValue = ioBuffer->Get()[ioBuffer->GetSize() - 1];
    
    FLOAT_TYPE *data = ioBuffer->Get();
    int size = ioBuffer->GetSize();
    
    WDL_TypedBuf<FLOAT_TYPE> result;
    BLUtils::ResizeFillZeros(&result, newSize);
    
    FLOAT_TYPE ratio = ((FLOAT_TYPE)(size - 1))/newSize;
    
    FLOAT_TYPE *resultData = result.Get();
    
    FLOAT_TYPE pos = 0.0;
    for (int i = 0; i < newSize; i++)
    {
        // Optim
        //int x = (int)(ratio * i);
        //FLOAT_TYPE diff = (ratio * i) - x;
        
        int x = (int)pos;
        FLOAT_TYPE diff = pos - x;
        pos += ratio;
        
        if (x >= size - 1)
            continue;
            
        FLOAT_TYPE a = data[x];
        FLOAT_TYPE b = data[x + 1];
        
        FLOAT_TYPE val = a*(1.0 - diff) + b*diff;
        
        resultData[i] = val;
    }
    
    *ioBuffer = result;
    
    // Fix last value
    ioBuffer->Get()[ioBuffer->GetSize() - 1] = lastValue;
}
template void BLUtils::ResizeLinear2(WDL_TypedBuf<float> *ioBuffer,
                          int newSize);
template void BLUtils::ResizeLinear2(WDL_TypedBuf<double> *ioBuffer,
                          int newSize);

#if 0 // Disable, to avoid including libmfcc in each plugins
// Use https://github.com/jsawruk/libmfcc
//
// NOTE: Not tested !
void
BLUtils::FreqsToMfcc(WDL_TypedBuf<FLOAT_TYPE> *result,
                   const WDL_TypedBuf<FLOAT_TYPE> freqs,
                   FLOAT_TYPE sampleRate)
{
    int numFreqs = freqs.GetSize();
    FLOAT_TYPE *spectrum = freqs.Get();
    
    int numFilters = 48;
    int numCoeffs = 13;
    
    result->Resize(numCoeffs);
    
    FLOAT_TYPE *resultData = result->Get();
    
    for (int coeff = 0; coeff < numCoeffs; coeff++)
	{
		FLOAT_TYPE res = GetCoefficient(spectrum, (int)sampleRate, numFilters, 128, coeff);
        
        resultData[coeff] = res;
	}
}
#endif

// See: https://haythamfayek.com/2016/04/21/speech-processing-for-machine-learning.html
template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::FreqToMel(FLOAT_TYPE freq)
{
    FLOAT_TYPE res = 2595.0*std::log10((BL_FLOAT)(1.0 + freq/700.0));
    
    return res;
}
template float BLUtils::FreqToMel(float freq);
template double BLUtils::FreqToMel(double freq);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::MelToFreq(FLOAT_TYPE mel)
{
    FLOAT_TYPE res = 700.0*(std::pow((FLOAT_TYPE)10.0, (FLOAT_TYPE)(mel/2595.0)) - 1.0);
    
    return res;
}
template float BLUtils::MelToFreq(float mel);
template double BLUtils::MelToFreq(double mel);

// VERY GOOD !

// OPTIM PROF Infra
#if 0 // ORIGIN
void
BLUtils::FreqsToMelNorm(WDL_TypedBuf<FLOAT_TYPE> *resultMagns,
                      const WDL_TypedBuf<FLOAT_TYPE> &magns,
                      FLOAT_TYPE hzPerBin,
                      FLOAT_TYPE zeroValue)
{
    //BLUtils::ResizeFillZeros(resultMagns, magns.GetSize());
    
    // For dB
    resultMagns->Resize(magns.GetSize());
    BLUtils::FillAllValue(resultMagns, zeroValue);
    
    FLOAT_TYPE maxFreq = hzPerBin*(magns.GetSize() - 1);
    FLOAT_TYPE maxMel = FreqToMel(maxFreq);
    
    // Optim
    FLOAT_TYPE melCoeff = maxMel/resultMagns->GetSize();
    FLOAT_TYPE idCoeff = (1.0/maxFreq)*resultMagns->GetSize();
    
    int resultMagnsSize = resultMagns->GetSize();
    FLOAT_TYPE *resultMagnsData = resultMagns->Get();
    
    int magnsSize = magns.GetSize();
    FLOAT_TYPE *magnsData = mangs.Get();
    
    for (int i = 0; i < resultMagnsSize; i++)
    {
        // Optim
        //FLOAT_TYPE mel = i*maxMel/resultMagns->GetSize();
        FLOAT_TYPE mel = i*melCoeff;
        
        FLOAT_TYPE freq = MelToFreq(mel);
        
        if (maxFreq < BL_EPS)
            return;
        
        // Optim
        //FLOAT_TYPE id0 = (freq/maxFreq) * resultMagns->GetSize();
        FLOAT_TYPE id0 = freq*idCoeff;
        
        FLOAT_TYPE t = id0 - (int)(id0);
        
        if ((int)id0 >= magnsSize)
            continue;
        
        int id1 = id0 + 1;
        if (id1 >= magnsSize)
            continue;
        
        FLOAT_TYPE magn0 = magnsData[(int)id0];
        FLOAT_TYPE magn1 = magnsData[id1];
        
        FLOAT_TYPE magn = (1.0 - t)*magn0 + t*magn1;
        
        resultMagnsData[i] = magn;
    }
}
#else // OPTIMIZED
template <typename FLOAT_TYPE>
void
BLUtils::FreqsToMelNorm(WDL_TypedBuf<FLOAT_TYPE> *resultMagns,
                      const WDL_TypedBuf<FLOAT_TYPE> &magns,
                      FLOAT_TYPE hzPerBin,
                      FLOAT_TYPE zeroValue)
{
    //BLUtils::ResizeFillZeros(resultMagns, magns.GetSize());
    
    // For dB
    resultMagns->Resize(magns.GetSize());
    BLUtils::FillAllValue(resultMagns, zeroValue);
    
    FLOAT_TYPE maxFreq = hzPerBin*(magns.GetSize() - 1);
    FLOAT_TYPE maxMel = FreqToMel(maxFreq);
    
    // Optim
    FLOAT_TYPE melCoeff = maxMel/resultMagns->GetSize();
    FLOAT_TYPE idCoeff = (1.0/maxFreq)*resultMagns->GetSize();
    
    int resultMagnsSize = resultMagns->GetSize();
    FLOAT_TYPE *resultMagnsData = resultMagns->Get();
    int magnsSize = magns.GetSize();
    FLOAT_TYPE *magnsData = magns.Get();
    
    for (int i = 0; i < resultMagnsSize; i++)
    {
        FLOAT_TYPE mel = i*melCoeff;
        
        FLOAT_TYPE freq = MelToFreq(mel);
        
        if (maxFreq < BL_EPS)
            return;
        
        FLOAT_TYPE id0 = freq*idCoeff;
        
        // Optim
        // (cast from FLOAT_TYPE to int is costly)
        int id0i = (int)id0;
        
        FLOAT_TYPE t = id0 - id0i;
        
        if (id0i >= magnsSize)
            continue;
        
        // NOTE: this optim doesn't compute exactly the same thing than the original version
        int id1 = id0i + 1;
        if (id1 >= magnsSize)
            continue;
        
        FLOAT_TYPE magn0 = magnsData[id0i];
        FLOAT_TYPE magn1 = magnsData[id1];
        
        FLOAT_TYPE magn = (1.0 - t)*magn0 + t*magn1;
        
        resultMagnsData[i] = magn;
    }
}
template void BLUtils::FreqsToMelNorm(WDL_TypedBuf<float> *resultMagns,
                           const WDL_TypedBuf<float> &magns,
                           float hzPerBin,
                           float zeroValue);
template void BLUtils::FreqsToMelNorm(WDL_TypedBuf<double> *resultMagns,
                           const WDL_TypedBuf<double> &magns,
                           double hzPerBin,
                           double zeroValue);
#endif

template <typename FLOAT_TYPE>
void
BLUtils::MelToFreqsNorm(WDL_TypedBuf<FLOAT_TYPE> *resultMagns,
                      const WDL_TypedBuf<FLOAT_TYPE> &magns,
                      FLOAT_TYPE hzPerBin, FLOAT_TYPE zeroValue)
{
    //BLUtils::ResizeFillZeros(resultMagns, magns.GetSize());
    
    // For dB
    resultMagns->Resize(magns.GetSize());
    BLUtils::FillAllValue(resultMagns, zeroValue);
    
    FLOAT_TYPE maxFreq = hzPerBin*(magns.GetSize() - 1);
    FLOAT_TYPE maxMel = FreqToMel(maxFreq);
    
    int resultMagnsSize = resultMagns->GetSize();
    FLOAT_TYPE *resultMagnsData = resultMagns->Get();
    int magnsSize = magns.GetSize();
    FLOAT_TYPE *magnsData = magns.Get();
    
    // OPTIM: avoid division in loop
    BL_FLOAT coeff = (1.0/maxMel)*resultMagnsSize;
    
    for (int i = 0; i < resultMagnsSize; i++)
    {
        FLOAT_TYPE freq = hzPerBin*i;
        FLOAT_TYPE mel = FreqToMel(freq);
        
        //FLOAT_TYPE id0 = (mel/maxMel) * resultMagnsSize;
        FLOAT_TYPE id0 = mel*coeff;
        
        if ((int)id0 >= magnsSize)
            continue;
            
        // Linear
        FLOAT_TYPE t = id0 - (int)(id0);
        
        int id1 = id0 + 1;
        if (id1 >= magnsSize)
            continue;
        
        FLOAT_TYPE magn0 = magnsData[(int)id0];
        FLOAT_TYPE magn1 = magnsData[id1];
        
        FLOAT_TYPE magn = (1.0 - t)*magn0 + t*magn1;
        
        // Nearest
        //FLOAT_TYPE magn = magns.Get()[(int)id0];
        
        resultMagnsData[i] = magn;
    }
}
template void BLUtils::MelToFreqsNorm(WDL_TypedBuf<float> *resultMagns,
                           const WDL_TypedBuf<float> &magns,
                           float hzPerBin, float zeroValue);
template void BLUtils::MelToFreqsNorm(WDL_TypedBuf<double> *resultMagns,
                           const WDL_TypedBuf<double> &magns,
                           double hzPerBin, double zeroValue);

template <typename FLOAT_TYPE>
void
BLUtils::MelToFreqsNorm2(WDL_TypedBuf<FLOAT_TYPE> *resultMagns,
                       const WDL_TypedBuf<FLOAT_TYPE> &magns,
                       FLOAT_TYPE hzPerBin, FLOAT_TYPE zeroValue)
{
    // For dB
    resultMagns->Resize(magns.GetSize());
    BLUtils::FillAllValue(resultMagns, zeroValue);
    
    FLOAT_TYPE maxFreq = hzPerBin*(magns.GetSize() - 1);
    FLOAT_TYPE maxMel = FreqToMel(maxFreq);
    
    int resultMagnsSize = resultMagns->GetSize();
    FLOAT_TYPE *resultMagnsData = resultMagns->Get();
    int magnsSize = magns.GetSize();
    FLOAT_TYPE *magnsData = magns.Get();
    
    for (int i = 0; i < resultMagnsSize; i++)
    {
        FLOAT_TYPE freq0 = hzPerBin*(i - 0.5);
        FLOAT_TYPE mel0 = FreqToMel(freq0);
        FLOAT_TYPE id0 = (mel0/maxMel) * resultMagnsSize;
        if (id0 < 0.0)
            id0 = 0.0;
        if ((int)id0 >= magnsSize)
            continue;
        
        FLOAT_TYPE freq1 = hzPerBin*(i + 0.5);
        FLOAT_TYPE mel1 = FreqToMel(freq1);
        FLOAT_TYPE id1 = (mel1/maxMel) * resultMagnsSize;
        if ((int)id1 >= magnsSize)
            id1 = magnsSize - 1;
        
        FLOAT_TYPE magn = 0.0;
        
        if (id1 - id0 > 1.0)
        // Scaling down
        // Scaling from biggest scale => must take several values
        {
            // Compute simple average
            FLOAT_TYPE avg = 0.0;
            int numValues = 0;
            for (int k = (int)id0; k <= (int)id1; k++)
            {
                avg += magnsData[k];
                numValues++;
            }
            if (numValues > 0)
                avg /= numValues;
            
            magn = avg;
        }
        else
        {
            // Scaling up
            // Scaling from smallest scale => must interpolate on the distination
            FLOAT_TYPE freq = hzPerBin*i;
            FLOAT_TYPE mel = FreqToMel(freq);
            
            FLOAT_TYPE i0 = (mel/maxMel) * resultMagnsSize;
            
            if ((int)i0 >= magnsSize)
                continue;
            
            // Linear
            FLOAT_TYPE t = i0 - (int)(i0);
            
            int i1 = i0 + 1;
            if (i1 >= magnsSize)
                continue;
            
            FLOAT_TYPE magn0 = magnsData[(int)i0];
            FLOAT_TYPE magn1 = magnsData[i1];
            
            magn = (1.0 - t)*magn0 + t*magn1;
        }
        
        // Nearest
        //FLOAT_TYPE magn = magns.Get()[(int)id0];
        
        resultMagnsData[i] = magn;
    }
}
template void BLUtils::MelToFreqsNorm2(WDL_TypedBuf<float> *resultMagns,
                            const WDL_TypedBuf<float> &magns,
                            float hzPerBin, float zeroValue);
template void BLUtils::MelToFreqsNorm2(WDL_TypedBuf<double> *resultMagns,
                            const WDL_TypedBuf<double> &magns,
                            double hzPerBin, double zeroValue);

template <typename FLOAT_TYPE>
void
BLUtils::FreqsToMelNorm2(WDL_TypedBuf<FLOAT_TYPE> *resultMagns,
                       const WDL_TypedBuf<FLOAT_TYPE> &magns,
                       FLOAT_TYPE hzPerBin,
                       FLOAT_TYPE zeroValue)
{
    // For dB
    resultMagns->Resize(magns.GetSize());
    BLUtils::FillAllValue(resultMagns, zeroValue);
    
    FLOAT_TYPE maxFreq = hzPerBin*(magns.GetSize() - 1);
    FLOAT_TYPE maxMel = FreqToMel(maxFreq);
    
    if (maxFreq < BL_EPS)
        return;
    
    // Optim
    FLOAT_TYPE melCoeff = maxMel/resultMagns->GetSize();
    FLOAT_TYPE idCoeff = (1.0/maxFreq)*resultMagns->GetSize();
    
    int resultMagnsSize = resultMagns->GetSize();
    FLOAT_TYPE *resultMagnsData = resultMagns->Get();
    int magnsSize = magns.GetSize();
    FLOAT_TYPE *magnsData = magns.Get();
    
    for (int i = 0; i < resultMagnsSize; i++)
    {
        FLOAT_TYPE mel0 = (i - 0.5)*melCoeff;
        if (mel0 < 0.0)
            mel0 = 0.0;
        FLOAT_TYPE freq0 = MelToFreq(mel0);
        FLOAT_TYPE id0 = freq0*idCoeff;
        if (id0 < 0.0)
            id0 = 0.0;
        if ((int)id0 >= magnsSize)
            continue;
        
        FLOAT_TYPE mel1 = (i + 0.5)*melCoeff;
        if (mel1 < 0.0)
            mel1 = 0.0;
        FLOAT_TYPE freq1 = MelToFreq(mel1);
        FLOAT_TYPE id1 = freq1*idCoeff;
        if ((int)id1 >= magnsSize)
            id1 = magnsSize - 1;
        
        FLOAT_TYPE magn = 0.0;
        
        if (id1 - id0 > 1.0)
            // Scaling down
            // Scaling from biggest scale => must take several values
        {
            // Compute simple average
            FLOAT_TYPE avg = 0.0;
            int numValues = 0;
            for (int k = (int)id0; k <= (int)id1; k++)
            {
                avg += magnsData[k];
                numValues++;
            }
            if (numValues > 0)
                avg /= numValues;
            
            magn = avg;
        }
        else
        {
            // Scaling up
            // Scaling from smallest scale => must interpolate on the distination
            FLOAT_TYPE mel = i*melCoeff;
            
            FLOAT_TYPE freq = MelToFreq(mel);
            
            if (maxFreq < BL_EPS)
                return;
            
            FLOAT_TYPE i0 = freq*idCoeff;
            
            // Optim
            // (cast from FLOAT_TYPE to int is costly)
            int ii0 = (int)i0;
            
            FLOAT_TYPE t = i0 - ii0;
            
            if (ii0 >= magnsSize)
                continue;
            
            // NOTE: this optim doesn't compute exactly the same thing than the original version
            int i1 = ii0 + 1;
            if (i1 >= magnsSize)
                continue;
            
            FLOAT_TYPE magn0 = magnsData[ii0];
            FLOAT_TYPE magn1 = magnsData[i1];
            
            magn = (1.0 - t)*magn0 + t*magn1;
        }
        
        resultMagnsData[i] = magn;
    }
}
template void BLUtils::FreqsToMelNorm2(WDL_TypedBuf<float> *resultMagns,
                            const WDL_TypedBuf<float> &magns,
                            float hzPerBin,
                            float zeroValue);
template void BLUtils::FreqsToMelNorm2(WDL_TypedBuf<double> *resultMagns,
                            const WDL_TypedBuf<double> &magns,
                            double hzPerBin,
                            double zeroValue);

// Ok
template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::MelNormIdToFreq(FLOAT_TYPE idx, FLOAT_TYPE hzPerBin, int bufferSize)
{
    //FLOAT_TYPE maxFreq = hzPerBin*(bufferSize - 1);
    FLOAT_TYPE maxFreq = hzPerBin*(bufferSize/2);
    
    FLOAT_TYPE maxMel = FreqToMel(maxFreq);
    
    FLOAT_TYPE mel = (idx/(bufferSize/2))*maxMel;
    
    FLOAT_TYPE result = MelToFreq(mel);
    
    return result;
}
template float BLUtils::MelNormIdToFreq(float idx, float hzPerBin, int bufferSize);
template double BLUtils::MelNormIdToFreq(double idx, double hzPerBin, int bufferSize);

// GOOD !
template <typename FLOAT_TYPE>
int
BLUtils::FreqIdToMelNormId(int idx, FLOAT_TYPE hzPerBin, int bufferSize)
{
    FLOAT_TYPE maxFreq = hzPerBin*(bufferSize/2);
    FLOAT_TYPE maxMel = FreqToMel(maxFreq);
    
    FLOAT_TYPE freq = hzPerBin*idx;
    FLOAT_TYPE mel = FreqToMel(freq);
        
    FLOAT_TYPE resultId = (mel/maxMel)*(bufferSize/2);
    
    resultId = bl_round(resultId);
    if (resultId < 0)
        resultId = 0;
    if (resultId > bufferSize/2 - 1)
        resultId = bufferSize/2;
    
    return resultId;
}
template int BLUtils::FreqIdToMelNormId(int idx, float hzPerBin, int bufferSize);
template int BLUtils::FreqIdToMelNormId(int idx, double hzPerBin, int bufferSize);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::FreqIdToMelNormIdF(FLOAT_TYPE idx, FLOAT_TYPE hzPerBin, int bufferSize)
{
    FLOAT_TYPE maxFreq = hzPerBin*(bufferSize/2);
    FLOAT_TYPE maxMel = FreqToMel(maxFreq);
    
    FLOAT_TYPE freq = hzPerBin*idx;
    FLOAT_TYPE mel = FreqToMel(freq);
    
    FLOAT_TYPE resultId = (mel/maxMel)*(bufferSize/2);
    
    if (resultId < 0)
        resultId = 0;
    if (resultId > bufferSize/2 - 1)
        resultId = bufferSize/2;
    
    return resultId;
}
template float BLUtils::FreqIdToMelNormIdF(float idx, float hzPerBin, int bufferSize);
template double BLUtils::FreqIdToMelNormIdF(double idx, double hzPerBin, int bufferSize);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::FreqToMelNormId(FLOAT_TYPE freq, FLOAT_TYPE hzPerBin, int bufferSize)
{
    FLOAT_TYPE maxFreq = hzPerBin*(bufferSize/2);
    FLOAT_TYPE maxMel = FreqToMel(maxFreq);
    
    FLOAT_TYPE mel = FreqToMel(freq);
    
    FLOAT_TYPE resultId = (mel/maxMel)*(bufferSize/2);
    
    resultId = bl_round(resultId);
    if (resultId < 0)
        resultId = 0;
    if (resultId > bufferSize/2 - 1)
        resultId = bufferSize/2;
    
    return resultId;
}
template float BLUtils::FreqToMelNormId(float freq, float hzPerBin, int bufferSize);
template double BLUtils::FreqToMelNormId(double freq, double hzPerBin, int bufferSize);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::FreqToMelNorm(FLOAT_TYPE freq, FLOAT_TYPE hzPerBin, int bufferSize)
{
    FLOAT_TYPE maxFreq = hzPerBin*(bufferSize/2);
    FLOAT_TYPE maxMel = FreqToMel(maxFreq);
    
    FLOAT_TYPE mel = FreqToMel(freq);
    
    FLOAT_TYPE result = mel/maxMel;
    
    return result;
}
template float BLUtils::FreqToMelNorm(float freq, float hzPerBin, int bufferSize);
template double BLUtils::FreqToMelNorm(double freq, double hzPerBin, int bufferSize);

// Touch plug param
// When param is modified out of GUI or indirectly,
// touch the host
// => Then automations can be read
//
// NOTE: take care with Waveform Tracktion
void
BLUtils::TouchPlugParam(Plugin *plug, int paramIdx)
{
    // Force host update param, for automation
    plug->BeginInformHostOfParamChange(paramIdx);
    double normValue = plug->GetParam(paramIdx)->GetNormalized();
    //plug->GetGUI()->SetParameterFromPlug(paramIdx, normValue, true);
    plug->SetParameterValue(paramIdx, normValue);
    plug->InformHostOfParamChange(paramIdx, plug->GetParam(paramIdx)->GetNormalized());
    plug->EndInformHostOfParamChange(paramIdx);
}

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::FindNearestHarmonic(FLOAT_TYPE value, FLOAT_TYPE refValue)
{
    if (value < refValue)
        return refValue;
    
    FLOAT_TYPE coeff = value / refValue;
    FLOAT_TYPE rem = coeff - (int)coeff;
    
    FLOAT_TYPE result = value;
    if (rem < 0.5)
    {
        result = refValue*((int)coeff);
    }
    else
    {
        result = refValue*((int)coeff + 1);
    }
    
    return result;
}
template float BLUtils::FindNearestHarmonic(float value, float refValue);
template double BLUtils::FindNearestHarmonic(double value, double refValue);

// Recursive function to return gcd of a and b
//
// See: https://www.geeksforgeeks.org/program-find-gcd-floating-point-numbers/
//
template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::gcd(FLOAT_TYPE a, FLOAT_TYPE b)
{
#define EPS 0.001
    
    if (a < b)
        return gcd(b, a);
    
    // base case
    if (std::fabs(b) < EPS)
        return a;
    
    else
        return (gcd(b, a - std::floor(a / b) * b));
}
template float BLUtils::gcd(float a, float b);
template double BLUtils::gcd(double a, double b);

// Function to find gcd of array of numbers
//
// See: https://www.geeksforgeeks.org/gcd-two-array-numbers/
//
template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::gcd(const vector<FLOAT_TYPE> &arr)
{
    FLOAT_TYPE result = arr[0];
    for (int i = 1; i < arr.size(); i++)
        result = gcd(arr[i], result);
    
    return result;
}
template float BLUtils::gcd(const vector<float> &arr);
template double BLUtils::gcd(const vector<double> &arr);

template <typename FLOAT_TYPE>
void
BLUtils::MixParamToCoeffs(FLOAT_TYPE mix, FLOAT_TYPE *coeff0, FLOAT_TYPE *coeff1)
{
    //mix = (mix - 0.5)*2.0;
    
    if (mix <= 0.0)
    {
        *coeff0 = 1.0;
        *coeff1 = 1.0 + mix;
    }
    else if (mix > 0.0)
    {
        *coeff0 = 1.0 - mix;
        *coeff1 = 1.0;
    }
}
template void BLUtils::MixParamToCoeffs(float mix, float *coeff0, float *coeff1);
template void BLUtils::MixParamToCoeffs(double mix, double *coeff0, double *coeff1);

template <typename FLOAT_TYPE>
void
BLUtils::SmoothDataWin(WDL_TypedBuf<FLOAT_TYPE> *ioData,
                     const WDL_TypedBuf<FLOAT_TYPE> &win)
{
    WDL_TypedBuf<FLOAT_TYPE> data = *ioData;
    SmoothDataWin(ioData, data, win);
}
template void BLUtils::SmoothDataWin(WDL_TypedBuf<float> *ioData,
                          const WDL_TypedBuf<float> &win);
template void BLUtils::SmoothDataWin(WDL_TypedBuf<double> *ioData,
                          const WDL_TypedBuf<double> &win);

template <typename FLOAT_TYPE>
void
BLUtils::SmoothDataWin(WDL_TypedBuf<FLOAT_TYPE> *result,
                     const WDL_TypedBuf<FLOAT_TYPE> &data,
                     const WDL_TypedBuf<FLOAT_TYPE> &win)
{
    result->Resize(data.GetSize());
    
    int resultSize = result->GetSize();
    FLOAT_TYPE *resultData = result->Get();
    int winSize = win.GetSize();
    FLOAT_TYPE *winData = win.Get();
    FLOAT_TYPE *dataData = data.Get();
    
    for (int i = 0; i < resultSize; i++)
    {
        FLOAT_TYPE sumVal = 0.0;
        FLOAT_TYPE sumCoeff = 0.0;
        for (int j = 0; j < winSize; j++)
        {
            int idx = i + j - winSize/2;
            if ((idx < 0) || (idx > resultSize - 1))
                continue;
            
            FLOAT_TYPE val = dataData[idx];
            FLOAT_TYPE coeff = winData[j];
            
            sumVal += val*coeff;
            sumCoeff += coeff;
        }
        
        if (sumCoeff > BL_EPS)
        {
            sumVal /= sumCoeff;
        }
        
        resultData[i] = sumVal;
    }
}
template void BLUtils::SmoothDataWin(WDL_TypedBuf<float> *result,
                          const WDL_TypedBuf<float> &data,
                          const WDL_TypedBuf<float> &win);
template void BLUtils::SmoothDataWin(WDL_TypedBuf<double> *result,
                          const WDL_TypedBuf<double> &data,
                          const WDL_TypedBuf<double> &win);

template <typename FLOAT_TYPE>
void
BLUtils::SmoothDataPyramid(WDL_TypedBuf<FLOAT_TYPE> *result,
                         const WDL_TypedBuf<FLOAT_TYPE> &data,
                         int maxLevel)
{
    WDL_TypedBuf<FLOAT_TYPE> xCoordinates;
    xCoordinates.Resize(data.GetSize());
    
    int dataSize = data.GetSize();
    FLOAT_TYPE *xCoordinatesData = xCoordinates.Get();
    
    for (int i = 0; i < dataSize; i++)
    {
        xCoordinatesData[i] = i;
    }
    
    WDL_TypedBuf<FLOAT_TYPE> yCoordinates = data;
    
    int level = 1;
    while(level < maxLevel)
    {
        WDL_TypedBuf<FLOAT_TYPE> newXCoordinates;
        WDL_TypedBuf<FLOAT_TYPE> newYCoordinates;
        
        // Comppute new size
        FLOAT_TYPE newSize = xCoordinates.GetSize()/2.0;
        newSize = ceil(newSize);
        
        // Prepare
        newXCoordinates.Resize((int)newSize);
        newYCoordinates.Resize((int)newSize);
        
        // Copy the last value (in case of odd size)
        newXCoordinates.Get()[(int)newSize - 1] =
                xCoordinates.Get()[xCoordinates.GetSize() - 1];
        newYCoordinates.Get()[(int)newSize - 1] =
            yCoordinates.Get()[yCoordinates.GetSize() - 1];
        
        // Divide by 2
        
        int xCoordinatesSize = xCoordinates.GetSize();
        FLOAT_TYPE *xCoordinatesData = xCoordinates.Get();
        int yCoordinatesSize = yCoordinates.GetSize();
        FLOAT_TYPE *yCoordinatesData = yCoordinates.Get();
        FLOAT_TYPE *newXCoordinatesData = newXCoordinates.Get();
        FLOAT_TYPE *newYCoordinatesData = newYCoordinates.Get();
        
        for (int i = 0; i < xCoordinatesSize; i += 2)
        {
            // x
            FLOAT_TYPE x0 = xCoordinatesData[i];
            FLOAT_TYPE x1 = x0;
            if (i + 1 < xCoordinatesSize)
                x1 = xCoordinatesData[i + 1];
            
            // y
            FLOAT_TYPE y0 = yCoordinatesData[i];
            
            FLOAT_TYPE y1 = y0;
            if (i + 1 < yCoordinatesSize)
                y1 = yCoordinatesData[i + 1];
            
            // new
            FLOAT_TYPE newX = (x0 + x1)/2.0;
            FLOAT_TYPE newY = (y0 + y1)/2.0;
            
            // add
            newXCoordinatesData[i/2] = newX;
            newYCoordinatesData[i/2] = newY;
        }
        
        xCoordinates = newXCoordinates;
        yCoordinates = newYCoordinates;
        
        level++;
    }
    
    // Prepare the result
    result->Resize(data.GetSize());
    BLUtils::FillAllZero(result);
    
    // Put the low LOD values into the result
    
    int xCoordinatesSize = xCoordinates.GetSize();
    //FLOAT_TYPE *xCoordinatesData = xCoordinates.Get();
    FLOAT_TYPE *yCoordinatesData = yCoordinates.Get();
    int resultSize = result->GetSize();
    FLOAT_TYPE *resultData = result->Get();
    
    for (int i = 0; i < xCoordinatesSize; i++)
    {
        FLOAT_TYPE x = xCoordinatesData[i];
        FLOAT_TYPE y = yCoordinatesData[i];
        
        x = bl_round(x);
        
        // check bounds
        if (x < 0.0)
            x = 0.0;
        if (x > resultSize - 1)
            x = resultSize - 1;
        
        resultData[(int)x] = y;
    }
    
    // And complete the values that remained zero
    BLUtils::FillMissingValues(result, false, (FLOAT_TYPE)0.0);
}
template void BLUtils::SmoothDataPyramid(WDL_TypedBuf<float> *result,
                              const WDL_TypedBuf<float> &data,
                              int maxLevel);
template void BLUtils::SmoothDataPyramid(WDL_TypedBuf<double> *result,
                              const WDL_TypedBuf<double> &data,
                              int maxLevel);

template <typename FLOAT_TYPE>
void
BLUtils::ApplyWindowMin(WDL_TypedBuf<FLOAT_TYPE> *values, int winSize)
{
    WDL_TypedBuf<FLOAT_TYPE> result;
    result.Resize(values->GetSize());
    
    int resultSize = result.GetSize();
    FLOAT_TYPE *resultData = result.Get();
    FLOAT_TYPE *valuesData = values->Get();
    
    for (int i = 0; i < resultSize; i++)
    {
        FLOAT_TYPE minVal = INF;
        for (int j = 0; j < winSize; j++)
        {
            int idx = i + j - winSize/2;
            
            if ((idx < 0) || (idx > resultSize - 1))
                continue;
            
            FLOAT_TYPE val = valuesData[idx];
            if (val < minVal)
                minVal = val;
        }
        
        resultData[i] = minVal;
    }
    
    *values = result;
}
template void BLUtils::ApplyWindowMin(WDL_TypedBuf<float> *values, int winSize);
template void BLUtils::ApplyWindowMin(WDL_TypedBuf<double> *values, int winSize);

template <typename FLOAT_TYPE>
void
BLUtils::ApplyWindowMax(WDL_TypedBuf<FLOAT_TYPE> *values, int winSize)
{
    WDL_TypedBuf<FLOAT_TYPE> result;
    result.Resize(values->GetSize());
    
    int resultSize = result.GetSize();
    FLOAT_TYPE *resultData = result.Get();
    FLOAT_TYPE *valuesData = values->Get();
    
    for (int i = 0; i < resultSize; i++)
    {
        FLOAT_TYPE maxVal = -INF;
        for (int j = 0; j < winSize; j++)
        {
            int idx = i + j - winSize/2;
            
            if ((idx < 0) || (idx > resultSize - 1))
                continue;
            
            FLOAT_TYPE val = valuesData[idx];
            if (val > maxVal)
                maxVal = val;
        }
        
        resultData[i] = maxVal;
    }
    
    *values = result;
}
template void BLUtils::ApplyWindowMax(WDL_TypedBuf<float> *values, int winSize);
template void BLUtils::ApplyWindowMax(WDL_TypedBuf<double> *values, int winSize);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeMean(const WDL_TypedBuf<FLOAT_TYPE> &values)
{
    if (values.GetSize() == 0)
        return 0.0;
    
    // SIMD
    
    //int valuesSize = values.GetSize();
    //FLOAT_TYPE *valuesData = values.Get();
    
    FLOAT_TYPE sum = ComputeSum(values);
    
    //FLOAT_TYPE sum = 0.0;
    //for (int i = 0; i < valuesSize; i++)
    //{
    //    FLOAT_TYPE val = valuesData[i];
    //
    //    sum += val;
    //}
    
    FLOAT_TYPE result = sum/values.GetSize();
    
    return result;
}
template float BLUtils::ComputeMean(const WDL_TypedBuf<float> &values);
template double BLUtils::ComputeMean(const WDL_TypedBuf<double> &values);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeSigma(const WDL_TypedBuf<FLOAT_TYPE> &values)
{
    if (values.GetSize() == 0)
        return 0.0;
    
    FLOAT_TYPE mean = ComputeMean(values);
    
    FLOAT_TYPE sum = 0.0;
    
    int valuesSize = values.GetSize();
    FLOAT_TYPE *valuesData = values.Get();
    
    for (int i = 0; i < valuesSize; i++)
    {
        FLOAT_TYPE val = valuesData[i];
        
        FLOAT_TYPE diff = std::fabs(val - mean);
        
        sum += diff;
    }
    
    FLOAT_TYPE result = sum/values.GetSize();
    
    return result;
}
template float BLUtils::ComputeSigma(const WDL_TypedBuf<float> &values);
template double BLUtils::ComputeSigma(const WDL_TypedBuf<double> &values);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeMean(const vector<FLOAT_TYPE> &values)
{
    if (values.empty())
        return 0.0;
    
    FLOAT_TYPE sum = 0.0;
    for (int i = 0; i < values.size(); i++)
    {
        FLOAT_TYPE val = values[i];
        
        sum += val;
    }
    
    FLOAT_TYPE result = sum/values.size();
    
    return result;
}
template float BLUtils::ComputeMean(const vector<float> &values);
template double BLUtils::ComputeMean(const vector<double> &values);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeSigma(const vector<FLOAT_TYPE> &values)
{
    if (values.size())
        return 0.0;
    
    FLOAT_TYPE mean = ComputeMean(values);
    
    FLOAT_TYPE sum = 0.0;
    for (int i = 0; i < values.size(); i++)
    {
        FLOAT_TYPE val = values[i];
        
        FLOAT_TYPE diff = std::fabs(val - mean);
        
        sum += diff;
    }
    
    FLOAT_TYPE result = sum/values.size();
    
    return result;
}
template float BLUtils::ComputeSigma(const vector<float> &values);
template double BLUtils::ComputeSigma(const vector<double> &values);

template <typename FLOAT_TYPE>
void
BLUtils::GetMinMaxFreqAxisValues(FLOAT_TYPE *minHzValue, FLOAT_TYPE *maxHzValue,
                               int bufferSize, FLOAT_TYPE sampleRate)
{
    FLOAT_TYPE hzPerBin = sampleRate/bufferSize;
    
    *minHzValue = 1*hzPerBin;
    *maxHzValue = (bufferSize/2)*hzPerBin;
}
template void BLUtils::GetMinMaxFreqAxisValues(float *minHzValue, float *maxHzValue,
                                    int bufferSize, float sampleRate);
template void BLUtils::GetMinMaxFreqAxisValues(double *minHzValue, double *maxHzValue,
                                    int bufferSize, double sampleRate);

template <typename FLOAT_TYPE>
void
BLUtils::GenNoise(WDL_TypedBuf<FLOAT_TYPE> *ioBuf)
{
    int ioBufSize = ioBuf->GetSize();
    FLOAT_TYPE *ioBufData = ioBuf->Get();
    
    for (int i = 0; i < ioBufSize; i++)
    {
        FLOAT_TYPE noise = ((FLOAT_TYPE)std::rand())/RAND_MAX;
        ioBufData[i] = noise;
    }
}
template void BLUtils::GenNoise(WDL_TypedBuf<float> *ioBuf);
template void BLUtils::GenNoise(WDL_TypedBuf<double> *ioBuf);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeDist(FLOAT_TYPE p0[2], FLOAT_TYPE p1[2])
{
    FLOAT_TYPE a = p0[0] - p1[0];
    FLOAT_TYPE b = p0[1] - p1[1];
    
    FLOAT_TYPE d2 = a*a + b*b;
    FLOAT_TYPE dist = std::sqrt(d2);
    
    return dist;
}
template float BLUtils::ComputeDist(float p0[2], float p1[2]);
template double BLUtils::ComputeDist(double p0[2], double p1[2]);

template <typename FLOAT_TYPE>
void
BLUtils::CutHalfSamples(WDL_TypedBuf<FLOAT_TYPE> *samples, bool cutDown)
{
    // FIX: fixes scrolling and spreading the last value when stop playing
    // - UST: feed clipper also when not playing
    // => the last value scrolls (that makes a line)
#define FIX_SPREAD_LAST_VALUE 1
    
    // Find first value
    FLOAT_TYPE firstValue = 0.0;
    
    for (int i = 0; i < samples->GetSize(); i++)
    {
        FLOAT_TYPE samp = samples->Get()[i];
        if ((samp >= 0.0) && cutDown)
        {
            firstValue = samp;
            break;
        }
        
        if ((samp <= 0.0) && !cutDown)
        {
            firstValue = samp;
            break;
        }
    }
    
    // Cut half, and replace undefined values by prev valid value
    FLOAT_TYPE prevValue = firstValue;
    for (int i = 0; i < samples->GetSize(); i++)
    {
        FLOAT_TYPE samp = samples->Get()[i];
        
#if !FIX_SPREAD_LAST_VALUE
        if ((samp >= 0.0) && !cutDown)
#else
        if ((samp > 0.0) && !cutDown)
#endif
        {
            samp = prevValue;
            
            samples->Get()[i] = samp;
            
            continue;
        }

#if !FIX_SPREAD_LAST_VALUE
        if ((samp <= 0.0) && cutDown)
#else
        if ((samp < 0.0) && cutDown)
#endif
        {
            samp = prevValue;
            
            samples->Get()[i] = samp;
            
            continue;
        }
        
        prevValue = samp;
    }
}
template void BLUtils::CutHalfSamples(WDL_TypedBuf<float> *samples, bool cutDown);
template void BLUtils::CutHalfSamples(WDL_TypedBuf<double> *samples, bool cutDown);

template <typename FLOAT_TYPE>
bool
BLUtils::IsMono(const WDL_TypedBuf<FLOAT_TYPE> &leftSamples,
              const WDL_TypedBuf<FLOAT_TYPE> &rightSamples,
              FLOAT_TYPE eps)
{
    if (leftSamples.GetSize() != rightSamples.GetSize())
        return false;
    
#if USE_SIMD
    int bufferSize = leftSamples.GetSize();
    const FLOAT_TYPE *buf0Data = leftSamples.Get();
    const FLOAT_TYPE *buf1Data = rightSamples.Get();
    
    if (_useSimd && (bufferSize % SIMD_PACK_SIZE == 0))
    {
        for (int i = 0; i < bufferSize; i += SIMD_PACK_SIZE)
        {
            simdpp::float64<SIMD_PACK_SIZE> v0 = simdpp::load(buf0Data);
            simdpp::float64<SIMD_PACK_SIZE> v1 = simdpp::load(buf1Data);
            
            simdpp::float64<SIMD_PACK_SIZE> d = v1 - v0;
            
            simdpp::float64<SIMD_PACK_SIZE> a = simdpp::abs(d);
            
            FLOAT_TYPE r = simdpp::reduce_max(a);
            if (r > eps)
                return false;
            
            buf0Data += SIMD_PACK_SIZE;
            buf1Data += SIMD_PACK_SIZE;
        }
        
        // Finished
        return true;
    }
#endif
    
    for (int i = 0; i < leftSamples.GetSize(); i++)
    {
        FLOAT_TYPE left = leftSamples.Get()[i];
        FLOAT_TYPE right = rightSamples.Get()[i];
        
        if (std::fabs(left - right) > eps)
            return false;
    }
    
    return true;
}
template bool BLUtils::IsMono(const WDL_TypedBuf<float> &leftSamples,
                   const WDL_TypedBuf<float> &rightSamples,
                   float eps);
template bool BLUtils::IsMono(const WDL_TypedBuf<double> &leftSamples,
                   const WDL_TypedBuf<double> &rightSamples,
                   double eps);

template <typename FLOAT_TYPE>
void
BLUtils::Smooth(WDL_TypedBuf<FLOAT_TYPE> *ioCurrentValues,
              WDL_TypedBuf<FLOAT_TYPE> *ioPrevValues,
              FLOAT_TYPE smoothFactor)
{
    if (ioCurrentValues->GetSize() != ioPrevValues->GetSize())
        return;
    
#if USE_SIMD_OPTIM
    int nFrames = ioCurrentValues->GetSize();
    FLOAT_TYPE *currentBuf = ioCurrentValues->Get();
    FLOAT_TYPE *prevBuf = ioPrevValues->Get();
    Mix(currentBuf, currentBuf, prevBuf, nFrames, smoothFactor);
    
    // *ioPrevValues = *ioCurrentValues;
#else
    for (int i = 0; i < ioCurrentValues->GetSize(); i++)
    {
        FLOAT_TYPE val = ioCurrentValues->Get()[i];
        FLOAT_TYPE prevVal = ioPrevValues->Get()[i];
        
        FLOAT_TYPE newVal = smoothFactor*prevVal + (1.0 - smoothFactor)*val;
        
        ioCurrentValues->Get()[i] = newVal;
    }
#endif
    
    // Warinig, this line ins important!
    *ioPrevValues = *ioCurrentValues;
}
template void BLUtils::Smooth(WDL_TypedBuf<float> *ioCurrentValues,
                            WDL_TypedBuf<float> *ioPrevValues,
                            float smoothFactor);
template void BLUtils::Smooth(WDL_TypedBuf<double> *ioCurrentValues,
                            WDL_TypedBuf<double> *ioPrevValues,
                            double smoothFactor);


template <typename FLOAT_TYPE>
void
BLUtils::Smooth(vector<WDL_TypedBuf<FLOAT_TYPE> > *ioCurrentValues,
              vector<WDL_TypedBuf<FLOAT_TYPE> > *ioPrevValues,
              FLOAT_TYPE smoothFactor)
{
    if (ioCurrentValues->size() != ioPrevValues->size())
        return;
    
    for (int i = 0; i < ioCurrentValues->size(); i++)
    {
        WDL_TypedBuf<FLOAT_TYPE> &current = (*ioCurrentValues)[i];
        //WDL_TypedBuf<FLOAT_TYPE> &prev = (*ioCurrentValues)[i];
        WDL_TypedBuf<FLOAT_TYPE> &prev = (*ioPrevValues)[i];
    
        if (current.GetSize() != prev.GetSize())
            return; // abort
        
#if !USE_SIMD_OPTIM
        for (int j = 0; j < current.GetSize(); j++)
        {
            FLOAT_TYPE val = current.Get()[j];
            FLOAT_TYPE prevVal = prev.Get()[j];
        
            FLOAT_TYPE newVal = smoothFactor*prevVal + (1.0 - smoothFactor)*val;
        
            current.Get()[j] = newVal;
        }
#else
        Smooth(&current, &prev, smoothFactor);
#endif
    }
    
    *ioPrevValues = *ioCurrentValues;
}
template void BLUtils::Smooth(vector<WDL_TypedBuf<float> > *ioCurrentValues,
                            vector<WDL_TypedBuf<float> > *ioPrevValues,
                            float smoothFactor);
template void BLUtils::Smooth(vector<WDL_TypedBuf<double> > *ioCurrentValues,
                            vector<WDL_TypedBuf<double> > *ioPrevValues,
                            double smoothFactor);

template <typename FLOAT_TYPE>
void
BLUtils::SmoothMax(WDL_TypedBuf<FLOAT_TYPE> *ioCurrentValues,
                 WDL_TypedBuf<FLOAT_TYPE> *ioPrevValues,
                 FLOAT_TYPE smoothFactor)
{
    if (ioCurrentValues->GetSize() != ioPrevValues->GetSize())
        return;
    
#if !USE_SIMD_OPTIM
    for (int i = 0; i < ioCurrentValues->GetSize(); i++)
    {
        FLOAT_TYPE val = ioCurrentValues->Get()[i];
        FLOAT_TYPE prevVal = ioPrevValues->Get()[i];
        
        if (val > prevVal)
            prevVal = val;
        
        FLOAT_TYPE newVal = smoothFactor*prevVal + (1.0 - smoothFactor)*val;
        
        ioCurrentValues->Get()[i] = newVal;
    }
    
    *ioPrevValues = *ioCurrentValues;
#else
    ComputeMax(ioPrevValues, *ioCurrentValues);
    Smooth(ioCurrentValues, ioPrevValues, smoothFactor);
#endif
}
template void BLUtils::SmoothMax(WDL_TypedBuf<float> *ioCurrentValues,
                               WDL_TypedBuf<float> *ioPrevValues,
                               float smoothFactor);
template void BLUtils::SmoothMax(WDL_TypedBuf<double> *ioCurrentValues,
                               WDL_TypedBuf<double> *ioPrevValues,
                               double smoothFactor);

// Detect all maxima in the data
// NOTE: for the moment take 0 for the minimal value
template <typename FLOAT_TYPE>
void
BLUtils::FindMaxima(const WDL_TypedBuf<FLOAT_TYPE> &data,
                  WDL_TypedBuf<FLOAT_TYPE> *result)
{
    result->Resize(data.GetSize());
    BLUtils::FillAllZero(result);
    
    FLOAT_TYPE prevValue = 0.0;
    for (int i = 0; i < data.GetSize(); i++)
    {
        FLOAT_TYPE currentValue = data.Get()[i];
        
        FLOAT_TYPE nextValue = 0.0;
        if (i < data.GetSize() - 1)
            nextValue = data.Get()[i + 1];
        
        if ((currentValue > prevValue) && (currentValue > nextValue))
        {
            result->Get()[i] = currentValue;
        }
        
        prevValue = currentValue;
    }
}
template void BLUtils::FindMaxima(const WDL_TypedBuf<float> &data,
                       WDL_TypedBuf<float> *result);
template void BLUtils::FindMaxima(const WDL_TypedBuf<double> &data,
                       WDL_TypedBuf<double> *result);

// Fill with 1.0 where there is a maxima, 0.0 elsewhere
template <typename FLOAT_TYPE>
void
BLUtils::FindMaxima2(const WDL_TypedBuf<FLOAT_TYPE> &data,
                   WDL_TypedBuf<FLOAT_TYPE> *result)
{
    result->Resize(data.GetSize());
    BLUtils::FillAllZero(result);
    
    FLOAT_TYPE prevValue = 0.0;
    for (int i = 0; i < data.GetSize(); i++)
    {
        FLOAT_TYPE currentValue = data.Get()[i];
        
        FLOAT_TYPE nextValue = 0.0;
        if (i < data.GetSize() - 1)
            nextValue = data.Get()[i + 1];
        
        if ((currentValue > prevValue) && (currentValue > nextValue))
        {
            result->Get()[i] = 1.0;
        }
        
        prevValue = currentValue;
    }
}
template void BLUtils::FindMaxima2(const WDL_TypedBuf<float> &data,
                        WDL_TypedBuf<float> *result);
template void BLUtils::FindMaxima2(const WDL_TypedBuf<double> &data,
                        WDL_TypedBuf<double> *result);

template <typename FLOAT_TYPE>
void
BLUtils::FindMaxima(WDL_TypedBuf<FLOAT_TYPE> *ioData)
{
    WDL_TypedBuf<FLOAT_TYPE> data = *ioData;
    FindMaxima(data, ioData);
}
template void BLUtils::FindMaxima(WDL_TypedBuf<float> *ioData);
template void BLUtils::FindMaxima(WDL_TypedBuf<double> *ioData);

// Fill with 1.0 or max value where there is a maxima, 0.0 elsewhere
// If keepMaxValue is true, fill with max value instead of 1.0
template <typename FLOAT_TYPE>
void
BLUtils::FindMaxima2D(int width, int height,
                    const WDL_TypedBuf<FLOAT_TYPE> &data,
                    WDL_TypedBuf<FLOAT_TYPE> *result,
                    bool keepMaxValue)
{
    result->Resize(data.GetSize());
    BLUtils::FillAllZero(result);
    
    for (int j = 0; j < height; j++)
    {
        for (int i = 0; i < width; i++)
        {
            FLOAT_TYPE currentValue = data.Get()[i + j*width];
            
            bool maximumFound = true;
            for (int i0 = i - 1; i0 <= i + 1; i0++)
            {
                if (!maximumFound)
                    break;
                
                for (int j0 = j - 1; j0 <= j + 1; j0++)
                {
                    if ((i0 >= 0) && (i0 < width) &&
                        (j0 >= 0) && (j0 < height))
                    {
                        FLOAT_TYPE val = data.Get()[i0 + j0*width];
                        
                        if (val > currentValue)
                        {
                            maximumFound = false;
                            break;
                        }
                    }
                }
            }
            
            if (maximumFound)
            {
                FLOAT_TYPE val = 1.0;
                if (keepMaxValue)
                    val = currentValue;
                
                result->Get()[i + j*width] = val;
            }
        }
    }
}
template void BLUtils::FindMaxima2D(int width, int height,
                         const WDL_TypedBuf<float> &data,
                         WDL_TypedBuf<float> *result,
                         bool keepMaxValue);
template void BLUtils::FindMaxima2D(int width, int height,
                         const WDL_TypedBuf<double> &data,
                         WDL_TypedBuf<double> *result,
                         bool keepMaxValue);

template <typename FLOAT_TYPE>
void
BLUtils::FindMaxima2D(int width, int height, WDL_TypedBuf<FLOAT_TYPE> *ioData,
                    bool keepMaxValue)
{
    WDL_TypedBuf<FLOAT_TYPE> data = *ioData;
    FindMaxima2D(width, height, data, ioData, keepMaxValue);
}
template void BLUtils::FindMaxima2D(int width, int height, WDL_TypedBuf<float> *ioData,
                         bool keepMaxValue);
template void BLUtils::FindMaxima2D(int width, int height, WDL_TypedBuf<double> *ioData,
                         bool keepMaxValue);

template <typename FLOAT_TYPE>
void
BLUtils::FindMinima(const WDL_TypedBuf<FLOAT_TYPE> &data,
                  WDL_TypedBuf<FLOAT_TYPE> *result,
                  FLOAT_TYPE infValue)
{
    result->Resize(data.GetSize());
    BLUtils::FillAllValue(result, infValue);
    
    FLOAT_TYPE prevValue = infValue;
    for (int i = 0; i < data.GetSize(); i++)
    {
        FLOAT_TYPE currentValue = data.Get()[i];
        
        FLOAT_TYPE nextValue = infValue;
        if (i < data.GetSize() - 1)
            nextValue = data.Get()[i + 1];
        
        if ((currentValue < prevValue) && (currentValue < nextValue))
        {
            result->Get()[i] = currentValue;
        }
        
        prevValue = currentValue;
    }
}
template void BLUtils::FindMinima(const WDL_TypedBuf<float> &data,
                       WDL_TypedBuf<float> *result,
                       float infValue);
template void BLUtils::FindMinima(const WDL_TypedBuf<double> &data,
                       WDL_TypedBuf<double> *result,
                       double infValue);

template <typename FLOAT_TYPE>
void
BLUtils::FindMinima(const WDL_TypedBuf<FLOAT_TYPE> &data,
                  WDL_TypedBuf<FLOAT_TYPE> *result)
{
    if (data.GetSize() == 0)
        return;
    
    result->Resize(data.GetSize());
    BLUtils::FillAllValue(result, (FLOAT_TYPE)1.0);
    
    FLOAT_TYPE prevValue = data.Get()[0];
    for (int i = 0; i < data.GetSize(); i++)
    {
        FLOAT_TYPE currentValue = data.Get()[i];
        
        FLOAT_TYPE nextValue = 1.0;
        if (i < data.GetSize() - 1)
            nextValue = data.Get()[i + 1];
        
        if ((currentValue < prevValue) && (currentValue < nextValue))
        {
            result->Get()[i] = currentValue;
        }
        
        prevValue = currentValue;
    }
}
template void BLUtils::FindMinima(const WDL_TypedBuf<float> &data,
                       WDL_TypedBuf<float> *result);
template void BLUtils::FindMinima(const WDL_TypedBuf<double> &data,
                       WDL_TypedBuf<double> *result);

// Fill with 0.0 where there is a minima, 0.0 elsewhere
template <typename FLOAT_TYPE>
void
BLUtils::FindMinima2(const WDL_TypedBuf<FLOAT_TYPE> &data,
                   WDL_TypedBuf<FLOAT_TYPE> *result)
{
    if (data.GetSize() == 0)
        return;
    
    result->Resize(data.GetSize());
    BLUtils::FillAllValue(result, (FLOAT_TYPE)1.0);
    
    FLOAT_TYPE prevValue = data.Get()[0];
    for (int i = 0; i < data.GetSize(); i++)
    {
        FLOAT_TYPE currentValue = data.Get()[i];
        
        FLOAT_TYPE nextValue = 1.0;
        if (i < data.GetSize() - 1)
            nextValue = data.Get()[i + 1];
        
        if ((currentValue < prevValue) && (currentValue < nextValue))
        {
            result->Get()[i] = 0.0;
        }
        
        prevValue = currentValue;
    }
}
template void BLUtils::FindMinima2(const WDL_TypedBuf<float> &data,
                        WDL_TypedBuf<float> *result);
template void BLUtils::FindMinima2(const WDL_TypedBuf<double> &data,
                        WDL_TypedBuf<double> *result);

template <typename FLOAT_TYPE>
void
BLUtils::ComputeDerivative(const WDL_TypedBuf<FLOAT_TYPE> &values,
                         WDL_TypedBuf<FLOAT_TYPE> *result)
{
    if (values.GetSize() == 0)
        return;
    
    result->Resize(values.GetSize());
    
    int size = values.GetSize();
    FLOAT_TYPE *valuesBuf = values.Get();
    FLOAT_TYPE *resultBuf = result->Get();
    
    FLOAT_TYPE prevValue = values.Get()[0];
    for (int i = 0; i < size; i++)
    {
        FLOAT_TYPE val = valuesBuf[i];
        FLOAT_TYPE diff = val - prevValue;
        
        resultBuf[i] = diff;
        
        prevValue = val;
    }
}
template void BLUtils::ComputeDerivative(const WDL_TypedBuf<float> &values,
                              WDL_TypedBuf<float> *result);
template void BLUtils::ComputeDerivative(const WDL_TypedBuf<double> &values,
                              WDL_TypedBuf<double> *result);

template <typename FLOAT_TYPE>
void
BLUtils::ComputeDerivative(WDL_TypedBuf<FLOAT_TYPE> *ioValues)
{
    WDL_TypedBuf<FLOAT_TYPE> values = *ioValues;
    ComputeDerivative(values, ioValues);
}
template void BLUtils::ComputeDerivative(WDL_TypedBuf<float> *ioValues);
template void BLUtils::ComputeDerivative(WDL_TypedBuf<double> *ioValues);

void
BLUtils::SwapColors(LICE_MemBitmap *bmp)
{
    int w = bmp->getWidth();
    int h = bmp->getHeight();
    
    LICE_pixel *buf = bmp->getBits();
    
    for (int i = 0; i < w*h; i++)
    {
        LICE_pixel &pix = buf[i];
        
        int r = LICE_GETR(pix);
        int g = LICE_GETG(pix);
        int b = LICE_GETB(pix);
        int a = LICE_GETA(pix);
        
        pix = LICE_RGBA(b, g, r, a);
    }
}

// See: http://paulbourke.net/miscellaneous/correlate/
template <typename FLOAT_TYPE>
void
BLUtils::CrossCorrelation2D(const vector<WDL_TypedBuf<FLOAT_TYPE> > &image,
                          const vector<WDL_TypedBuf<FLOAT_TYPE> > &mask,
                          vector<WDL_TypedBuf<FLOAT_TYPE> > *corr)
{
    if (image.empty())
        return;
    
    if (image.size() != mask.size())
        return;
    
    // Allocate
    int lineSize = image[0].GetSize();
    
    corr->resize(image.size());
    for (int i = 0; i < image.size(); i++)
    {
        (*corr)[i].Resize(lineSize);
    }
    
    //
    FLOAT_TYPE imageAvg = BLUtils::ComputeAvg(image);
    FLOAT_TYPE maskAvg = BLUtils::ComputeAvg(mask);
    
    // Compute
    for (int i = 0; i < image.size(); i++)
    {
        for (int j = 0; j < lineSize; j++)
        {
            FLOAT_TYPE rij = 0.0;
            
            // Sum
            for (int ii = -(int)image.size()/2; ii < (int)image.size()/2; ii++)
            {
                for (int jj = -lineSize/2; jj < lineSize/2; jj++)
                {
                    // Mask
                    if (ii + (int)image.size()/2 < 0)
                        continue;
                    if (ii + (int)image.size()/2 >= (int)mask.size())
                        continue;
                 
                    if (jj + lineSize/2 < 0)
                        continue;
                    if (jj + lineSize/2 >= mask[ii + (int)image.size()/2].GetSize())
                        continue;
                    
                    FLOAT_TYPE maskVal = mask[ii + (int)image.size()/2].Get()[jj + lineSize/2];
                    
                    // Image
                    if (i + ii < 0)
                        continue;
                    if (i + ii >= (int)image.size())
                        continue;
                    
                    if (j + jj < 0)
                        continue;
                    if (j + jj >= image[i + ii].GetSize())
                        continue;
                    
                    FLOAT_TYPE imageVal = image[i + ii].Get()[j + jj];
                    
                    FLOAT_TYPE r = (maskVal - maskAvg)*(imageVal - imageAvg);
                    
                    rij += r;
                }
            }
            
            (*corr)[i].Get()[j] = rij;
        }
    }
}
template void BLUtils::CrossCorrelation2D(const vector<WDL_TypedBuf<float> > &image,
                               const vector<WDL_TypedBuf<float> > &mask,
                               vector<WDL_TypedBuf<float> > *corr);
template void BLUtils::CrossCorrelation2D(const vector<WDL_TypedBuf<double> > &image,
                               const vector<WDL_TypedBuf<double> > &mask,
                               vector<WDL_TypedBuf<double> > *corr);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::BinaryImageMatch(const vector<WDL_TypedBuf<FLOAT_TYPE> > &image0,
                        const vector<WDL_TypedBuf<FLOAT_TYPE> > &image1)
{
    FLOAT_TYPE matchScore0 = 0.0; //BinaryImageMatchAux(image0, image1);
    FLOAT_TYPE matchScore1 = BinaryImageMatchAux(image1, image0);
    
    FLOAT_TYPE matchScore = (matchScore0 + matchScore1)/2.0;
    
    return matchScore;
}
template float BLUtils::BinaryImageMatch(const vector<WDL_TypedBuf<float> > &image0,
                              const vector<WDL_TypedBuf<float> > &image1);
template double BLUtils::BinaryImageMatch(const vector<WDL_TypedBuf<double> > &image0,
                               const vector<WDL_TypedBuf<double> > &image1);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::BinaryImageMatchAux(const vector<WDL_TypedBuf<FLOAT_TYPE> > &image0,
                           const vector<WDL_TypedBuf<FLOAT_TYPE> > &image1)
{
    int numHit = 0;
    int numMiss = 0;
    int numForeground = 0;
    
    for (int i = 0; i < image0.size(); i++)
    {
        for (int j = 0; j < image0[i].GetSize(); j++)
        {
            FLOAT_TYPE val0 = image0[i].Get()[j];
            FLOAT_TYPE val1 = image1[i].Get()[j];
            
            if (val0 > 0.0)
            {
                numForeground++;
                
                if (val1 > 0.0)
                {
                    numHit++;
                }
                else
                {
                    numMiss++;
                }
            }
        }
    }
    
    FLOAT_TYPE matchScore = 0.0;
    if (numForeground > 0)
    {
        matchScore = ((FLOAT_TYPE)(numHit - numMiss))/numForeground;
    }
    
    return matchScore;
}
template float BLUtils::BinaryImageMatchAux(const vector<WDL_TypedBuf<float> > &image0,
                                 const vector<WDL_TypedBuf<float> > &image1);
template double BLUtils::BinaryImageMatchAux(const vector<WDL_TypedBuf<double> > &image0,
                                  const vector<WDL_TypedBuf<double> > &image1);

template <typename FLOAT_TYPE>
void
BLUtils::BuildMinMaxMapHoriz(vector<WDL_TypedBuf<FLOAT_TYPE> > *values)
{
    //const FLOAT_TYPE undefineValue = -1e15;
    const FLOAT_TYPE undefineValue = -1.0;
    
    if (values->empty())
        return;
    
    // Init
    vector<WDL_TypedBuf<FLOAT_TYPE> > minMaxMap = *values;
    for (int i = 0; i < minMaxMap.size(); i++)
    {
        BLUtils::FillAllValue(&minMaxMap[i], undefineValue);
    }
    
    // Iterate over lines
    for (int i = 0; i < (*values)[0].GetSize(); i++)
    {
        // Generate the source line
        WDL_TypedBuf<FLOAT_TYPE> line;
        line.Resize(values->size());
        for (int j = 0; j < values->size(); j++)
        {
            line.Get()[j] = (*values)[j].Get()[i];
        }
        
        // Process
        WDL_TypedBuf<FLOAT_TYPE> minima;
        FindMinima2(line, &minima);
        
        WDL_TypedBuf<FLOAT_TYPE> maxima;
        FindMaxima2(line, &maxima);
        
        WDL_TypedBuf<FLOAT_TYPE> newLine;
        newLine.Resize(line.GetSize());
        BLUtils::FillAllValue(&newLine, undefineValue);
        
        for (int j = 0; j < line.GetSize(); j++)
        {
            FLOAT_TYPE mini = minima.Get()[j];
            FLOAT_TYPE maxi = maxima.Get()[j];
            
            if (mini < 1.0)
                newLine.Get()[j] = 0.0;
            
            if (maxi > 0.0)
                newLine.Get()[j] = 1.0;
        }
        
        bool extendBounds = true;
        BLUtils::FillMissingValues3(&newLine, extendBounds, undefineValue);
        
        // Copy the result line
        for (int j = 0; j < values->size(); j++)
        {
            minMaxMap[j].Get()[i] = newLine.Get()[j];
        }
    }
    
    // Finish
    *values = minMaxMap;
}
template void BLUtils::BuildMinMaxMapHoriz(vector<WDL_TypedBuf<float> > *values);
template void BLUtils::BuildMinMaxMapHoriz(vector<WDL_TypedBuf<double> > *values);

template <typename FLOAT_TYPE>
void
BLUtils::BuildMinMaxMapVert(vector<WDL_TypedBuf<FLOAT_TYPE> > *values)
{
    const FLOAT_TYPE undefineValue = -1.0;
    
    // Init
    vector<WDL_TypedBuf<FLOAT_TYPE> > minMaxMap = *values;
    for (int i = 0; i < minMaxMap.size(); i++)
    {
        BLUtils::FillAllValue(&minMaxMap[i], undefineValue);
    }
    
    // Iterate over lines
    for (int i = 0; i < values->size(); i++)
    {
        const WDL_TypedBuf<FLOAT_TYPE> &line = (*values)[i];
        
        WDL_TypedBuf<FLOAT_TYPE> minima;
        FindMinima2(line, &minima);
        
        WDL_TypedBuf<FLOAT_TYPE> maxima;
        FindMaxima2(line, &maxima);
        
        WDL_TypedBuf<FLOAT_TYPE> newLine;
        newLine.Resize(line.GetSize());
        BLUtils::FillAllValue(&newLine, undefineValue);
        
        for (int j = 0; j < line.GetSize(); j++)
        {
            FLOAT_TYPE mini = minima.Get()[j];
            FLOAT_TYPE maxi = maxima.Get()[j];
            
            if (mini < 1.0)
                newLine.Get()[j] = 0.0;
            
            if (maxi > 0.0)
                newLine.Get()[j] = 1.0;
        }
        
        bool extendBounds = false;
        BLUtils::FillMissingValues3(&newLine, extendBounds, undefineValue);
        
        minMaxMap[i] = newLine;
    }
    
    // Finish
    *values = minMaxMap;
}
template void BLUtils::BuildMinMaxMapVert(vector<WDL_TypedBuf<float> > *values);
template void BLUtils::BuildMinMaxMapVert(vector<WDL_TypedBuf<double> > *values);

template <typename FLOAT_TYPE>
void
BLUtils::Normalize(WDL_TypedBuf<FLOAT_TYPE> *values)
{
    FLOAT_TYPE minimum = BLUtils::ComputeMin(*values);
    FLOAT_TYPE maximum = BLUtils::ComputeMax(*values);
    
#if !USE_SIMD_OPTIM
    for (int i = 0; i < values->GetSize(); i++)
    {
        FLOAT_TYPE val = values->Get()[i];
        
        if (std::fabs(maximum - minimum) > 0.0)
            val = (val - minimum)/(maximum - minimum);
        else
            val = 0.0;
        
        values->Get()[i] = val;
    }
#else
    FLOAT_TYPE diff = std::fabs(maximum - minimum);
    if (diff > 0.0)
    {
        AddValues(values, -minimum);
        
        FLOAT_TYPE coeff = 1.0/diff;
        MultValues(values, coeff);
    }
    else
    {
        FillAllZero(values);
    }
#endif
}
template void BLUtils::Normalize(WDL_TypedBuf<float> *values);
template void BLUtils::Normalize(WDL_TypedBuf<double> *values);

template <typename FLOAT_TYPE>
void
BLUtils::Normalize(FLOAT_TYPE *values, int numValues)
{
    FLOAT_TYPE min_val = INF;
    FLOAT_TYPE max_val = -INF;
    
    int i;
    for (i = 0; i < numValues; i++)
    {
        if (values[i] < min_val)
            min_val = values[i];
        if (values[i] > max_val)
            max_val = values[i];
    }
    
    if (max_val - min_val > BL_EPS)
    {
        for (i = 0; i < numValues; i++)
        {
            values[i] = (values[i] - min_val)/(max_val - min_val);
        }
    }
}
template void BLUtils::Normalize(float *values, int numValues);
template void BLUtils::Normalize(double *values, int numValues);

template <typename FLOAT_TYPE>
void
BLUtils::Normalize(WDL_TypedBuf<FLOAT_TYPE> *values,
                 FLOAT_TYPE minimum, FLOAT_TYPE maximum)
{
#if !USE_SIMD_OPTIM
    for (int i = 0; i < values->GetSize(); i++)
    {
        FLOAT_TYPE val = values->Get()[i];
        
        if (std::fabs(maximum - minimum) > 0.0)
            val = (val - minimum)/(maximum - minimum);
        else
            val = 0.0;
        
        values->Get()[i] = val;
    }
#else
    FLOAT_TYPE diff = std::fabs(maximum - minimum);
    if (diff > 0.0)
    {
        AddValues(values, -minimum);
        
        FLOAT_TYPE coeff = 1.0/diff;
        MultValues(values, coeff);
    }
    else
    {
        FillAllZero(values);
    }
#endif
}
template void BLUtils::Normalize(WDL_TypedBuf<float> *values,
                      float minimum, float maximum);
template void BLUtils::Normalize(WDL_TypedBuf<double> *values,
                      double minimum, double maximum);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::Normalize(FLOAT_TYPE value, FLOAT_TYPE minimum, FLOAT_TYPE maximum)
{
    FLOAT_TYPE result = 0.0;
    if (std::fabs(maximum - minimum) > 0.0)
        result = (value - minimum)/(maximum - minimum);
    
    return result;
}
template float BLUtils::Normalize(float value, float minimum, float maximum);
template double BLUtils::Normalize(double value, double minimum, double maximum);

template <typename FLOAT_TYPE>
void
BLUtils::Normalize(deque<WDL_TypedBuf<FLOAT_TYPE> > *values)
{
    FLOAT_TYPE minValue = INF;
    FLOAT_TYPE maxValue = -INF;
    
    for (int i = 0; i < values->size(); i++)
    {
        for (int j = 0; j < (*values)[i].GetSize(); j++)
        {
            FLOAT_TYPE val = (*values)[i].Get()[j];
            
            if (val < minValue)
                minValue = val;
            
            if (val > maxValue)
                maxValue = val;
        }
    }
    
    for (int i = 0; i < values->size(); i++)
    {
        for (int j = 0; j < (*values)[i].GetSize(); j++)
        {
            FLOAT_TYPE val = (*values)[i].Get()[j];
            
            if (maxValue - minValue > BL_EPS)
                val = (val - minValue)/(maxValue - minValue);
            
            (*values)[i].Get()[j] = val;
        }
    }
}
template void BLUtils::Normalize(deque<WDL_TypedBuf<float> > *values);
template void BLUtils::Normalize(deque<WDL_TypedBuf<double> > *values);

void
BLUtils::NormalizeMagns(WDL_TypedBuf<WDL_FFT_COMPLEX> *values)
{
    for (int i = 0; i < values->GetSize(); i++)
    {
        WDL_FFT_COMPLEX &val = values->Get()[i];
        double magn = COMP_MAGN(val);
        if (magn > 0.0)
        {
            val.re /= magn;
            val.im /= magn;
        }
    }
}

void
BLUtils::NormalizeMagnsF(WDL_TypedBuf<WDL_FFT_COMPLEX> *values)
{
    for (int i = 0; i < values->GetSize(); i++)
    {
        WDL_FFT_COMPLEX &val = values->Get()[i];
        float magn = COMP_MAGNF(val);
        if (magn > 0.0)
        {
            val.re /= magn;
            val.im /= magn;
        }
    }
}

template <typename FLOAT_TYPE>
void
BLUtils::Normalize(WDL_TypedBuf<FLOAT_TYPE> *values, FLOAT_TYPE *minVal, FLOAT_TYPE *maxVal)
{
    *minVal = BLUtils::ComputeMin(*values);
    *maxVal = BLUtils::ComputeMax(*values);
    
    if (std::fabs(*maxVal - *minVal) > BL_EPS)
    {
        for (int i = 0; i < values->GetSize(); i++)
        {
            FLOAT_TYPE val = values->Get()[i];
            
            val = (val - *minVal)/(*maxVal - *minVal);
            
            values->Get()[i] = val;
        }
    }
}
template void BLUtils::Normalize(WDL_TypedBuf<float> *values, float *minVal, float *maxVal);
template void BLUtils::Normalize(WDL_TypedBuf<double> *values, double *minVal, double *maxVal);

template <typename FLOAT_TYPE>
void
BLUtils::DeNormalize(WDL_TypedBuf<FLOAT_TYPE> *values, FLOAT_TYPE minVal, FLOAT_TYPE maxVal)
{
    if (std::fabs(maxVal - minVal) > BL_EPS)
    {
        for (int i = 0; i < values->GetSize(); i++)
        {
            FLOAT_TYPE val = values->Get()[i];
            
            val = val*(maxVal - minVal) + minVal;
            
            values->Get()[i] = val;
        }
    }
}
template void BLUtils::DeNormalize(WDL_TypedBuf<float> *values, float minVal, float maxVal);
template void BLUtils::DeNormalize(WDL_TypedBuf<double> *values, double minVal, double maxVal);

template <typename FLOAT_TYPE>
void
BLUtils::NormalizeFilter(WDL_TypedBuf<FLOAT_TYPE> *values)
{
    FLOAT_TYPE sum = BLUtils::ComputeSum(*values);
    
    if (std::fabs(sum) < BL_EPS)
        return;
    
    FLOAT_TYPE sumInv = 1.0/sum;
    
    for (int i = 0; i < values->GetSize(); i++)
    {
        FLOAT_TYPE val = values->Get()[i];
        
        val *= sumInv;
        
        values->Get()[i] = val;
    }
}
template void BLUtils::NormalizeFilter(WDL_TypedBuf<float> *values);
template void BLUtils::NormalizeFilter(WDL_TypedBuf<double> *values);

// Chroma
template <typename FLOAT_TYPE>
static FLOAT_TYPE
ComputeC0Freq(FLOAT_TYPE aTune)
{
    FLOAT_TYPE AMinus1 = aTune/32.0;
    
    FLOAT_TYPE toneMult = std::pow((FLOAT_TYPE)2.0, (FLOAT_TYPE)(1.0/12.0));
    
    FLOAT_TYPE ASharpMinus1 = AMinus1*toneMult;
    FLOAT_TYPE BMinus1 = ASharpMinus1*toneMult;
    
    FLOAT_TYPE C0 = BMinus1*toneMult;
    
    return C0;
}
template float ComputeC0Freq(float aTune);
template double ComputeC0Freq(double aTune);

template <typename FLOAT_TYPE>
void
BLUtils::BinsToChromaBins(int numBins, WDL_TypedBuf<FLOAT_TYPE> *chromaBins,
                        FLOAT_TYPE sampleRate, FLOAT_TYPE aTune)
{
    chromaBins->Resize(numBins);
    
    // Corresponding to A 440
    //#define C0_TONE 16.35160
    
    FLOAT_TYPE c0Freq = ComputeC0Freq(aTune);
    
    FLOAT_TYPE toneMult = std::pow((FLOAT_TYPE)2.0, (FLOAT_TYPE)(1.0/12.0));
    
    FLOAT_TYPE hzPerBin = sampleRate/(numBins*2);
    
    // Do not take 0Hz!
    if (chromaBins->GetSize() > 0)
        chromaBins->Get()[0] = 0.0;
    
    FLOAT_TYPE c0FreqInv = 1.0/c0Freq; //
    FLOAT_TYPE logToneMultInv = 1.0/std::log(toneMult); //
    FLOAT_TYPE inv12 = 1.0/12.0; //
    for (int i = 1; i < chromaBins->GetSize(); i++)
    {
        FLOAT_TYPE freq = i*hzPerBin;
        
        // See: https://pages.mtu.edu/~suits/NoteFreqCalcs.html
        //FLOAT_TYPE fRatio = freq / c0Freq;
        FLOAT_TYPE fRatio = freq*c0FreqInv;
        //FLOAT_TYPE tone = std::log(fRatio)/std::log(toneMult);
        FLOAT_TYPE tone = std::log(fRatio)*logToneMultInv;
        
        // Shift by one (strange...)
        tone += 1.0;
        
        // Adjust to the axis labels
        tone -= (1.0/12.0);
        
        tone = fmod(tone, (FLOAT_TYPE)12.0);
        
        //FLOAT_TYPE toneNorm = tone/12.0;
        FLOAT_TYPE toneNorm = tone*inv12;
        
        FLOAT_TYPE binNumF = toneNorm*chromaBins->GetSize();
        
        //int binNum = bl_round(binNumF);
        FLOAT_TYPE binNum = binNumF; // Keep float
        
        if ((binNum >= 0) && (binNum < chromaBins->GetSize()))
            chromaBins->Get()[i] = binNum;
    }
}
template void BLUtils::BinsToChromaBins(int numBins, WDL_TypedBuf<float> *chromaBins,
                             float sampleRate, float aTune);
template void BLUtils::BinsToChromaBins(int numBins, WDL_TypedBuf<double> *chromaBins,
                             double sampleRate, double aTune);

template <typename FLOAT_TYPE>
void
BLUtils::ComputeSamplesByFrequency(WDL_TypedBuf<FLOAT_TYPE> *freqSamples,
                                 FLOAT_TYPE sampleRate,
                                 const WDL_TypedBuf<FLOAT_TYPE> &magns,
                                 const WDL_TypedBuf<FLOAT_TYPE> &phases,
                                 int tBinNum)
{
    freqSamples->Resize(magns.GetSize());
 
    FLOAT_TYPE t = ((FLOAT_TYPE)tBinNum*2.0)/sampleRate;
    
    WDL_TypedBuf<FLOAT_TYPE> freqs;
    BLUtils::FftFreqs(&freqs, magns.GetSize(), sampleRate);
    
    for (int i = 0; i < magns.GetSize(); i++)
    {
        FLOAT_TYPE freq = freqs.Get()[i];
        
        FLOAT_TYPE magn = magns.Get()[i];
        FLOAT_TYPE phase = phases.Get()[i];
        
        FLOAT_TYPE samp = magn*std::sin((FLOAT_TYPE)(2.0*M_PI*freq*t + phase));
        
        freqSamples->Get()[i] = samp;
    }
}
template void BLUtils::ComputeSamplesByFrequency(WDL_TypedBuf<float> *freqSamples,
                                      float sampleRate,
                                      const WDL_TypedBuf<float> &magns,
                                      const WDL_TypedBuf<float> &phases,
                                      int tBinNum);
template void BLUtils::ComputeSamplesByFrequency(WDL_TypedBuf<double> *freqSamples,
                                      double sampleRate,
                                      const WDL_TypedBuf<double> &magns,
                                      const WDL_TypedBuf<double> &phases,
                                      int tBinNum);

template <typename FLOAT_TYPE>
void
BLUtils::SeparatePeaks2D(int width, int height,
                       WDL_TypedBuf<FLOAT_TYPE> *ioData,
                       bool keepMaxValue)
{
    WDL_TypedBuf<FLOAT_TYPE> result = *ioData;
    
    if (!keepMaxValue)
        BLUtils::FillAllValue(&result, (FLOAT_TYPE)1.0);
    
    for (int j = 0; j < height; j++)
    {
        for (int i = 0; i < width; i++)
        {
            FLOAT_TYPE currentVal = ioData->Get()[i + j*width];
            
            // Test horizontal
            if ((i > 0) && (i < width - 1))
            {
                FLOAT_TYPE val0 = ioData->Get()[i - 1 + j*width];
                FLOAT_TYPE val1 = ioData->Get()[i + 1 + j*width];
                       
                if (((currentVal < val0) && (currentVal < val1)) ||
                    ((currentVal <= val0) && (currentVal < val1)) ||
                    ((currentVal < val0) && (currentVal <= val1)) ||
                    ((currentVal <= val0) && (currentVal <= val1))) //
                {
                    result.Get()[i + j*width] = 0.0;
                    
                    continue;
                }
            }
            
            // Test vertical
            if ((j > 0) && (j < height - 1))
            {
                FLOAT_TYPE val0 = ioData->Get()[i + (j - 1)*width];
                FLOAT_TYPE val1 = ioData->Get()[i + (j + 1)*width];
                
                if (((currentVal < val0) && (currentVal < val1)) ||
                    ((currentVal <= val0) && (currentVal < val1)) ||
                    ((currentVal < val0) && (currentVal <= val1)) ||
                    ((currentVal <= val0) && (currentVal <= val1))) //
                {
                    result.Get()[i + j*width] = 0.0;
                    
                    continue;
                }
            }
            
            // Test oblic (anti-slash)
            if ((i > 0) && (j > 0) &&
                ((i < width - 1) && (j < height - 1)))
            {
                FLOAT_TYPE val0 = ioData->Get()[(i - 1) + (j - 1)*width];
                FLOAT_TYPE val1 = ioData->Get()[(i + 1) + (j + 1)*width];
                
                if (((currentVal < val0) && (currentVal < val1)) ||
                    ((currentVal <= val0) && (currentVal < val1)) ||
                    ((currentVal < val0) && (currentVal <= val1)) ||
                    ((currentVal <= val0) && (currentVal <= val1))) //
                {
                    result.Get()[i + j*width] = 0.0;
                    
                    continue;
                }
            }
            
            // Test oblic (slash)
            if ((i > 0) && (j > 0) &&
                ((i < width - 1) && (j < height - 1)))
            {
                FLOAT_TYPE val0 = ioData->Get()[(i - 1) + (j + 1)*width];
                FLOAT_TYPE val1 = ioData->Get()[(i + 1) + (j - 1)*width];
                
                if (((currentVal < val0) && (currentVal < val1)) ||
                    ((currentVal <= val0) && (currentVal < val1)) ||
                    ((currentVal < val0) && (currentVal <= val1)) ||
                    ((currentVal <= val0) && (currentVal <= val1))) //
                {
                    result.Get()[i + j*width] = 0.0;
                    
                    continue;
                }
            }
        }
    }
    
    *ioData = result;
}
template void BLUtils::SeparatePeaks2D(int width, int height,
                            WDL_TypedBuf<float> *ioData,
                            bool keepMaxValue);
template void BLUtils::SeparatePeaks2D(int width, int height,
                            WDL_TypedBuf<double> *ioData,
                            bool keepMaxValue);

template <typename FLOAT_TYPE>
void
BLUtils::SeparatePeaks2D2(int width, int height,
                        WDL_TypedBuf<FLOAT_TYPE> *ioData,
                        bool keepMaxValue)
{
#define WIN_SIZE 3
#define HALF_WIN_SIZE 1 //WIN_SIZE*0.5
    
    WDL_TypedBuf<FLOAT_TYPE> result = *ioData;
    
    if (!keepMaxValue)
        BLUtils::FillAllValue(&result, (FLOAT_TYPE)1.0);
    
    for (int j = 0; j < height; j++)
    {
        for (int i = 0; i < width; i++)
        {
            FLOAT_TYPE currentVal = ioData->Get()[i + j*width];
            
            bool minimumFound = false;
            
            // Test horizontal
            for (int i0 = i - HALF_WIN_SIZE; i0 <= i + HALF_WIN_SIZE; i0++)
            {
                if (minimumFound)
                    break;
                
                for (int i1 = i - HALF_WIN_SIZE; i1 <= i + HALF_WIN_SIZE; i1++)
                {
                    int j0 = j - HALF_WIN_SIZE;
                    int j1 = j + HALF_WIN_SIZE;
                    
                    if ((i0 >= 0) && (i0 < width) &&
                        (i1 >= 0) && (i1 < width) &&
                        (j0 >= 0) && (j0 < height) &&
                        (j1 >= 0) && (j1 < height))
                    {
                        FLOAT_TYPE val0 = ioData->Get()[i0 + j0*width];
                        FLOAT_TYPE val1 = ioData->Get()[i1 + j1*width];
                        
                        if (((currentVal < val0) && (currentVal < val1))) // ||
                            //((currentVal <= val0) && (currentVal < val1)) ||
                            //((currentVal < val0) && (currentVal <= val1)))
                        {
                            result.Get()[i + j*width] = 0.0;
                            
                            minimumFound = true;
                            
                            break;
                        }
                    }
                }
            }
            
            // Test vertical
            for (int j0 = j - HALF_WIN_SIZE; j0 <= j + HALF_WIN_SIZE; j0++)
            {
                if (minimumFound)
                    break;
                
                for (int j1 = j - HALF_WIN_SIZE; j1 <= j + HALF_WIN_SIZE; j1++)
                {
                    int i0 = i - HALF_WIN_SIZE;
                    int i1 = i + HALF_WIN_SIZE;
                    
                    if ((i0 >= 0) && (i0 < width) &&
                        (i1 >= 0) && (i1 < width) &&
                        (j0 >= 0) && (j0 < height) &&
                        (j1 >= 0) && (j1 < height))
                    {
                        FLOAT_TYPE val0 = ioData->Get()[i0 + j0*width];
                        FLOAT_TYPE val1 = ioData->Get()[i1 + j1*width];
                        
                        if (((currentVal < val0) && (currentVal < val1))) // ||
                            //((currentVal <= val0) && (currentVal < val1)) ||
                            //((currentVal < val0) && (currentVal <= val1)))
                        {
                            result.Get()[i + j*width] = 0.0;
                            
                            minimumFound = true;
                            
                            break;
                        }
                    }
                }
            }

        }
    }
    
    *ioData = result;
}
template void BLUtils::SeparatePeaks2D2(int width, int height,
                             WDL_TypedBuf<float> *ioData,
                             bool keepMaxValue);
template void BLUtils::SeparatePeaks2D2(int width, int height,
                             WDL_TypedBuf<double> *ioData,
                             bool keepMaxValue);

template <typename FLOAT_TYPE>
void
BLUtils::Reshape(WDL_TypedBuf<FLOAT_TYPE> *ioBuf,
               int inWidth, int inHeight,
               int outWidth, int outHeight)
{
    if (ioBuf->GetSize() != inWidth*inHeight)
        return;
    
    if (ioBuf->GetSize() != outWidth*outHeight)
        return;
    
    WDL_TypedBuf<FLOAT_TYPE> result;
    result.Resize(ioBuf->GetSize());
    
#if 0
    int idx = 0;
    int idy = 0;
    for (int j = 0; j < inHeight; j++)
    {
        for (int i = 0; i < inWidth; i++)
        {
            result.Get()[i + j*inWidth] = ioBuf->Get()[idx + idy*outWidth];
            
            idx++;
            if (idx >= outWidth)
            {
                idx = 0;
                idy++;
            }
        }
    }
#endif
    
#if 1
    if (inWidth > outWidth)
    {
        int idx = 0;
        int idy = 0;
        int stride = 0;
        for (int j = 0; j < outHeight; j++)
        {
            for (int i = 0; i < outWidth; i++)
            {
                result.Get()[i + j*outWidth] = ioBuf->Get()[(idx + stride) + idy*inWidth];
            
                idx++;
                if (idx >= outWidth)
                {
                    idx = 0;
                    idy++;
                }
            
                if (idy >= inHeight)
                {
                    idy = 0;
                    stride += outWidth;
                }
            }
        }
    }
    else // outWidth > inWidth
    {
        int idx = 0;
        int idy = 0;
        int stride = 0;
        for (int j = 0; j < inHeight; j++)
        {
            for (int i = 0; i < inWidth; i++)
            {
                result.Get()[(idx + stride) + idy*outWidth] = ioBuf->Get()[i + j*inWidth];
                
                idx++;
                if (idx >= inWidth)
                {
                    idx = 0;
                    idy++;
                }
                
                if (idy >= outHeight)
                {
                    idy = 0;
                    stride += inWidth;
                }
            }
        }
    }
        
#endif
    
    *ioBuf = result;
}
template void BLUtils::Reshape(WDL_TypedBuf<float> *ioBuf,
                    int inWidth, int inHeight,
                    int outWidth, int outHeight);
template void BLUtils::Reshape(WDL_TypedBuf<double> *ioBuf,
                    int inWidth, int inHeight,
                    int outWidth, int outHeight);

template <typename FLOAT_TYPE>
void
BLUtils::Transpose(WDL_TypedBuf<FLOAT_TYPE> *ioBuf,
                 int width, int height)
{
    if (ioBuf->GetSize() != width*height)
        return;
    
    WDL_TypedBuf<FLOAT_TYPE> result = *ioBuf;
    
    for (int j = 0; j < height; j++)
    {
        for (int i = 0; i < width; i++)
        {
            FLOAT_TYPE val = ioBuf->Get()[i + j*width];
            result.Get()[j + i*height] = val;
        }
    }
    
    *ioBuf = result;
}
template void BLUtils::Transpose(WDL_TypedBuf<float> *ioBuf,
                      int width, int height);
template void BLUtils::Transpose(WDL_TypedBuf<double> *ioBuf,
                      int width, int height);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeVariance(const WDL_TypedBuf<FLOAT_TYPE> &data)
{
    if (data.GetSize() == 0)
        return 0.0;
    
    FLOAT_TYPE avg = ComputeAvg(data);
    
    FLOAT_TYPE variance = 0.0;
    for (int i = 0; i < data.GetSize(); i++)
    {
        FLOAT_TYPE val = data.Get()[i];
            
        variance += (val - avg)*(val - avg);
    }
    
    variance /= data.GetSize();
    
    return variance;
}
template float BLUtils::ComputeVariance(const WDL_TypedBuf<float> &data);
template double BLUtils::ComputeVariance(const WDL_TypedBuf<double> &data);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeVariance(const vector<FLOAT_TYPE> &data)
{
    if (data.size() == 0)
        return 0.0;
    
    FLOAT_TYPE avg = ComputeAvg(data);
    
    FLOAT_TYPE variance = 0.0;
    for (int i = 0; i < data.size(); i++)
    {
        FLOAT_TYPE val = data[i];
        
        variance += (val - avg)*(val - avg);
    }
    
    variance /= data.size();
    
    return variance;
}
template float BLUtils::ComputeVariance(const vector<float> &data);
template double BLUtils::ComputeVariance(const vector<double> &data);

template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::ComputeVariance2(const vector<FLOAT_TYPE> &data)
{
    if (data.size() == 0)
        return 0.0;
    
    if (data.size() == 1)
        return data[0];
    
    FLOAT_TYPE avg = ComputeAvg(data);
    
    FLOAT_TYPE variance = 0.0;
    for (int i = 0; i < data.size(); i++)
    {
        FLOAT_TYPE val = data[i];
        
        // This formula is used in mask predictor comp3
        variance += val*val - avg*avg;
    }
    
    variance /= data.size();
    
    return variance;
}
template float BLUtils::ComputeVariance2(const vector<float> &data);
template double BLUtils::ComputeVariance2(const vector<double> &data);

// Checked! That computes Lagrange interp
template <typename FLOAT_TYPE>
FLOAT_TYPE
BLUtils::LagrangeInterp4(FLOAT_TYPE x,
                       FLOAT_TYPE p0[2], FLOAT_TYPE p1[2],
                       FLOAT_TYPE p2[2], FLOAT_TYPE p3[2])
{
    FLOAT_TYPE pts[4][2] = { { p0[0], p0[1] },
                         { p1[0], p1[1] },
                         { p2[0], p2[1] },
                         { p3[0], p3[1] } };
    
    FLOAT_TYPE l[4] = { 1.0, 1.0, 1.0, 1.0 };
    for (int j = 0; j < 4; j++)
    {
        for (int m = 0; m < 4; m++)
        {
            if (m != j)
            {
                FLOAT_TYPE xxm = x - pts[m][0];
                FLOAT_TYPE xjxm = pts[j][0] - pts[m][0];
                
                //if (std::fabs(xjxm) > BL_EPS)
                //{
                l[j] *= xxm/xjxm;
                //}
            }
        }
    }
    
    FLOAT_TYPE y = 0.0;
    for (int j = 0; j < 4; j++)
    {
        y += pts[j][1]*l[j];
    }
    
    return y;
}
template float BLUtils::LagrangeInterp4(float x,
                             float p0[2], float p1[2],
                             float p2[2], float p3[2]);
template double BLUtils::LagrangeInterp4(double x,
                              double p0[2], double p1[2],
                              double p2[2], double p3[2]);

void
BLUtils::ConvertToGUIFloatType(WDL_TypedBuf<BL_GUI_FLOAT> *dst,
                               const WDL_TypedBuf<float> &src)
{
    if (sizeof(BL_FLOAT) == sizeof(float))
    {
        *((WDL_TypedBuf<float> *)dst) = src;
        
        return;
    }
    
    dst->Resize(src.GetSize());
    for (int i = 0; i < src.GetSize(); i++)
    {
        dst->Get()[i] = src.Get()[i];
    }
}

void
BLUtils::ConvertToGUIFloatType(WDL_TypedBuf<BL_GUI_FLOAT> *dst,
                             const WDL_TypedBuf<double> &src)
{
    if (sizeof(BL_GUI_FLOAT) == sizeof(double))
    {
        *((WDL_TypedBuf<double> *)dst) = src;
        return;
    }
    
    dst->Resize(src.GetSize());
    for (int i = 0; i < src.GetSize(); i++)
    {
        dst->Get()[i] = src.Get()[i];
    }
}

void
BLUtils::ConvertToFloatType(WDL_TypedBuf<BL_FLOAT> *dst,
                            const WDL_TypedBuf<float> &src)
{
    if (sizeof(BL_FLOAT) == sizeof(float))
    {
        *((WDL_TypedBuf<float> *)dst) = src;
        return;
    }
    
    dst->Resize(src.GetSize());
    for (int i = 0; i < src.GetSize(); i++)
    {
        dst->Get()[i] = src.Get()[i];
    }
}

void
BLUtils::ConvertToFloatType(WDL_TypedBuf<BL_FLOAT> *dst,
                            const WDL_TypedBuf<double> &src)
{
    if (sizeof(BL_FLOAT) == sizeof(double))
    {
        *((WDL_TypedBuf<double> *)dst) = src;
        
        return;
    }
                
    dst->Resize(src.GetSize());
    for (int i = 0; i < src.GetSize(); i++)
    {
        dst->Get()[i] = src.Get()[i];
    }
}

                                
void
BLUtils::FixDenormal(WDL_TypedBuf<BL_FLOAT> *data)
{
    for (int i = 0; i < data->GetSize(); i++)
    {
        BL_FLOAT &val = data->Get()[i];
        FIX_FLT_DENORMAL(val);
    }
}

// See: https://stackoverflow.com/questions/8424170/1d-linear-convolution-in-ansi-c-code
// a is Signal
// b is Kernel
// result size is: SignalLen + KernelLen - 1
void
BLUtils::Convolve(const WDL_TypedBuf<BL_FLOAT> &a,
                const WDL_TypedBuf<BL_FLOAT> &b,
                WDL_TypedBuf<BL_FLOAT> *result,
                int convoMode)
{
    // Standard convolution
    if (a.GetSize() != b.GetSize())
        return;
    result->Resize(a.GetSize() + b.GetSize() - 1);
    BLUtils::FillAllZero(result);
    
    for (int i = 0; i < a.GetSize() + b.GetSize() - 1; i++)
    {
        int kmin, kmax, k;
        
        result->Get()[i] = 0.0;
        
        kmin = (i >= b.GetSize() - 1) ? i - (b.GetSize() - 1) : 0;
        kmax = (i < a.GetSize() - 1) ? i : a.GetSize() - 1;
        
        for (k = kmin; k <= kmax; k++)
        {
            result->Get()[i] += a.Get()[k] * b.Get()[i - k];
        }
    }
    
    // Manage the modes
    //
    if (convoMode == CONVO_MODE_FULL)
        // We are ok
        return;
    
    if (convoMode == CONVO_MODE_SAME)
    {
        int maxSize = (a.GetSize() > b.GetSize()) ? a.GetSize() : b.GetSize();
        
        int numCrop = result->GetSize() - maxSize;
        int numCrop0 = numCrop/2;
        int numCrop1 = numCrop0;
        
        // Adjust
        if (numCrop0 + numCrop1 < numCrop)
            numCrop0++;
        
        if (numCrop0 > 0)
        {
            BLUtils::ConsumeLeft(result, numCrop0);
            result->Resize(result->GetSize() - numCrop1);
        }
        
        return;
    }
    
    if (convoMode == CONVO_MODE_VALID)
    {
        int maxSize = (a.GetSize() > b.GetSize()) ? a.GetSize() : b.GetSize();
        int minSize = (a.GetSize() < b.GetSize()) ? a.GetSize() : b.GetSize();
        
        int numCrop2 = (result->GetSize() - (maxSize - minSize + 1))/2;
        
        if (numCrop2 > 0)
        {
            BLUtils::ConsumeLeft(result, numCrop2);
            result->Resize(result->GetSize() - numCrop2);
        }
        
        return;
    }
}

// See: https://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
//
// Returns 1 if the lines intersect, otherwise 0. In addition, if the lines
// intersect the intersection point may be stored in the floats i_x and i_y.
static int
get_line_intersection(BL_FLOAT p0_x, BL_FLOAT p0_y, BL_FLOAT p1_x, BL_FLOAT p1_y,
                      BL_FLOAT p2_x, BL_FLOAT p2_y, BL_FLOAT p3_x, BL_FLOAT p3_y,
                      BL_FLOAT *i_x, BL_FLOAT *i_y)
{
    BL_FLOAT s1_x, s1_y, s2_x, s2_y;
    s1_x = p1_x - p0_x;
    s1_y = p1_y - p0_y;
    
    s2_x = p3_x - p2_x;
    s2_y = p3_y - p2_y;
    
    BL_FLOAT s, t;
    s = (-s1_y * (p0_x - p2_x) + s1_x * (p0_y - p2_y)) / (-s2_x * s1_y + s1_x * s2_y);
    t = ( s2_x * (p0_y - p2_y) - s2_y * (p0_x - p2_x)) / (-s2_x * s1_y + s1_x * s2_y);
    
    if (s >= 0 && s <= 1 && t >= 0 && t <= 1)
    {
        // Collision detected
        if (i_x != NULL)
            *i_x = p0_x + (t * s1_x);
        if (i_y != NULL)
            *i_y = p0_y + (t * s1_y);
        
        return 1;
    }
    
    return 0; // No collision
}

bool
BLUtils::SegSegIntersect(BL_FLOAT seg0[2][2], BL_FLOAT seg1[2][2])
{
    int inter =
        get_line_intersection(seg0[0][0], seg0[0][1], seg0[1][0], seg0[1][1],
                              seg1[0][0], seg1[0][1], seg1[1][0], seg1[1][1],
                              NULL, NULL);
    
    return (bool)inter;
}

void
BLUtils::AddIntermediatePoints(const WDL_TypedBuf<BL_FLOAT> &x,
                             const WDL_TypedBuf<BL_FLOAT> &y,
                             WDL_TypedBuf<BL_FLOAT> *newX,
                             WDL_TypedBuf<BL_FLOAT> *newY)
{
    if (x.GetSize() != y.GetSize())
        return;
    
    if (x.GetSize() < 2)
        return;
    
    //
    newX->Resize(x.GetSize()*2);
    newY->Resize(y.GetSize()*2);
    
    for (int i = 0; i < newX->GetSize() - 1; i++)
    {
        if (i % 2 == 0)
        {
            // Simply copy the value
            newX->Get()[i] = x.Get()[i/2];
            newY->Get()[i] = y.Get()[i/2];
            
            continue;
        }
        
        // Take care of last bound
        if (i/2 + 1 >= x.GetSize())
            break;
        
        // Take the middle
        newX->Get()[i] = (x.Get()[i/2] + x.Get()[i/2 + 1])*0.5;
        newY->Get()[i] = (y.Get()[i/2] + y.Get()[i/2 + 1])*0.5;
    }
    
    // Last value
    newX->Get()[newX->GetSize() - 1] = x.Get()[x.GetSize() - 1];
    newY->Get()[newY->GetSize() - 1] = y.Get()[y.GetSize() - 1];
}

long int
BLUtils::GetTimeMillis()
{
    // Make the demo flag to blink
    struct timeval tp;
    gettimeofday(&tp, NULL);
    long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
    
    return ms;
}

bool
BLUtils::GetFullPlugResourcesPath(const IPluginBase &plug, WDL_String *resPath)
{
#define DUMMY_RES_FILE "dummy.txt"
    
    EResourceLocation resourceFound =
        LocateResource(DUMMY_RES_FILE,
                       "txt",
                       *resPath,
                       plug.GetBundleID(),
                       NULL, //GetWinModuleHandle(),
                       SHARED_RESOURCES_SUBPATH);// defined in plugin config.h
    
    if (resourceFound == EResourceLocation::kNotFound)
    {
        return false;
    }
    
    // Crop "/dummy.txt" from the path.
    if (resPath->GetLength() >= strlen(DUMMY_RES_FILE) + 1)
        resPath->SetLen(resPath->GetLength() - (strlen(DUMMY_RES_FILE) + 1));
    
    return true;
}
