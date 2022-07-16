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
 
#include <SySource.h>
#include <IXYPadControlExt.h>
#include <IIconLabelControl.h>
#include <MorphoObj.h>
#include <SySourceManager.h>

#include "SySourcesView.h"

SySourcesView::SySourcesView(Plugin *plug,
                             View3DPluginInterface *view3DListener,
                             MorphoObj *morphoObj,
                             SySourceManager *sourceManager)
{
    mPlug = plug;
    mView3DListener = view3DListener;

    mMorphoObj = morphoObj;
    mSourceManager = sourceManager;
    
    mListener = NULL;

    mXYPad = NULL;
    mIconLabel = NULL;

    //
    mGUIHelper = NULL;

    // Waterfall GUI
    mWaterfallGraphX = 0;
    mWaterfallGraphY = 0;
    memset(mWaterfallGraphBitmapFN, '\0', FILENAME_SIZE);
}

SySourcesView::~SySourcesView() {}

void
SySourcesView::SetListener(SySourcesViewListener *listener)
{
    mListener = listener;
}

void
SySourcesView::SetGUIHelper(GUIHelper12 *guiHelper)
{
    mGUIHelper = guiHelper;
}

void
SySourcesView::SetWaterfallGUIParams(int graphX, int graphY,
                                     const char *graphBitmapFN)
{
    mWaterfallGraphX = graphX;
    mWaterfallGraphY = graphY;
    
    strcpy(mWaterfallGraphBitmapFN, graphBitmapFN);
}

void
SySourcesView::SetXYPad(IXYPadControlExt *xyPad)
{
    mXYPad = xyPad;

    if (mXYPad != NULL)
        mXYPad->SetListener(this);
}

void
SySourcesView::SetIconLabel(IIconLabelControl *label)
{
    mIconLabel = label;
}

void
SySourcesView::OnHandleChanged(int handleNum)
{
    //mSourceManager->SetCurrentSourceIdx(handleNum - 1);
    mMorphoObj->SetCurrentSySourceIdx(handleNum);
    
    Refresh();
    
    if (mListener != NULL)
    {
        SySource *source = mSourceManager->GetCurrentSource();
        mListener->SySourceChanged(source);
    }
}

void
SySourcesView::ClearGUI()
{
    for (int i = 0; i < mSourceManager->GetNumSources(); i++)
    {
        SySource *source = mSourceManager->GetSource(i);
        if (source != NULL)
            source->OnUIClose();
    }
}

void
SySourcesView::Refresh()
{
    // refresh XY pad
    if (mXYPad != NULL)
    {
        int numSources = mSourceManager->GetNumSources();

        // Handle 0 is "listener"
        for (int i = 0; i < mXYPad->GetNumHandles(); i++)
        {
            // Display handle?
            bool enabled = (i < numSources);
            mXYPad->SetHandleEnabled(i, enabled);
        }

        for (int i = 0; i < numSources; i++)
        {
            // Grey out handl?
            SySource *source = mSourceManager->GetSource(i);
            if (!source->GetSourceMute() && source->CanOutputSound())
                mXYPad->SetHandleState(i, IXYPadControlExt::HANDLE_NORMAL);
            else // Muted
                mXYPad->SetHandleState(i, IXYPadControlExt::HANDLE_GRAYED_OUT);
        }
        
        // Highlight current source handle?
        SySource *currentSource = mSourceManager->GetCurrentSource();
        if (currentSource != NULL) // Can be null if selection the "listener" handle
        {
            int currentSourceIdx = mSourceManager->GetCurrentSourceIdx();
            if (!currentSource->GetSourceMute() && currentSource->CanOutputSound())
                mXYPad->SetHandleState(currentSourceIdx,
                                       IXYPadControlExt::HANDLE_HIGHLIGHTED);
        }
    }

    // Refresh icon label
    if (mIconLabel != NULL)
    {
        // Set label name
        SySource *currentSource = mSourceManager->GetCurrentSource();
        if (currentSource != NULL)
        {
            char sourceName[FILENAME_SIZE];
            currentSource->GetName(sourceName);

            mIconLabel->SetLabelText(sourceName);

            // Set label icon
            int currentSourceIdx = mSourceManager->GetCurrentSourceIdx();
            if (currentSourceIdx != -1)
            {
                // Base
                int iconNum = currentSourceIdx*3;
                if (!currentSource->GetSourceMute() &&
                    currentSource->CanOutputSound())
                    // Hilighted
                    iconNum += 1; // 1 is "hilighted"
                else
                    iconNum += 2; // 2 is "mute"
                
                mIconLabel->SetIconNum(iconNum);
            }
        }
    }

    EnableCurrentView(); // ?
}

void
SySourcesView::OnUIOpen()
{
    SySource *source = mSourceManager->GetCurrentSource();
    if (source != NULL)
        source->OnUIOpen();
}

void
SySourcesView::OnUIClose()
{
    SySource *source = mSourceManager->GetCurrentSource();
    if (source != NULL)
        source->OnUIClose();
}

void
SySourcesView::OnIdle()
{
    // Nothing to do
}

void
SySourcesView::RecreateControls()
{
    // Recreate spectro controls
    for (int i = 0; i < mSourceManager->GetNumSources(); i++)
    {
        SySource *source = mSourceManager->GetSource(i);
        if (source == NULL)
            continue;

        // Try to create waterfall controls
        // If already created, nothing will be done by CreateWaterfallControls()
        source->CreateWaterfallControls(mGUIHelper,
                                        mPlug, mView3DListener,
                                        mWaterfallGraphX, mWaterfallGraphY,
                                        mWaterfallGraphBitmapFN);
    }

    // Don't forget to enable the current spectro/waterfall view
    EnableCurrentView();
}

void
SySourcesView::EnableCurrentView()
{
    int currentSourceIdx = mSourceManager->GetCurrentSourceIdx();
    if (currentSourceIdx != -1)
    {
        for (int i = 0; i < mSourceManager->GetNumSources(); i++)
        {
            bool enabled = (i == currentSourceIdx);

            SySource *source = mSourceManager->GetSource(i);
            if (source != NULL)
                source->SetViewEnabled(enabled);
        }
    }
}
