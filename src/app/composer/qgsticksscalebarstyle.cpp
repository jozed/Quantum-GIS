/***************************************************************************
                            qgsticksscalebarstyle.cpp
                            -------------------------
    begin                : June 2008
    copyright            : (C) 2008 by Marco Hugentobler
    email                : marco.hugentobler@karto.baug.ethz.ch
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsticksscalebarstyle.h"
#include "qgscomposerscalebar.h"
#include <QPainter>

QgsTicksScaleBarStyle::QgsTicksScaleBarStyle(const QgsComposerScaleBar* bar): QgsScaleBarStyle(bar)
{
  mTickPosition = MIDDLE;
}

QgsTicksScaleBarStyle::QgsTicksScaleBarStyle(): QgsScaleBarStyle(0)
{
  mTickPosition = MIDDLE;
}

QgsTicksScaleBarStyle::~QgsTicksScaleBarStyle()
{

}

QString QgsTicksScaleBarStyle::name() const
{
  return "Line with Ticks";
}

void QgsTicksScaleBarStyle::draw(QPainter* p, double xOffset) const
{
  if(!mScaleBar)
    {
      return;
    }
  double barTopPosition = mScaleBar->fontHeight() + mScaleBar->labelBarSpace() + mScaleBar->boxContentSpace();
  double middlePosition = barTopPosition + mScaleBar->height()/2.0;
  double bottomPosition = barTopPosition + mScaleBar->height();

  p->save();
  p->setPen(mScaleBar->pen());

  QList<QPair<double, double> > segmentInfo;
  mScaleBar->segmentPositions(segmentInfo);

  QList<QPair<double, double> >::const_iterator segmentIt = segmentInfo.constBegin();
  for(; segmentIt != segmentInfo.constEnd(); ++segmentIt)
    {
      p->drawLine(segmentIt->first + xOffset, barTopPosition, segmentIt->first + xOffset, barTopPosition + mScaleBar->height());
      switch(mTickPosition)
	{
	case DOWN:
	  p->drawLine(xOffset + segmentIt->first, barTopPosition, xOffset + segmentIt->first + mScaleBar->segmentMM(), barTopPosition);
	  break;
	case MIDDLE:
	  p->drawLine(xOffset + segmentIt->first, middlePosition, xOffset + segmentIt->first + mScaleBar->segmentMM(), middlePosition); 
	  break;
	case UP:
	  p->drawLine(xOffset + segmentIt->first, bottomPosition, xOffset + segmentIt->first + mScaleBar->segmentMM(), bottomPosition); 
	  break;
	}
    }

  //draw last tick
  if(!segmentInfo.isEmpty())
    {
      double lastTickPositionX = segmentInfo.last().first + mScaleBar->segmentMM();
      p->drawLine(lastTickPositionX + xOffset, barTopPosition, lastTickPositionX + xOffset, barTopPosition + mScaleBar->height());
    }

  p->restore();

  //draw labels using the default method
  drawLabels(p);
}


