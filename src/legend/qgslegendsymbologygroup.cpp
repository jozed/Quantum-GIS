/***************************************************************************
 *   Copyright (C) 2005 by Tim Sutton   *
 *   aps02ts@macbuntu   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include "qgsapplication.h"
#include "qgslegendlayerfile.h"
#include "qgslegendlayerfilegroup.h"
#include "qgslegendsymbologygroup.h"
#include "qgsmaplayer.h"
#include <QCoreApplication>
#include <QIcon>

QgsLegendSymbologyGroup::QgsLegendSymbologyGroup(QTreeWidgetItem * theItem, QString theString)
    : QgsLegendItem( theItem, theString)
{
  mType = LEGEND_SYMBOL_GROUP;
  QIcon myIcon(QgsApplication::themePath()+"mIconSymbology.png");
  setText(0, theString);
  setIcon(0,myIcon);
}


QgsLegendSymbologyGroup::~QgsLegendSymbologyGroup()
{}

QgsLegendItem::DRAG_ACTION QgsLegendSymbologyGroup::accept(LEGEND_ITEM_TYPE type)
{
    return NO_ACTION;
}

QgsLegendItem::DRAG_ACTION QgsLegendSymbologyGroup::accept(const QgsLegendItem* li) const
{
  return NO_ACTION;
}

/** Overloads cmpare function of QListViewItem
  * @note The symbology group must always be the second in the list
  */
int QgsLegendSymbologyGroup::compare (QTreeWidgetItem * i,int col, bool ascending)
{
  QgsLegendItem * myItem = dynamic_cast<QgsLegendItem *>(i) ;
  if (myItem->type() == QgsLegendItem::LEGEND_PROPERTY_GROUP)
  {
    return 1;
  }
  else 
  {
    return -1;
  }
}

void QgsLegendSymbologyGroup::updateLayerSymbologySettings(const QgsMapLayer* thelayer)
{
    //find the legend layer group node
    QgsLegendItem* parent = dynamic_cast<QgsLegendItem*>(this->parent());
    if(!parent)
    {
	return;
    }
    QgsLegendItem* sibling = 0;
    QgsLegendLayerFileGroup* group = 0;
    for(sibling = parent->firstChild(); sibling != 0; sibling = sibling->nextSibling())
    {
	group = dynamic_cast<QgsLegendLayerFileGroup*>(sibling);
	if(group)
	{
	    break;
	}
    }

    if(!group)
    {
	return;
    }

    //go through all the entries and apply QgsMapLayer::copySymbologySettings
    QgsLegendLayerFile* f = 0;
    QgsMapLayer* mylayer = 0;
    for(sibling = group->firstChild(); sibling != 0; sibling = sibling->nextSibling())
    {
	f = dynamic_cast<QgsLegendLayerFile*>(sibling);
	if( f && (f->layer() != thelayer))
	{
	    mylayer = f->layer();
	    if(mylayer)
	    {
		mylayer->copySymbologySettings(*thelayer);
	    }
	}
    }
}
