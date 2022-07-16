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
 
#include <stdlib.h>

#include <SoSource.h>
#include <ITabsBarControl.h>
#include <MorphoObj.h>
#include <SoSourceManager.h>

#include "SoSourcesView.h"

SoSourcesView::SoSourcesView(Plugin *plug,
                             View3DPluginInterface *view3DListener,
                             MorphoObj *morphoObj,
                             SoSourceManager *sourceManager)
{
    mPlug = plug;
    mView3DListener = view3DListener;
    
    mMorphoObj = morphoObj;
    mSourceManager = sourceManager;
        
    mListener = NULL;
        
    mTabsBar = NULL;

    //
    mGUIHelper = NULL;
    
    // Spectro GUI
    mSpectroGraphX = 0;
    mSpectroGraphY = 0;
    memset(mSpectroGraphBitmapFN, '\0', FILENAME_SIZE);

    // Waterfall GUI
    mWaterfallGraphX = 0;
    mWaterfallGraphY = 0;
    memset(mWaterfallGraphBitmapFN, '\0', FILENAME_SIZE);
}

SoSourcesView::~SoSourcesView() {}

void
SoSourcesView::SetListener(SoSourcesViewListener *listener)
{
    mListener = listener;
}

void
SoSourcesView::SetGUIHelper(GUIHelper12 *guiHelper)
{
    mGUIHelper = guiHelper;
}

void
SoSourcesView::SetSpectroGUIParams(int graphX, int graphY,
                                   const char *graphBitmapFN)
{
    mSpectroGraphX = graphX;
    mSpectroGraphY = graphY;
    
    strcpy(mSpectroGraphBitmapFN, graphBitmapFN);
}

void
SoSourcesView::SetWaterfallGUIParams(int graphX, int graphY,
                                     const char *graphBitmapFN)
{
    mWaterfallGraphX = graphX;
    mWaterfallGraphY = graphY;
    
    strcpy(mWaterfallGraphBitmapFN, graphBitmapFN);
}

void
SoSourcesView::SetTabsBar(ITabsBarControl *tabsBar)
{
    mTabsBar = tabsBar;

    if (mTabsBar != NULL)
        mTabsBar->SetListener(this);
}

// Tabs bar
void
SoSourcesView::OnTabSelected(int tabNum)
{
    //mSourceManager->SetCurrentSourceIdx(tabNum);
    mMorphoObj->SetCurrentSoSourceIdx(tabNum);
    
    Refresh();

    if (mListener != NULL)
    {
        SoSource *source = mSourceManager->GetCurrentSource();
        mListener->SoSourceChanged(source);
    }
}

void
SoSourcesView::OnTabClose(int tabNum)
{
    //mSourceManager->RemoveSource(tabNum);
    //Refresh();

    if (mListener != NULL)
        mListener->OnRemoveSource(tabNum);
}

void
SoSourcesView::ReCreateTabs(int numTabs)
{
    if (mTabsBar == NULL)
        return;

    for (int i = 0; i < numTabs; i++)
        mTabsBar->NewTab("");
    
    //Refresh();
}

void
SoSourcesView::ClearGUI()
{
    for (int i = 0; i < mSourceManager->GetNumSources(); i++)
    {
        SoSource *source = mSourceManager->GetSource(i);
        if (source != NULL)
            source->OnUIClose();
    }
}

// Refresh all
void
SoSourcesView::Refresh()
{
    if (mTabsBar != NULL)
    {
        // Update tabs bar
        for (int i = 0; i < mTabsBar->GetNumTabs(); i++)
        {
            SoSource *source = mSourceManager->GetSource(i);
            if (source != NULL)
            {
                char name[FILENAME_SIZE];
                source->GetName(name);
            
                mTabsBar->SetTabName(i, name);
            }
        }

        // Update the sources manager
        int idx = mTabsBar->GetSelectedTab();
        mSourceManager->SetCurrentSourceIdx(idx);

        if (mListener != NULL)
        {
            SoSource *source = mSourceManager->GetCurrentSource();
            mListener->SoSourceChanged(source);
        }
    }

    EnableCurrentView();
}

void
SoSourcesView::OnUIOpen()
{
    SoSource *source = mSourceManager->GetCurrentSource();
    if (source != NULL)
        source->OnUIOpen();
}

void
SoSourcesView::OnUIClose()
{
    SoSource *source = mSourceManager->GetCurrentSource();
    if (source != NULL)
        source->OnUIClose();
}

void
SoSourcesView::OnIdle()
{
    SoSource *source = mSourceManager->GetCurrentSource();
    if (source != NULL)
        source->CheckRecomputeSpectro();
}

void
SoSourcesView::NewTab()
{
    if (mTabsBar == NULL)
        return;

    if (mTabsBar->GetNumTabs() >= MAX_NUM_SOURCES)
        return;
    
    mTabsBar->NewTab("");
    mTabsBar->SelectTab(mTabsBar->GetNumTabs() - 1);

    mSourceManager->NewSource();

    RecreateControls();
    
    Refresh();
}

void
SoSourcesView::SelectTab(int tabNum)
{
    if (mTabsBar == NULL)
        return;
    
    mTabsBar->SelectTab(tabNum);

    Refresh();
}

void
SoSourcesView::RecreateControls()
{
    // Recreate spectro controls
    for (int i = 0; i < mSourceManager->GetNumSources(); i++)
    {
        SoSource *source = mSourceManager->GetSource(i);
        if (source == NULL)
            continue;

        // Try to create spectro controls
        // If already created, nothing will be done by CreateSpectroControls()
        source->CreateSpectroControls(mGUIHelper, mPlug,
                                      mSpectroGraphX, mSpectroGraphY,
                                      mSpectroGraphBitmapFN);

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
SoSourcesView::EnableCurrentView()
{
    int currentSourceIdx = mSourceManager->GetCurrentSourceIdx();
    if (currentSourceIdx != -1)
    {
        for (int i = 0; i < mSourceManager->GetNumSources(); i++)
        {
            bool enabled = (i == currentSourceIdx);

            SoSource *source = mSourceManager->GetSource(i);
            if (source != NULL)
                source->SetViewEnabled(enabled);
        }
    }
}
