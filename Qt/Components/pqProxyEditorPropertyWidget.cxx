/*=========================================================================

   Program: ParaView
   Module:  pqProxyEditorPropertyWidget.cxx

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
#include "pqProxyEditorPropertyWidget.h"

#include "pqPropertyLinks.h"
#include "pqProxyWidgetDialog.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QtDebug>

//-----------------------------------------------------------------------------
pqProxyEditorPropertyWidget::pqProxyEditorPropertyWidget(
  vtkSMProxy *smproxy, vtkSMProperty* smproperty, QWidget *parentObject)
: Superclass(smproxy, parentObject)
{
  this->setShowLabel(false);

  QPushButton *button = new QPushButton(
    QString("Edit %1 ...").arg(smproperty->GetXMLLabel()), this);
  button->setObjectName("PushButton");
  this->connect(button, SIGNAL(clicked()), SLOT(buttonClicked()));
  button->setEnabled(false);
  this->Button = button;

  QHBoxLayout *hbox= new QHBoxLayout(this);
  hbox->setMargin(0);
  hbox->addWidget(button);

  this->addPropertyLink(this, "proxyToEdit", SIGNAL(dummySignal()), smproperty);

  PV_DEBUG_PANELS() << "pqProxyEditorPropertyWidget for a ProxyProperty.";
}

//-----------------------------------------------------------------------------
pqProxyEditorPropertyWidget::~pqProxyEditorPropertyWidget()
{
  delete this->Editor;
}

//-----------------------------------------------------------------------------
void pqProxyEditorPropertyWidget::setProxyToEdit(pqSMProxy smproxy)
{
  this->ProxyToEdit = smproxy;
  this->Button->setEnabled(smproxy != NULL);
  if (this->Editor && this->Editor->proxy() != smproxy)
    {
    delete this->Editor;
    }
}

//-----------------------------------------------------------------------------
pqSMProxy pqProxyEditorPropertyWidget::proxyToEdit() const
{
  return pqSMProxy(this->ProxyToEdit.GetPointer());
}

//-----------------------------------------------------------------------------
void pqProxyEditorPropertyWidget::buttonClicked()
{
  if (!this->ProxyToEdit.GetPointer())
    {
    qCritical() << "No proxy to edit!";
    return;
    }

  if (this->Editor == NULL)
    {
    this->Editor = new pqProxyWidgetDialog(this->ProxyToEdit.GetPointer(), this);
    this->Editor->setEnableSearchBar(true);
    this->Editor->setSettingsKey(
      QString("pqProxyEditorPropertyWidget.%1.%2")
        .arg(this->proxy()->GetXMLName())
        .arg(this->property()->GetXMLName()));
    this->connect(this->Editor, SIGNAL(accepted()), SIGNAL(changeAvailable()));
    }
  this->Editor->setWindowTitle(this->Button->text());
  this->Editor->setObjectName("EditProxy");
  this->Editor->show();
}
