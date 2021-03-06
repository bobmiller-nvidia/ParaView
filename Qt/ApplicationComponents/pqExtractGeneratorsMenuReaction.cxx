/*=========================================================================

   Program: ParaView
   Module:  pqExtractGeneratorsMenuReaction.cxx

   Copyright (c) 2005,2006 Sandia Corporation, Kitware Inc.
   All rights reserved.

   ParaView is a free software; you can redistribute it and/or modify it
   under the terms of the ParaView license version 1.2.

   See License_v1.2.txt for the full ParaView license.
   A copy of this license can be obtained by contacting
   Kitware Inc.
   28 Corporate Drive
   Clifton Park, NY 12065
   USA

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

========================================================================*/
#include "pqExtractGeneratorsMenuReaction.h"

#include "pqActiveObjects.h"
#include "pqApplicationCore.h"
#include "pqExtractGenerator.h"
#include "pqOutputPort.h"
#include "pqPipelineSource.h"
#include "pqProxyGroupMenuManager.h"
#include "pqProxySelection.h"
#include "pqServerManagerModel.h"
#include "pqUndoStack.h"
#include "pqView.h"
#include "vtkNew.h"
#include "vtkSMDocumentation.h"
#include "vtkSMExtractsController.h"
#include "vtkSMOutputPort.h"
#include "vtkSMProxySelectionModel.h"
#include "vtkSMSessionProxyManager.h"

#include <QMenu>

//-----------------------------------------------------------------------------
pqExtractGeneratorsMenuReaction::pqExtractGeneratorsMenuReaction(
  pqProxyGroupMenuManager* parentObject, bool hideDisabledActions)
  : Superclass(parentObject)
  , HideDisabledActions(hideDisabledActions)
{
  this->Timer.setInterval(10);
  this->Timer.setSingleShot(true);

  auto& activeObjects = pqActiveObjects::instance();
  this->Timer.connect(&activeObjects, SIGNAL(serverChanged(pqServer*)), SLOT(start()));
  this->Timer.connect(&activeObjects, SIGNAL(dataUpdated()), SLOT(start()));
  this->Timer.connect(&activeObjects, SIGNAL(portChanged(pqOutputPort*)), SLOT(start()));
  this->Timer.connect(&activeObjects, SIGNAL(viewChanged(pqView*)), SLOT(start()));
  this->Timer.connect(
    pqApplicationCore::instance(), SIGNAL(forceFilterMenuRefresh()), SLOT(start()));
  this->connect(parentObject->menu(), SIGNAL(aboutToShow()), SLOT(updateEnableState()));

  this->connect(parentObject, SIGNAL(triggered(const QString&, const QString&)),
    SLOT(createExtractGenerator(const QString&, const QString&)));
}

//-----------------------------------------------------------------------------
pqExtractGeneratorsMenuReaction::~pqExtractGeneratorsMenuReaction()
{
}

//-----------------------------------------------------------------------------
void pqExtractGeneratorsMenuReaction::updateEnableState(bool)
{
  auto& activeObjects = pqActiveObjects::instance();
  pqServer* server = activeObjects.activeServer();
  bool enabled = (server != nullptr && server->isMaster());

  // Make sure we already have a selection model
  auto selModel = pqActiveObjects::instance().activeSourcesSelectionModel();
  enabled = enabled && (selModel != nullptr);

  pqProxySelection selection;
  pqProxySelectionUtilities::copy(selModel, selection);

  std::vector<vtkSMProxy*> ports;
  for (const auto& item : selection)
  {
    auto port = qobject_cast<pqOutputPort*>(item);
    auto source = qobject_cast<pqPipelineSource*>(item);
    if (port)
    {
      source = port->getSource();
    }
    else if (source)
    {
      port = source->getOutputPort(0);
    }

    if (source && source->modifiedState() != pqProxy::UNINITIALIZED && port)
    {
      ports.push_back(port->getOutputPortProxy());
    }
  }

  auto view = (activeObjects.activeView() ? activeObjects.activeView()->getProxy() : nullptr);

  vtkNew<vtkSMExtractsController> controller;
  auto manager = qobject_cast<pqProxyGroupMenuManager*>(this->parent());
  Q_ASSERT(manager != nullptr);

  auto actionList = manager->actions();
  for (auto& actn : actionList)
  {
    auto prototype = manager->getPrototype(actn);
    if (prototype == nullptr || !enabled)
    {
      actn->setEnabled(false);
      actn->setVisible(this->HideDisabledActions ? false : true);
      actn->setStatusTip("Requires an input");
    }
    else if (controller->CanExtract(prototype, ports) || controller->CanExtract(prototype, view))
    {
      actn->setEnabled(true);
      actn->setVisible(true);
      actn->setStatusTip(prototype->GetDocumentation()->GetShortHelp());
    }
    else
    {
      actn->setEnabled(false);
      actn->setVisible(this->HideDisabledActions ? false : true);
      // FIXME: get test from domains.
      // actn->setStatusTip("Requires an input");
    }
  }
}

//-----------------------------------------------------------------------------
pqExtractGenerator* pqExtractGeneratorsMenuReaction::createExtractGenerator(
  const QString& group, const QString& name) const
{
  auto& activeObjects = pqActiveObjects::instance();
  auto pxm = activeObjects.proxyManager();
  if (!pxm)
  {
    return nullptr;
  }

  auto view = activeObjects.activeView() ? activeObjects.activeView()->getProxy() : nullptr;
  auto port =
    activeObjects.activePort() ? activeObjects.activePort()->getOutputPortProxy() : nullptr;
  auto prototype = pxm->GetPrototypeProxy(group.toLocal8Bit().data(), name.toLocal8Bit().data());

  vtkNew<vtkSMExtractsController> controller;
  vtkSMProxy* input = nullptr;
  if (controller->CanExtract(prototype, port))
  {
    input = port;
  }
  else if (controller->CanExtract(prototype, view))
  {
    input = view;
  }
  else
  {
    return nullptr;
  }

  BEGIN_UNDO_SET(QString("Create Extract Generator '%1'").arg(name));
  auto generator = controller->CreateExtractGenerator(input, name.toLocal8Bit());
  END_UNDO_SET();
  auto smmodel = pqApplicationCore::instance()->getServerManagerModel();
  return generator ? smmodel->findItem<pqExtractGenerator*>(generator) : nullptr;
}
