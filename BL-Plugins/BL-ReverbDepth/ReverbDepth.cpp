#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <FftProcessObj16.h>

#include <GUIHelper12.h>
#include <GraphControl12.h>
#include <SecureRestarter.h>

#include <GraphControl12.h>
#include <SpectrogramDisplay2.h>

#include <BLUtils.h>
#include <BLUtilsPlug.h>

#include <BLDebug.h>
#include <BlaTimer.h>

#include <USTDepthReverbTest.h>

#include <MultiViewer.h>
#include <BLReverbViewer.h>

#include <MultiViewer2.h>

#include <MouseCustomControl.h>

#include "config.h"
#include "bl_config.h"

#include "ReverbDepth.h"

#include "IPlug_include_in_plug_src.h"


// 1 second -> 20 seconds
#define VIEWER_DEFAULT_DURATION 1.0
#define VIEWER_MIN_DURATION     0.1
#define VIEWER_MAX_DURATION     20.0

#define MOUSE_ZOOM_COEFF 0.01 //0.1 //1.0

// Must be the same buffer size as in SamplesToSpectrogram
#define BUFFER_SIZE 2048

#define UPDATE_VIEW_ON_PARAM_CHANGE 1


//
class ReverbDepthCustomMouseControl //: public MouseCustomControl
    : public GraphCustomControl
{
public:
    ReverbDepthCustomMouseControl(ReverbDepth *plug);
  
    virtual ~ReverbDepthCustomMouseControl() {}
  
    virtual void OnMouseUp(float x, float y, const IMouseMod &pMod) override;
  
protected:
    ReverbDepth *mPlug;
};

ReverbDepthCustomMouseControl::ReverbDepthCustomMouseControl(ReverbDepth *plug)
{
    mPlug = plug;
}

void
ReverbDepthCustomMouseControl::OnMouseUp(float x, float y, const IMouseMod &pMod)
{
    mPlug->OnMouseUp();
}

class ReverbDepthCustomControl : public GraphCustomControl
{
public:
    ReverbDepthCustomControl(ReverbDepth *plug) { mPlug = plug; };
  
    virtual ~ReverbDepthCustomControl() {}
  
    virtual void OnMouseDrag(float x, float y, float dX, float dY,
                             const IMouseMod &pMod) override;
  
protected:
    ReverbDepth *mPlug;
};

void
ReverbDepthCustomControl::OnMouseDrag(float x, float y, float dX, float dY,
                                      const IMouseMod &pMod)
{
    //int maxDelta = (abs(dX) > abs(dY)) ? dX : dY;
    int maxDelta = dY;
  
    mPlug->UpdateTimeZoom(maxDelta);
}

//

enum EParams
{
    kGraph = 0,

    kWet,
    kDry,
  
    kRoomSize,
    kRevWidth,
    kDamping,
    
    kUseFilter,
    
    kUseEarly,
    kEarlyRoomSize,
    kEarlyIntermicDist,
    kEarlyNormDepth,
  
    kUseReverbTail,
    
    kEarlyOrder,
    kEarlyReflectCoeff,
    
    kNumParams
};

const int kNumPresets = 1;

enum ELayout
{
    kWidth = PLUG_WIDTH,
    kHeight = PLUG_HEIGHT,
    
    kGraphX = 0,
    kGraphY = 0,
    
    kUseReverbTailX = 40,
    kUseReverbTailY = 400,
    
    kWetX = 140,
    kWetY = 400,
    kWetFrames = 180,
  
    kDryX = 240,
    kDryY = 400,
    kDryFrames = 180,
  
    kRoomSizeX = 340,
    kRoomSizeY = 400,
    kRoomSizeFrames = 180,
    
    kRevWidthX = 440,
    kRevWidthY = 400,
    kRevWidthFrames = 180,
    
    kDampingX = 540,
    kDampingY = 400,
    kDampingFrames = 180,
    
    kUseFilterX = 640,
    kUseFilterY = 400,
  
    //
    kUseEarlyX = 40,
    kUseEarlyY = 500,
  
    kEarlyRoomSizeX = 140,
    kEarlyRoomSizeY = 500,
    kEarlyRoomSizeFrames = 180,
    
    kEarlyIntermicDistX = 240,
    kEarlyIntermicDistY = 500,
    kEarlyIntermicDistFrames = 180,
    
    kEarlyNormDepthX = 340,
    kEarlyNormDepthY = 500,
    kEarlyNormDepthFrames = 180,
    
    kEarlyOrderX = 440,
    kEarlyOrderY = 500,
    kEarlyOrderFrames = 180,
  
    kEarlyReflectCoeffX = 540,
    kEarlyReflectCoeffY = 500,
    kEarlyReflectCoeffFrames = 180,
    
    kLogoAnimFrames = 31
};

//
ReverbDepth::ReverbDepth(const InstanceInfo &info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
    TRACE;
    
    InitNull();
    InitParams();

    Init();
  
#if IPLUG_EDITOR // http://bit.ly/2S64BDd
    mMakeGraphicsFunc = [&]() { return this->MyMakeGraphics(); };
    
    mLayoutFunc = [&](IGraphics* pGraphics) { this->MyMakeLayout(pGraphics); };
#endif
    
    BL_PROFILE_RESET;
}

ReverbDepth::~ReverbDepth()
{
    if (mReverb != NULL)
        delete mReverb;

    if (mMultiViewer != NULL)
        delete mMultiViewer;

    if (mReverbViewer != NULL)
        delete mReverbViewer;

    if (mCustomControl != NULL)
        delete mCustomControl;

    if (mCustomMouseControl != NULL)
        delete mCustomMouseControl;
    
    if (mGUIHelper != NULL)
        delete mGUIHelper;
}

IGraphics *
ReverbDepth::MyMakeGraphics()
{
    int fps = BLUtilsPlug::GetPlugFPS(PLUG_FPS);
    
    IGraphics *graphics = MakeGraphics(*this,
                                       PLUG_WIDTH, PLUG_HEIGHT,
                                       fps,
                                       GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
    
    return graphics;
}

void
ReverbDepth::MyMakeLayout(IGraphics *pGraphics)
{
    ENTER_PARAMS_MUTEX;
    
    // Remove all the controls, useful for example just after a GUI resize
    pGraphics->RemoveAllControls();

    // IGraphics: DEFAULT_FONT
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    
    pGraphics->LoadFont("font-regular", FONT_REGULAR_FN);
    pGraphics->LoadFont("font-light", FONT_LIGHT_FN);
    pGraphics->LoadFont("font-bold", FONT_BOLD_FN);

    // Style: BLULAB_V3
    pGraphics->LoadFont("OpenSans-ExtraBold", FONT_OPENSANS_EXTRA_BOLD_FN);
    pGraphics->LoadFont("Roboto-Bold", FONT_ROBOTO_BOLD_FN);
    
    pGraphics->AttachBackground(BACKGROUND_FN);

#ifdef __linux__
    pGraphics->AttachTextEntryControl();
#endif
    
#if 0 // Debug
    pGraphics->ShowControlBounds(true);
#endif
    
    // For rollover buttons
    pGraphics->EnableMouseOver(true);
    
    CreateControls(pGraphics);
    
    ApplyParams();
    
    // Demo mode
    mDemoManager.Init(this, pGraphics);
    
    mUIOpened = true;
    
    LEAVE_PARAMS_MUTEX;
}

void
ReverbDepth::InitNull()
{
    BLUtilsPlug::PlugInits();
    
    mUIOpened = false;
    mControlsCreated = false;
    
    // Init WDL FFT
    FftProcessObj16::Init();
    
    mGUIHelper = NULL;
    mGraph = NULL;
    mSpectrogramDisplay = NULL;
    mSpectrogramDisplayState = NULL;
    
    mReverb = NULL;
    mMultiViewer = NULL;
    mReverbViewer = NULL;
    mCustomControl = NULL;
    mCustomMouseControl = NULL;
    
    mReverb = NULL;

    mIsInitialized = false;
}

void
ReverbDepth::InitParams()
{
    //
    mConfig.mDry = 40.0;
    GetParam(kDry)->InitDouble("Dry", 40.0, 0.0, 100.0, 0.1, "%");
    mConfig.mWet = 33.0;
    GetParam(kWet)->InitDouble("Wet", 33.0, 0.0, 100.0, 0.1, "%");
    mConfig.mRoomSize = 50.0;
    GetParam(kRoomSize)->InitDouble("RoomSize", 50.0, 0.0, 100.0, 0.1, "%");
    mConfig.mRevWidth = 100.0;
    GetParam(kRevWidth)->InitDouble("Width", 100.0, 0.0, 100.0, 0.1, "%");
    mConfig.mDamping = 50.0;
    GetParam(kDamping)->InitDouble("Damping", 50.0, 0.0, 100.0, 0.1, "%");
  
    //
    mConfig.mUseFilter = false;
    GetParam(kUseFilter)->InitInt("UseFilter", 0, 0, 1, "", IParam::kFlagMeta);
  
    //
    mConfig.mUseEarly = false;
    GetParam(kUseEarly)->InitInt("UseEarly", 0, 0, 1, "", IParam::kFlagMeta);

    mConfig.mEarlyRoomSize = 10.0;
    GetParam(kEarlyRoomSize)->InitDouble("EarlyIntermicDist", 10.0, 0.0, 100.0, 0.1, "m");
    mConfig.mEarlyIntermicDist = 0.1;
    GetParam(kEarlyIntermicDist)->InitDouble("EarlyIntermicDist", 0.1, 0.0, 1.0, 0.001, "m");
    mConfig.mEarlyNormDepth = 50.0;
    GetParam(kEarlyNormDepth)->InitDouble("EarlyDepth", 50.0, 0.0, 100.0, 0.1, "%");

    mConfig.mEarlyOrder = 2;
    GetParam(kEarlyOrder)->InitInt("EarlyOrder", 2, 1, 4, "");

    mConfig.mEarlyReflectCoeff = 100.0;
    GetParam(kEarlyReflectCoeff)->InitDouble("EarlyReflectCoeff", 100.0, 0.0, 100.0, 0.1, "%");
  
    //
    mConfig.mUseReverbTail = false;
    GetParam(kUseReverbTail)->InitInt("UseReverbTail", 0, 0, 1, "", IParam::kFlagMeta);
}

void
ReverbDepth::ApplyParams()
{
    if (mReverb != NULL)
    {             
        mReverb->SetDry(mConfig.mDry);
        mReverb->SetWet(mConfig.mWet);
        mReverb->SetRoomSize(mConfig.mRoomSize);
        mReverb->SetWidth(mConfig.mRevWidth);
        mReverb->SetDamping(mConfig.mDamping);
        mReverb->SetUseFilter(mConfig.mUseFilter);
        mReverb->SetUseEarlyReflections(mConfig.mUseEarly);
        mReverb->SetEarlyRoomSize(mConfig.mEarlyRoomSize);
        mReverb->SetEarlyNormDepth(mConfig.mEarlyNormDepth);
        mReverb->SetUseReverbTail(mConfig.mUseReverbTail);
        mReverb->SetEarlyOrder(mConfig.mEarlyOrder);
        mReverb->SetEarlyReflectCoeff(mConfig.mEarlyReflectCoeff);
        
        //mReverbViewer->Update();
    }    

    if (mSpectrogramDisplay != NULL)
        mSpectrogramDisplay->UpdateSpectrogram(false);
            
    // For GUI resize
    GUIHelper12::RefreshAllParameters(this, kNumParams);
}

void
ReverbDepth::Init()
{
    if (mIsInitialized)
        return;
    
    BL_FLOAT sampleRate = GetSampleRate();
  
    mReverb = new USTDepthReverbTest(sampleRate);
    
    mMultiViewer = new MultiViewer2(sampleRate, BUFFER_SIZE);
  
    mCustomControl = new ReverbDepthCustomControl(this);
    mCustomMouseControl = new ReverbDepthCustomMouseControl(this);

    mReverbViewer = new BLReverbViewer(mReverb, mMultiViewer,
                                       VIEWER_DEFAULT_DURATION, sampleRate);
  
    mViewerTimeDuration = VIEWER_DEFAULT_DURATION;
  
    //
    ApplyParams();
    
    mIsInitialized = true;
}

void
ReverbDepth::ProcessBlock(iplug::sample **inputs,
                          iplug::sample **outputs, int nFrames)
{
    // Mutex is already locked for us.
    
    // Be sure to have sound even when the UI is closed
    BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);

    if (!mIsInitialized)
        return;
    
    if (mGraph != NULL)
        mGraph->Lock();
    
    BL_PROFILE_BEGIN;
    
    FIX_FLT_DENORMAL_INIT();
    
    vector<WDL_TypedBuf<BL_FLOAT> > &in = mTmpBuf0;
    vector<WDL_TypedBuf<BL_FLOAT> > &scIn = mTmpBuf1;
    vector<WDL_TypedBuf<BL_FLOAT> > &out = mTmpBuf2;
    BLUtilsPlug::GetPlugIOBuffers(this, inputs, outputs, nFrames,
                              &in, &scIn, &out);
  

    // Cubase 10, Sierra
    // in or out can be empty...
    if (in.empty() || out.empty())
    {
        if (mGraph != NULL)
            mGraph->Unlock();
        
        return;
    }
    
    // Warning: there is a bug in Logic EQ plugin:
    // - when not playing, ProcessDoubleReplacing is still called continuously
    // - and the values are not zero ! (1e-5 for example)
    // This is the same for Protools, and if the plugin consumes, this slows all without stop
    // For example when selecting "offline"
    // Can be the case if we switch to the offline quality option:
    // All slows down, and Protools or Logix doesn't prompt for insufficient resources
    mSecureRestarter.Process(in);
    
    // Set the outputs to 0
    //
    // FIX: on Protools, in render mode, after play is finished,
    // there is a buzz sound
    for (int i = 0; i < out.size(); i++)
    {
        WDL_TypedBuf<BL_FLOAT> &out0 = out[i];
        BLUtils::FillAllZero(&out0);
    }

    if (out.size() == 2)
    {
        if (in.size() == 1)
        {
            if (mReverb != NULL)
                mReverb->Process(in[0], &out[0], &out[1]);
        }
        else if (in.size() == 2)
        {
            WDL_TypedBuf<BL_FLOAT> stereoInput[2] = { in[0], in[1] };
            
            if (mReverb != NULL)
                mReverb->Process(stereoInput, &out[0], &out[1]);
        }
          
        BLUtilsPlug::PlugCopyOutputs(out, outputs, nFrames);
    }
    else
        // Bad number of outputs
    {
        BLUtilsPlug::BypassPlug(inputs, outputs, nFrames);
    }
  
    
    // Demo mode
    if (mDemoManager.MustProcess())
    {
        mDemoManager.Process(outputs, nFrames);
    }
  
    if (mGraph != NULL)
        mGraph->Unlock();
    
    BL_PROFILE_END;
}

void
ReverbDepth::CreateControls(IGraphics *pGraphics)
{
    if (mGUIHelper == NULL)
        mGUIHelper = new GUIHelper12(GUIHelper12::STYLE_BLUELAB);

    // TODO: mouse catcher
    
    // Graph
    mGraph = mGUIHelper->CreateGraph(this, pGraphics,
                                     kGraphX, kGraphY,
                                     GRAPH_FN,
                                     kGraph);

    mSpectrogramDisplay = new SpectrogramDisplay2(mSpectrogramDisplayState);
    mSpectrogramDisplayState = mSpectrogramDisplay->GetState();
    
    if (mMultiViewer != NULL)
        mMultiViewer->SetGraph(mGraph, mSpectrogramDisplay, mGUIHelper);    

    mGraph->AddCustomControl(mCustomControl);

    // HACK, to avoid creating a IPanelMouseControl like in iPlug1
    // Now, mCustomControl and mCustomMouseControl must be the same object
    // And now, we need to set UPDATE_VIEW_ON_PARAM_CHANGE to 1
    mGraph->AddCustomControl(mCustomMouseControl);
        
    //
    mGUIHelper->CreateToggleButton(pGraphics,
                                   kUseReverbTailX,
                                   kUseReverbTailY,
                                   CHECKBOX_FN, kUseReverbTail, "USE REV TAIL",
                                   GUIHelper12::SIZE_SMALL);
    
    mGUIHelper->CreateKnob(pGraphics,
                           kDryX, kDryY,
                           KNOB_SMALL_FN,
                           kDryFrames,
                           kDry,
                           TEXTFIELD_FN,
                           "DRY",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kWetX, kWetY,
                           KNOB_SMALL_FN,
                           kWetFrames,
                           kWet,
                           TEXTFIELD_FN,
                           "WET",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kRoomSizeX, kRoomSizeY,
                           KNOB_SMALL_FN,
                           kRoomSizeFrames,
                           kRoomSize,
                           TEXTFIELD_FN,
                           "ROOM SIZE",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kRevWidthX, kRevWidthY,
                           KNOB_SMALL_FN,
                           kRevWidthFrames,
                           kRevWidth,
                           TEXTFIELD_FN,
                           "WIDTH",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kDampingX, kDampingY,
                           KNOB_SMALL_FN,
                           kDampingFrames,
                           kDamping,
                           TEXTFIELD_FN,
                           "DAMPING",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateToggleButton(pGraphics,
                                   kUseFilterX,
                                   kUseFilterY,
                                   CHECKBOX_FN, kUseFilter, "FILTER",
                                   GUIHelper12::SIZE_SMALL);

    mGUIHelper->CreateToggleButton(pGraphics,
                                   kUseEarlyX,
                                   kUseEarlyY,
                                   CHECKBOX_FN, kUseEarly, "EARLY REF",
                                   GUIHelper12::SIZE_SMALL);

    mGUIHelper->CreateKnob(pGraphics,
                           kEarlyRoomSizeX, kEarlyRoomSizeY,
                           KNOB_SMALL_FN,
                           kEarlyRoomSizeFrames,
                           kEarlyRoomSize,
                           TEXTFIELD_FN,
                           "ER ROOM SIZE",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kEarlyIntermicDistX, kEarlyIntermicDistY,
                           KNOB_SMALL_FN,
                           kEarlyIntermicDistFrames,
                           kEarlyIntermicDist,
                           TEXTFIELD_FN,
                           "ER MIC DIST",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kEarlyNormDepthX, kEarlyNormDepthY,
                           KNOB_SMALL_FN,
                           kEarlyNormDepthFrames,
                           kEarlyNormDepth,
                           TEXTFIELD_FN,
                           "ER NORM DEPTH",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kEarlyOrderX, kEarlyOrderY,
                           KNOB_SMALL_FN,
                           kEarlyOrderFrames,
                           kEarlyOrder,
                           TEXTFIELD_FN,
                           "ER ORDER",
                           GUIHelper12::SIZE_DEFAULT);

    mGUIHelper->CreateKnob(pGraphics,
                           kEarlyReflectCoeffX, kEarlyReflectCoeffY,
                           KNOB_SMALL_FN,
                           kEarlyReflectCoeffFrames,
                           kEarlyReflectCoeff,
                           TEXTFIELD_FN,
                           "ER REFLECT COEFF",
                           GUIHelper12::SIZE_DEFAULT);
    
    
    // Version
    mGUIHelper->CreateVersion(this, pGraphics, PLUG_VERSION_STR); //, GUIHelper12::BOTTOM);
    
    // Logo
    mGUIHelper->CreateLogoAnim(this, pGraphics, LOGO_FN,
                               kLogoAnimFrames, GUIHelper12::BOTTOM);
    
    // Plugin name
    mGUIHelper->CreatePlugName(this, pGraphics, PLUGNAME_FN, GUIHelper12::BOTTOM);
    
    // Help button
    mGUIHelper->CreateHelpButton(this, pGraphics,
                                 HELP_BUTTON_FN, MANUAL_FN,
                                 GUIHelper12::BOTTOM);
    
    mGUIHelper->CreateDemoMessage(pGraphics);
    
    //
    mControlsCreated = true;
}

void
ReverbDepth::OnReset()
{
    TRACE;
    ENTER_PARAMS_MUTEX;

    // Logic X has a bug: when it restarts after stop,
    // sometimes it provides full volume directly, making a sound crack
    mSecureRestarter.Reset();
    
    BL_FLOAT sampleRate = GetSampleRate();
    int blockSize = GetBlockSize();
    if (mReverb != NULL)
        mReverb->Reset(sampleRate, blockSize);

    if (mMultiViewer != NULL)
        mMultiViewer->Reset(sampleRate, BUFFER_SIZE);

    if (mReverbViewer != NULL)
        mReverbViewer->Reset(sampleRate);

    LEAVE_PARAMS_MUTEX;
    
    BL_PROFILE_RESET;
}

void
ReverbDepth::OnParamChange(int paramIdx)
{  
    if (!mIsInitialized)
        return;
  
    ENTER_PARAMS_MUTEX;
    
    switch (paramIdx)
    {
        case kDry:
        {
            BL_FLOAT value = GetParam(kDry)->Value();
            BL_FLOAT dry = value/100.0;
      
            if (mReverb != NULL)
                mReverb->SetDry(dry);

#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
            
            mConfig.mDry = dry;
            SaveConfig();
        }
        break;
    
        case kWet:
        {
            BL_FLOAT value = GetParam(kWet)->Value();
            BL_FLOAT wet = value/100.0;
            
            if (mReverb != NULL)
                mReverb->SetWet(wet);

#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
      
            mConfig.mWet = wet;
            SaveConfig();
        }
        break;
    
        case kRoomSize:
        {
            BL_FLOAT value = GetParam(kRoomSize)->Value();
            BL_FLOAT roomSize = value/100.0;
            
            if (mReverb != NULL)
                mReverb->SetRoomSize(roomSize);
            
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
      
            mConfig.mRoomSize = roomSize;
            SaveConfig();
        }
        break;
        
        case kRevWidth:
        {
            BL_FLOAT value = GetParam(kRevWidth)->Value();
            BL_FLOAT revWidth = value/100.0;
            
            if (mReverb != NULL)
                mReverb->SetWidth(revWidth);
            
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
            
            mConfig.mRevWidth = revWidth;
            SaveConfig();
        }
        break;
    
        case kDamping:
        {
            BL_FLOAT value = GetParam(kDamping)->Value();
            BL_FLOAT damping = value/100.0;
            
            if (mReverb != NULL)
                mReverb->SetDamping(damping);
            
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
      
            mConfig.mDamping = damping;
            SaveConfig();
        }
        break;
        
        case kUseFilter:
        {
            int value = GetParam(kUseFilter)->Value();
            bool useFilter = (value == 1);
            
            if (mReverb != NULL)
                mReverb->SetUseFilter(useFilter);
            
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
            
            mConfig.mUseFilter = useFilter;
            SaveConfig();
        }
        break;
        
        case kUseEarly:
        {
            int value = GetParam(kUseEarly)->Value();
            bool useEarly = (value == 1);
            
            if (mReverb != NULL)
                mReverb->SetUseEarlyReflections(useEarly);
            
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
            
            mConfig.mUseEarly = useEarly;
            SaveConfig();
        }
        break;
      
        case kEarlyRoomSize:
        {
            BL_FLOAT roomSize = GetParam(kEarlyRoomSize)->Value();
            
            if (mReverb != NULL)
                mReverb->SetEarlyRoomSize(roomSize);
            
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
            
            mConfig.mEarlyRoomSize = roomSize;
            SaveConfig();
        }
        break;
    
        case kEarlyIntermicDist:
        {
            BL_FLOAT intermicDist = GetParam(kEarlyIntermicDist)->Value();
            
            if (mReverb != NULL)
                mReverb->SetEarlyIntermicDist(intermicDist);
            
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
            
            mConfig.mEarlyIntermicDist = intermicDist;
            SaveConfig();
        }
        break;
        
        case kEarlyNormDepth:
        {
            BL_FLOAT value = GetParam(kEarlyNormDepth)->Value();
            BL_FLOAT normDepth = value/100.0;
            
            if (mReverb != NULL)
                mReverb->SetEarlyNormDepth(normDepth);
            
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
            
            mConfig.mEarlyNormDepth = normDepth;
            SaveConfig();
        }
        break;
      
        case kUseReverbTail:
        {
            int value = GetParam(kUseReverbTail)->Value();
            bool useReverbTail = (value == 1);
            
            if (mReverb != NULL)
                mReverb->SetUseReverbTail(useReverbTail);
            
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
            
            mConfig.mUseReverbTail = useReverbTail;
            SaveConfig();
        }
        break;
    
        case kEarlyOrder:
        {
            int order = GetParam(kEarlyOrder)->Value();
            
            if (mReverb != NULL)
                mReverb->SetEarlyOrder(order);
            
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
            
            mConfig.mEarlyOrder = order;
            SaveConfig();
        }
        break;
        
        case kEarlyReflectCoeff:
        {
            BL_FLOAT value = GetParam(kEarlyReflectCoeff)->Value();
            BL_FLOAT reflectCoeff = value/100.0;
            
            if (mReverb != NULL)
                mReverb->SetEarlyReflectCoeff(reflectCoeff);
            
#if UPDATE_VIEW_ON_PARAM_CHANGE
            if (mReverbViewer != NULL)
                mReverbViewer->Update();
#endif
            
            mConfig.mEarlyReflectCoeff = reflectCoeff;
            SaveConfig();
        }
        break;
        
        default:
            break;
    }
    
    LEAVE_PARAMS_MUTEX;
}

void
ReverbDepth::OnUIOpen()
{    
    IEditorDelegate::OnUIOpen();
    
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;
    LEAVE_PARAMS_MUTEX;
}

void
ReverbDepth::OnUIClose()
{   
    // Make sure we are not currently processing block
    // (process block send stuff to UI)
    ENTER_PARAMS_MUTEX;

    mGraph = NULL;

    if (mMultiViewer != NULL)
        mMultiViewer->SetGraph(NULL, NULL);
    
    mSpectrogramDisplay = NULL;
    
    // Make sure we won't go to ProcessBlock() again
    mUIOpened = false;
    
    LEAVE_PARAMS_MUTEX;
}

int
ReverbDepth::UnserializeState(const IByteChunk &pChunk, int startPos)
{
    TRACE;
  
    //IMutexLock lock(this);
  
    int res = iplug::IPluginBase::UnserializeParams(pChunk, startPos);

    if (mReverbViewer != NULL)
        mReverbViewer->Update();
  
    return res;
}

void
ReverbDepth::OnMouseUp()
{
    if (mReverbViewer != NULL)
        mReverbViewer->Update();
}

void
ReverbDepth::UpdateTimeZoom(int mouseDelta)
{ 
    BL_FLOAT dZoom = mouseDelta*MOUSE_ZOOM_COEFF;
    
    mViewerTimeDuration *= (1.0 + dZoom);
    if (mViewerTimeDuration < VIEWER_MIN_DURATION)
        mViewerTimeDuration = VIEWER_MIN_DURATION;
    
    if (mViewerTimeDuration > VIEWER_MAX_DURATION)
        mViewerTimeDuration = VIEWER_MAX_DURATION;

    if (mReverbViewer != NULL)
        mReverbViewer->SetDuration(mViewerTimeDuration);
}

void
ReverbDepth::SaveConfig()
{
    FILE *file = fopen("/Users/applematuer/Documents/BlueLabAudio-Debug/ReverbDepth-config.txt", "wb");
  
    fprintf(file, "useReverbTail, dry, wet, roomSize, width, damping, useFilter, useEarly, earlyRoomSize, earlyIntermicDist, earlyNormDepth, earlyOrder, earlyReflectCoeff\n");
  
    fprintf(file, "%g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g\n",
            mConfig.mUseReverbTail, mConfig.mDry, mConfig.mWet, mConfig.mRoomSize,
            mConfig.mRevWidth, mConfig.mDamping,
            mConfig.mUseFilter, mConfig.mUseEarly,
            mConfig.mEarlyRoomSize, mConfig.mEarlyIntermicDist, mConfig.mEarlyNormDepth,
            mConfig.mEarlyOrder, mConfig.mEarlyReflectCoeff);
  
    fclose(file);
}
