/***************************************************************************
    qgsmaptool.h  -  base class for map canvas tools
    ----------------------
    begin                : January 2006
    copyright            : (C) 2006 by Martin Dobias
    email                : wonder.sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
/* $Id$ */

#ifndef QGSMAPTOOL_H
#define QGSMAPTOOL_H

// TODO: moznost enabled / disabled - ak je napr. nevhodny typ layeru
// (a vtedy sa aj kurzor docasne nastavi ten original)

class QgsMapCanvas;
class QMouseEvent;
class QgsPoint;
class QPoint;
class QCursor;
class QAction;

class QgsMapTool
{
  public:
    
    //! virtual destructor
    virtual ~QgsMapTool();
    
    //! Mouse move event for overriding
    virtual void canvasMoveEvent(QMouseEvent * e) = 0;

    //! Mouse press event for overriding
    virtual void canvasPressEvent(QMouseEvent * e) = 0;

    //! Mouse release event for overriding
    virtual void canvasReleaseEvent(QMouseEvent * e) = 0;
    
    //! Called when rendering has finished
    virtual void renderComplete() {}
    
    void setAction(QAction* action) { mAction = action; }
    
    QAction* action() { return mAction; }
    
    virtual const char* toolName() { return "generic tool"; }
    
  protected:

    //! constructor takes map canvas as a parameter
    QgsMapTool(QgsMapCanvas* canvas);
        
    //! transformation from screen coordinates to map coordinates
    QgsPoint toMapCoords(const QPoint& point);
    
    //! transformation from map coordinates to screen coordinates
    QPoint toCanvasCoords(const QgsPoint& point);
    
    //! pointer to map canvas
    QgsMapCanvas* mCanvas;
    
    //! cursor used in map tool
    QCursor* mCursor;
    
    //! optionally map tool can have pointer to action
    //! which will be used to set that action as active
    QAction* mAction;
};

#endif
