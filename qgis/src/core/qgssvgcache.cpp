/***************************************************************************
              qgssvgcache.cpp  -  SVG rendering and pixmap caching
                             -------------------
    begin                : Sat Jul 17 2004
    copyright            : (C) 2004 by Lars Luthman
    email                : larsl at users dot sourceforge dot org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
/*  $Id$ */

#include <iostream>

#include <qimage.h>
#include <qpainter.h>
#include <q3picture.h>
#include <qsettings.h>
#include <qmessagebox.h>

#include "qgssvgcache.h"
//Added by qt3to4:
#include <QPixmap>
#include <QSvgRenderer>


QgsSVGCache::QgsSVGCache() {
  QSettings settings;
  oversampling = settings.readNumEntry("/qgis/svgoversampling", 4);
  pixelLimit = settings.readNumEntry("/qgis/svgcachesize", 200000);
  totalPixels = 0;
}
QgsSVGCache::~QgsSVGCache() 
{
  //QMapIterator<QString, QSvgRenderer *> i(pictureMap);
  //while (i.hasNext()) {
  //  i.next();
  //  delete i.value() ;
  //}
  //pictureMap.clear();
}

QSvgRenderer *  QgsSVGCache::getPicture(QString filename) 
{

  PictureMap::const_iterator i = pictureMap.find(filename);
  while (i != pictureMap.end()) 
  {
    return  (*i).second ;
  }

  QSvgRenderer * mySVG;
  mySVG->load(filename);

  pictureMap[filename] = mySVG;

  return mySVG;
}  

QPixmap QgsSVGCache::getPixmap(QString filename, double scaleFactor) 
{
  
  // make the symbols smaller
  scaleFactor *= 0.30;
  std::pair<QString, double> myPair(filename, scaleFactor) ;
  PixmapMap::iterator i = pixmapMap.find(myPair);
  while (i != pixmapMap.end()) 
  {
    QPixmap myPixmap =  i->second ;
    return myPixmap;
  }
  
  // if not, try to load it
#if QGISDEBUG > 2
  std::cerr<<"SVGCACHE: loading "<<filename<<"["<<scaleFactor<<"]"<<std::endl;
#endif
  QSvgRenderer mySVG;
  mySVG.load(filename);
  int width=mySVG.defaultSize().width();
  width=static_cast<int>(static_cast<double>(width)*scaleFactor);
  int height=mySVG.defaultSize().height();
  height=static_cast<int>(static_cast<double>(height)*scaleFactor);
  
  //prevent 0 width or height, which would cause a crash
  if (width == 0) {
    width = 1;
  }
  if (height == 0) {
    height = 1;
  }
  
  QPixmap myPixmap = QPixmap(width,height);
  myPixmap.fill(QColor(255,255,255.0)); //transparent
  QPainter myPainter(&myPixmap);
  mySVG.render(&myPainter);  
  pixmapMap[std::pair<QString, double>(filename, scaleFactor)] = myPixmap;
  return myPixmap;
}
  

void QgsSVGCache::clear() {
  pixmapMap.clear();
  fifo = std::queue<std::pair<QString, double> >();
  totalPixels = 0;
}


void QgsSVGCache::setOversampling(int oversamplingFactor) {
  oversampling = oversamplingFactor;
}
  

int QgsSVGCache::getOversampling() const {
  return oversampling;
}
