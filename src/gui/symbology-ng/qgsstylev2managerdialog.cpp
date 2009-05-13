
#include "qgsstylev2managerdialog.h"

#include "qgsstylev2.h"
#include "qgssymbolv2.h"
#include "qgssymbollayerv2utils.h"
#include "qgsvectorcolorrampv2.h"

#include "qgssymbolv2propertiesdialog.h"
#include "qgsvectorgradientcolorrampv2dialog.h"

#include <QFile>
#include <QInputDialog>
#include <QStandardItemModel>

#include "qgsapplication.h"
#include "qgslogger.h"


static QString iconPath(QString iconFile)
{
  // try active theme
  QString path = QgsApplication::activeThemePath();
  if ( QFile::exists( path + iconFile ) )
    return path + iconFile;

  // use default theme
  return QgsApplication::defaultThemePath() + iconFile;
}

///////

QgsStyleV2ManagerDialog::QgsStyleV2ManagerDialog(QgsStyleV2* style, QString styleFilename, QWidget* parent)
  : QDialog(parent), mStyle(style), mStyleFilename(styleFilename)
{

  setupUi(this);

  // setup icons
  btnAddItem->setIcon( QIcon( iconPath( "symbologyAdd.png" ) ) );
  btnEditItem->setIcon( QIcon( iconPath( "symbologyEdit.png" ) ) );
  btnRemoveItem->setIcon( QIcon( iconPath( "symbologyRemove.png" ) ) );

  connect(this, SIGNAL(finished(int)), this, SLOT(onFinished()));
  
  connect(listItems, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(editItem()));
  
  connect(btnAddItem, SIGNAL(clicked()), this, SLOT(addItem()));
  connect(btnEditItem, SIGNAL(clicked()), this, SLOT(editItem()));
  connect(btnRemoveItem, SIGNAL(clicked()), this, SLOT(removeItem()));

  QStandardItemModel* model = new QStandardItemModel(listItems);
  listItems->setModel(model);
  
  populateTypes();
  
  connect(cboItemType, SIGNAL(currentIndexChanged(int)), this, SLOT(populateList()));
  
  populateList();

}

void QgsStyleV2ManagerDialog::onFinished()
{
  // TODO: save only when modified
  if (!mStyleFilename.isEmpty())
    mStyle->save(mStyleFilename);
}

void QgsStyleV2ManagerDialog::populateTypes()
{
  // save current selection index in types combo
  int current = (cboItemType->count() > 0 ? cboItemType->currentIndex() : 0);

  int markerCount = 0, lineCount = 0, fillCount = 0;
  
  QStringList symbolNames = mStyle->symbolNames();
  for (int i = 0; i < symbolNames.count(); ++i)
  {
    switch (mStyle->symbolRef(symbolNames[i])->type())
    {
      case QgsSymbolV2::Marker: markerCount++; break;
      case QgsSymbolV2::Line: lineCount++; break;
      case QgsSymbolV2::Fill: fillCount++; break;
      default: Q_ASSERT(0 && "unknown symbol type"); break;
    }
  }
    
  cboItemType->clear();
  cboItemType->addItem(QString("Marker symbol (%1)").arg(markerCount), QVariant(QgsSymbolV2::Marker));
  cboItemType->addItem(QString("Line symbol (%1)").arg(lineCount), QVariant(QgsSymbolV2::Line));
  cboItemType->addItem(QString("Fill symbol (%1)").arg(fillCount), QVariant(QgsSymbolV2::Fill));
    
  cboItemType->addItem(QString("Color ramp (%1)").arg(mStyle->colorRampCount()), QVariant(3));
  
  // update current index to previous selection
  cboItemType->setCurrentIndex(current);

}

void QgsStyleV2ManagerDialog::populateList()
{
  // get current symbol type
  int itemType = currentItemType();
  
  if (itemType < 3)
    populateSymbols(itemType);
  else
    populateColorRamps();
}

void QgsStyleV2ManagerDialog::populateSymbols(int type)
{
  QStandardItemModel* model = qobject_cast<QStandardItemModel*>(listItems->model());
  model->clear();
  
  QStringList symbolNames = mStyle->symbolNames();
  
  for (int i = 0; i < symbolNames.count(); ++i)
  {
    QString name = symbolNames[i];
    QgsSymbolV2* symbol = mStyle->symbol(name);
    if (symbol->type() == type)
    {
      QStandardItem* item = new QStandardItem(name);
      QIcon icon = QgsSymbolLayerV2Utils::symbolPreviewIcon(symbol, listItems->iconSize());
      item->setIcon(icon);
      // add to model
      model->appendRow(item);
    }
    delete symbol;
  }

}

void QgsStyleV2ManagerDialog::populateColorRamps()
{
  QStandardItemModel* model = qobject_cast<QStandardItemModel*>(listItems->model());
  model->clear();
  
  QStringList colorRamps = mStyle->colorRampNames();
  
  for (int i = 0; i < colorRamps.count(); ++i)
  {
    QString name = colorRamps[i];
    QgsVectorColorRampV2* ramp = mStyle->colorRamp(name);
    
    QStandardItem* item = new QStandardItem(name);
    QIcon icon = QgsSymbolLayerV2Utils::colorRampPreviewIcon(ramp, listItems->iconSize());
    item->setIcon(icon);
    model->appendRow(item);
    delete ramp;
  }
}

int QgsStyleV2ManagerDialog::currentItemType()
{
  int idx = cboItemType->currentIndex();
  return cboItemType->itemData(idx).toInt();
}

QString QgsStyleV2ManagerDialog::currentItemName()
{
  QModelIndex index = listItems->selectionModel()->currentIndex();
  if (!index.isValid())
    return QString();
  return index.model()->data(index, 0).toString();
}

void QgsStyleV2ManagerDialog::addItem()
{
  if (currentItemType() < 3)
    addSymbol();
  else if (currentItemType() == 3)
    addColorRamp();
  else
    Q_ASSERT(0 && "not implemented");
  
  populateList();
  populateTypes();
}

bool QgsStyleV2ManagerDialog::addSymbol()
{
  // create new symbol with current type
  QgsSymbolV2* symbol;
  switch (currentItemType())
  {
    case QgsSymbolV2::Marker: symbol = new QgsMarkerSymbolV2(); break;
    case QgsSymbolV2::Line:   symbol = new QgsLineSymbolV2(); break;
    case QgsSymbolV2::Fill:   symbol = new QgsFillSymbolV2(); break;
    default: Q_ASSERT(0 && "unknown symbol type"); break;
  }
  
  // get symbol design
  QgsSymbolV2PropertiesDialog dlg(symbol, this);
  if (dlg.exec() == 0)
  {
    delete symbol;
    return false;
  }
  
  // get name
  bool ok;
  QString name = QInputDialog::getText(this, "Symbol name",
          "Please enter name for new symbol:", QLineEdit::Normal, "new symbol", &ok);
  if (!ok || name.isEmpty())
  {
    delete symbol;
    return false;
  }
  
  // add new symbol to style and re-populate the list
  mStyle->addSymbol(name, symbol);
  return true;
}

bool QgsStyleV2ManagerDialog::addColorRamp()
{
  // TODO: random color ramp
/*
  QStringList rampTypes;
  rampTypes << "Gradient" << "Random";
  bool ok;
  QString rampType = QInputDialog.getItem(this, "Color ramp type",
        "Please select color ramp type:", rampTypes, 0, false, &ok);
  if (!ok || rampType.isEmpty())
    return false;
*/
  QString rampType = "Gradient";
  
  QgsVectorColorRampV2* ramp;
  if (rampType == "Gradient")
  {
    QgsVectorGradientColorRampV2* gradRamp = new QgsVectorGradientColorRampV2();
    QgsVectorGradientColorRampV2Dialog dlg(gradRamp, this);
    if (!dlg.exec())
    {
      delete gradRamp;
      return false;
    }
    ramp = gradRamp;
  }
  else
  {
    /*
    QgsVectorRandomColorRampV2* randRamp = new QgsVectorRandomColorRampV2();
    DlgRandomColorRamp dlg(randRamp, this);
    if (!dlg.exec())
    {
      delete randRamp;
      return false;
    }
    ramp = randRamp;
    */
  }
  
  // get name
  bool ok;
  QString name = QInputDialog::getText(this, "Color ramp name",
       "Please enter name for new color ramp:", QLineEdit::Normal, "new color ramp", &ok);
  if (!ok || name.isEmpty())
  {
    delete ramp;
    return false;
  }
  
  // add new symbol to style and re-populate the list
  mStyle->addColorRamp(name, ramp);
  return true;
}


void QgsStyleV2ManagerDialog::editItem()
{
  if (currentItemType() < 3)
    editSymbol();
  else if (currentItemType() == 3)
    editColorRamp();
  else
    Q_ASSERT(0 && "not implemented");
  
  populateList();
}

bool QgsStyleV2ManagerDialog::editSymbol()
{
  QString symbolName = currentItemName();
  if (symbolName.isEmpty())
    return false;
  
  QgsSymbolV2* symbol = mStyle->symbol(symbolName);
  
  // let the user edit the symbol and update list when done
  QgsSymbolV2PropertiesDialog dlg(symbol, this);
  if (dlg.exec() == 0)
  {
    delete symbol;
    return false;
  }
  
  // by adding symbol to style with the same name the old effectively gets overwritten
  mStyle->addSymbol(symbolName, symbol);
  return true;
}

bool QgsStyleV2ManagerDialog::editColorRamp()
{
  QString name = currentItemName();
  if (name.isEmpty())
    return false;
  
  QgsVectorColorRampV2* ramp = mStyle->colorRamp(name);
  
  if (ramp->type() == "gradient")
  {
    QgsVectorGradientColorRampV2* gradRamp = static_cast<QgsVectorGradientColorRampV2*>(ramp);
    QgsVectorGradientColorRampV2Dialog dlg(gradRamp, this);
    if (!dlg.exec())
    {
      delete ramp;
      return false;
    }
  }
  else
  {
    // TODO: random color ramp
    //dlg = DlgRandomColorRamp(ramp, self)
  }
  
  mStyle->addColorRamp(name, ramp);
  return true;
}


void QgsStyleV2ManagerDialog::removeItem()
{
  if (currentItemType() < 3)
    removeSymbol();
  else if (currentItemType() == 3)
    removeColorRamp();
  else
    Q_ASSERT(0 && "not implemented");
  
  populateList();
  populateTypes();
}

bool QgsStyleV2ManagerDialog::removeSymbol()
{
  QString symbolName = currentItemName();
  if (symbolName.isEmpty())
    return false;
  
  // delete from style and update list
  mStyle->removeSymbol(symbolName);
  return true;
}

bool QgsStyleV2ManagerDialog::removeColorRamp()
{
  QString rampName = currentItemName();
  if (rampName.isEmpty())
    return false;
  
  mStyle->removeColorRamp(rampName);
}
