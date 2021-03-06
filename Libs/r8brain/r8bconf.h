//$ nobt
//$ nocpp

/**
 * @file r8bconf.h
 *
 * @brief The "configuration" inclusion file you can modify.
 *
 * This is the "configuration" inclusion file for the "r8brain-free-src"
 * sample rate converter. You may redefine the macros here as you see fit.
 *
 * r8brain-free-src Copyright (c) 2013-2019 Aleksey Vaneev
 * See the "License.txt" file for license.
 */

#ifndef R8BCONF_INCLUDED
#define R8BCONF_INCLUDED

#include <BLTypes.h>

// Niko
#define R8B_FASTTIMING 1 // should have no effect for x2 or x4 oversampling
#define R8B_EXTFFT 0 //1 // double the latency
#define R8B_PFFFT 1 // Limit to 24 bit precision

// Niko
#ifndef R8B_FLOAT
#define R8B_FLOAT double
#endif

#if defined( _WIN32 ) || defined( _WIN64 )
	#define R8B_WIN 1
#elif defined( __APPLE__ )
	#define R8B_MAC 1
#else // defined( __APPLE__ )
	#define R8B_LNX 1 // Assume Linux (Unix) platform by default.
#endif // defined( __APPLE__ )

#if !defined( R8B_FLTLEN )
	/**
	 * This macro defines the default fractional delay filter length. Macro is
	 * used by the r8b::CDSPResampler class.
	 */

	#define R8B_FLTLEN 28
#endif // !defined( R8B_FLTLEN )

#if !defined( R8B_FLTFRACS )
	/**
	 * This macro defines the default number of fractional delay filters that
	 * are sampled by the filter bank. Macro is used by the r8b::CDSPResampler
	 * class. In order to get consistent results when resampling to/from
	 * different sample rates, it is suggested to set this macro to a suitable
	 * prime number.
	 */

	#define R8B_FLTFRACS 1733
#endif // !defined( R8B_FLTFRACS )

#if !defined( R8B_IPP )
	/**
	 * Set the R8B_IPP macro definition to 1 to enable the use of Intel IPP's
	 * fast Fourier transform functions. Also uncomment and correct the IPP
	 * header inclusion macros.
	 *
	 * Do not forget to call the ippInit() function at the start of the
	 * application, before using this library's functions.
	 */

	#define R8B_IPP 0

//	#include <ippcore.h>
//	#include <ipps.h>
#endif // !defined( R8B_IPP )

#if !defined( R8BASSERT )
	/**
	 * Assertion macro used to check for certain run-time conditions. By
	 * default no action is taken if assertion fails.
	 *
	 * @param e Expression to check.
	 */

	#define R8BASSERT( e )
#endif // !defined( R8BASSERT )

#if !defined( R8BCONSOLE )
	/**
	 * Console output macro, used to output various resampler status strings,
	 * including filter design parameters, convolver parameters.
	 *
	 * @param e Expression to send to the console, usually consists of a
	 * standard "printf" format string followed by several parameters
	 * (__VA_ARGS__).
	 */

	#define R8BCONSOLE( ... )
#endif // !defined( R8BCONSOLE )

#if !defined( R8B_BASECLASS )
	/**
	 * Macro defines the name of the class from which all classes that are
	 * designed to be created on heap are derived. The default
	 * r8b::CStdClassAllocator class uses "stdlib" memory allocation
	 * functions.
	 *
	 * The classes that are best placed on stack or as class members are not
	 * derived from any class.
	 */

	#define R8B_BASECLASS :: r8b :: CStdClassAllocator
#endif // !defined( R8B_BASECLASS )

#if !defined( R8B_MEMALLOCCLASS )
	/**
	 * Macro defines the name of the class that implements raw memory
	 * allocation functions, see the r8b::CStdMemAllocator class for details.
	 */

	#define R8B_MEMALLOCCLASS :: r8b :: CStdMemAllocator
#endif // !defined( R8B_MEMALLOCCLASS )

#if !defined( R8B_FILTER_CACHE_MAX )
	/**
	 * This macro specifies the number of filters kept in the cache at most.
	 * The actual number can be higher if many different filters are in use at
	 * the same time.
	 */

	#define R8B_FILTER_CACHE_MAX 96
#endif // !defined( R8B_FILTER_CACHE_MAX )

#if !defined( R8B_FRACBANK_CACHE_MAX )
	/**
	 * This macro specifies the number of whole-number stepping fractional
	 * delay filter banks kept in the cache at most. The actual number can be
	 * higher if many different filter banks are in use at the same time. As
	 * filter banks are usually big objects, it is advisable to keep this
	 * cache size small.
	 */

	#define R8B_FRACBANK_CACHE_MAX 12
#endif // !defined( R8B_FRACBANK_CACHE_MAX )

#if !defined( R8B_FLTTEST )
	/**
	 * This macro, when equal to 1, enables fractional delay filter bank
	 * testing: in this mode the filter bank becomes dynamic member of the
	 * CDSPFracInterpolator object instead of being a global static object.
	 */

	#define R8B_FLTTEST 0
#endif // !defined( R8B_FLTTEST )

#if !defined( R8B_FASTTIMING )
	/**
	 * This macro, when equal to 1, enables fast approach to interpolation
	 * sample timing. This approach improves interpolation performance
	 * (by around 15%) at the expense of a minor sample timing drift which is
	 * on the order of 1e-6 samples per 10 billion output samples. This
	 * setting does not apply to whole-number stepping if it is in use as this
	 * stepping provides zero timing error without performance impact. Also
	 * does not apply to the cases when whole-numbered resampling is in actual
	 * use.
	 */

	#define R8B_FASTTIMING 0
#endif // !defined( R8B_FASTTIMING )

#if !defined( R8B_EXTFFT )
	/**
	 * This macro, when equal to 1, extends length of low-pass filters' FFT
	 * block by a factor of 2 by zero-padding them. This usually improves the
	 * overall time performance of the resampler at the expense of higher
	 * overall latency (initial processing delay). If such delay is not an
	 * issue, setting this macro to 1 is preferrable. This macro can only have
	 * a value of 0 or 1.
	 */

	#define R8B_EXTFFT 0
#endif // !defined( R8B_EXTFFT )

#if !defined( R8B_PFFFT )
	/**
	 * When defined as 1, enables PFFFT routines which are fast, but limited
	 * to 24-bit precision.
	 */

	#define R8B_PFFFT 0
#endif // !defined( R8B_PFFFT )

#if R8B_PFFFT
	#include "pffft.h"
	#define R8B_FLOATFFT 1
#endif // R8B_PFFFT

#if !defined( R8B_FLOATFFT )
	/**
	 * The R8B_FLOATFFT definition enables double-to-float buffer conversion
	 * for FFT operations for algorithms that work with "float" values.
	 */

	#define R8B_FLOATFFT 0
#endif // !defined( R8B_FLOATFFT )

#endif // R8BCONF_INCLUDED
