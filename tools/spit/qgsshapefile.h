/***************************************************************************
                          qgsshapefile.h  -  description
                             -------------------
    begin                : Fri Dec 19 2003
    copyright            : (C) 2003 by Denis Antipov
    email                : 
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
/* $Id$ */

#ifndef QGSSHAPEFILE_H
#define QGSSHAPEFILE_H

#include <vector>
#include <qstring.h>
#include <ogrsf_frmts.h>
#include <libpq++.h>
#include <qprogressdialog.h>

class OGRLayer;
class OGRDataSource;
 
class QgsShapeFile
{
  Q_OBJECT
  public:

  QgsShapeFile(QString filename);
  ~QgsShapeFile();
  int getFeatureCount();
  const char * getFeatureClass();
  bool insertLayer(QString dbname, QString srid, PgDatabase * conn, QProgressDialog * pro);
    
  bool is_valid();
  const char * getName();
  std::vector <QString> column_names;
  std::vector <QString> column_types;


  private:
  OGRDataSource *ogrDataSource;
  OGRLayer * ogrLayer;
  bool import_cancelled;
  bool valid;
  int features;
  const char * filename;
  const char * geom_type;

  public slots:
  void cancelImport();
};

#endif
