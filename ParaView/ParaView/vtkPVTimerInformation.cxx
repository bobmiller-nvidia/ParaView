/*=========================================================================

  Program:   ParaView
  Module:    vtkPVTimerInformation.cxx

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkPVTimerInformation.h"
#include "vtkObjectFactory.h"
#include "vtkDataObject.h"
#include "vtkQuadricClustering.h"
#include "vtkByteSwap.h"
#include "vtkTimerLog.h"
#include "vtkPVApplication.h"
#include "vtkClientServerStream.h"

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkPVTimerInformation);
vtkCxxRevisionMacro(vtkPVTimerInformation, "1.5");



//----------------------------------------------------------------------------
vtkPVTimerInformation::vtkPVTimerInformation()
{
  this->NumberOfLogs = 0;
  this->Logs = NULL;
}


//----------------------------------------------------------------------------
vtkPVTimerInformation::~vtkPVTimerInformation()
{
  int idx;
  
  for (idx = 0; idx < this->NumberOfLogs; ++idx)
    {
    if (this->Logs && this->Logs[idx])
      {
      delete [] this->Logs[idx];
      this->Logs[idx] = NULL;
      }
    }

  if (this->Logs)
    {
    delete [] this->Logs;
    this->Logs = NULL;
    }
  this->NumberOfLogs = 0;
}


//----------------------------------------------------------------------------
void vtkPVTimerInformation::InsertLog(int id, char* log)
{
  if (id >= this->NumberOfLogs)
    {
    this->Reallocate(id+1);
    }
  if (this->Logs[id])
    {
    delete [] this->Logs[id];
    this->Logs[id] = NULL;
    }
  this->Logs[id] = log;
}

//----------------------------------------------------------------------------
void vtkPVTimerInformation::Reallocate(int num)
{
  int idx;
  char** newLogs;

  if (num == this->NumberOfLogs)
    {
    return;
    }

  if (num < this->NumberOfLogs)
    {
    vtkWarningMacro("Trying to shrink logs from " << this->NumberOfLogs
                    << " to " << num);
    return;
    }

  newLogs = new char*[num];
  for (idx = 0; idx < num; ++idx)
    {
    newLogs[idx] = NULL;
    }

  // Copy existing logs.
  for (idx = 0; idx < this->NumberOfLogs; ++idx)
    {
    newLogs[idx] = this->Logs[idx];
    this->Logs[idx] = NULL;
    }
  
  if (this->Logs)
    {
    delete [] this->Logs;
    }

  this->Logs = newLogs;
  this->NumberOfLogs = num;
} 



//----------------------------------------------------------------------------
// This ignores the object, and gets the log from the timer.
void vtkPVTimerInformation::CopyFromObject(vtkObject* o)
{
  ostrstream *fptr;
  int length;
  char *str;
  vtkPVApplication* pvApp;
  float threshold = 0.001;

  pvApp = vtkPVApplication::SafeDownCast(o);
  if (pvApp)
    {
    threshold = pvApp->GetLogThreshold();
    }
  
  length = vtkTimerLog::GetNumberOfEvents() * 40;
  if (length > 0)
    {
    str = new char [length];
    fptr = new ostrstream(str, length);

    if (fptr->fail())
      {
      vtkErrorMacro(<< "Unable to string stream");
      return;
      }
    else
      {
      //*fptr << "Hello world !!!\n ()";
      vtkTimerLog::DumpLogWithIndents(fptr, threshold);

      length = fptr->pcount();
      str[length] = '\0';

      delete fptr;
      
      }
    this->InsertLog(0, str);
    }  
}

//----------------------------------------------------------------------------
void vtkPVTimerInformation::CopyFromMessage(unsigned char* msg)
{
  int endianMarker;
  int length, num, idx;

  memcpy((unsigned char*)&endianMarker, msg, sizeof(int));
  if (endianMarker != 1)
    {
    // Mismatch endian between client and server.
    vtkByteSwap::SwapVoidRange((void*)msg, 2, sizeof(int));
    memcpy((unsigned char*)&endianMarker, msg, sizeof(int));
    if (endianMarker != 1)
      {
      vtkErrorMacro("Could not decode information.");
      return;
      }
    }
  msg += sizeof(int);
  memcpy((unsigned char*)&num, msg, sizeof(int));
  msg += sizeof(int);

  // now get the logs.
  for (idx = 0; idx < num; ++idx)
    {
    char* log;
    length = strlen((const char*)msg);
    log = new char[length+1];
    memcpy(log, msg, length+1);
    this->InsertLog(idx, log);
    log = NULL;
    msg += length+1;
    }
}

//----------------------------------------------------------------------------
void vtkPVTimerInformation::AddInformation(vtkPVInformation* info)
{
  int oldNum;
  int num, idx;
  vtkPVTimerInformation* pdInfo;
  char* log;
  int length;
  char* copyLog;

  pdInfo = vtkPVTimerInformation::SafeDownCast(info);

  oldNum = this->NumberOfLogs;
  num = pdInfo->GetNumberOfLogs();
  if (num <= 0)
    {
    return;
    }

  for (idx = 0; idx < num; ++idx)
    {
    log = pdInfo->GetLog(idx);
    if (log)
      {
      length = strlen(log);
      copyLog = new char[length+1];
      memcpy(copyLog, log, length+1);
      this->InsertLog((idx+oldNum), copyLog);
      copyLog = NULL;
      }
    }
}

void vtkPVTimerInformation::CopyToStream(vtkClientServerStream* css) const
{ 
  css->Reset();
  *css << vtkClientServerStream::Reply
       << this->NumberOfLogs;
  int idx;
  for (idx = 0; idx < this->NumberOfLogs; ++idx)
    {
    *css << (const char*)this->Logs[idx];
    }
  *css << vtkClientServerStream::End;
}


//----------------------------------------------------------------------------
void
vtkPVTimerInformation::CopyFromStream(const vtkClientServerStream* css)
{ 
  int idx;
  for (idx = 0; idx < this->NumberOfLogs; ++idx)
    {
    delete []this->Logs[idx];
    }
    
  if(!css->GetArgument(0, 0, &this->NumberOfLogs))
    {
    vtkErrorMacro("Error NumberOfLogs from message.");
    return;
    }
  this->Reallocate(this->NumberOfLogs);
  for (idx = 0; idx < this->NumberOfLogs; ++idx)
    {
    char* log;
    if(!css->GetArgument(0, idx+1, &log))
      {
      vtkErrorMacro("Error parsing LOD geometry memory size from message.");
      return;
      }
    this->Logs[idx] = strcpy(new char[strlen(log)+1], log);
    }
}


//----------------------------------------------------------------------------
int vtkPVTimerInformation::GetNumberOfLogs()
{
  return this->NumberOfLogs;
}

//----------------------------------------------------------------------------
char* vtkPVTimerInformation::GetLog(int idx)
{
  if (idx < 0 || idx >= this->NumberOfLogs)
    {
    return NULL;
    }
  return this->Logs[idx];
}

//----------------------------------------------------------------------------
void vtkPVTimerInformation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "NumberOfLogs: " << this->NumberOfLogs << endl;
  int idx;
  for (idx = 0; idx < this->NumberOfLogs; ++idx)
    {
    os << indent << "Log " << idx << ": \n";
    if (this->Logs[idx])
      {
      os << this->Logs[idx] << endl;
      }
    else
      {
      os << "NULL\n";
      }
    }
}

  



