/*=========================================================================

   Program: ParaView
   Module:    pqXYPlotDisplayProxyEditor.cxx

   Copyright (c) 2005,2006 Sandia Corporation, Kitware Inc.
   All rights reserved.

   ParaView is a free software; you can redistribute it and/or modify it
   under the terms of the ParaView license version 1.1. 

   See License_v1.1.txt for the full ParaView license.
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

=========================================================================*/
#include "pqXYPlotDisplayProxyEditor.h"
#include "ui_pqXYPlotDisplayEditor.h"

#include "vtkSMArraySelectionDomain.h"
#include "vtkSMIntVectorProperty.h"
#include "vtkSMProxy.h"
#include "vtkSMStringVectorProperty.h"
#include "vtkDataObject.h"

#include <QColorDialog>
#include <QHeaderView>
#include <QItemDelegate>
#include <QList>
#include <QPointer>
#include <QPixmap>
#include <QtDebug>

#include "pqCheckableHeaderModel.h"
#include "pqComboBoxDomain.h"
#include "pqLineChartRepresentation.h"
#include "pqPropertyLinks.h"
#include "pqSignalAdaptors.h"
#include "pqSMAdaptor.h"
#include "pqTreeWidgetCheckHelper.h"
#include "pqTreeWidgetItemObject.h"


//-----------------------------------------------------------------------------
class pqLineSeriesEditorModelItem
{
public:
  pqLineSeriesEditorModelItem(const QString &variable, const QString &legend,
      const QColor &color);
  ~pqLineSeriesEditorModelItem() {}

  void setColor(const QColor &color);

  QString Variable;
  QString LegendName;
  QPixmap LineColor;
  bool Enabled;
};


//-----------------------------------------------------------------------------
class pqLineSeriesEditorModel : public pqCheckableHeaderModel
{
public:
  pqLineSeriesEditorModel(pqLineChartRepresentation *display,
      QObject *parent=0);
  virtual ~pqLineSeriesEditorModel();

  /// \name QAbstractItemModel Methods
  //@{
  /// \brief
  ///   Gets the number of rows for a given index.
  /// \param parent The parent index.
  /// \return
  ///   The number of rows for the given index.
  virtual int rowCount(const QModelIndex &parent=QModelIndex()) const;

  /// \brief
  ///   Gets the number of columns for a given index.
  /// \param parent The parent index.
  /// \return
  ///   The number of columns for the given index.
  virtual int columnCount(const QModelIndex &parent=QModelIndex()) const;

  /// \brief
  ///   Gets whether or not the given index has child items.
  /// \param parent The parent index.
  /// \return
  ///   True if the given index has child items.
  virtual bool hasChildren(const QModelIndex &parent=QModelIndex()) const;

  /// \brief
  ///   Gets a model index for a given location.
  /// \param row The row number.
  /// \param column The column number.
  /// \param parent The parent index.
  /// \return
  ///   A model index for the given location.
  virtual QModelIndex index(int row, int column,
      const QModelIndex &parent=QModelIndex()) const;

  /// \brief
  ///   Gets the parent for a given index.
  /// \param index The model index.
  /// \return
  ///   A model index for the parent of the given index.
  virtual QModelIndex parent(const QModelIndex &index) const;

  /// \brief
  ///   Gets the data for a given model index.
  /// \param index The model index.
  /// \param role The role to get data for.
  /// \return
  ///   The data for the given model index.
  virtual QVariant data(const QModelIndex &index,
      int role=Qt::DisplayRole) const;

  /// \brief
  ///   Gets the flags for a given model index.
  ///
  /// The flags for an item indicate if it is enabled, editable, etc.
  ///
  /// \param index The model index.
  /// \return
  ///   The flags for the given model index.
  virtual Qt::ItemFlags flags(const QModelIndex &index) const;

  /// \brief
  ///   Sets the data for the given model index.
  /// \param index The model index.
  /// \param value The new data for the given role.
  /// \param role The role to set data for.
  /// \return
  ///   True if the data was changed successfully.
  virtual bool setData(const QModelIndex &index, const QVariant &value, 
      int role=Qt::EditRole);

  virtual QVariant headerData(int section, Qt::Orientation orient,
      int role=Qt::DisplayRole) const;
  //@}

  void setDisplay(pqLineChartRepresentation *display);
  void reloadSeries();

  void setSeriesEnabled(int row, bool enabled);
  void setSeriesColor(int row, const QColor &color);

private:
  void cleanupItems();

private:
  QList<pqLineSeriesEditorModelItem *> Items;
  QPointer<pqLineChartRepresentation> Display;
};


//-----------------------------------------------------------------------------
class pqLineSeriesEditorDelegate : public QItemDelegate
{
public:
  pqLineSeriesEditorDelegate(QObject *parent=0);
  virtual ~pqLineSeriesEditorDelegate() {}

protected:
  virtual void drawDecoration(QPainter *painter,
      const QStyleOptionViewItem &options, const QRect &area,
      const QPixmap &pixmap) const;
};


//-----------------------------------------------------------------------------
class pqXYPlotDisplayProxyEditor::pqInternal : public Ui::pqXYPlotDisplayEditor
{
public:
  pqInternal()
    {
    this->XAxisArrayDomain = 0;
    this->XAxisArrayAdaptor = 0;
    this->AttributeModeAdaptor = 0;
    this->Model = 0;
    this->InChange = false;
    }

  ~pqInternal()
    {
    delete this->XAxisArrayAdaptor;
    delete this->XAxisArrayDomain;
    delete this->AttributeModeAdaptor;
    }

  pqPropertyLinks Links;
  pqSignalAdaptorComboBox* XAxisArrayAdaptor;
  pqSignalAdaptorComboBox* AttributeModeAdaptor;
  pqComboBoxDomain* XAxisArrayDomain;

  QPointer<pqLineChartRepresentation> Display;
  pqLineSeriesEditorModel *Model;

  bool InChange;
};


//-----------------------------------------------------------------------------
pqLineSeriesEditorModelItem::pqLineSeriesEditorModelItem(
    const QString &variable, const QString &legend, const QColor &color)
  : Variable(variable), LegendName(legend), LineColor(16, 16)
{
  this->Enabled = false;

  this->setColor(color);
}

void pqLineSeriesEditorModelItem::setColor(const QColor &color)
{
  this->LineColor.fill(color);
}


//-----------------------------------------------------------------------------
pqLineSeriesEditorModel::pqLineSeriesEditorModel(
    pqLineChartRepresentation *display, QObject *parentObject)
  : pqCheckableHeaderModel(parentObject), Items(), Display(display)
{
  // Set up the column headers.
  this->insertHeaderSections(Qt::Horizontal, 0, 1);
  this->setCheckable(0, Qt::Horizontal, true);
  this->setCheckState(0, Qt::Horizontal, Qt::Unchecked);

  // Change the index check state when the header checkbox is clicked.
  this->connect(this, SIGNAL(headerDataChanged(Qt::Orientation, int, int)),
      this, SLOT(setIndexCheckState(Qt::Orientation, int, int)));
}

pqLineSeriesEditorModel::~pqLineSeriesEditorModel()
{
  this->cleanupItems();
}

int pqLineSeriesEditorModel::rowCount(const QModelIndex &parentIndex) const
{
  if(!parentIndex.isValid())
    {
    return this->Items.size();
    }

  return 0;
}

int pqLineSeriesEditorModel::columnCount(const QModelIndex &) const
{
  return 2;
}

bool pqLineSeriesEditorModel::hasChildren(const QModelIndex &parentIndex) const
{
  return !parentIndex.isValid() && this->Items.size() > 0;
}

QModelIndex pqLineSeriesEditorModel::index(int row, int column,
    const QModelIndex &parentIndex) const
{
  if(!parentIndex.isValid() && column >= 0 && column < 2 && row >= 0 &&
      row < this->Items.size())
    {
    return this->createIndex(row, column);
    }

  return QModelIndex();
}

QModelIndex pqLineSeriesEditorModel::parent(const QModelIndex &) const
{
  return QModelIndex();
}

QVariant pqLineSeriesEditorModel::data(const QModelIndex &idx, int role) const
{
  if(idx.isValid() && idx.model() == this)
    {
    if(role == Qt::DisplayRole || role == Qt::EditRole ||
        role == Qt::ToolTipRole)
      {
      if(idx.column() == 0)
        {
        return QVariant(this->Items[idx.row()]->Variable);
        }
      else
        {
        return QVariant(this->Items[idx.row()]->LegendName);
        }
      }
    else if(role == Qt::CheckStateRole)
      {
      if(idx.column() == 0)
        {
        return QVariant(
            this->Items[idx.row()]->Enabled ? Qt::Checked : Qt::Unchecked);
        }
      }
    else if(role == Qt::DecorationRole)
      {
      if(idx.column() == 1)
        {
        return QVariant(this->Items[idx.row()]->LineColor);
        }
      }
    }

  return QVariant();
}

Qt::ItemFlags pqLineSeriesEditorModel::flags(const QModelIndex &idx) const
{
  Qt::ItemFlags result = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
  if(idx.isValid() && idx.model() == this)
    {
    if(idx.column() == 0)
      {
      result |= Qt::ItemIsUserCheckable;
      }
    else if(idx.column() == 1)
      {
      result |= Qt::ItemIsEditable;
      }
    }

  return result;
}

bool pqLineSeriesEditorModel::setData(const QModelIndex &idx,
    const QVariant &value, int role)
{
  bool result = false;
  if(idx.isValid() && idx.model() == this)
    {
    pqLineSeriesEditorModelItem *item = this->Items[idx.row()];
    if(idx.column() == 1 && (role == Qt::DisplayRole || role == Qt::EditRole))
      {
      QString name = value.toString();
      if(!name.isEmpty())
        {
        if(name != item->LegendName)
          {
          item->LegendName = name;
          int series = this->Display->getSeriesIndex(item->Variable);
          this->Display->setSeriesLabel(series, item->LegendName);
          this->Display->renderViewEventually();
          emit this->dataChanged(idx, idx);
          }

        result = true;
        }
      }
    else if(idx.column() == 0 && role == Qt::CheckStateRole)
      {
      int state = value.toInt();
      bool changed = false;
      if(state == Qt::Checked)
        {
        changed = !item->Enabled;
        item->Enabled = true;
        result = true;
        }
      else if(state == Qt::Unchecked)
        {
        changed = item->Enabled;
        item->Enabled = false;
        result = true;
        }

      if(result && changed)
        {
        int series = this->Display->getSeriesIndex(item->Variable);
        this->Display->setSeriesEnabled(series, item->Enabled);
        this->Display->renderViewEventually();
        emit this->dataChanged(idx, idx);
        this->updateCheckState(0, Qt::Horizontal);
        }
      }
    }

  if(result)
    {
    emit this->dataChanged(idx, idx);
    }

  return result;
}

QVariant pqLineSeriesEditorModel::headerData(int section,
    Qt::Orientation orient, int role) const
{
  if(orient == Qt::Horizontal && role == Qt::DisplayRole)
    {
    if(section == 0)
      {
      return QVariant(QString("Variable"));
      }
    else if(section == 1)
      {
      return QVariant(QString("Legend Name"));
      }
    }
  else
    {
    return pqCheckableHeaderModel::headerData(section, orient, role);
    }

  return QVariant();
}

void pqLineSeriesEditorModel::setDisplay(pqLineChartRepresentation *display)
{
  this->Display = display;
}

void pqLineSeriesEditorModel::reloadSeries()
{
  this->cleanupItems();
  this->Items.clear();
  if(!this->Display.isNull())
    {
    int total = this->Display->getNumberOfSeries();
    for(int i = 0; i < total; i++)
      {
      QColor seriesColor;
      QString seriesName, seriesLabel;
      this->Display->getSeriesName(i, seriesName);
      this->Display->getSeriesLabel(i, seriesLabel);
      this->Display->getSeriesColor(i, seriesColor);
      pqLineSeriesEditorModelItem *item = new pqLineSeriesEditorModelItem(
          seriesName, seriesLabel, seriesColor);
      item->Enabled = this->Display->isSeriesEnabled(i);
      this->Items.append(item);
      }
    }

  this->reset();
  this->updateCheckState(0, Qt::Horizontal);
}

void pqLineSeriesEditorModel::setSeriesEnabled(int row, bool enabled)
{
  if(row >= 0 && row < this->Items.size())
    {
    pqLineSeriesEditorModelItem *item = this->Items[row];
    if(item->Enabled != enabled)
      {
      item->Enabled = enabled;
      QModelIndex idx = this->createIndex(row, 0);
      emit this->dataChanged(idx, idx);
      this->updateCheckState(0, Qt::Horizontal);
      }
    }
}

void pqLineSeriesEditorModel::setSeriesColor(int row, const QColor &color)
{
  if(row >= 0 && row < this->Items.size())
    {
    pqLineSeriesEditorModelItem *item = this->Items[row];
    item->setColor(color);
    QModelIndex idx = this->createIndex(row, 1);
    emit this->dataChanged(idx, idx);
    }
}

void pqLineSeriesEditorModel::cleanupItems()
{
  QList<pqLineSeriesEditorModelItem *>::Iterator iter = this->Items.begin();
  for( ; iter != this->Items.end(); ++iter)
    {
    delete *iter;
    }
}


//-----------------------------------------------------------------------------
pqLineSeriesEditorDelegate::pqLineSeriesEditorDelegate(QObject *parentObject)
  : QItemDelegate(parentObject)
{
}

void pqLineSeriesEditorDelegate::drawDecoration(QPainter *painter,
    const QStyleOptionViewItem &options, const QRect &area,
    const QPixmap &pixmap) const
{
  // Remove the selected flag from the state to make sure the pixmap
  // color is not modified.
  QStyleOptionViewItem newOptions = options;
  newOptions.state = options.state & ~QStyle::State_Selected;
  QItemDelegate::drawDecoration(painter, newOptions, area, pixmap);
}


//-----------------------------------------------------------------------------
pqXYPlotDisplayProxyEditor::pqXYPlotDisplayProxyEditor(pqRepresentation* display, QWidget* p)
  : pqDisplayPanel(display, p)
{
  this->Internal = new pqXYPlotDisplayProxyEditor::pqInternal();
  this->Internal->setupUi(this);

  this->Internal->SeriesList->setItemDelegate(
      new pqLineSeriesEditorDelegate(this));
  this->Internal->Model = new pqLineSeriesEditorModel(0, this);
  this->Internal->SeriesList->setModel(this->Internal->Model);
  this->Internal->SeriesList->header()->setResizeMode(QHeaderView::Stretch);

  QObject::connect(
    this->Internal->SeriesList, SIGNAL(activated(const QModelIndex &)),
    this, SLOT(activateItem(const QModelIndex &)));

  this->Internal->XAxisArrayAdaptor = new pqSignalAdaptorComboBox(
    this->Internal->XAxisArray);

  this->Internal->AttributeModeAdaptor = new pqSignalAdaptorComboBox(
    this->Internal->AttributeMode);

  QObject::connect(this->Internal->UseArrayIndex, SIGNAL(toggled(bool)), 
    this, SLOT(updateAllViews()), Qt::QueuedConnection);

  QObject::connect(this->Internal->XAxisArrayAdaptor,
    SIGNAL(currentTextChanged(const QString&)), 
    this, SLOT(updateAllViews()),
    Qt::QueuedConnection);

  QObject::connect(this->Internal->AttributeModeAdaptor,
    SIGNAL(currentTextChanged(const QString&)), 
    this, SLOT(onAttributeModeChanged()),
    Qt::QueuedConnection);

  QObject::connect(this->Internal->ViewData, SIGNAL(stateChanged(int)),
    this, SLOT(updateAllViews()), Qt::QueuedConnection);

  QItemSelectionModel *model = this->Internal->SeriesList->selectionModel();
  QObject::connect(model,
    SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)),
    this, SLOT(updateOptionsWidgets()));
  QObject::connect(model,
    SIGNAL(currentChanged(const QModelIndex &, const QModelIndex &)),
    this, SLOT(updateOptionsWidgets()));

  QObject::connect(this->Internal->SeriesEnabled, SIGNAL(stateChanged(int)),
    this, SLOT(setCurrentSeriesEnabled(int)));
  QObject::connect(
    this->Internal->ColorButton, SIGNAL(chosenColorChanged(const QColor &)),
    this, SLOT(setCurrentSeriesColor(const QColor &)));
  QObject::connect(this->Internal->Thickness, SIGNAL(valueChanged(int)),
    this, SLOT(setCurrentSeriesThickness(int)));
  QObject::connect(this->Internal->StyleList, SIGNAL(currentIndexChanged(int)),
    this, SLOT(setCurrentSeriesStyle(int)));
  QObject::connect(this->Internal->AxisList, SIGNAL(currentIndexChanged(int)),
    this, SLOT(setCurrentSeriesAxes(int)));

  this->setDisplay(display);
}

//-----------------------------------------------------------------------------
pqXYPlotDisplayProxyEditor::~pqXYPlotDisplayProxyEditor()
{
  delete this->Internal;
}

//-----------------------------------------------------------------------------
void pqXYPlotDisplayProxyEditor::reloadSeries()
{
  this->Internal->Model->reloadSeries();
  this->updateOptionsWidgets();
}

//-----------------------------------------------------------------------------
void pqXYPlotDisplayProxyEditor::setDisplay(pqRepresentation* disp)
{
  pqLineChartRepresentation* display = qobject_cast<pqLineChartRepresentation*>(disp);
  if (this->Internal->Display == display)
    {
    return;
    }

  this->setEnabled(false);

  // Clean up stuff setup during previous call to setDisplay.
  this->Internal->Links.removeAllPropertyLinks();
  this->Internal->Model->setDisplay(0);
  this->Internal->Model->reloadSeries();
  delete this->Internal->XAxisArrayDomain;
  this->Internal->XAxisArrayDomain = 0;
  if(this->Internal->Display)
    {
    this->disconnect(this->Internal->Display, 0, this, 0);
    }

  this->Internal->Display = display;
  this->Internal->Model->setDisplay(display);
  if (!this->Internal->Display)
    {
    // Display is null, nothing to do.
    return;
    }

  vtkSMProxy* proxy = display->getProxy();

  if (!proxy || proxy->GetXMLName() != QString("XYPlotRepresentation"))
    {
    qDebug() << "Proxy must be a XYPlotRepresentation display to be editable in "
      "pqXYPlotDisplayProxyEditor.";
    return;
    }
  this->setEnabled(true);

  // Setup links for visibility.
  this->Internal->Links.addPropertyLink(this->Internal->ViewData,
    "checked", SIGNAL(stateChanged(int)),
    proxy, proxy->GetProperty("Visibility"));

  // Setup UseYArrayIndex links.
  this->Internal->Links.addPropertyLink(this->Internal->UseArrayIndex,
    "checked", SIGNAL(toggled(bool)),
    proxy, proxy->GetProperty("UseYArrayIndex"));

  // Attribute mode.
  this->Internal->Links.addPropertyLink(this->Internal->AttributeModeAdaptor,
    "currentText", SIGNAL(currentTextChanged(const QString&)),
    proxy, proxy->GetProperty("AttributeType"));

  // pqComboBoxDomain will ensure that when ever the domain changes the
  // widget is updated as well.
  this->Internal->XAxisArrayDomain = new pqComboBoxDomain(
    this->Internal->XAxisArray, proxy->GetProperty("XArrayName"));
  // This is useful to initially populate the combobox.
  this->Internal->XAxisArrayDomain->forceDomainChanged();

  // This link will ensure that when ever the widget selection changes,
  // the property XArrayName will be updated as well.
  this->Internal->Links.addPropertyLink(this->Internal->XAxisArrayAdaptor,
    "currentText", SIGNAL(currentTextChanged(const QString&)),
    proxy, proxy->GetProperty("XArrayName"));

  this->connect(this->Internal->Display, SIGNAL(seriesListChanged()),
      this, SLOT(reloadSeries()));
  this->connect(
      this->Internal->Display, SIGNAL(enabledStateChanged(int, bool)),
      this, SLOT(updateItemEnabled(int)));
  this->connect(
      this->Internal->Display, SIGNAL(colorChanged(int, const QColor &)),
      this, SLOT(updateItemColor(int, const QColor &)));
  this->connect(
      this->Internal->Display, SIGNAL(styleChanged(int, Qt::PenStyle)),
      this, SLOT(updateItemStyle(int, Qt::PenStyle)));

  this->reloadSeries();
}

//-----------------------------------------------------------------------------
void pqXYPlotDisplayProxyEditor::onAttributeModeChanged()
{
  vtkSMProxy* proxy = this->Internal->Display->getProxy();
  vtkSMIntVectorProperty* at = vtkSMIntVectorProperty::SafeDownCast(
    proxy->GetProperty("AttributeType"));
  // FIXME HACK: The domain uses unchecked elements to update the values,
  // hence  we update the unchecked element.
  at->SetUncheckedElement(0, at->GetElement(0));
  proxy->GetProperty("AttributeType")->UpdateDependentDomains();

  this->updateAllViews();
}

//-----------------------------------------------------------------------------
void pqXYPlotDisplayProxyEditor::activateItem(const QModelIndex &index)
{
  if(!this->Internal->Display || !index.isValid() || index.column() != 1)
    {
    // We are interested in clicks on the color swab alone.
    return;
    }

  QColor color;
  this->Internal->Display->getSeriesColor(index.row(), color);
  color = QColorDialog::getColor(color, this);
  if (color.isValid())
    {
    this->Internal->Display->setSeriesColor(index.row(), color);
    this->updateAllViews();
    }
}

void pqXYPlotDisplayProxyEditor::updateOptionsWidgets()
{
  QItemSelectionModel *model = this->Internal->SeriesList->selectionModel();
  if(model)
    {
    // Use the selection list to determine the tri-state of the
    // enabled and legend check boxes.
    this->Internal->SeriesEnabled->blockSignals(true);
    this->Internal->SeriesEnabled->setCheckState(this->getEnabledState());
    this->Internal->SeriesEnabled->blockSignals(false);

    // Show the options for the current item.
    QModelIndex current = model->currentIndex();
    QModelIndexList indexes = model->selectedIndexes();
    if((!current.isValid() || !model->isSelected(current)) &&
        indexes.size() > 0)
      {
      current = indexes.last();
      }

    this->Internal->ColorButton->blockSignals(true);
    this->Internal->Thickness->blockSignals(true);
    this->Internal->StyleList->blockSignals(true);
    this->Internal->AxisList->blockSignals(true);
    if(current.isValid())
      {
      QColor color;
      this->Internal->Display->getSeriesColor(current.row(), color);
      this->Internal->ColorButton->setChosenColor(color);
      this->Internal->Thickness->setValue(
          this->Internal->Display->getSeriesThickness(current.row()));
      this->Internal->StyleList->setCurrentIndex(
          (int)this->Internal->Display->getSeriesStyle(current.row()) - 1);
      this->Internal->AxisList->setCurrentIndex(
          this->Internal->Display->getSeriesAxesIndex(current.row()));
      }
    else
      {
      this->Internal->ColorButton->setChosenColor(Qt::white);
      this->Internal->Thickness->setValue(1);
      this->Internal->StyleList->setCurrentIndex(0);
      this->Internal->AxisList->setCurrentIndex(0);
      }

    this->Internal->ColorButton->blockSignals(false);
    this->Internal->Thickness->blockSignals(false);
    this->Internal->StyleList->blockSignals(false);
    this->Internal->AxisList->blockSignals(false);

    // Disable the widgets if nothing is selected or current.
    bool hasItems = indexes.size() > 0;
    this->Internal->SeriesEnabled->setEnabled(hasItems);
    this->Internal->ColorButton->setEnabled(hasItems);
    this->Internal->Thickness->setEnabled(hasItems);
    this->Internal->StyleList->setEnabled(hasItems);
    this->Internal->AxisList->setEnabled(hasItems);
    }
}

void pqXYPlotDisplayProxyEditor::setCurrentSeriesEnabled(int state)
{
  if(state == Qt::PartiallyChecked)
    {
    // Ignore changes to partially checked state.
    return;
    }

  bool enabled = state == Qt::Checked;
  this->Internal->SeriesEnabled->setTristate(false);
  QItemSelectionModel *model = this->Internal->SeriesList->selectionModel();
  if(model)
    {
    this->Internal->Display->beginSeriesChanges();
    this->Internal->InChange = true;
    QModelIndexList indexes = model->selectedIndexes();
    QModelIndexList::Iterator iter = indexes.begin();
    for( ; iter != indexes.end(); ++iter)
      {
      this->Internal->Display->setSeriesEnabled(iter->row(), enabled);
      this->Internal->Model->setSeriesEnabled(iter->row(), enabled);
      }

    this->Internal->InChange = false;
    this->Internal->Display->endSeriesChanges();
    this->updateAllViews();
    }
}

void pqXYPlotDisplayProxyEditor::setCurrentSeriesColor(const QColor &color)
{
  QItemSelectionModel *model = this->Internal->SeriesList->selectionModel();
  if(model)
    {
    this->Internal->Display->beginSeriesChanges();
    this->Internal->InChange = true;
    QModelIndexList indexes = model->selectedIndexes();
    QModelIndexList::Iterator iter = indexes.begin();
    for( ; iter != indexes.end(); ++iter)
      {
      this->Internal->Display->setSeriesColor(iter->row(), color);
      this->Internal->Model->setSeriesColor(iter->row(), color);
      }

    this->Internal->InChange = false;
    this->Internal->Display->endSeriesChanges();
    this->updateAllViews();
    }
}

void pqXYPlotDisplayProxyEditor::setCurrentSeriesThickness(int thickness)
{
  QItemSelectionModel *model = this->Internal->SeriesList->selectionModel();
  if(model)
    {
    this->Internal->Display->beginSeriesChanges();
    this->Internal->InChange = true;
    QModelIndexList indexes = model->selectedIndexes();
    QModelIndexList::Iterator iter = indexes.begin();
    for( ; iter != indexes.end(); ++iter)
      {
      this->Internal->Display->setSeriesThickness(iter->row(), thickness);
      }

    this->Internal->InChange = false;
    this->Internal->Display->endSeriesChanges();
    this->updateAllViews();
    }
}

void pqXYPlotDisplayProxyEditor::setCurrentSeriesStyle(int listIndex)
{
  QItemSelectionModel *model = this->Internal->SeriesList->selectionModel();
  if(model)
    {
    this->Internal->Display->beginSeriesChanges();
    this->Internal->InChange = true;
    Qt::PenStyle lineStyle = (Qt::PenStyle)(listIndex + 1);
    QModelIndexList indexes = model->selectedIndexes();
    QModelIndexList::Iterator iter = indexes.begin();
    for( ; iter != indexes.end(); ++iter)
      {
      this->Internal->Display->setSeriesStyle(iter->row(), lineStyle);
      }

    this->Internal->InChange = false;
    this->Internal->Display->endSeriesChanges();
    this->updateAllViews();
    }
}

void pqXYPlotDisplayProxyEditor::setCurrentSeriesAxes(int listIndex)
{
  QItemSelectionModel *model = this->Internal->SeriesList->selectionModel();
  if(model)
    {
    this->Internal->Display->beginSeriesChanges();
    this->Internal->InChange = true;
    QModelIndexList indexes = model->selectedIndexes();
    QModelIndexList::Iterator iter = indexes.begin();
    for( ; iter != indexes.end(); ++iter)
      {
      this->Internal->Display->setSeriesAxesIndex(iter->row(), listIndex);
      }

    this->Internal->InChange = false;
    this->Internal->Display->endSeriesChanges();
    this->updateAllViews();
    }
}

void pqXYPlotDisplayProxyEditor::updateItemEnabled(int index)
{
  if(this->Internal->InChange)
    {
    return;
    }

  // If the index is part of the selection, update the enabled check box.
  QModelIndex changed = this->Internal->Model->index(index, 0);
  QItemSelectionModel *model = this->Internal->SeriesList->selectionModel();
  if(model && model->isSelected(changed))
    {
    Qt::CheckState enabledState = this->getEnabledState();
    this->Internal->SeriesEnabled->blockSignals(true);
    this->Internal->SeriesEnabled->setCheckState(enabledState);
    this->Internal->SeriesEnabled->blockSignals(false);
    }
}

void pqXYPlotDisplayProxyEditor::updateItemColor(int index,
    const QColor &color)
{
  if(this->Internal->InChange)
    {
    return;
    }

  // Update the pixmap for the item.
  this->Internal->Model->setSeriesColor(index, color);

  // If the index is the 'current', update the color button.
  QModelIndex changed = this->Internal->Model->index(index, 0);
  QItemSelectionModel *model = this->Internal->SeriesList->selectionModel();
  if(model && model->isSelected(changed))
    {
    QModelIndex current = model->currentIndex();
    if(!current.isValid() || !model->isSelected(current))
      {
      current = model->selectedIndexes().last();
      }

    if(changed.row() == current.row())
      {
      this->Internal->ColorButton->blockSignals(true);
      this->Internal->ColorButton->setChosenColor(color);
      this->Internal->ColorButton->blockSignals(false);
      }
    }
}

void pqXYPlotDisplayProxyEditor::updateItemStyle(int index,
    Qt::PenStyle lineStyle)
{
  if(this->Internal->InChange)
    {
    return;
    }

  // If the index is the 'current', update the style combo box.
  QModelIndex changed = this->Internal->Model->index(index, 0);
  QItemSelectionModel *model = this->Internal->SeriesList->selectionModel();
  if(model && model->isSelected(changed))
    {
    QModelIndex current = model->currentIndex();
    if(!current.isValid() || !model->isSelected(current))
      {
      current = model->selectedIndexes().last();
      }

    if(changed.row() == current.row())
      {
      this->Internal->StyleList->blockSignals(true);
      this->Internal->StyleList->setCurrentIndex((int)lineStyle - 1);
      this->Internal->StyleList->blockSignals(false);
      }
    }
}

Qt::CheckState pqXYPlotDisplayProxyEditor::getEnabledState() const
{
  Qt::CheckState enabledState = Qt::Unchecked;
  QItemSelectionModel *model = this->Internal->SeriesList->selectionModel();
  if(model)
    {
    // Use the selection list to determine the tri-state of the
    // enabled check box.
    bool enabled = false;
    QModelIndexList indexes = model->selectedIndexes();
    QModelIndexList::Iterator iter = indexes.begin();
    for(int i = 0; iter != indexes.end(); ++iter, ++i)
      {
      enabled = this->Internal->Display->isSeriesEnabled(iter->row());
      if(i == 0)
        {
        enabledState = enabled ? Qt::Checked : Qt::Unchecked;
        }
      else if((enabled && enabledState == Qt::Unchecked) ||
          (!enabled && enabledState == Qt::Checked))
        {
        enabledState = Qt::PartiallyChecked;
        break;
        }
      }
    }

  return enabledState;
}


