/*=========================================================================

  Program:   ParaView
  Module:    vtkCPCxxHelper.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkCPCxxHelper.h"

#include "vtkInitializationHelper.h"
#include "vtkObjectFactory.h"
#include "vtkProcessModule.h"
#include "vtkPVConfig.h" // Required to get build options for paraview
#include "vtkPVOptions.h"
#include "vtkSMObject.h"
#include "vtkSMProperty.h"
#include "vtkSMProxyManager.h"
#include "vtkSMSession.h"

#include <assert.h>
#include <string>
#include <vtksys/ios/sstream>
#include <vtksys/SystemTools.hxx>

//----------------------------------------------------------------------------
// Needed when we don't use the vtkStandardNewMacro.
vtkInstantiatorNewMacro(vtkCPCxxHelper);

vtkWeakPointer<vtkCPCxxHelper> vtkCPCxxHelper::Instance;


//----------------------------------------------------------------------------
vtkCPCxxHelper::vtkCPCxxHelper()
{
  this->Options = 0;
}

//----------------------------------------------------------------------------
vtkCPCxxHelper::~vtkCPCxxHelper()
{
  if(this->Options)
    {
    this->Options->Delete();
    this->Options = 0;
    }
  vtkInitializationHelper::Finalize();
}

//----------------------------------------------------------------------------
vtkCPCxxHelper* vtkCPCxxHelper::New()
{
  if (vtkCPCxxHelper::Instance.GetPointer() == 0)
    {
    // Try the factory first
    vtkCPCxxHelper* instance = (vtkCPCxxHelper*)
      vtkObjectFactory::CreateInstance("vtkCPCxxHelper");
    // if the factory did not provide one, then create it here
    if (!instance)
      {
      instance = new vtkCPCxxHelper;
      }

    vtkCPCxxHelper::Instance = instance;

    int argc = 1;
    char** argv = new char*[1];
    std::string CWD = vtksys::SystemTools::GetCurrentWorkingDirectory();
    argv[0] = new char[CWD.size()+1];
#ifdef COPROCESSOR_WIN32_BUILD
    strcpy_s(argv[0], strlen(CWD.c_str()), CWD.c_str());
#else
    strcpy(argv[0], CWD.c_str());
#endif

    vtkCPCxxHelper::Instance->Options = vtkPVOptions::New();
    vtkCPCxxHelper::Instance->Options->SetSymmetricMPIMode(1);

    vtkInitializationHelper::Initialize(
        argc, argv, vtkProcessModule::PROCESS_BATCH,
        vtkCPCxxHelper::Instance->Options);

    // Setup default session.
    vtkIdType connectionId = vtkSMSession::ConnectToSelf();
    assert(connectionId != 0);
    (void)connectionId;

    delete []argv[0];
    delete []argv;
    }
  else
    {
    vtkCPCxxHelper::Instance->Register(NULL);
    }

  return vtkCPCxxHelper::Instance;
}

//----------------------------------------------------------------------------
void vtkCPCxxHelper::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
