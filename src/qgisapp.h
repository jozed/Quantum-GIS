/***************************************************************************
                          qgisapp.h  -  description
                             -------------------
    begin                : Sat Jun 22 2002
    copyright            : (C) 2002 by Gary E.Sherman
    email                : sherman@mrcc.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGISAPP_H
#define QGISAPP_H
class QCanvas;
class QRect;
class QCanvasView;
class QStringList;
#include "qgisappbase.h"

class QgsMapCanvas;
/*! \class QgisApp
 * \brief Main window for the Qgis application
 */
class QgisApp : public QgisAppBase  {
public: 
	QgisApp(QWidget *parent=0, const char * name=0, WFlags fl = WType_TopLevel );
	
	~QgisApp();
	//public slots:
	//! Add a layer to the map
	void addLayer();
	//! Exit Qgis
	void fileExit();
	//! Zoom out
 	void zoomOut();
	//! Zoom int
  	void zoomIn();
	//! Read Well Known Binary stream from PostGIS
	void readWKB(const char *, QStringList tables);
	//! Draw a point on the map canvas
	void drawPoint(double x, double y);
private:
	//! Map canvase 
QgsMapCanvas *mapCanvas;
//! Table of contents (legend) for the map
QWidget *mapToc;
//! scale factor
 double scaleFactor;
 //! Current map window extent in real-world coordinates
 QRect *mapWindow;

};

#endif
