/*=========================================================================

  Program:   ParaView
  Module:    vtkPVMultiDisplayPartDisplay.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkPVMultiDisplayPartDisplay.h"

#include "vtkMultiProcessController.h"
#include "vtkObjectFactory.h"
#include "vtkPVApplication.h"
#include "vtkPVProcessModule.h"
#include "vtkClientServerStream.h"


//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkPVMultiDisplayPartDisplay);
vtkCxxRevisionMacro(vtkPVMultiDisplayPartDisplay, "1.11");


//----------------------------------------------------------------------------
vtkPVMultiDisplayPartDisplay::vtkPVMultiDisplayPartDisplay()
{
}

//----------------------------------------------------------------------------
vtkPVMultiDisplayPartDisplay::~vtkPVMultiDisplayPartDisplay()
{
}

//----------------------------------------------------------------------------
void vtkPVMultiDisplayPartDisplay::SetLODCollectionDecision(int)
{
  // Always colect LOD.
  this->Superclass::SetLODCollectionDecision(1);
}

//----------------------------------------------------------------------------
void vtkPVMultiDisplayPartDisplay::Update()
{
  // Update like normal, but make sure the LOD is collected.
  // I encountered a bug. First render was missing the LOD on the client.
  this->Superclass::SetLODCollectionDecision(1);
  this->Superclass::Update();
}

//----------------------------------------------------------------------------
void vtkPVMultiDisplayPartDisplay::CreateParallelTclObjects(vtkPVApplication *pvApp)
{
  this->Superclass::CreateParallelTclObjects(pvApp);
  vtkPVProcessModule* pm = pvApp->GetProcessModule();

  if(pvApp->GetClientMode())
    {
    // We need this because the socket controller has no way of distinguishing
    // between processes.
    pm->GetStream()
      << vtkClientServerStream::Invoke
      << this->CollectID << "SetClientFlag" << 1
      << vtkClientServerStream::End;
    pm->GetStream()
      << vtkClientServerStream::Invoke
      << this->LODCollectID << "SetClientFlag" << 1
      << vtkClientServerStream::End;
    pm->SendStreamToClient();
    }
  else
    {
    vtkErrorMacro("Cannot run tile display without client-server mode.");
    }
}

//----------------------------------------------------------------------------
void vtkPVMultiDisplayPartDisplay::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
}


  



