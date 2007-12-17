/***************************************************************************
                              qgsmaptoolmovevertex.cpp    
                              ------------------------
  begin                : June 28, 2007
  copyright            : (C) 2007 by Marco Hugentobler
  email                : marco dot hugentobler at karto dot baug dot ethz dot ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmaptoolmovevertex.h"
#include "qgsmapcanvas.h"
#include "qgsrubberband.h"
#include "qgsvectorlayer.h"

QgsMapToolMoveVertex::QgsMapToolMoveVertex(QgsMapCanvas* canvas): QgsMapToolVertexEdit(canvas)
{

}

QgsMapToolMoveVertex::~QgsMapToolMoveVertex()
{
  removeRubberBands();
}

void QgsMapToolMoveVertex::canvasMoveEvent(QMouseEvent * e)
{
  //list of rubber bands, snapping results and point index to move
  //must have equal size
  int rbSize = mRubberBands.size();
  int srSize = mRecentSnappingResults.size();
  int mpSize = mRubberBandMovingPoints.size();

  if(!(rbSize == srSize && rbSize == mpSize))
    {
      return;
    }

  //create rubber band lists and index list of moving points
  QList<QgsSnappingResult>::iterator sr_it = mRecentSnappingResults.begin();
  QList<int>::iterator mp_it = mRubberBandMovingPoints.begin();
  QList<QgsRubberBand*>::iterator rb_it = mRubberBands.begin();

  QList<QgsSnappingResult> snapResults;
  if(mSnapper.snapToBackgroundLayers(e->pos(), snapResults) != 0)
  {
    return; //error, bail out
  }
  QgsPoint posMapCoord = snapPointFromResults(snapResults, e->pos());

  for(; sr_it != mRecentSnappingResults.end(); ++sr_it, ++mp_it, ++rb_it)
    {
      if(*mp_it != -1)
	{
	  (*rb_it)->movePoint(*mp_it, posMapCoord);
	}
    }
}

void QgsMapToolMoveVertex::canvasPressEvent(QMouseEvent * e)
{
  removeRubberBands();
  mRecentSnappingResults.clear();
  mRubberBandMovingPoints.clear();

  //do snap -> new recent snapping results
  if(mSnapper.snapToCurrentLayer(e->pos(), mRecentSnappingResults, QgsSnapper::SNAP_TO_VERTEX) != 0)
    {
      //error
    }

  if(mRecentSnappingResults.size() < 1)
    {
      displaySnapToleranceWarning();
      return;
    }
  
  //create rubber band lists and index lists of moving points
  QList<QgsSnappingResult>::iterator it = mRecentSnappingResults.begin();
  for(; it != mRecentSnappingResults.end(); ++it)
    {
      QgsRubberBand* rb = createRubberBand();
      if(it->beforeVertexNr == -1 && it->afterVertexNr == -1) //usually point layers
	{
	  rb->addPoint(it->snappedVertex, true);
	  mRubberBandMovingPoints.push_back(-1);
	}
      else if(it->beforeVertexNr == -1)
	{
	  rb->addPoint(it->afterVertex, false);
	  rb->addPoint(it->snappedVertex, true);
	  //consider that the first rubber band point is added twice
	  mRubberBandMovingPoints.push_back(2); 
	}
      else if(it->afterVertexNr == -1)
	{
	  rb->addPoint(it->beforeVertex, false);
	  rb->addPoint(it->snappedVertex, true);
	  //consider that the first rubber band point is added twice
	  mRubberBandMovingPoints.push_back(2);
	}
      else
	{
	  rb->addPoint(it->beforeVertex, false);
	  rb->addPoint(it->snappedVertex, false);
	  rb->addPoint(it->afterVertex, true);
	  //consider that the first rubber band point is added twice
	  mRubberBandMovingPoints.push_back(2);
	}
      mRubberBands.push_back(rb);
    }

  //create rubber band list for snapping results
}

void QgsMapToolMoveVertex::canvasReleaseEvent(QMouseEvent * e)
{
  QgsMapLayer* currentLayer = mCanvas->currentLayer();
  QgsVectorLayer* vlayer = 0;
  if(currentLayer)
    {
      vlayer = dynamic_cast<QgsVectorLayer*>(currentLayer);
    }

  if(vlayer && mRecentSnappingResults.size() > 0)
    {
      //snap point to background layers
      QgsPoint snappedPointMapCoord;
      QgsPoint snappedPointLayerCoord;
      QList<QgsSnappingResult> snapResults;

      if(mSnapper.snapToBackgroundLayers(e->pos(), snapResults) != 0)
      {
	  //error
      }
      //add segment points in case of topological editing
      insertSegmentVerticesForSnap(snapResults, vlayer);
      //and get target point of snap
      snappedPointMapCoord = snapPointFromResults(snapResults, e->pos());
      

      snappedPointLayerCoord = toLayerCoords(vlayer, snappedPointMapCoord);

      //and change the feature points
      QList<QgsSnappingResult>::iterator sr_it = mRecentSnappingResults.begin();
      for(; sr_it != mRecentSnappingResults.end(); ++sr_it)
	{ 
	  if(!vlayer->moveVertexAt(snappedPointLayerCoord.x(), snappedPointLayerCoord.y(), sr_it->snappedAtGeometry, sr_it->snappedVertexNr))
	    {
	      //error
	    }
	}

    }

  removeRubberBands();

  mRecentSnappingResults.clear();
  mRubberBandMovingPoints.clear();

  mCanvas->refresh();
}

void QgsMapToolMoveVertex::deactivate()
{
  removeRubberBands();
}

void QgsMapToolMoveVertex::removeRubberBands()
{
  //cleanup rubber bands and list
  QList<QgsRubberBand*>::iterator rb_it = mRubberBands.begin();
  for(;rb_it != mRubberBands.end(); ++rb_it)
    {
      delete *rb_it;
    }
  mRubberBands.clear();
}
