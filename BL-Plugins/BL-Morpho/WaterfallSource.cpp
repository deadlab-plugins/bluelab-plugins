#include <MorphoWaterfallGUI.h>
#include <BorderCustomDrawer.h>

#include "WaterfallSource.h"

WaterfallSource::WaterfallSource(BL_FLOAT sampleRate,
                                 MorphoPlugMode plugMode)
{
    mWaterfallViewMode = MORPHO_WATERFALL_VIEW_TRACKING;

    // Waterfall
    mWaterfallAngle0 = WATERFALL_DEFAULT_CAM_ANGLE0;
    mWaterfallAngle1 = WATERFALL_DEFAULT_CAM_ANGLE1;
    mWaterfallCameraFov = WATERFALL_DEFAULT_CAM_FOV;

    mWaterfallGUI = new MorphoWaterfallGUI(sampleRate, plugMode);
}

WaterfallSource::~WaterfallSource()
{
    delete mWaterfallGUI;
}

void
WaterfallSource::SetWaterfallViewMode(WaterfallViewMode mode)
{
    mWaterfallViewMode = mode;

    mWaterfallGUI->SetWaterfallViewMode(mWaterfallViewMode);
}

WaterfallViewMode
WaterfallSource::GetWaterfallViewMode() const
{
    return mWaterfallViewMode;
}

void
WaterfallSource::SetWaterfallCameraAngle0(BL_FLOAT angle)
{
    mWaterfallAngle0 = angle;
    
    mWaterfallGUI->SetWaterfallCameraAngle0(mWaterfallAngle0);
}

void
WaterfallSource::SetWaterfallCameraAngle1(BL_FLOAT angle)
{
    mWaterfallAngle1 = angle;

    mWaterfallGUI->SetWaterfallCameraAngle1(mWaterfallAngle1);
}

void
WaterfallSource::SetWaterfallCameraFov(BL_FLOAT angle)
{
    mWaterfallCameraFov = angle;

    mWaterfallGUI->SetWaterfallCameraFov(mWaterfallCameraFov);
}

void
WaterfallSource::Reset(BL_FLOAT sampleRate)
{
    mWaterfallGUI->Reset(sampleRate);
}

void
WaterfallSource::OnUIOpen()
{
    mWaterfallGUI->OnUIOpen();
}

void
WaterfallSource::OnUIClose()
{
    mWaterfallGUI->OnUIClose();
}

void
WaterfallSource::SetViewEnabled(bool flag)
{
    mWaterfallGUI->SetGraphEnabled(flag);
}

void
WaterfallSource::CreateWaterfallControls(GUIHelper12 *guiHelper,
                                         Plugin *plug,
                                         View3DPluginInterface *view3DListener,
                                         int grapX, int graphY,
                                         const char *graphBitmapFN)
{
    if (mWaterfallGUI->IsControlsCreated())
        // Already created
        return;
        
    IGraphics *graphics = plug->GetUI();
    if (graphics == NULL)
        return;
    
    GraphControl12 *graph =
        mWaterfallGUI->CreateControls(guiHelper, plug, graphics,
                                      grapX, graphY, graphBitmapFN,
                                      0, 0);

    // Border
    float borderWidth = 1.0;
    IColor borderColor(255, 255, 255, 255);
    BorderCustomDrawer *borderDrawer =
        new BorderCustomDrawer(borderWidth, borderColor);
    graph->AddCustomDrawer(borderDrawer);
    
    mWaterfallGUI->SetView3DListener(view3DListener);
    
    mWaterfallGUI->SetWaterfallCameraAngle0(mWaterfallAngle0);
    mWaterfallGUI->SetWaterfallCameraAngle1(mWaterfallAngle1);
    mWaterfallGUI->SetWaterfallCameraFov(mWaterfallCameraFov);
}
