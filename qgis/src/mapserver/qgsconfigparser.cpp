/***************************************************************************
                              qgsconfigparser.cpp
                              -------------------
  begin                : May 26, 2010
  copyright            : (C) 2010 by Marco Hugentobler
  email                : marco dot hugentobler at sourcepole dot ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsconfigparser.h"
#include "qgsapplication.h"
#include "qgscomposerlabel.h"
#include "qgscomposermap.h"
#include "qgscomposition.h"
#include "qgsrasterlayer.h"
#include "qgsvectorlayer.h"
#include <sqlite3.h>


QgsConfigParser::QgsConfigParser()
    : mFallbackParser( 0 )
    , mScaleDenominator( 0 )
    , mOutputUnits( QgsMapRenderer::Millimeters )
{
  setDefaultLegendSettings();
}

QgsConfigParser::~QgsConfigParser()
{
  //remove the external GML datasets
  for ( QMap<QString, QDomDocument*>::iterator it = mExternalGMLDatasets.begin(); it != mExternalGMLDatasets.end(); ++it )
  {
    delete it.value();
  }
}

void QgsConfigParser::setDefaultLegendSettings()
{
  mLegendBoxSpace = 2;
  mLegendLayerSpace = 3;
  mLegendSymbolSpace = 2;
  mLegendIconLabelSpace = 2;
  mLegendSymbolWidth = 7;
  mLegendSymbolHeight = 4;
}

void QgsConfigParser::setFallbackParser( QgsConfigParser* p )
{
  if ( !p )
  {
    return;
  }
  delete mFallbackParser;
  mFallbackParser = p;
}

void QgsConfigParser::addExternalGMLData( const QString& layerName, QDomDocument* gmlDoc )
{
  mExternalGMLDatasets.insert( layerName, gmlDoc );
}

void QgsConfigParser::appendExGeographicBoundingBox( QDomElement& layerElem,
    QDomDocument& doc,
    const QgsRectangle& layerExtent,
    const QgsCoordinateReferenceSystem& layerCRS ) const
{
  if ( layerElem.isNull() )
  {
    return;
  }

  QgsCoordinateReferenceSystem wgs84;
  wgs84.createFromEpsg( 4326 );

  //Ex_GeographicBoundingBox
  //transform the layers native CRS into WGS84
  QgsCoordinateTransform exGeoTransform( layerCRS, wgs84 );
  QgsRectangle wgs84BoundingRect = exGeoTransform.transformBoundingBox( layerExtent );

  QDomElement ExGeoBBoxElement = doc.createElement( "EX_GeographicBoundingBox" );
  QDomElement wBoundLongitudeElement = doc.createElement( "westBoundLongitude" );
  QDomText wBoundLongitudeText = doc.createTextNode( QString::number( wgs84BoundingRect.xMinimum() ) );
  wBoundLongitudeElement.appendChild( wBoundLongitudeText );
  ExGeoBBoxElement.appendChild( wBoundLongitudeElement );
  QDomElement eBoundLongitudeElement = doc.createElement( "eastBoundLongitude" );
  QDomText eBoundLongitudeText = doc.createTextNode( QString::number( wgs84BoundingRect.xMaximum() ) );
  eBoundLongitudeElement.appendChild( eBoundLongitudeText );
  ExGeoBBoxElement.appendChild( eBoundLongitudeElement );
  QDomElement sBoundLatitudeElement = doc.createElement( "southBoundLatitude" );
  QDomText sBoundLatitudeText = doc.createTextNode( QString::number( wgs84BoundingRect.yMinimum() ) );
  sBoundLatitudeElement.appendChild( sBoundLatitudeText );
  ExGeoBBoxElement.appendChild( sBoundLatitudeElement );
  QDomElement nBoundLatitudeElement = doc.createElement( "northBoundLatitude" );
  QDomText nBoundLatitudeText = doc.createTextNode( QString::number( wgs84BoundingRect.yMaximum() ) );
  nBoundLatitudeElement.appendChild( nBoundLatitudeText );
  ExGeoBBoxElement.appendChild( nBoundLatitudeElement );
  layerElem.appendChild( ExGeoBBoxElement );

  //BoundingBox element
  QDomElement bBoxElement = doc.createElement( "BoundingBox" );
  if ( layerCRS.isValid() )
  {
    bBoxElement.setAttribute( "CRS", "EPSG:" + QString::number( layerCRS.epsg() ) );
  }

  bBoxElement.setAttribute( "minx", QString::number( layerExtent.xMinimum() ) );
  bBoxElement.setAttribute( "miny", QString::number( layerExtent.yMinimum() ) );
  bBoxElement.setAttribute( "maxx", QString::number( layerExtent.xMaximum() ) );
  bBoxElement.setAttribute( "maxy", QString::number( layerExtent.yMaximum() ) );
  layerElem.appendChild( bBoxElement );
}

QList<int> QgsConfigParser::createCRSListForLayer( QgsMapLayer* theMapLayer ) const
{
  QList<int> epsgNumbers;
  QgsVectorLayer* theVectorLayer = dynamic_cast<QgsVectorLayer*>( theMapLayer );

  if ( theVectorLayer ) //append the source SRS. In future, all systems supported by proj4 should be appended
  {
    QString myDatabaseFileName = QgsApplication::srsDbFilePath();
    sqlite3      *myDatabase;
    const char   *myTail;
    sqlite3_stmt *myPreparedStatement;
    int           myResult;

    //check the db is available
    myResult = sqlite3_open( myDatabaseFileName.toLocal8Bit().data(), &myDatabase );
    if ( myResult )
    {
      //if the database cannot be opened, add at least the epsg number of the source coordinate system
      epsgNumbers.push_back( theMapLayer->srs().epsg() );
      return epsgNumbers;
    };
    QString mySql = "select auth_id from tbl_srs where auth_name='EPSG' ";
    myResult = sqlite3_prepare( myDatabase, mySql.toUtf8(), mySql.length(), &myPreparedStatement, &myTail );
    if ( myResult == SQLITE_OK )
    {
      while ( sqlite3_step( myPreparedStatement ) == SQLITE_ROW )
      {
        epsgNumbers.push_back( QString::fromUtf8(( char * )sqlite3_column_text( myPreparedStatement, 0 ) ).toLong() );
      }
    }
    sqlite3_finalize( myPreparedStatement );
    sqlite3_close( myDatabase );
  }
  else //rasters cannot be reprojected. Use the epsg number of the layers native CRS
  {
    int rasterEpsg = theMapLayer->srs().epsg();
    epsgNumbers.push_back( rasterEpsg );
  }
  return epsgNumbers;
}

bool QgsConfigParser::exGeographicBoundingBox( const QDomElement& layerElement, QgsRectangle& rect ) const
{
  if ( layerElement.isNull() )
  {
    return false;
  }

  QDomElement exGeogElem = layerElement.firstChildElement( "EX_GeographicBoundingBox" );
  if ( exGeogElem.isNull() )
  {
    return false;
  }

  bool ok = true;
  //minx
  QDomElement westBoundElem = exGeogElem.firstChildElement( "westBoundLongitude" );
  if ( westBoundElem.isNull() )
  {
    return false;
  }
  double minx = westBoundElem.text().toDouble( &ok );
  if ( !ok )
  {
    return false;
  }
  //maxx
  QDomElement eastBoundElem = exGeogElem.firstChildElement( "eastBoundLongitude" );
  if ( eastBoundElem.isNull() )
  {
    return false;
  }
  double maxx = eastBoundElem.text().toDouble( &ok );
  if ( !ok )
  {
    return false;
  }
  //miny
  QDomElement southBoundElem = exGeogElem.firstChildElement( "southBoundLatitude" );
  if ( southBoundElem.isNull() )
  {
    return false;
  }
  double miny = southBoundElem.text().toDouble( &ok );
  if ( !ok )
  {
    return false;
  }
  //maxy
  QDomElement northBoundElem = exGeogElem.firstChildElement( "northBoundLatitude" );
  if ( northBoundElem.isNull() )
  {
    return false;
  }
  double maxy = northBoundElem.text().toDouble( &ok );
  if ( !ok )
  {
    return false;
  }

  rect.setXMinimum( minx );
  rect.setXMaximum( maxx );
  rect.setYMinimum( miny );
  rect.setYMaximum( maxy );

  return true;
}

bool QgsConfigParser::crsSetForLayer( const QDomElement& layerElement, QSet<int>& crsSet ) const
{
  if ( layerElement.isNull() )
  {
    return false;
  }

  crsSet.clear();
  bool intConversionOk;

  QDomNodeList crsNodeList = layerElement.elementsByTagName( "CRS" );
  for ( int i = 0; i < crsNodeList.size(); ++i )
  {
    int epsg = crsNodeList.at( i ).toElement().text().remove( 0, 5 ).toInt( &intConversionOk );
    if ( intConversionOk )
    {
      crsSet.insert( epsg );
    }
  }

  return true;
}

void QgsConfigParser::appendCRSElementsToLayer( QDomElement& layerElement, QDomDocument& doc, const QList<int>& crsList ) const
{
  if ( layerElement.isNull() )
  {
    return;
  }

  //In case the number of advertised CRS is constrained
  QSet<int> epsgSet = supportedOutputCrsSet();

  QList<int>::const_iterator crsIt = crsList.constBegin();
  for ( ; crsIt != crsList.constEnd(); ++crsIt )
  {
    if ( !epsgSet.isEmpty() && !epsgSet.contains( *crsIt ) ) //consider epsg output constraint
    {
      continue;
    }
    QDomElement crsElement = doc.createElement( "CRS" );
    QDomText epsgText = doc.createTextNode( "EPSG:" + QString::number( *crsIt ) );
    crsElement.appendChild( epsgText );
    layerElement.appendChild( crsElement );
  }
}

QgsComposition* QgsConfigParser::createPrintComposition( const QString& composerTemplate, QgsMapRenderer* mapRenderer, const QMap< QString, QString >& parameterMap ) const
{
  QList<QgsComposerMap*> composerMaps;
  QList<QgsComposerLabel*> composerLabels;

  QgsComposition* c = initComposition( composerTemplate, mapRenderer, composerMaps, composerLabels );
  if ( !c )
  {
    return 0;
  }

  QMap< QString, QString >::const_iterator dpiIt = parameterMap.find( "DPI" );
  if ( dpiIt != parameterMap.constEnd() )
  {
    c->setPrintResolution( dpiIt.value().toInt() );
  }

  //replace composer map parameters
  QList<QgsComposerMap*>::iterator mapIt = composerMaps.begin();
  QgsComposerMap* currentMap = 0;
  for ( ; mapIt != composerMaps.end(); ++mapIt )
  {
    currentMap = *mapIt;
    if ( !currentMap )
    {
      continue;
    }

    //search composer map title in parameter map-> string
    QMap< QString, QString >::const_iterator titleIt = parameterMap.find( "MAP" + QString::number( currentMap->id() ) );
    if ( titleIt == parameterMap.constEnd() )
    {
      //remove map from composition if not referenced by the request
      c->removeItem( *mapIt );
      delete( *mapIt );
      continue;
    }
    QString replaceString = titleIt.value();
    QStringList replacementList = replaceString.split( "/" );

    //get map extent from string
    if ( replacementList.size() > 0 )
    {
      QStringList coordList = replacementList.at( 0 ).split( "," );
      if ( coordList.size() > 3 )
      {
        bool xMinOk, yMinOk, xMaxOk, yMaxOk;
        double xmin = coordList.at( 0 ).toDouble( &xMinOk );
        double ymin = coordList.at( 1 ).toDouble( &yMinOk );
        double xmax = coordList.at( 2 ).toDouble( &xMaxOk );
        double ymax = coordList.at( 3 ).toDouble( &yMaxOk );
        if ( xMinOk && yMinOk && xMaxOk && yMaxOk )
        {
          currentMap->setNewExtent( QgsRectangle( xmin, ymin, xmax, ymax ) );
        }
      }
    }

    //get rotation from string
    if ( replacementList.size() > 1 )
    {
      bool rotationOk;
      double rotation = replacementList.at( 1 ).toDouble( &rotationOk );
      if ( rotationOk )
      {
        currentMap->setMapRotation( rotation );
      }
    }

    //get layer list from string
    if ( replacementList.size() > 2 )
    {
      QStringList layerSet;
      QStringList wmsLayerList = replacementList.at( 2 ).split( "," );
      QStringList wmsStyleList;
      if ( replacementList.size() > 3 )
      {
        wmsStyleList = replacementList.at( 3 ).split( "," );
      }

      for ( int i = 0; i < wmsLayerList.size(); ++i )
      {
        QString styleName;
        if ( wmsStyleList.size() > i )
        {
          styleName = wmsStyleList.at( i );
        }
        QList<QgsMapLayer*> layerList = mapLayerFromStyle( wmsLayerList.at( i ), styleName );
        QList<QgsMapLayer*>::const_iterator mapIdIt = layerList.constBegin();
        for ( ; mapIdIt != layerList.constEnd(); ++mapIdIt )
        {
          if ( *mapIdIt )
          {
            layerSet.push_back(( *mapIdIt )->getLayerID() );
          }
        }
      }

      currentMap->setLayerSet( layerSet );
      currentMap->setKeepLayerSet( true );
    }
  }

  //replace label text
  QList<QgsComposerLabel*>::const_iterator labelIt = composerLabels.constBegin();
  QgsComposerLabel* currentLabel = 0;

  for ( ; labelIt != composerLabels.constEnd(); ++labelIt )
  {
    currentLabel = *labelIt;
    QMap< QString, QString >::const_iterator titleIt = parameterMap.find( currentLabel->id().toUpper() );
    if ( titleIt == parameterMap.constEnd() )
    {
      //remove exported labels not referenced in the request
      if ( !currentLabel->id().isEmpty() )
      {
        c->removeItem( currentLabel );
        delete( currentLabel );
      }
      continue;
    }

    currentLabel->setText( titleIt.value() );
    currentLabel->adjustSizeToText();
  }

  return c;
}
