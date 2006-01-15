/***************************************************************************
                        qgsspit.h  -  description
                           -------------------
  begin                : Fri Dec 19 2003
  copyright            : (C) 2003 by Denis Antipov
                       : (C) 2004 by Gary Sherman
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
#include <vector>
#include <algorithm>
#include <qstringlist.h>
#include <qsettings.h>
#include "qgsshapefile.h"
#include "ui_qgsspitbase.h"
extern "C"
{
#include <libpq-fe.h>
}

class QgsSpit : public QDialog, private Ui::QgsSpitBase
{
  Q_OBJECT;
public:
  QgsSpit( QWidget *parent = 0, Qt::WFlags fl = 0 );
  ~QgsSpit();
  //! Populate the list of available database connections
  void populateConnectionList();
  //! Connect to the selected database
  void dbConnect();
  //! Return a list of selected tables
  QStringList selectedTables();
  //! Return the connection info
  QString connInfo();
  //! Create a new PostgreSQL connection
  void newConnection();
  //! Edit a PostgreSQL connection
  void editConnection();
  //! Remove a PostgreSQL connection
  void removeConnection();
  //! Add file to the queue
  void addFile();
  //! Remove selected file from the queue
  void removeFile();
  //! Remove all files from the queue
  void removeAllFiles();
  //! Use the default SRID (Spatial Reference ID)
  void useDefaultSrid();
  //! Use the default geometry field name (the_geom)
  void useDefaultGeom();
  //! Show brief help
  void helpInfo();
  //! Get schemas available in the database
  void getSchema();
  void updateSchema();
  //! Import shapefiles into PostgreSQL
  void import();
  //! Edit the column names for a shapefile in the queue
  void editColumns( int, int, int, const QPoint & );
  //! Edit import properties of a shapefile in the queue
  void editShapefile( int, int, int, const QPoint & );

private:

  // Enum of table columns indexes
  enum ShpTableColumns
  {
    ColFILENAME = 0,
    ColFEATURECLASS = 1, // is editable
    ColFEATURECOUNT = 2,
    ColDBRELATIONNAME = 3,  // is editable
    ColDBSCHEMA = 4  // is editable
  };
  
  PGconn* checkConnection();
  QStringList schema_list;
  QStringList geometry_list;
  int total_features;
  std::vector <QgsShapeFile *> fileList;
  int defSrid;
  QString defGeom;
  int defaultSridValue;
  QString defaultGeomValue;
  QString gl_key;
};
