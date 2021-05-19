//
//  BLScanDisplay.cpp
//  UST
//
//  Created by applematuer on 7/30/19.
//
//

#ifdef IGRAPHICS_NANOVG

#include <GraphControl12.h>

#include <BLUtils.h>

#include "BLScanDisplay.h"

// We display 2 seconds of samples
#define NUM_SECONDS 4.0 //2.0
#define GRAPH_NUM_POINTS 256

// Curves
#define BLUE_COLOR_SCHEME 1

#define CURVE_FILL_ALPHA 0.1

// On this version, scrolling display is not debugged well (it jitters)
// So, let's bet on sweep update !
#define SWEEP_UPDATE 1

// Use a kind of gamma curve for the zoom (so 1 remains 1, but small values are incerased)
// Smart zoom.
#define GAMMA_ZOOM 0 //1


BLScanDisplay::BLScanDisplay(BL_GUI_FLOAT sampleRate)
{
    mSampleRate = sampleRate;
    
    mZoom = 1.0;
    
#if SWEEP_UPDATE
    mSweepPos = 0;
    mSweepPosClip = 0;
#endif
    
    mGraph = NULL;

    // Curves
    mAxisCurve = NULL;
    mWaveformUpCurve = NULL;
    mWaveformDownCurve = NULL;
    mWaveformClipUpCurve = NULL;
    mWaveformClipDownCurve = NULL;
    mSweepBarCurve = NULL;

    CreateCurves();
    
    Reset(sampleRate);

    mIsEnabled = false;
}

BLScanDisplay::~BLScanDisplay()
{
  delete mAxisCurve;
  delete mWaveformUpCurve;
  delete mWaveformDownCurve;
  delete mWaveformClipUpCurve;
  delete mWaveformClipDownCurve;
  delete mSweepBarCurve;
}

void
BLScanDisplay::SetGraph(GraphControl12 *graph)
{
    mGraph = graph;
    
    if (mGraph != NULL)
    {
        mGraph->SetBounds(0.0, 0.0, 1.0, 1.0);
        mGraph->SetClearColor(0, 0, 0, 255);
                
#if BLUE_COLOR_SCHEME
        mAxisCurve->SetColor(170, 202, 209);
#endif
	
        mAxisCurve->SetAlpha(1.0);
        mAxisCurve->SetLineWidth(2.0);
        mAxisCurve->SetFill(false);
        mAxisCurve->SetFillAlpha(CURVE_FILL_ALPHA);
        mAxisCurve->SetSingleValueH(true);
        mAxisCurve->SetYScale(Scale::LINEAR, -2.0, 2.0);
        
        // Must set view size before value...
        int width;
        int height;
        mGraph->GetSize(&width, &height);
        mAxisCurve->SetViewSize(width, height);
        
        mAxisCurve->SetSingleValueH((BL_GUI_FLOAT)0.0);
        
        // Waveform curves
        //
        
#define FILL_ORIGIN_Y_OFFSET 0.001
        
        // Waveform up        
#if BLUE_COLOR_SCHEME 
        // Orange
        mWaveformUpCurve->SetColor(234, 101, 0);
        mWaveformDownCurve->SetColor(234, 101, 0);
        
        mWaveformClipUpCurve->SetColor(113, 130, 182);
        mWaveformClipDownCurve->SetColor(113, 130, 182);
#endif
        
        mWaveformUpCurve->SetAlpha(1.0);
        mWaveformUpCurve->SetLineWidth(-1.0); // Disable draw line over fill
        mWaveformUpCurve->SetFill(true, 0.5 - FILL_ORIGIN_Y_OFFSET);
        mWaveformUpCurve->SetFillAlpha(1.0);
        mWaveformUpCurve->SetYScale(Scale::LINEAR, -2.0, 2.0);
        
        // Waveform down
        
        mWaveformDownCurve->SetAlpha(1.0);
        mWaveformDownCurve->SetLineWidth(-1.0);
        mWaveformDownCurve->SetFillAlpha(1.0);
        mWaveformDownCurve->SetFill(true, 0.5 + FILL_ORIGIN_Y_OFFSET);
        mWaveformDownCurve->SetYScale(Scale::LINEAR, -2.0, 2.0);
        
        // Waveform clip up
        mWaveformClipUpCurve->SetAlpha(1.0);
        mWaveformClipUpCurve->SetLineWidth(-1.0);
        mWaveformClipUpCurve->SetFillAlpha(1.0);
        mWaveformClipUpCurve->SetFill(true, 0.5 - FILL_ORIGIN_Y_OFFSET);
        mWaveformClipUpCurve->SetYScale(Scale::LINEAR, -2.0, 2.0);
        
        // Waveform clip down
        mWaveformClipDownCurve->SetAlpha(1.0);
        mWaveformClipDownCurve->SetLineWidth(-1.0);
        mWaveformClipDownCurve->SetFillAlpha(1.0);
        mWaveformClipDownCurve->SetFill(true, 0.5 + FILL_ORIGIN_Y_OFFSET);
        mWaveformClipDownCurve->SetYScale(Scale::LINEAR, -2.0, 2.0);
                        
        // Sweep bar
        mSweepBarCurve->SetColor(255, 255, 255);
        mSweepBarCurve->SetAlpha(1.0);
        mSweepBarCurve->SetLineWidth(1.0);
        mSweepBarCurve->SetSingleValueV(true);
        
        // Add curved
        mGraph->AddCurve(mAxisCurve);
        mGraph->AddCurve(mWaveformUpCurve);
        mGraph->AddCurve(mWaveformDownCurve);
        mGraph->AddCurve(mWaveformClipUpCurve);
        mGraph->AddCurve(mWaveformClipDownCurve);
        mGraph->AddCurve(mSweepBarCurve);
    }
}

void
BLScanDisplay::Reset(BL_GUI_FLOAT sampleRate)
{
    mSampleRate = sampleRate;
    
    WDL_TypedBuf<BL_GUI_FLOAT> zeros;
    BLUtils::ResizeFillZeros(&zeros, GRAPH_NUM_POINTS);
    
    // To bufferize
    mCurrentSamples.Resize(0);
    mCurrentClippedSamples.Resize(0);
    
    // To display
    mCurrentDecimValuesUp = zeros;
    mCurrentDecimValuesDown = zeros;
    
    mCurrentDecimValuesUpClip = zeros;
    mCurrentDecimValuesDownClip = zeros;
    //
    
#if SWEEP_UPDATE
    mSweepPos = 0;
    mSweepPosClip = 0;
#endif

    if (mGraph != NULL)
    {        
        mWaveformUpCurve->SetValues5(mCurrentDecimValuesUp);
        mWaveformDownCurve->SetValues5(mCurrentDecimValuesDown);
        
        mWaveformClipUpCurve->SetValues5(mCurrentDecimValuesUpClip);
        mWaveformClipDownCurve->SetValues5(mCurrentDecimValuesDownClip);
    }
}

void
BLScanDisplay::AddSamples(const WDL_TypedBuf<BL_FLOAT> &samplesIn)
{
    if (!mIsEnabled)
        return;
    
    WDL_TypedBuf<BL_GUI_FLOAT> samples;
    BLUtils::ConvertToGUIFloatType(&samples, samplesIn);
    
    mCurrentSamples.Add(samples.Get(), samples.GetSize());
    
    int numSamples = GetNumSamples();
    long oneLineSizeSamples = numSamples/GRAPH_NUM_POINTS;
    
    while(mCurrentSamples.GetSize() >= oneLineSizeSamples)
    {
        // Buffer management
        WDL_TypedBuf<BL_GUI_FLOAT> bufSamples;
        bufSamples.Add(mCurrentSamples.Get(), oneLineSizeSamples);
        BLUtils::ConsumeLeft(&mCurrentSamples, oneLineSizeSamples);
    
        // Find min and max (equaivalent to decimation to get a single value)
        BL_GUI_FLOAT decimLineMin;
        BL_GUI_FLOAT decimLineMax;
        DecimateSamplesOneLine(bufSamples, &decimLineMin, &decimLineMax);

        // Cut
        if (decimLineMin > 0.0)
            decimLineMin = 0.0;
        
        if (decimLineMax < 0.0)
            decimLineMax = 0.0;
    
        // For zoom
#if !SWEEP_UPDATE
        mCurrentDecimValuesUp.Add(&decimLineMax, 1);
        BLUtils::ConsumeLeft(&mCurrentDecimValuesUp, 1);
        
        mCurrentDecimValuesDown.Add(&decimLineMin, 1);
        BLUtils::ConsumeLeft(&mCurrentDecimValuesDown, 1);
#else
        mCurrentDecimValuesUp.Get()[mSweepPos] = decimLineMax;;
        mCurrentDecimValuesDown.Get()[mSweepPos] = decimLineMin;
        
        mSweepPos = (mSweepPos + 1) % GRAPH_NUM_POINTS;
#endif
        
        AddSamplesZoom();
        
        UpdateSweepBar();
    }
}

void
BLScanDisplay::AddClippedSamples(const WDL_TypedBuf<BL_FLOAT> &samplesIn)
{
    if (!mIsEnabled)
        return;
    
    WDL_TypedBuf<BL_GUI_FLOAT> samples;
    BLUtils::ConvertToGUIFloatType(&samples, samplesIn);
    
    mCurrentClippedSamples.Add(samples.Get(), samples.GetSize());
    
    int numSamples = GetNumSamples();
    long oneLineSizeSamples = numSamples/GRAPH_NUM_POINTS;
    
    while(mCurrentClippedSamples.GetSize() >= oneLineSizeSamples)
    {
        // Buffer management
        WDL_TypedBuf<BL_GUI_FLOAT> bufSamples;
        bufSamples.Add(mCurrentClippedSamples.Get(), oneLineSizeSamples);
        BLUtils::ConsumeLeft(&mCurrentClippedSamples, oneLineSizeSamples);
        
        // Find min and max (equaivalent to decimation to get a single value)
        BL_GUI_FLOAT decimLineMin;
        BL_GUI_FLOAT decimLineMax;
        DecimateSamplesOneLine(bufSamples, &decimLineMin, &decimLineMax);
        
        // Cut
        if (decimLineMin > 0.0)
            decimLineMin = 0.0;
        
        if (decimLineMax < 0.0)
            decimLineMax = 0.0;
        
        // For zoom
#if !SWEEP_UPDATE
        mCurrentDecimValuesUpClip.Add(&decimLineMax, 1);
        BLUtils::ConsumeLeft(&mCurrentDecimValuesUpClip, 1);
        
        mCurrentDecimValuesDownClip.Add(&decimLineMin, 1);
        BLUtils::ConsumeLeft(&mCurrentDecimValuesDownClip, 1);
#else
        mCurrentDecimValuesUpClip.Get()[mSweepPosClip] = decimLineMax;
        mCurrentDecimValuesDownClip.Get()[mSweepPosClip] = decimLineMin;
        
        mSweepPosClip = (mSweepPosClip + 1) % GRAPH_NUM_POINTS;
#endif
        
        //
        AddSamplesZoomClip();
    }
}

void
BLScanDisplay::SetDirty()
{
    if (mGraph != NULL)
        mGraph->SetDataChanged();
}

void
BLScanDisplay::SetEnabled(bool flag)
{
#if 0
    if (flag == mIsEnabled)
        return;
    
    if (!flag)
    {
        ResetSweepBar();
    }
#endif
    
    mIsEnabled = flag;
}

void
BLScanDisplay::SetZoom(BL_GUI_FLOAT zoom)
{
    mZoom = zoom;
    
#if GAMMA_ZOOM
    // Smaller zoom value if we use param shape
    mZoom = 1.0 + (mZoom - 1.0)*0.5;
#endif
    
    AddSamplesZoom();
    AddSamplesZoomClip();
    
    if (mGraph != NULL)
        mGraph->SetDataChanged();
}

void
BLScanDisplay::ResetSweepBar()
{
    // Rewind sweep line
    if (mSweepBarCurve != NULL)
        mSweepBarCurve->SetSingleValueV((BL_FLOAT)0.0);
    
    if (mGraph != NULL)
    {
        mGraph->SetDataChanged();
    }
}

void
BLScanDisplay::AddSamplesZoom()
{
    // Graph
    if (mGraph != NULL)
    {
        WDL_TypedBuf<BL_GUI_FLOAT> decimValuesUp = mCurrentDecimValuesUp;
        WDL_TypedBuf<BL_GUI_FLOAT> decimValuesDown = mCurrentDecimValuesDown;
        
#if !GAMMA_ZOOM
        BLUtils::MultValues(&decimValuesUp, mZoom);
        BLUtils::MultValues(&decimValuesDown, mZoom);
#else
        BLUtils::ApplyParamShapeWaveform(&decimValuesUp, mZoom);
        BLUtils::ApplyParamShapeWaveform(&decimValuesDown, mZoom);
#endif
        
        mWaveformUpCurve->SetValues5(decimValuesUp);
        mWaveformDownCurve->SetValues5(decimValuesDown);
    }
}

void
BLScanDisplay::AddSamplesZoomClip()
{
    if (mGraph != NULL)
    {
        WDL_TypedBuf<BL_GUI_FLOAT> decimValuesUp = mCurrentDecimValuesUpClip;
        WDL_TypedBuf<BL_GUI_FLOAT> decimValuesDown = mCurrentDecimValuesDownClip;
        
#if !GAMMA_ZOOM
        BLUtils::MultValues(&decimValuesUp, mZoom);
        BLUtils::MultValues(&decimValuesDown, mZoom);
#else
        BLUtils::ApplyParamShapeWaveform(&decimValuesUp, mZoom);
        BLUtils::ApplyParamShapeWaveform(&decimValuesDown, mZoom);
#endif
        
        mWaveformClipUpCurve->SetValues5(decimValuesUp);
        mWaveformClipDownCurve->SetValues5(decimValuesDown);
    }
}

long
BLScanDisplay::GetNumSamples()
{
    long numSamples = mSampleRate*NUM_SECONDS;
    
    return numSamples;
}

// Find min and max (equaivalent to decimation to get a single value)
void
BLScanDisplay::DecimateSamplesOneLine(const WDL_TypedBuf<BL_GUI_FLOAT> &bufSamples,
                                      BL_GUI_FLOAT *decimLineMin,
                                      BL_GUI_FLOAT *decimLineMax)
{
    BL_GUI_FLOAT minVal = BLUtils::ComputeMin(bufSamples);
    BL_GUI_FLOAT maxVal = BLUtils::ComputeMax(bufSamples);
    
    *decimLineMin = minVal;
    *decimLineMax = maxVal;
}

void
BLScanDisplay::UpdateSweepBar()
{
    if (!mIsEnabled)
        return;
    
    if (mGraph != NULL)
    {
        BL_GUI_FLOAT pos = ((BL_GUI_FLOAT)mSweepPos)/GRAPH_NUM_POINTS;
        mSweepBarCurve->SetSingleValueV(pos);
    }
}

void
BLScanDisplay::CreateCurves()
{
  mAxisCurve = new GraphCurve5(GRAPH_NUM_POINTS);
  mWaveformUpCurve = new GraphCurve5(GRAPH_NUM_POINTS);
  mWaveformDownCurve = new GraphCurve5(GRAPH_NUM_POINTS);
  mWaveformClipUpCurve = new GraphCurve5(GRAPH_NUM_POINTS);
  mWaveformClipDownCurve = new GraphCurve5(GRAPH_NUM_POINTS);
  mSweepBarCurve = new GraphCurve5(GRAPH_NUM_POINTS);
}

#endif // IGRAPHICS_NANOVG
