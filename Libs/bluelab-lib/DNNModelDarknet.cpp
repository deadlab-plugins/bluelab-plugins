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
//  DNNModelDarknet.cpp
//  BL-Rebalance
//
//  Created by applematuer on 5/24/20.
//
//

#include <darknet.h>

extern "C" {
#include "../darknet/src/bl_utils.h"
    // From darknet
#include "../darknet/src/denormal.h"
}

extern "C" {
#include <fmem.h>

#ifdef WIN32
#include <fmemopen-win.h>
#endif
}

#include <BLUtils.h>
#include <BLUtilsPlug.h>
#include <BLDebug.h>

#include <PPMFile.h>

#include <Scale.h>
#include <Rebalance_defs.h>

#include "DNNModelDarknet.h"

// Was just a test to avoid writing the temporary file to disk (failed)
#define USE_FMEMOPEN_WIN 0 //1

//#define NORMALIZE_INPUT 1

#define NUM_STEMS 4

#define IMAGE_WIDTH REBALANCE_NUM_SPECTRO_FREQS
#define IMAGE_HEIGHT REBALANCE_NUM_SPECTRO_COLS

//#define DENORMAL_FLUSH_TO_ZERO 1 //0 //1
// Can be dangerous to keep it at 1, depending on the targt hosts
#define DENORMAL_FLUSH_TO_ZERO 0 

// Use debug loaded data, just for testing?
#define DEBUG_DATA 0 //1

// model trained with normalized data?
#define USE_NORMALIZATION 1

// Output is not normalized at all, it has negative values, and values > 1.
//#define FIX_OUTPUT_NORM 1

// Was a test (interesting, but needs more testing)
//#define OTHER_IS_REST2 0

// Was a test
//#define SET_OTHER_TO_ZERO 0

// For debugging memory
#define DEBUG_NO_MODEL 0 //1

// From bl-darknet/rebalance.c
void
amp_to_db_norm(float *buf, int size)
{
    for (int i = 0; i < size; i++)
    {
        float v = buf[i];
        v = bl_amp_to_db_norm(v);
        buf[i] = v;
    }
}

DNNModelDarknet::DNNModelDarknet()
{
    mNet = NULL;
    
    mDbgThreshold = 0.0;
    for (int i = 0; i < NUM_STEM_SOURCES; i++)
        mMaskScales[i] = 1.0;

    mScale = new Scale();

#if DENORMAL_FLUSH_TO_ZERO
    denormal_flushtozero();
#endif

    _darknet_predict_only_flag = 1;
}

DNNModelDarknet::~DNNModelDarknet()
{
#if !DEBUG_NO_MODEL
    if (mNet != NULL)
        free_network(mNet);
#endif
    
    delete mScale;

#if DENORMAL_FLUSH_TO_ZERO
    denormal_reset();
#endif
}

bool
DNNModelDarknet::Load(const char *modelFileName,
                      const char *resourcePath)
{
#if DEBUG_NO_MODEL
    return true;
#endif
    
#ifdef WIN32
    return false;
#endif
    
    // Test fmem on Mac
    //bool res = LoadWinTest(modelFileName, resourcePath);
    //return res;
    
    char cfgFullFileName[2048];
    sprintf(cfgFullFileName, "%s/%s.cfg", resourcePath, modelFileName);
    
    char weightsFileFullFileName[2048];
    sprintf(weightsFileFullFileName, "%s/%s.weights", resourcePath, modelFileName);
    
    mNet = load_network(cfgFullFileName, weightsFileFullFileName, 0);
    set_batch_network(mNet, 1);
    
    return true;
}

// For WIN32
bool
DNNModelDarknet::LoadWin(const char *modelRcName, const char *weightsRcName)
{
#if DEBUG_NO_MODEL
    return true;
#endif
    
#ifdef WIN32

#if 0 // iPlug1
    void *modelRcBuf;
	long modelRcSize;
	bool loaded = ((IGraphicsWin *)pGraphics)->LoadWindowsResource(modelRcId,
                                                                   "RCDATA",
                                                                   &modelRcBuf,
                                                                   &modelRcSize);
	if (!loaded)
		return false;
    
    void *weightsRcBuf;
	long weightsRcSize;
	loaded = ((IGraphicsWin *)pGraphics)->LoadWindowsResource(weightsRcId,
                                                              "RCDATA",
                                                              &weightsRcBuf,
                                                              &weightsRcSize);
    if (!loaded)
		return false;
#endif

#if 0 //1 // iPlug2, with IGraphics
    WDL_TypedBuf<uint8_t> modelRcBuf = pGraphics->LoadResource(modelRcName, "CFG");
    long modelRcSize = modelRcBuf.GetSize();
    if (modelRcSize == 0)
        return false;
    
    WDL_TypedBuf<uint8_t> weightsRcBuf =
        pGraphics->LoadResource(weightsRcName, "WEIGHTS");
    long weightsRcSize = weightsRcBuf.GetSize();
    if (weightsRcSize == 0)
        return false;
#endif

#if 1 // iPlug2, without IGraphics
    WDL_TypedBuf<uint8_t> modelRcBuf =
        BLUtilsPlug::LoadWinResource(modelRcName, "CFG");
    long modelRcSize = modelRcBuf.GetSize();
    if (modelRcSize == 0)
        return false;

    WDL_TypedBuf<uint8_t> weightsRcBuf =
        BLUtilsPlug::LoadWinResource(weightsRcName, "WEIGHTS");
    long weightsRcSize = weightsRcBuf.GetSize();
    if (weightsRcSize == 0)
        return false;
#endif

    // Model
#if !USE_FMEMOPEN_WIN
    fmem fmem0;
    fmem_init(&fmem0);
    FILE *file0 = fmem_open(&fmem0, "w+");
    //fmem_mem(&fmem0, &modelRcBuf.Get(), modelRcSize);
    fwrite(modelRcBuf.Get(), 1, modelRcSize, file0);
    fflush(file0);
    fseek(file0, 0L, SEEK_SET);

#if FMEM_FORCE_FLUSH_HACK_WIN32
#ifdef WIN32
    // Force to flush. On Win32 (not x64), the file is badly flushed otherwise.
    fclose(file0);
    file0 = fopen(fmem0.win_temp_file, "r");
#endif
#endif

#else
    FILE* file0 = fmemopen_win(modelRcBuf.Get(), modelRcSize, "w+");
#endif

    // Weights
#if !USE_FMEMOPEN_WIN
    fmem fmem1;
    fmem_init(&fmem1);
    FILE *file1 = fmem_open(&fmem1, "wb+");
    //fmem_mem(&fmem1, &weightsRcBuf, &weightsRcSize);
    fwrite(weightsRcBuf.Get(), 1, weightsRcSize, file1);
    fflush(file1);
    fseek(file1, 0L, SEEK_SET);

#if FMEM_FORCE_FLUSH_HACK_WIN32
#ifdef WIN32
    // Force to flush. On Win32 (not x64), the file is badly flushed otherwise.
    fclose(file1);
    file1 = fopen(fmem1.win_temp_file, "rb");
#endif
#endif

#else
    FILE* file1 = fmemopen_win(weightsRcBuf.Get(), weightsRcSize, "wb+");
#endif

    // Load network
    mNet = load_network_file(file0, file1, 0);
    set_batch_network(mNet, 1);
    
    // Model
    fclose(file0);
#if !USE_FMEMOPEN_WIN
    fmem_term(&fmem0);
#endif

    // Weights
    fclose(file1);
#if !USE_FMEMOPEN_WIN
    fmem_term(&fmem1);
#endif

	return true;
#endif

    return false;
}

void
DNNModelDarknet::SetMaskScale(int maskNum, BL_FLOAT scale)
{
    if (maskNum >= NUM_STEM_SOURCES)
        return;
    
    mMaskScales[maskNum] = scale;
}

void
DNNModelDarknet::Predict(const WDL_TypedBuf<BL_FLOAT> &input,
                         vector<WDL_TypedBuf<BL_FLOAT> > *masks)
{
#if DEBUG_NO_MODEL
    masks->resize(NUM_STEMS);
    for (int i = 0; i < NUM_STEMS; i++)
    {
        (*masks)[i].Resize(input.GetSize());
        BLUtils::FillAllValue(&(*masks)[i], 0.25);
    }
    
    return;
#endif
    
    WDL_TypedBuf<BL_FLOAT> &input0 = mTmpBuf0;
    input0 = input;
    
    WDL_TypedBuf<float> &X = mTmpBuf1;
    X.Resize(input0.GetSize()*NUM_STEMS);
    for (int i = 0; i < X.GetSize(); i++)
    {
        BL_FLOAT val = input0.Get()[i % input0.GetSize()];
        
        X.Get()[i] = val;
    }

#if DEBUG_DATA
    // DEBUG: load .bin example
#define DBG_INPUT_IMAGE_FNAME "/home/niko/Documents/Dev/plugs-dev/bluelab/BL-Plugins/BL-Rebalance/DNNPack/training/mix" //mix-sum"
    
    // Load
    WDL_TypedBuf<float> dbgMix;
    dbgMix.Resize(IMAGE_WIDTH*IMAGE_HEIGHT);
    float *dbgMixData = dbgMix.Get();
    bl_load_image_bin(IMAGE_WIDTH, IMAGE_HEIGHT, 1,
                      dbgMixData, DBG_INPUT_IMAGE_FNAME);

    for (int i = 0; i < X.GetSize(); i++)
    {
        BL_FLOAT val = dbgMix.Get()[i % dbgMix.GetSize()];
        
        X.Get()[i] = val;
    }
#endif   
    
    // Process
    //bl_normalize(X.Get(), X.GetSize());

#if USE_NORMALIZATION
    // New normalization (see Janson 2017)
    float norm = bl_max(X.Get(), X.GetSize());
    if (norm > 0.0)
        bl_mult2(X.Get(), 1.0f/norm, X.GetSize());
#endif
    
    // NOTE: must check this well, and if we provide function input in db or not
    // Here, this is good for PROCESS_SIGNAL_DB=0
    amp_to_db_norm(X.Get(), X.GetSize());

    // Should not be necessary: amp_to_db_norm already gives normalized result...
    //bl_normalize(X.Get(), X.GetSize()); 
    
    //#if DENORMAL_FLUSH_TO_ZERO
    //denormal_flushtozero();
    //#endif
    
     // ?
    srand(2222222);
    // Prediction
    float *pred = network_predict(mNet, X.Get());

    //#if DENORMAL_FLUSH_TO_ZERO
    //denormal_reset();
    //#endif
    
#if DEBUG_DATA
#define SAVE_IMG_FNAME "/home/niko/Documents/BlueLabAudio-Debug/X"
    //bl_save_image(IMAGE_WIDTH, IMAGE_HEIGHT, 1, X.Get(), SAVE_IMG_FNAME, 1);
    bl_save_image_mc(IMAGE_WIDTH, IMAGE_HEIGHT, 4, X.Get(), SAVE_IMG_FNAME);
    bl_save_image_txt(IMAGE_WIDTH, IMAGE_HEIGHT, 1, X.Get(), SAVE_IMG_FNAME);
    
#define SAVE_PRED_FNAME "/home/niko/Documents/BlueLabAudio-Debug/pred"
    bl_save_image_mc(IMAGE_WIDTH, IMAGE_HEIGHT, 4, pred, SAVE_PRED_FNAME);
    bl_save_image_txt(IMAGE_WIDTH, IMAGE_HEIGHT, 4, pred, SAVE_PRED_FNAME);
 
    exit(0);
#endif
    
    masks->resize(NUM_STEMS);
    for (int i = 0; i < NUM_STEMS; i++)
    {
        (*masks)[i].Resize(input0.GetSize());
    }
    
    for (int i = 0; i < X.GetSize(); i++)
    {
        int maskIndex = i/input0.GetSize();
        
        float val = pred[i];
        
        (*masks)[maskIndex].Get()[i % input0.GetSize()] = val;
    }

    for (int i = 0; i < NUM_STEMS; i++)
    {
        BLUtils::ClipMin(&(*masks)[i], (BL_FLOAT)0.0);
    }
}

#if 0
void
DNNModelDarknet::SetDbgThreshold(BL_FLOAT thrs)
{
    mDbgThreshold = thrs;
}
#endif

// To test on Mac, the mechanisme using fmem
bool
DNNModelDarknet::LoadWinTest(const char *modelFileName, const char *resourcePath)
{
#if DEBUG_NO_MODEL
    return true;
#endif
    
    // NOTE: with the fmem implementation "funopen", there is a memory problem
    // in darknet fgetl().
    //Detected with valgrind during plugin scan.
    // With the implementation "tmpfile", there is no memory problem.
    
    //fprintf(stderr, "modelFileName: %s\n", modelFileName);
    
    char cfgFullFileName[2048];
    sprintf(cfgFullFileName, "%s/%s.cfg", resourcePath, modelFileName);
    
    char weightsFileFullFileName[2048];
    sprintf(weightsFileFullFileName, "%s/%s.weights", resourcePath, modelFileName);
    
    // Read the two files in memory
    
    // File 0
    FILE *f0 = fopen(cfgFullFileName, "rb");
    fseek(f0, 0, SEEK_END);
    long fsize0 = ftell(f0);
    fseek(f0, 0, SEEK_SET);
    
    void *buf0 = malloc(fsize0 + 1);
    fread(buf0, 1, fsize0, f0);
    fclose(f0);
    
    // File 1
    FILE *f1 = fopen(weightsFileFullFileName, "rb");
    fseek(f1, 0, SEEK_END);
    long fsize1 = ftell(f1);
    fseek(f1, 0, SEEK_SET);
    
    void *buf1 = malloc(fsize1 + 1);
    fread(buf1, 1, fsize1, f1);
    fclose(f1);
    
    // Read with fmem
    //
    
    // Model
    fmem fmem0;
    fmem_init(&fmem0);
    //FILE *file0 = fmem_open(&fmem0, "r");
    FILE *file0 = fmem_open(&fmem0, "w+");
    fwrite(buf0, 1, fsize0, file0);
    fflush(file0);
    fseek(file0, 0L, SEEK_SET);
    
    // Weights
    fmem fmem1;
    fmem_init(&fmem1);
    //FILE *file1 = fmem_open(&fmem1, "rb");
    FILE *file1 = fmem_open(&fmem1, "wb+");
    fwrite(buf1, 1, fsize1, file1);
    fflush(file1);
    fseek(file1, 0L, SEEK_SET);
    
    // Load network
    mNet = load_network_file(file0, file1, 0);
    set_batch_network(mNet, 1);
    
    // Model
    fclose(file0);
    fmem_term(&fmem0);
    
    // Weights
    fclose(file1);
    fmem_term(&fmem1);
    
    free(buf0);
    free(buf1);
    
    return true;
}
