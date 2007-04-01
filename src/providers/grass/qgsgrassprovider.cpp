/***************************************************************************
    qgsgrassprovider.cpp -  Data provider for GRASS format
                             -------------------
    begin                : March, 2004
    copyright            : (C) 2004 by Gary E.Sherman, Radim Blazek
    email                : sherman@mrcc.com, blazek@itc.it
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

#include <string.h>
#include <iostream>
#include <vector>
#include <cfloat>

#include <Q3CString>
#include <QDir>
#include <QString>
#include <QDateTime>
#include <QMessageBox>
#include <QTextCodec>

#include "qgis.h"
#include "qgsapplication.h"
#include "qgsdataprovider.h"
#include "qgsfeature.h"
#include "qgsfield.h"
#include "qgsrect.h"
#include "qgsspatialrefsys.h"

extern "C" {
#include <grass/gprojects.h>
#include <grass/gis.h>
#include <grass/dbmi.h>
#include <grass/Vect.h>
}

#include "qgsgrass.h"
#include "qgsgrassprovider.h"

std::vector<GLAYER> QgsGrassProvider::mLayers;
std::vector<GMAP> QgsGrassProvider::mMaps;


static QString GRASS_KEY = "grass"; // XXX verify this
static QString GRASS_DESCRIPTION = "Grass provider"; // XXX verify this



QgsGrassProvider::QgsGrassProvider(QString uri)
    : QgsVectorDataProvider(uri)
{
#ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider URI: " << uri.toLocal8Bit().data() << std::endl;
#endif

    QgsGrass::init();

    QTime time;
    time.start();

    mValid = false;
    
    // Parse URI 
    QDir dir ( uri );  // it is not a directory in fact
    QString myURI = dir.path();  // no dupl '/'

    mLayer = dir.dirName();
    myURI = myURI.left( dir.path().findRev('/') );
    dir = QDir(myURI);
    mMapName = dir.dirName();
    dir.cdUp(); 
    mMapset = dir.dirName();
    dir.cdUp(); 
    mLocation = dir.dirName();
    dir.cdUp(); 
    mGisdbase = dir.path();
    
    #ifdef QGISDEBUG
    std::cerr << "gisdbase: " << mGisdbase.toLocal8Bit().data() << std::endl;
    std::cerr << "location: " << mLocation.toLocal8Bit().data() << std::endl;
    std::cerr << "mapset: "   << mMapset.toLocal8Bit().data() << std::endl;
    std::cerr << "mapName: "  << mMapName.toLocal8Bit().data() << std::endl;
    std::cerr << "layer: "    << mLayer.toLocal8Bit().data() << std::endl;
    #endif

    /* Parse Layer, supported layers <field>_point, <field>_line, <field>_area
    *  Layer is opened even if it is empty (has no features) 
    */
    mLayerField = -1;       
    if ( mLayer.compare("boundary") == 0 ) { // currently not used
        mLayerType = BOUNDARY;
	mGrassType = GV_BOUNDARY;
    } else if ( mLayer.compare("centroid") == 0 ) { // currently not used
        mLayerType = CENTROID;
	mGrassType = GV_CENTROID;
    } else {
        mLayerField = grassLayer ( mLayer );
        if ( mLayerField == -1 ) {
	    std::cerr << "Invalid layer name, no underscore found: " << mLayer.toLocal8Bit().data() << std::endl;
	    return;
        }

        mGrassType = grassLayerType ( mLayer );
        
	if ( mGrassType == GV_POINT ) {
	    mLayerType = POINT;
	} else if ( mGrassType == GV_LINES ) {
	    mLayerType = LINE;
	} else if ( mGrassType == GV_AREA ) {
	    mLayerType = POLYGON;
	} else {
	    std::cerr << "Invalid layer name, wrong type: " << mLayer.toLocal8Bit().data() << std::endl;
	    return;
	}
        
    }
    #ifdef QGISDEBUG
    std::cerr << "mLayerField: " << mLayerField << std::endl;
    std::cerr << "mLayerType: " << mLayerType << std::endl;
    #endif

    if ( mLayerType == BOUNDARY || mLayerType == CENTROID ) {
	std::cerr << "Layer type not supported." << std::endl;
	return;
    }

    // Set QGIS type
    switch ( mLayerType ) {
	case POINT:
	case CENTROID:
            mQgisType = QGis::WKBPoint; 
	    break;
	case LINE:
	case BOUNDARY:
            mQgisType = QGis::WKBLineString; 
	    break;
	case POLYGON:
	    mQgisType = QGis::WKBPolygon;
	    break;
    }

    mLayerId = openLayer(mGisdbase, mLocation, mMapset, mMapName, mLayerField);
    if ( mLayerId < 0 ) {
	std::cerr << "Cannot open GRASS layer:" << myURI.toLocal8Bit().data() << std::endl;
	return;
    }
    #ifdef QGISDEBUG
    std::cerr << "mLayerId: " << mLayerId << std::endl;
    #endif

    mMap = layerMap(mLayerId);

    // Getting the total number of features in the layer
    mNumberFeatures = 0;
    mCidxFieldIndex = -1;
    if ( mLayerField >= 0 ) {
	mCidxFieldIndex = Vect_cidx_get_field_index ( mMap, mLayerField);
	if ( mCidxFieldIndex >= 0 ) {
	    mNumberFeatures = Vect_cidx_get_type_count ( mMap, mLayerField, mGrassType );
	    mCidxFieldNumCats = Vect_cidx_get_num_cats_by_index ( mMap, mCidxFieldIndex );
	}
    } else {
	// TODO nofield layers
	mNumberFeatures = 0;
	mCidxFieldNumCats = 0;
    }
    mNextCidx = 0;

    #ifdef QGISDEBUG
    std::cerr << "mNumberFeatures = " << mNumberFeatures << " mCidxFieldIndex = " << mCidxFieldIndex
              << " mCidxFieldNumCats = " << mCidxFieldNumCats << std::endl;
    #endif


    // Create selection array
    mSelectionSize = allocateSelection ( mMap, &mSelection );
    resetSelection(1); // TODO ? - where what reset

    mMapVersion = mMaps[mLayers[mLayerId].mapId].version;

    // Init structures
    mPoints = Vect_new_line_struct ();
    mCats = Vect_new_cats_struct ();
    mList = Vect_new_list ();

    mValid = true;

    #ifdef QGISDEBUG
    std::cerr << "New GRASS layer opened, time (ms): " << time.elapsed() << std::endl;
    #endif
}

void QgsGrassProvider::update ( void )
{
    #ifdef QGISDEBUG
    std::cerr << "*** QgsGrassProvider::update ***" << std::endl;
    #endif

    mValid = false;

    if ( ! mMaps[mLayers[mLayerId].mapId].valid ) return;

    // Getting the total number of features in the layer
    // It may happen that the field disappeares from the map (deleted features, new map without that field)
    mNumberFeatures = 0;
    mCidxFieldIndex = -1;
    if ( mLayerField >= 0 ) {
	mCidxFieldIndex = Vect_cidx_get_field_index ( mMap, mLayerField);
	if ( mCidxFieldIndex >= 0 ) {
	    mNumberFeatures = Vect_cidx_get_type_count ( mMap, mLayerField, mGrassType );
	    mCidxFieldNumCats = Vect_cidx_get_num_cats_by_index ( mMap, mCidxFieldIndex );
	}
    } else {
	// TODO nofield layers
	mNumberFeatures = 0;
	mCidxFieldNumCats = 0;
    }
    mNextCidx = 0;

    #ifdef QGISDEBUG
    std::cerr << "mNumberFeatures = " << mNumberFeatures << " mCidxFieldIndex = " << mCidxFieldIndex
              << " mCidxFieldNumCats = " << mCidxFieldNumCats << std::endl;
    #endif

    // Create selection array
    if ( mSelection ) free ( mSelection );
    mSelectionSize = allocateSelection ( mMap, &mSelection );
    resetSelection(1); 
    
    mMapVersion = mMaps[mLayers[mLayerId].mapId].version;

    mValid = true;
}

int QgsGrassProvider::allocateSelection( struct Map_info *map, char **selection )
{
    int size;
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::allocateSellection" << std::endl;
    #endif
    
    int nlines = Vect_get_num_lines ( map );
    int nareas = Vect_get_num_areas ( map );
    
    if ( nlines > nareas ) {
	size = nlines + 1;
    } else {
	size = nareas + 1;
    }
    #ifdef QGISDEBUG
    std::cerr << "nlines = " << nlines << " nareas = " << nareas << " size = " << size << std::endl;
    #endif

    *selection = (char *) malloc ( size );

    return size;
}

QgsGrassProvider::~QgsGrassProvider()
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::~QgsGrassProvider()" << std::endl;
    #endif
    closeLayer ( mLayerId );
}


QString QgsGrassProvider::storageType() const
{
  return "GRASS (Geographic Resources Analysis and Support System) file";
}

#if 0
bool QgsGrassProvider::getNextFeature(QgsFeature& feature,
                                      bool fetchGeometry,
                                      QgsAttributeList attlist,
                                      uint featureQueueSize)
{
    int cat, type, id;
    unsigned char *wkb;
    int wkbsize;

    #if QGISDEBUG > 3
    std::cout << "QgsGrassProvider::getNextFeature()" << std::endl;
    #endif

    if ( isEdited() || isFrozen() || !mValid )
	return false;
    
    if ( mCidxFieldIndex < 0 ) return false; // No features, no features in this layer
    
    // Get next line/area id
    int found = 0;
    while ( mNextCidx < mCidxFieldNumCats ) {
	Vect_cidx_get_cat_by_index ( mMap, mCidxFieldIndex, mNextCidx++, &cat, &type, &id );
	// Warning: selection array is only of type line/area of current layer -> check type first

	if ( !(type & mGrassType) ) continue;
	if ( !mSelection[id] ) continue;
        found = 1;
	break;
    }
    if ( !found ) return false; // No more features
    #if QGISDEBUG > 3
    std::cout << "cat = " << cat << " type = " << type << " id = " << id << std::endl;
    #endif

    feature = QgsFeature(id);

    // TODO int may be 64 bits (memcpy)
    if ( type & (GV_POINTS | GV_LINES) ) { /* points or lines */
	Vect_read_line ( mMap, mPoints, mCats, id);
	int npoints = mPoints->n_points;
	
	if ( type & GV_POINTS ) {
	    wkbsize = 1 + 4 + 2*8;
	} else { // GV_LINES
	    wkbsize = 1+4+4+npoints*2*8;
	}	    
	wkb = new unsigned char[wkbsize];
	unsigned char *wkbp = wkb;
	wkbp[0] = (unsigned char) QgsApplication::endian();
	wkbp += 1;

	/* WKB type */
	memcpy (wkbp, &mQgisType, 4);
	wkbp += 4;
	
	/* number of points */
	if ( type & GV_LINES ) {
	    memcpy (wkbp, &npoints, 4);
	    wkbp += 4;
	}
	
	for ( int i = 0; i < npoints; i++ ) {
	    memcpy (wkbp, &(mPoints->x[i]), 8);
	    memcpy (wkbp+8, &(mPoints->y[i]), 8);
	    wkbp += 16;
	}
    } else { // GV_AREA
	Vect_get_area_points ( mMap, id, mPoints );
	int npoints = mPoints->n_points;

	wkbsize = 1+4+4+4+npoints*2*8; // size without islands
	wkb = new unsigned char[wkbsize];
	wkb[0] = (unsigned char) QgsApplication::endian();
	int offset = 1;

	/* WKB type */
	memcpy ( wkb+offset, &mQgisType, 4);
	offset += 4;

	/* Number of rings */
	int nisles = Vect_get_area_num_isles ( mMap, id );
	int nrings = 1 + nisles; 
	memcpy (wkb+offset, &nrings, 4);
	offset += 4;

	/* Outer ring */
	memcpy (wkb+offset, &npoints, 4);
	offset += 4;
	for ( int i = 0; i < npoints; i++ ) {
	    memcpy (wkb+offset, &(mPoints->x[i]), 8);
	    memcpy (wkb+offset+8, &(mPoints->y[i]), 8);
	    offset += 16;
	}
	
	/* Isles */
	for ( int i = 0; i < nisles; i++ ) {
	    Vect_get_isle_points ( mMap, Vect_get_area_isle (mMap, id, i), mPoints );
	    npoints = mPoints->n_points;
	    
	    // add space
	    wkbsize += 4+npoints*2*8;
	    wkb = (unsigned char *) realloc (wkb, wkbsize);

	    memcpy (wkb+offset, &npoints, 4);
	    offset += 4;
	    for ( int i = 0; i < npoints; i++ ) {
		memcpy (wkb+offset, &(mPoints->x[i]), 8);
		memcpy (wkb+offset+8, &(mPoints->y[i]), 8);
		offset += 16;
	    }
	}
    }

    feature.setGeometryAndOwnership(wkb, wkbsize);

    setFeatureAttributes( mLayerId, cat, &feature, attlist );  
    
    return true;
}
#endif //0

bool QgsGrassProvider::getNextFeature(QgsFeature& feature, uint featureQueueSize)
{
  return false; //soon...
}

void QgsGrassProvider::resetSelection( bool sel)
{
    #ifdef QGISDEBUG
    std::cout << "QgsGrassProvider::resetSelection()" << std::endl;
    #endif
    if ( !mValid ) return;
    memset ( mSelection, (int) sel, mSelectionSize );
    mNextCidx = 0;
}

#if 0
/**
* Select features based on a bounding rectangle. Features can be retrieved
* with calls to getFirstFeature and getNextFeature.
* @param mbr QgsRect containing the extent to use in selecting features
*/
void QgsGrassProvider::select(QgsRect rect, bool useIntersect)
{
    #ifdef QGISDEBUG
    std::cout << "QgsGrassProvider::select() useIntersect = " << useIntersect << std::endl;
    #endif

    if ( isEdited() || isFrozen() || !mValid )
	return;

    // check if outdated and update if necessary
    int mapId = mLayers[mLayerId].mapId;
    if ( mapOutdated(mapId) ) {
        updateMap ( mapId );
    }
    if ( mMapVersion < mMaps[mapId].version ) {
        update();
    }
    if ( attributesOutdated(mapId) ) {
	loadAttributes (mLayers[mLayerId]);
    }

    resetSelection(0);
    
    if ( !useIntersect ) { // select by bounding boxes only
	BOUND_BOX box;
	box.N = rect.yMax(); box.S = rect.yMin(); 
	box.E = rect.xMax(); box.W = rect.xMin(); 
	box.T = PORT_DOUBLE_MAX; box.B = -PORT_DOUBLE_MAX; 
	if ( mLayerType == POINT || mLayerType == CENTROID || mLayerType == LINE || mLayerType == BOUNDARY ) {
	    Vect_select_lines_by_box(mMap, &box, mGrassType, mList);
	} else if ( mLayerType == POLYGON ) {
	    Vect_select_areas_by_box(mMap, &box, mList);
	}

    } else { // check intersection
	struct line_pnts *Polygon;
	
	Polygon = Vect_new_line_struct();

	Vect_append_point( Polygon, rect.xMin(), rect.yMin(), 0);
	Vect_append_point( Polygon, rect.xMax(), rect.yMin(), 0);
	Vect_append_point( Polygon, rect.xMax(), rect.yMax(), 0);
	Vect_append_point( Polygon, rect.xMin(), rect.yMax(), 0);
	Vect_append_point( Polygon, rect.xMin(), rect.yMin(), 0);

	if ( mLayerType == POINT || mLayerType == CENTROID || mLayerType == LINE || mLayerType == BOUNDARY ) {
	    Vect_select_lines_by_polygon ( mMap, Polygon, 0, NULL, mGrassType, mList);
	} else if ( mLayerType == POLYGON ) {
	    Vect_select_areas_by_polygon ( mMap, Polygon, 0, NULL, mList);
	}

	Vect_destroy_line_struct (Polygon);
    }
    for ( int i = 0; i < mList->n_values; i++ ) {
        if ( mList->value[i] <= mSelectionSize ) {
	    mSelection[mList->value[i]] = 1;
	} else {
	    std::cerr << "Selected element out of range" << std::endl;
	}
    }
	
    #ifdef QGISDEBUG
    std::cout << mList->n_values << " features selected" << std::endl;
    #endif
}
#endif //0

void QgsGrassProvider::select(QgsAttributeList fetchAttributes, QgsRect rect, bool fetchGeometry, \
			      bool useIntersect)
{
  return; //soon...
}



QgsRect QgsGrassProvider::extent()
{
    BOUND_BOX box;
    Vect_get_map_box ( mMap, &box );

    return QgsRect( box.W, box.S, box.E, box.N);
}

/** 
* Return the feature type
*/
QGis::WKBTYPE QgsGrassProvider::geometryType() const
{
    return mQgisType;
}
/** 
* Return the feature type
*/
long QgsGrassProvider::featureCount() const 
{
    return mNumberFeatures;
}

/**
* Return the number of fields
*/
uint QgsGrassProvider::fieldCount() const
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::fieldCount() return:" << mLayers[mLayerId].fields.size() << std::endl;
    #endif
    return mLayers[mLayerId].fields.size();
}

/**
* Return fields
*/
const QgsFieldMap & QgsGrassProvider::fields() const
{
      return mLayers[mLayerId].fields;
}

int QgsGrassProvider::keyField()
{
      return mLayers[mLayerId].keyColumn;
}

void QgsGrassProvider::reset()
{
    if ( isEdited() || isFrozen() || !mValid )
	return;

    int mapId = mLayers[mLayerId].mapId;
    if ( mapOutdated(mapId) ) {
        updateMap ( mapId );
    }
    if ( mMapVersion < mMaps[mapId].version ) {
        update();
    }
    if ( attributesOutdated(mapId) ) {
	loadAttributes (mLayers[mLayerId]);
    }
    
    resetSelection(1);
    mNextCidx = 0;
}

QString QgsGrassProvider::minValue(uint position)
{
    if ( position >= fieldCount() ) {
	std::cerr << "Warning: access requested to invalid position in QgsGrassProvider::minValue()" 
	          << std::endl;
    }
    return QString::number( mLayers[mLayerId].minmax[position][0], 'f', 2 );
}

 
QString QgsGrassProvider::maxValue(uint position)
{
    if ( position >= fieldCount() ) {
	std::cerr << "Warning: access requested to invalid position in QgsGrassProvider::maxValue()" 
	          << std::endl;
    }
    return QString::number( mLayers[mLayerId].minmax[position][1], 'f', 2 );
}

bool QgsGrassProvider::isValid(){
    #ifdef QGISDEBUG
    QString validString = mValid?"true":"false";
    std::cerr << "QgsGrassProvider::isValid() returned: " << validString.toLocal8Bit().data() << std::endl;
    #endif
    return mValid;
}

// ------------------------------------------------------------------------------------------------------
// Compare categories in GATT
static int cmpAtt ( const void *a, const void *b ) {
    GATT *p1 = (GATT *) a;
    GATT *p2 = (GATT *) b;
    return (p1->cat - p2->cat);
}

/* returns layerId or -1 on error */
int QgsGrassProvider::openLayer(QString gisdbase, QString location, QString mapset, QString mapName, int field)
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::openLayer()" << std::endl;
    std::cerr << "gisdbase: " << gisdbase.toLocal8Bit().data() << std::endl;
    std::cerr << "location: " << location.toLocal8Bit().data() << std::endl;
    std::cerr << "mapset: "   << mapset.toLocal8Bit().data() << std::endl;
    std::cerr << "mapName: "  << mapName.toLocal8Bit().data() << std::endl;
    std::cerr << "field: "    << field << std::endl;
    #endif

    // Check if this layer is already opened

    for ( unsigned int i = 0; i <  mLayers.size(); i++) {
	if ( !(mLayers[i].valid) ) continue;

	GMAP *mp = &(mMaps[mLayers[i].mapId]);

	if ( mp->gisdbase == gisdbase && mp->location == location && 
	     mp->mapset == mapset && mp->mapName == mapName && mLayers[i].field == field )
	{
	    // the layer already exists, return layer id
	    #ifdef QGISDEBUG
	    std::cerr << "The layer is already opened with ID = " << i << std::endl;
	    #endif
	    mLayers[i].nUsers++;
	    return i;
	}
    }

    // Create a new layer
    GLAYER layer;
    layer.valid = false;
    layer.field = field; 
    layer.nUsers = 1; 

    // Open map
    layer.mapId = openMap ( gisdbase, location, mapset, mapName );
    if ( layer.mapId < 0 ) {
	std::cerr << "Cannot open vector map" << std::endl;
	return -1;
    }
    #ifdef QGISDEBUG
    std::cerr << "layer.mapId = " << layer.mapId << std::endl;
    #endif
    layer.map = mMaps[layer.mapId].map;

    layer.attributes = 0; // because loadLayerSourcesFromMap will release old
    loadLayerSourcesFromMap ( layer );

    layer.valid = true;

    // Add new layer to layers
    mLayers.push_back(layer);
	
    #ifdef QGISDEBUG
    std::cerr << "New layer successfully opened" << layer.nAttributes << std::endl;
    #endif
        
    return mLayers.size() - 1; 
}

void QgsGrassProvider::loadLayerSourcesFromMap ( GLAYER &layer )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::loadLayerSourcesFromMap" << std::endl;
    #endif

    // Reset and free 
    layer.fields.clear();
    if ( layer.attributes ) { 
	for ( int i = 0; i < layer.nAttributes; i ++ ) {
	    for ( int j = 0; j < layer.nColumns; j ++ ) {
		if ( layer.attributes[i].values[j] )
		    free ( layer.attributes[i].values[j] );
	    }
	    free ( layer.attributes[i].values );
	}
	free ( layer.attributes );
    }
    loadAttributes ( layer );
}
    
void QgsGrassProvider::loadAttributes ( GLAYER &layer )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::loadLayerSourcesFromMap" << std::endl;
    #endif

    // TODO: free old attributes
    
    if ( !layer.map ) return;

    // Get field info
    layer.fieldInfo = Vect_get_field( layer.map, layer.field); // should work also with field = 0

    // Read attributes
    layer.nColumns = 0;
    layer.nAttributes = 0;
    layer.attributes = 0;
    layer.fields.clear();
    layer.keyColumn = -1;
    if ( layer.fieldInfo == NULL ) {
        #ifdef QGISDEBUG
	std::cerr << "No field info -> no attribute table" << std::endl;
        #endif
    } else { 
        #ifdef QGISDEBUG
	std::cerr << "Field info found -> open database" << std::endl;
        #endif
	dbDriver *databaseDriver = db_start_driver_open_database ( layer.fieldInfo->driver, 
								   layer.fieldInfo->database );

	if ( databaseDriver == NULL ) {
	    std::cerr << "Cannot open database " << layer.fieldInfo->database << " by driver " 
		      << layer.fieldInfo->driver << std::endl;
	} else {
            #ifdef QGISDEBUG
	    std::cerr << "Database opened -> open select cursor" << std::endl;
            #endif
	    dbString dbstr; 
	    db_init_string (&dbstr);
	    db_set_string (&dbstr, "select * from ");
	    db_append_string (&dbstr, layer.fieldInfo->table);
	    
            #ifdef QGISDEBUG
	    std::cerr << "SQL: " << db_get_string(&dbstr) << std::endl;
            #endif
	    dbCursor databaseCursor;
	    if ( db_open_select_cursor(databaseDriver, &dbstr, &databaseCursor, DB_SCROLL) != DB_OK ){
		layer.nColumns = 0;
		db_close_database_shutdown_driver ( databaseDriver );
		QMessageBox::warning( 0, "Warning", "Cannot select attributes from table '" + 
			         QString(layer.fieldInfo->table) + "'" );
	    } else {
		int nRecords = db_get_num_rows ( &databaseCursor );
                #ifdef QGISDEBUG
		std::cerr << "Number of records: " << nRecords << std::endl;
                #endif
		
		dbTable  *databaseTable = db_get_cursor_table (&databaseCursor);
		layer.nColumns = db_get_table_number_of_columns(databaseTable);

		layer.minmax = new double[layer.nColumns][2];

		// Read columns' description 
		for (int i = 0; i < layer.nColumns; i++) {
		    layer.minmax[i][0] = DBL_MAX;
		    layer.minmax[i][1] = -DBL_MAX;

		    dbColumn *column = db_get_table_column (databaseTable, i);

		    int ctype = db_sqltype_to_Ctype ( db_get_column_sqltype(column) );
        QVariant::Type qtype;
                    #ifdef QGISDEBUG
		    std::cerr << "column = " << db_get_column_name(column) 
			      << " ctype = " << ctype << std::endl;
                    #endif
		    
		    QString ctypeStr;
		    switch ( ctype ) {
			case DB_C_TYPE_INT:
			    ctypeStr = "integer";
          qtype = QVariant::Int;
			    break; 
			case DB_C_TYPE_DOUBLE:
			    ctypeStr = "double";
          qtype = QVariant::Double;
			    break; 
			case DB_C_TYPE_STRING:
			    ctypeStr = "string";
          qtype = QVariant::String;
			    break; 
			case DB_C_TYPE_DATETIME:
			    ctypeStr = "datetime";
          qtype = QVariant::String;
			    break; 
		    }
		    layer.fields[i] = QgsField( db_get_column_name(column), qtype, ctypeStr,
		                     db_get_column_length(column), db_get_column_precision(column) );
		    
		    if ( G_strcasecmp ( db_get_column_name(column), layer.fieldInfo->key) == 0 ) {
			layer.keyColumn = i;
		    }
		}
    
		if ( layer.keyColumn < 0 ) {
		    layer.fields.clear();
                    layer.nColumns = 0;

		    QMessageBox::warning( 0, "Warning", "Key column '" + QString(layer.fieldInfo->key) + 
			         "' not found in the table '" + QString(layer.fieldInfo->table) + "'" );
		} else {
		    // Read attributes to the memory
		    layer.attributes = (GATT *) malloc ( nRecords * sizeof(GATT) );
		    while ( 1 ) {
			int more;
				
			if ( db_fetch (&databaseCursor, DB_NEXT, &more) != DB_OK ) {
			    std::cout << "Cannot fetch DB record" << std::endl;
			    break;
			}
			if ( !more ) break; // no more records

			// Check cat value
			dbColumn *column = db_get_table_column (databaseTable, layer.keyColumn);
			dbValue *value = db_get_column_value(column);
			
			if ( db_test_value_isnull(value) ) continue;
			layer.attributes[layer.nAttributes].cat = db_get_value_int (value);
			if ( layer.attributes[layer.nAttributes].cat < 0 ) continue; 

			layer.attributes[layer.nAttributes].values = (char **) malloc ( layer.nColumns * sizeof(char*) );

			for (int i = 0; i < layer.nColumns; i++) {
			    column = db_get_table_column (databaseTable, i);
			    int sqltype = db_get_column_sqltype(column);
			    int ctype = db_sqltype_to_Ctype ( sqltype );
			    value = db_get_column_value(column);
			    db_convert_value_to_string ( value, sqltype, &dbstr);

			    #if QGISDEBUG > 3
			    std::cout << "column: " << db_get_column_name(column) << std::endl;
			    std::cout << "value: " << db_get_string(&dbstr) << std::endl;
			    #endif

			    layer.attributes[layer.nAttributes].values[i] = strdup ( db_get_string(&dbstr) );
			    if ( !db_test_value_isnull(value) )
                            {
				double dbl;
				if ( ctype == DB_C_TYPE_INT ) {
				    dbl = db_get_value_int ( value );
				} else if ( ctype == DB_C_TYPE_DOUBLE ) {
				    dbl = db_get_value_double ( value );
				} else {
				    dbl = 0;
				}
				
				if ( dbl < layer.minmax[i][0] ) {
				    layer.minmax[i][0] = dbl;
				}
				if ( dbl > layer.minmax[i][1] ) {
				    layer.minmax[i][1] = dbl;
				}
                            }
			}
			layer.nAttributes++;
		    }
		    // Sort attributes by category
		    qsort ( layer.attributes, layer.nAttributes, sizeof(GATT), cmpAtt );
		}
		db_close_cursor (&databaseCursor);
		db_close_database_shutdown_driver ( databaseDriver );
		db_free_string(&dbstr);

                #ifdef QGISDEBUG
		std::cerr << "fields.size = " << layer.fields.size() << std::endl;
		std::cerr << "number of attributes = " << layer.nAttributes << std::endl;
                #endif

	    }
	}
    }

    // Add cat if no attribute fields exist (otherwise qgis crashes)
    if ( layer.nColumns == 0 ) {
        layer.keyColumn = 0;
	layer.fields[0] = ( QgsField( "cat", QVariant::Int, "integer") );
	layer.minmax = new double[1][2];
	layer.minmax[0][0] = 0; 
	layer.minmax[0][1] = 0; 

	int cidx = Vect_cidx_get_field_index ( layer.map, layer.field );
	if ( cidx >= 0 ) {
	    int ncats, cat, type, id;
	    
	    ncats = Vect_cidx_get_num_cats_by_index ( layer.map, cidx );

	    if ( ncats > 0 ) {
		Vect_cidx_get_cat_by_index ( layer.map, cidx, 0, &cat, &type, &id );
	        layer.minmax[0][0] = cat; 

	        Vect_cidx_get_cat_by_index ( layer.map, cidx, ncats-1, &cat, &type, &id );
	        layer.minmax[0][1] = cat; 
	    }
	}
    }

    GMAP *map = &(mMaps[layer.mapId]);
    
    QFileInfo di ( map->gisdbase + "/" + map->location + "/" + map->mapset + "/vector/" + map->mapName + "/dbln" );
    map->lastAttributesModified = di.lastModified();
}

void QgsGrassProvider::closeLayer( int layerId )
{
    #ifdef QGISDEBUG
    std::cerr << "Close layer " << layerId << " nUsers = " << mLayers[layerId].nUsers << std::endl;
    #endif

    // TODO: not tested because delete is never used for providers
    mLayers[layerId].nUsers--;

    if ( mLayers[layerId].nUsers == 0 ) { // No more users, free sources
        #ifdef QGISDEBUG
        std::cerr << "No more users -> delete layer" << std::endl;
        #endif

        mLayers[layerId].valid = false;

	// Column names/types
	mLayers[layerId].fields.clear();
	
	// Attributes
        #ifdef QGISDEBUG
        std::cerr << "Delete attribute values" << std::endl;
        #endif
	for ( int i = 0; i < mLayers[layerId].nAttributes; i++ ) {
	    free ( mLayers[layerId].attributes[i].values );
	}
	free ( mLayers[layerId].attributes );
		
	delete[] mLayers[layerId].minmax;

	// Field info
	free ( mLayers[layerId].fieldInfo );

	closeMap ( mLayers[layerId].mapId );
    }
}

/* returns mapId or -1 on error */
int QgsGrassProvider::openMap(QString gisdbase, QString location, QString mapset, QString mapName)
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::openMap()" << std::endl;
    #endif

    QString tmpPath = gisdbase + "/" + location + "/" + mapset + "/" + mapName;

    // Check if this map is already opened
    for ( unsigned int i = 0; i <  mMaps.size(); i++) {
	if ( mMaps[i].valid && mMaps[i].path == tmpPath ) 
	{
	    // the map is already opened, return map id
            #ifdef QGISDEBUG
	    std::cerr << "The map is already opened with ID = " << i << std::endl;
            #endif
	    mMaps[i].nUsers++;
	    return i;
	}
    }

    GMAP map;
    map.valid = false;
    map.frozen = false;
    map.gisdbase = gisdbase;
    map.location = location;
    map.mapset = mapset;
    map.mapName = mapName;
    map.path = tmpPath;
    map.nUsers = 1;
    map.version = 1;
    map.update = 0;
    map.map = (struct Map_info *) malloc ( sizeof(struct Map_info) );

    // Set GRASS location
    QgsGrass::setLocation ( gisdbase, location ); 
#ifdef QGISDEBUG
	std::cerr << "Setting  gisdbase, location: " << gisdbase.toLocal8Bit().data() << ", " << location.toLocal8Bit().data() << std::endl;
#endif

    // Find the vector
    char *ms = G_find_vector2 ( (char *) mapName.ascii(), (char *) mapset.ascii()) ;

    if ( ms == NULL) {
        std::cerr << "Cannot find GRASS vector" << std::endl;
	return -1;
    }

    // Read the time of vector dir before Vect_open_old, because it may take long time (when the vector
    // could be owerwritten)
    QFileInfo di ( gisdbase + "/" + location + "/" + mapset + "/vector/" + mapName );
    map.lastModified = di.lastModified();
    
    di.setFile ( gisdbase + "/" + location + "/" + mapset + "/vector/" + mapName + "/dbln" );
    map.lastAttributesModified = di.lastModified();

    // Do we have topology and cidx (level2)
    int level = 2;
    QgsGrass::resetError();
    Vect_set_open_level (2);
    Vect_open_old_head ( map.map, (char *) mapName.ascii(), (char *) mapset.ascii());
    if ( QgsGrass::getError() == QgsGrass::FATAL ) {
	std::cerr << "Cannot open GRASS vector head on level2: " 
                  << QgsGrass::getErrorMessage().toLocal8Bit().data() << std::endl;
        level = 1;
    }
    else
    {
        Vect_close ( map.map );
    }

    if ( level == 1 )
    {
        QMessageBox::StandardButton ret = QMessageBox::question ( 0, "Warning",
                      "GRASS vector map " + mapName + 
                      + " does not have topology. Build topology?",
                      QMessageBox::Ok | QMessageBox::Cancel );

        if ( ret == QMessageBox::Cancel ) return -1;
    }

    // Open vector
    QgsGrass::resetError(); // to "catch" error after Vect_open_old()
    Vect_set_open_level (level);
    Vect_open_old ( map.map, (char *) mapName.ascii(), (char *) mapset.ascii());

    if ( QgsGrass::getError() == QgsGrass::FATAL ) {
	std::cerr << "Cannot open GRASS vector: " << QgsGrass::getErrorMessage().toLocal8Bit().data() << std::endl;
	return -1;
    }
 
    if ( level == 1 )
    {
        QgsGrass::resetError();
	Vect_build ( map.map, stderr );

	if ( QgsGrass::getError() == QgsGrass::FATAL ) {
	    std::cerr << "Cannot build topology: " 
                      << QgsGrass::getErrorMessage().toLocal8Bit().data() 
                      << std::endl;
	    return -1;
	}
    }

    #ifdef QGISDEBUG
    std::cerr << "GRASS map successfully opened" << std::endl;
    #endif
    
    map.valid = true;

    // Add new map to maps
    mMaps.push_back(map);

    return mMaps.size() - 1; // map id 
}

void QgsGrassProvider::updateMap ( int mapId )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::updateMap() mapId = " << mapId << std::endl;
    #endif

    /* Close map */
    GMAP *map = &(mMaps[mapId]);

    bool closeMap = map->valid;
    map->valid = false;
    map->version++;

    QgsGrass::setLocation ( (char *) map->gisdbase.ascii(), (char *) map->location.ascii() ); 

    // TODO: Should be done better / in other place ?
    // TODO: Is it necessary for close ?
    G__setenv( "MAPSET", (char *) map->mapset.ascii() );
    
    if ( closeMap ) Vect_close ( map->map );

    QFileInfo di ( map->gisdbase + "/" + map->location + "/" + map->mapset + "/vector/" + map->mapName );
    map->lastModified = di.lastModified();

    di.setFile ( map->gisdbase + "/" + map->location + "/" + map->mapset + "/vector/" + map->mapName + "/dbln" );
    map->lastAttributesModified = di.lastModified();

    // Reopen vector
    QgsGrass::resetError(); // to "catch" error after Vect_open_old()
    Vect_set_open_level (2);
    Vect_open_old ( map->map, (char *) map->mapName.ascii(), (char *) map->mapset.ascii());

    if ( QgsGrass::getError() == QgsGrass::FATAL ) {
	std::cerr << "Cannot reopen GRASS vector: " << QgsGrass::getErrorMessage().toLocal8Bit().data() << std::endl;

	// TODO if reopen fails, mLayers should be also updated
	return;
    }

    #ifdef QGISDEBUG
    std::cerr << "GRASS map successfully reopened for reading." << std::endl;
    #endif

    for ( unsigned int i = 0; i <  mLayers.size(); i++) {
	// if ( !(mLayers[i].valid) ) continue; // ?

	if  ( mLayers[i].mapId == mapId ) {
            loadLayerSourcesFromMap ( mLayers[i] );
	}
    }

    map->valid = true;
}

void QgsGrassProvider::closeMap( int mapId )
{
    #ifdef QGISDEBUG
    std::cerr << "Close map " << mapId << " nUsers = " << mMaps[mapId].nUsers << std::endl;
    #endif

    // TODO: not tested because delete is never used for providers
    mMaps[mapId].nUsers--;

    if ( mMaps[mapId].nUsers == 0 ) { // No more users, free sources
        #ifdef QGISDEBUG
        std::cerr << "No more users -> delete map" << std::endl;
        #endif

        // TODO: do this better, probably maintain QgsGrassEdit as one user
	if ( mMaps[mapId].update ) {
	    QMessageBox::warning( 0, "Warning", "The vector was currently edited, "
		                     "you can expect crash soon." );
	}

	if ( mMaps[mapId].valid )
	{
	    Vect_close ( mMaps[mapId].map );
	}
        mMaps[mapId].valid = false;
    }
}

bool QgsGrassProvider::mapOutdated( int mapId )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::mapOutdated()" << std::endl;
    #endif

    GMAP *map = &(mMaps[mapId]);
    
    QString dp = map->gisdbase + "/" + map->location + "/" + map->mapset + "/vector/" + map->mapName;
    QFileInfo di ( dp );

    if ( map->lastModified < di.lastModified() ) {
	#ifdef QGISDEBUG
	std::cerr << "**** The map " << mapId << " was modified ****" << std::endl;
	#endif
	
	return true;
    }

    return false;
}

bool QgsGrassProvider::attributesOutdated( int mapId )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::attributesOutdated()" << std::endl;
    #endif

    GMAP *map = &(mMaps[mapId]);
    
    QString dp = map->gisdbase + "/" + map->location + "/" + map->mapset + "/vector/" + map->mapName + "/dbln";
    QFileInfo di ( dp );

    if ( map->lastAttributesModified < di.lastModified() ) {
	#ifdef QGISDEBUG
	std::cerr << "**** The attributes of the map " << mapId << " were modified ****" << std::endl;
	#endif
	
	return true;
    }

    return false;
}

/** Set feature attributes */
void QgsGrassProvider::setFeatureAttributes ( int layerId, int cat, QgsFeature *feature )
{
    #if QGISDEBUG > 3
    std::cerr << "setFeatureAttributes cat = " << cat << std::endl;
    #endif
    if ( mLayers[layerId].nColumns > 0 ) {
	// find cat
	GATT key;
	key.cat = cat;
    
	GATT *att = (GATT *) bsearch ( &key, mLayers[layerId].attributes, mLayers[layerId].nAttributes,
		                       sizeof(GATT), cmpAtt);

	for (int i = 0; i < mLayers[layerId].nColumns; i++) {
	    if ( att != NULL ) {
		Q3CString cstr( att->values[i] );
		feature->addAttribute (i, QVariant(mEncoding->toUnicode(cstr)) );
	    } else { /* it may happen that attributes are missing -> set to empty string */
		feature->addAttribute (i, QVariant());
	    }
	}
    } else { 
	feature->addAttribute (0, QVariant(cat));
    }
}

void QgsGrassProvider::setFeatureAttributes ( int layerId, int cat, QgsFeature *feature, const QgsAttributeList& attlist)
{
    #if QGISDEBUG > 3
    std::cerr << "setFeatureAttributes cat = " << cat << std::endl;
    #endif
    if ( mLayers[layerId].nColumns > 0 ) {
	// find cat
	GATT key;
	key.cat = cat;
	GATT *att = (GATT *) bsearch ( &key, mLayers[layerId].attributes, mLayers[layerId].nAttributes,
		                       sizeof(GATT), cmpAtt);

	for (QgsAttributeList::const_iterator iter=attlist.begin(); iter!=attlist.end();++iter) {
	    if ( att != NULL ) {
		Q3CString cstr( att->values[*iter] );
		feature->addAttribute (*iter, QVariant( mEncoding->toUnicode(cstr) ));
	    } else { /* it may happen that attributes are missing -> set to empty string */
		feature->addAttribute (*iter, QVariant());
	    } 
	}
    } else { 
	feature->addAttribute (0, QVariant(cat));
    }
}

/** Get pointer to map */
struct Map_info *QgsGrassProvider::layerMap ( int layerId )
{
    return ( mMaps[mLayers[layerId].mapId].map );
}


QgsSpatialRefSys QgsGrassProvider::getSRS()
{
    QString WKT;

    struct Cell_head cellhd;

    QgsGrass::setLocation ( mGisdbase, mLocation ); 
    G_get_default_window(&cellhd);
    if (cellhd.proj != PROJECTION_XY) {
        struct Key_Value *projinfo = G_get_projinfo();
        struct Key_Value *projunits = G_get_projunits();
	char *wkt = GPJ_grass_to_wkt ( projinfo, projunits,  0, 0 );
	WKT = QString(wkt);
	free ( wkt);
    }
    
    QgsSpatialRefSys srs;
    srs.createFromWkt(WKT);
    
    return srs;
}

int QgsGrassProvider::grassLayer()
{
	return mLayerField;
}

int QgsGrassProvider::grassLayer(QString name)
{
    // Get field number
    int pos = name.find('_');

    if ( pos == -1 ) {
	return -1;
    }

    return name.left(pos).toInt();
}

int QgsGrassProvider::grassLayerType(QString name)
{
    int pos = name.find('_');

    if ( pos == -1 ) {
	return -1;
    }

    QString ts = name.right( name.length() - pos - 1 );
    if ( ts.compare("point") == 0 ) {
	return GV_POINT; // ?! centroids may be points
    } else if ( ts.compare("line") == 0 ) {
	return GV_LINES; 
    } else if ( ts.compare("polygon") == 0 ) {
	return GV_AREA; 
    }

    return -1;
}

//-----------------------------------------  Edit -------------------------------------------------------

bool QgsGrassProvider::isGrassEditable ( void )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::isGrassEditable" << std::endl;
    #endif

    if ( !isValid() ) 
	return false;

    /* Check if current user is owner of mapset */
    if ( G__mapset_permissions2((char*)mGisdbase.ascii(),(char*)mLocation.ascii(),(char*)mMapset.ascii()) != 1 )
	return false;

    // TODO: check format? (cannot edit OGR layers)

    return true;
}

bool QgsGrassProvider::isEdited ( void )
{
    #if QGISDEBUG > 3
    std::cerr << "QgsGrassProvider::isEdited" << std::endl;
    #endif

    GMAP *map = &(mMaps[mLayers[mLayerId].mapId]);
    return (map->update);
}

bool QgsGrassProvider::isFrozen ( void )
{
    #if QGISDEBUG > 3
    std::cerr << "QgsGrassProvider::isFrozen" << std::endl;
    #endif

    GMAP *map = &(mMaps[mLayers[mLayerId].mapId]);
    return (map->frozen);
}

void QgsGrassProvider::freeze()
{
#ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::freeze" << std::endl;
#endif

    if ( !isValid() ) return;

    GMAP *map = &(mMaps[mLayers[mLayerId].mapId]);

    if ( map->frozen ) return;
    
    map->frozen = true;
    Vect_close ( map->map );
}

void QgsGrassProvider::thaw()
{
#ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::thaw" << std::endl;
#endif

    if ( !isValid() ) return;
    GMAP *map = &(mMaps[mLayers[mLayerId].mapId]);

    if ( !map->frozen ) return;

    if ( reopenMap() ) 
    {
        map->frozen = false;
    }
}

bool QgsGrassProvider::startEdit ( void )
{
#ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::startEdit" << std::endl;
    std::cerr << "  uri = " << dataSourceUri().toLocal8Bit().data() << std::endl;
    std::cerr << "  mMaps.size() = " << mMaps.size() << std::endl;
#endif

    if ( !isGrassEditable() )
	return false;

    // Check number of maps (the problem may appear if static variables are not shared - runtime linker)
    if ( mMaps.size() == 0 ) {
	QMessageBox::warning( 0, "Warning", "No maps opened in mMaps, probably problem in runtime linking, " 
			      "static variables are not shared by provider and plugin." );
	return false;
    }

    /* Close map */
    GMAP *map = &(mMaps[mLayers[mLayerId].mapId]);
    map->valid = false;

    QgsGrass::setLocation ( (char *) map->gisdbase.ascii(), (char *) map->location.ascii() ); 

    // Set current mapset (mapset was previously checked by isGrassEditable() )
    // TODO: Should be done better / in other place ?
    G__setenv( "MAPSET", (char *) map->mapset.ascii() );

    Vect_close ( map->map );

    // TODO: Catch error 
     
    QgsGrass::resetError();
    int level = Vect_open_update ( map->map, (char *) map->mapName.ascii(), (char *) map->mapset.ascii() );
    if (  level < 2 ) { 
	if ( QgsGrass::getError() == QgsGrass::FATAL ) {
	    std::cerr << "Cannot open GRASS vector for update: " << QgsGrass::getErrorMessage().toLocal8Bit().data() << std::endl;
	} else {
	    std::cerr << "Cannot open GRASS vector for update on level 2." << std::endl;
	}
	
	// reopen vector for reading
	QgsGrass::resetError();
	Vect_set_open_level (2);
        level = Vect_open_old ( map->map, (char *) map->mapName.ascii(), (char *) map->mapset.ascii() );
    
	if ( level < 2 ) {
	    if ( QgsGrass::getError() == QgsGrass::FATAL ) {
		std::cerr << "Cannot reopen GRASS vector: " << QgsGrass::getErrorMessage().toLocal8Bit().data() << std::endl;
	    } else {
		std::cerr << "Cannot reopen GRASS vector on level 2." << std::endl;
	    }
	} else {
	    map->valid = true;
	}
	
	return false;
    }
    Vect_set_category_index_update ( map->map );

    // Write history
    Vect_hist_command ( map->map );

    #ifdef QGISDEBUG
    std::cerr << "Vector successfully reopened for update." << std::endl;
    #endif

    map->update = true;
    map->valid = true;

    return true;
}

bool QgsGrassProvider::closeEdit ( bool newMap )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::closeEdit" << std::endl;
    #endif

    if ( !isValid() )
	return false;

    /* Close map */
    GMAP *map = &(mMaps[mLayers[mLayerId].mapId]);
    
    if ( !(map->update) )
	return false;

    map->valid = false;
    map->version++;

    QgsGrass::setLocation ( (char *) map->gisdbase.ascii(), (char *) map->location.ascii() ); 

    // Set current mapset (mapset was previously checked by isGrassEditable() )
    // TODO: Should be done better / in other place ?
    // TODO: Is it necessary for build/close ?
    G__setenv( "MAPSET", (char *) map->mapset.ascii() );
    
    Vect_build_partial ( map->map, GV_BUILD_NONE, NULL);
    Vect_build ( map->map, stderr );

    // If a new map was created close the map and return
    if ( newMap )
    {
        std::cerr << "mLayers.size() = " << mLayers.size() << std::endl;
        map->update = false;
        // Map must be set as valid otherwise it is not closed and topo is not written
	map->valid = true;
        closeLayer( mLayerId );
        return true;
    }

    Vect_close ( map->map );

    map->update = false;

    if ( !reopenMap() ) return false;

    map->valid = true;

    return true;
}

bool QgsGrassProvider::reopenMap()
{
    GMAP *map = &(mMaps[mLayers[mLayerId].mapId]);

    QFileInfo di ( mGisdbase + "/" + mLocation + "/" + mMapset + "/vector/" + mMapName );
    map->lastModified = di.lastModified();

    di.setFile ( mGisdbase + "/" + mLocation + "/" + mMapset + "/vector/" + mMapset + "/dbln" );
    map->lastAttributesModified = di.lastModified();

    // Reopen vector
    QgsGrass::resetError(); // to "catch" error after Vect_open_old()
    Vect_set_open_level (2);

    Vect_open_old ( map->map, (char *) map->mapName.ascii(), (char *) map->mapset.ascii());

    if ( QgsGrass::getError() == QgsGrass::FATAL ) {
	std::cerr << "Cannot reopen GRASS vector: " << QgsGrass::getErrorMessage().toLocal8Bit().data() << std::endl;
	return false;
    }

    #ifdef QGISDEBUG
    std::cerr << "GRASS map successfully reopened for reading." << std::endl;
    #endif

    // Reload sources to layers
    for ( unsigned int i = 0; i <  mLayers.size(); i++) {
	// if ( !(mLayers[i].valid) ) continue; // ?

	if  ( mLayers[i].mapId == mLayers[mLayerId].mapId ) {
            loadLayerSourcesFromMap ( mLayers[i] );
	}
    }

    return true;
}

int QgsGrassProvider::numLines ( void )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::numLines" << std::endl;
    #endif

    return ( Vect_get_num_lines(mMap) );
}

int QgsGrassProvider::numNodes ( void )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::numNodes" << std::endl;
    #endif

    return ( Vect_get_num_nodes(mMap) );
}

int QgsGrassProvider::readLine ( struct line_pnts *Points, struct line_cats *Cats, int line )
{
    #if QGISDEBUG > 3
    std::cerr << "QgsGrassProvider::readLine" << std::endl;
    #endif

    if ( Points )
	Vect_reset_line ( Points );

    if ( Cats )
	Vect_reset_cats ( Cats );

    if ( !Vect_line_alive(mMap, line) ) return -1;

    return ( Vect_read_line(mMap, Points, Cats, line) );
}

bool QgsGrassProvider::nodeCoor ( int node, double *x, double *y )
{
    #if QGISDEBUG > 3
    std::cerr << "QgsGrassProvider::nodeCoor" << std::endl;
    #endif

    if ( !Vect_node_alive ( mMap, node) ) {
        *x = 0.0;
        *y = 0.0;
	return false;
    }
    
    Vect_get_node_coor ( mMap, node, x, y, NULL);
    return true;
}

bool QgsGrassProvider::lineNodes ( int line, int *node1, int *node2 )
{
    #if QGISDEBUG > 3
    std::cerr << "QgsGrassProvider::lineNodes" << std::endl;
    #endif
    
    if ( !Vect_line_alive(mMap, line) ) {
        *node1 = 0;
        *node2 = 0;
	return false;
    }
    
    Vect_get_line_nodes ( mMap, line, node1, node2 );
    return true;
}

int QgsGrassProvider::writeLine ( int type, struct line_pnts *Points, struct line_cats *Cats )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::writeLine n_points = " << Points->n_points 
	      << " n_cats = " << Cats->n_cats << std::endl;
    #endif

    if ( !isEdited() )
	return -1;

    return ( (int) Vect_write_line(mMap,type,Points,Cats) );
}

int QgsGrassProvider::rewriteLine ( int line, int type, struct line_pnts *Points, struct line_cats *Cats )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::rewriteLine n_points = " << Points->n_points 
	      << " n_cats = " << Cats->n_cats << std::endl;
    #endif

    if ( !isEdited() )
	return -1;

    return ( Vect_rewrite_line(mMap,line,type,Points,Cats) );
}


int QgsGrassProvider::deleteLine ( int line )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::deleteLine" << std::endl;
    #endif

    if ( !isEdited() )
	return -1;

    return ( Vect_delete_line(mMap,line) );
}

int QgsGrassProvider::findLine ( double x, double y, int type, double threshold )
{
    #if QGISDEBUG > 3
    std::cerr << "QgsGrassProvider::findLine" << std::endl;
    #endif

    return ( Vect_find_line(mMap,x,y,0,type,threshold,0,0) );
}

int QgsGrassProvider::findNode ( double x, double y, double threshold )
{
    return ( Vect_find_node ( mMap, x, y, 0, threshold, 0 ) );
}

bool QgsGrassProvider::lineAreas ( int line, int *left, int *right )
{
    #if QGISDEBUG > 3
    std::cerr << "QgsGrassProvider::lineAreas" << std::endl;
    #endif
    
    if ( !Vect_line_alive(mMap, line) ) {
        *left = 0;
        *right = 0;
	return false;
    }
    
    Vect_get_line_areas ( mMap, line, left, right );
    return true;
}

int QgsGrassProvider::centroidArea ( int centroid )
{
    #if QGISDEBUG > 3
    std::cerr << "QgsGrassProvider::centroidArea" << std::endl;
    #endif
    
    if ( !Vect_line_alive(mMap, centroid) ) {
	return 0;
    }
    
    return ( Vect_get_centroid_area(mMap,centroid) );
}

int QgsGrassProvider::nodeNLines ( int node )
{
    #if QGISDEBUG > 3
    std::cerr << "QgsGrassProvider::nodeNLines" << std::endl;
    #endif
    
    if ( !Vect_node_alive(mMap, node) ) {
	return 0;
    }
    
    return ( Vect_get_node_n_lines(mMap,node) );
}

int QgsGrassProvider::nodeLine ( int node, int idx )
{
    #if QGISDEBUG > 3
    std::cerr << "QgsGrassProvider::nodeLine" << std::endl;
    #endif
    
    if ( !Vect_node_alive(mMap, node) ) {
	return 0;
    }

    return ( Vect_get_node_line(mMap,node,idx) );
}

int QgsGrassProvider::lineAlive ( int line )
{
    #if QGISDEBUG > 3
    std::cerr << "QgsGrassProvider::lineAlive" << std::endl;
    #endif
    
    return ( Vect_line_alive(mMap, line) ) ;
}

int QgsGrassProvider::nodeAlive ( int node )
{
    #if QGISDEBUG > 3
    std::cerr << "QgsGrassProvider::nodeAlive" << std::endl;
    #endif
    
    return ( Vect_node_alive(mMap, node) ) ;
}

int QgsGrassProvider::numUpdatedLines ( void )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::numUpdatedLines" << std::endl;
    std::cerr << "  numUpdatedLines = " << Vect_get_num_updated_lines(mMap) << std::endl;
    #endif
    
    return ( Vect_get_num_updated_lines(mMap) ) ;
}

int QgsGrassProvider::numUpdatedNodes ( void )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::numUpdatedNodes" << std::endl;
    std::cerr << "  numUpdatedNodes = " << Vect_get_num_updated_nodes(mMap) << std::endl;
    #endif
    
    return ( Vect_get_num_updated_nodes(mMap) ) ;
}

int QgsGrassProvider::updatedLine ( int idx )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::updatedLine idx = " << idx << std::endl;
    std::cerr << "  updatedLine = " << Vect_get_updated_line( mMap, idx ) << std::endl;
    #endif
    
    return ( Vect_get_updated_line( mMap, idx ) ) ;
}

int QgsGrassProvider::updatedNode ( int idx )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::updatedNode idx = " << idx << std::endl;
    std::cerr << "  updatedNode = " << Vect_get_updated_node( mMap, idx ) << std::endl;
    #endif
    
    return ( Vect_get_updated_node( mMap, idx ) ) ;
}

// ------------------ Attributes -------------------------------------------------

QString *QgsGrassProvider::key ( int field )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::key() field = " << field << std::endl;
    #endif

    QString *key = new QString();

    struct  field_info *fi = Vect_get_field( mMap, field); // should work also with field = 0

    if ( fi == NULL ) {
        #ifdef QGISDEBUG
	std::cerr << "No field info -> no attributes" << std::endl;
        #endif
	return key;
    }

    key->setAscii(fi->key);
    return key;
}

std::vector<QgsField> *QgsGrassProvider::columns ( int field )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::columns() field = " << field << std::endl;
    #endif

    std::vector<QgsField> *col = new std::vector<QgsField>;
    
    struct  field_info *fi = Vect_get_field( mMap, field); // should work also with field = 0

    // Read attributes
    if ( fi == NULL ) {
        #ifdef QGISDEBUG
	std::cerr << "No field info -> no attributes" << std::endl;
        #endif
	return ( col );
    }

    #ifdef QGISDEBUG
    std::cerr << "Field info found -> open database" << std::endl;
    #endif
    QgsGrass::setMapset ( mGisdbase, mLocation, mMapset ); 
    dbDriver *driver = db_start_driver_open_database ( fi->driver, fi->database );

    if ( driver == NULL ) {
	std::cerr << "Cannot open database " << fi->database << " by driver " << fi->driver << std::endl;
	return ( col );
    }

    #ifdef QGISDEBUG
    std::cerr << "Database opened -> describe table" << std::endl;
    #endif

    dbString tableName;
    db_init_string ( &tableName );
    db_set_string ( &tableName, fi->table);
    
    dbTable *table;
    if(db_describe_table (driver, &tableName, &table) != DB_OK) {
	std::cerr << "Cannot describe table" << std::endl;
	return ( col );
    }

    int nCols = db_get_table_number_of_columns(table);

    for (int c = 0; c < nCols; c++) {
        dbColumn *column = db_get_table_column (table, c);

	int ctype = db_sqltype_to_Ctype( db_get_column_sqltype (column) );
	QString type;
  QVariant::Type qtype;
	switch ( ctype ) {
	    case DB_C_TYPE_INT:
		type = "int";
    qtype = QVariant::Int;
		break;
	    case DB_C_TYPE_DOUBLE:
		type = "double";
    qtype = QVariant::Double;
		break;
	    case DB_C_TYPE_STRING:
		type = "string";
    qtype = QVariant::String;
		break;
	    case DB_C_TYPE_DATETIME:
		type = "datetime";
    qtype = QVariant::String;
		break;
	}
        col->push_back ( QgsField( db_get_column_name (column), qtype, type, db_get_column_length(column), 0) );
    }
	
    db_close_database_shutdown_driver ( driver );

    return col;
}

QgsAttributeMap *QgsGrassProvider::attributes ( int field, int cat )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::attributes() field = " << field << " cat = " << cat << std::endl;
    #endif

    QgsAttributeMap *att = new QgsAttributeMap;

    struct  field_info *fi = Vect_get_field( mMap, field); // should work also with field = 0

    // Read attributes
    if ( fi == NULL ) {
        #ifdef QGISDEBUG
	std::cerr << "No field info -> no attributes" << std::endl;
        #endif
	return att;
    }

    #ifdef QGISDEBUG
    std::cerr << "Field info found -> open database" << std::endl;
    #endif
    QgsGrass::setMapset ( mGisdbase, mLocation, mMapset ); 
    dbDriver *driver = db_start_driver_open_database ( fi->driver, fi->database );

    if ( driver == NULL ) {
	std::cerr << "Cannot open database " << fi->database << " by driver " << fi->driver << std::endl;
	return att;
    }

    #ifdef QGISDEBUG
    std::cerr << "Database opened -> read attributes" << std::endl;
    #endif

    dbString dbstr; 
    db_init_string (&dbstr);
    QString query;
    query.sprintf("select * from %s where %s = %d", fi->table, fi->key, cat );
    db_set_string (&dbstr, (char *)query.ascii());
    
    #ifdef QGISDEBUG
    std::cerr << "SQL: " << db_get_string(&dbstr) << std::endl;
    #endif

    dbCursor databaseCursor;
    if ( db_open_select_cursor(driver, &dbstr, &databaseCursor, DB_SCROLL) != DB_OK ){
	db_close_database_shutdown_driver ( driver );
	std::cerr << "Cannot select attributes from table" << std::endl;
	return att;
    } 

    int nRecords = db_get_num_rows ( &databaseCursor );
    #ifdef QGISDEBUG
    std::cerr << "Number of records: " << nRecords << std::endl;
    #endif

    if ( nRecords < 1 ) {
	db_close_database_shutdown_driver ( driver );
        std::cerr << "No DB record" << std::endl;
	return att;
    }
    
    dbTable  *databaseTable = db_get_cursor_table (&databaseCursor);
    int nColumns = db_get_table_number_of_columns(databaseTable);

    int more;
    if ( db_fetch (&databaseCursor, DB_NEXT, &more) != DB_OK ) {
	db_close_database_shutdown_driver ( driver );
	std::cout << "Cannot fetch DB record" << std::endl;
	return att;
    }

    // Read columns' description 
    for (int i = 0; i < nColumns; i++) {
	dbColumn *column = db_get_table_column (databaseTable, i);
	db_convert_column_value_to_string (column, &dbstr);

        QString v = mEncoding->toUnicode(db_get_string(&dbstr));
	std::cerr << "Value: " << v.toLocal8Bit().data() << std::endl;
        att->insert(i, QVariant( v ) );
    }

    db_close_cursor (&databaseCursor);
    db_close_database_shutdown_driver ( driver );
    db_free_string(&dbstr);

    return att;
}

QString *QgsGrassProvider::updateAttributes ( int field, int cat, const QString &values )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::updateAttributes() field = " << field << " cat = " << cat << std::endl;
    #endif

    QString *error = new QString();
    struct  field_info *fi = Vect_get_field( mMap, field); // should work also with field = 0

    // Read attributes
    if ( fi == NULL ) {
        #ifdef QGISDEBUG
	std::cerr << "No field info -> no attributes" << std::endl;
        #endif
	error->setLatin1( "Cannot get field info" );
	return error;
    }

    #ifdef QGISDEBUG
    std::cerr << "Field info found -> open database" << std::endl;
    #endif
    QgsGrass::setMapset ( mGisdbase, mLocation, mMapset ); 
    dbDriver *driver = db_start_driver_open_database ( fi->driver, fi->database );

    if ( driver == NULL ) {
	std::cerr << "Cannot open database " << fi->database << " by driver " << fi->driver << std::endl;
	error->setAscii("Cannot open database");
	return error;
    }

    #ifdef QGISDEBUG
    std::cerr << "Database opened -> read attributes" << std::endl;
    #endif

    dbString dbstr; 
    db_init_string (&dbstr);
    QString query;
    
    query = "update " + QString(fi->table) + " set " + values + " where " + QString(fi->key) 
	    + " = " + QString::number(cat);

    #ifdef QGISDEBUG
    std::cerr << "query: " << query.toLocal8Bit().data() << std::endl;
    #endif

    // For some strange reason, mEncoding->fromUnicode(query) does not work, 
    // but probably it is not correct, because Qt widgets will use current locales for input
    //  -> it is possible to edit only in current locales at present
    // QCString qcs = mEncoding->fromUnicode(query);

    Q3CString qcs = query.toLocal8Bit().data();
    #ifdef QGISDEBUG
    std::cerr << "qcs: " << qcs.data() << std::endl;
    #endif
    
    char *cs = new char[qcs.length() + 1];
    strcpy(cs, (const char *)qcs);
    db_set_string (&dbstr, cs );
    delete[] cs;
    
    #ifdef QGISDEBUG
    std::cerr << "SQL: " << db_get_string(&dbstr) << std::endl;
    #endif

    int ret = db_execute_immediate (driver, &dbstr);

    if ( ret != DB_OK) { 
        std::cerr << "Error: " <<  db_get_error_msg() << std::endl;
	error->setLatin1( db_get_error_msg() );
    }

    db_close_database_shutdown_driver ( driver );
    db_free_string(&dbstr);

    return error;
}

int QgsGrassProvider::numDbLinks ( void )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::numDbLinks()" << std::endl;
    #endif

    return ( Vect_get_num_dblinks(mMap) );
}

int QgsGrassProvider::dbLinkField ( int link )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::dbLinkField()" << std::endl;
    #endif

    struct  field_info *fi = Vect_get_dblink ( mMap, link );

    if ( fi == NULL ) return 0;

    return ( fi->number );
}

QString *QgsGrassProvider::executeSql ( int field, const QString &sql )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::executeSql field = " << field << std::endl;
    #endif

    QString *error = new QString();
    struct  field_info *fi = Vect_get_field( mMap, field); // should work also with field = 0

    // Read attributes
    if ( fi == NULL ) {
        #ifdef QGISDEBUG
	std::cerr << "No field info -> no attributes" << std::endl;
        #endif
	error->setLatin1( "Cannot get field info" );
	return error;
    }

    #ifdef QGISDEBUG
    std::cerr << "Field info found -> open database" << std::endl;
    #endif

    QgsGrass::setMapset ( mGisdbase, mLocation, mMapset ); 
    dbDriver *driver = db_start_driver_open_database ( fi->driver, fi->database );

    if ( driver == NULL ) {
	std::cerr << "Cannot open database " << fi->database << " by driver " << fi->driver << std::endl;
	error->setAscii("Cannot open database");
	return error;
    }

    #ifdef QGISDEBUG
    std::cerr << "Database opened" << std::endl;
    #endif

    dbString dbstr; 
    db_init_string (&dbstr);
    db_set_string (&dbstr, (char *)sql.latin1());
    
    #ifdef QGISDEBUG
    std::cerr << "SQL: " << db_get_string(&dbstr) << std::endl;
    #endif

    int ret = db_execute_immediate (driver, &dbstr);

    if ( ret != DB_OK) { 
        std::cerr << "Error: " <<  db_get_error_msg() << std::endl;
	error->setLatin1( db_get_error_msg() );
    }

    db_close_database_shutdown_driver ( driver );
    db_free_string(&dbstr);

    return error;

}

QString *QgsGrassProvider::createTable ( int field, const QString &key, const QString &columns )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::createTable() field = " << field << std::endl;
    #endif

    QString *error = new QString();
    struct  field_info *fi = Vect_get_field( mMap, field); // should work also with field = 0

    // Read attributes
    if ( fi != NULL ) {
	#ifdef QGISDEBUG
	std::cerr << "The table for this field already exists" << std::endl;
	#endif
	error->setLatin1( "The table for this field already exists" );
	return error;
    }

    #ifdef QGISDEBUG
    std::cerr << "Field info not found -> create new table" << std::endl;
    #endif

    // We must set mapset before Vect_default_field_info
    QgsGrass::setMapset ( mGisdbase, mLocation, mMapset ); 

    int nLinks = Vect_get_num_dblinks( mMap );
    if ( nLinks == 0 ) {
        fi = Vect_default_field_info ( mMap, field, NULL, GV_1TABLE );
    } else {
        fi = Vect_default_field_info ( mMap, field, NULL, GV_MTABLE );
    }
    
    dbDriver *driver = db_start_driver_open_database ( fi->driver, fi->database );

    if ( driver == NULL ) {
	std::cerr << "Cannot open database " << fi->database << " by driver " << fi->driver << std::endl;
	error->setAscii("Cannot open database");
	return error;
    }

    #ifdef QGISDEBUG
    std::cerr << "Database opened -> create table" << std::endl;
    #endif

    dbString dbstr; 
    db_init_string (&dbstr);
    QString query;
    
    query.sprintf("create table %s ( %s )", fi->table, columns.latin1() );
    db_set_string (&dbstr, (char *)query.latin1());
    
    #ifdef QGISDEBUG
    std::cerr << "SQL: " << db_get_string(&dbstr) << std::endl;
    #endif

    int ret = db_execute_immediate (driver, &dbstr);

    if ( ret != DB_OK) { 
        std::cerr << "Error: " <<  db_get_error_msg() << std::endl;
	error->setLatin1( db_get_error_msg() );
    }

    db_close_database_shutdown_driver ( driver );
    db_free_string(&dbstr);

    if ( !error->isEmpty() ) return error;

    ret = Vect_map_add_dblink ( mMap, field, NULL, fi->table, (char *)key.latin1(), 
	                            fi->database, fi->driver);

    if ( ret == -1 ) { 
        std::cerr << "Error: Cannot add dblink" << std::endl;
	error->setLatin1( "Cannot create link to the table. The table was created!" );
    }

    return error;
}

QString *QgsGrassProvider::addColumn ( int field, const QString &column )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::addColumn() field = " << field << std::endl;
    #endif

    QString *error = new QString();
    struct  field_info *fi = Vect_get_field( mMap, field); // should work also with field = 0

    // Read attributes
    if ( fi == NULL ) {
        #ifdef QGISDEBUG
	std::cerr << "No field info" << std::endl;
        #endif
	error->setLatin1( "Cannot get field info" );
	return error;
    }

    QString query;
    
    query.sprintf("alter table %s add column %s", fi->table, column.latin1() );

    delete error;
    return executeSql ( field, query );
}

QString *QgsGrassProvider::insertAttributes ( int field, int cat )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::insertAttributes() field = " << field << " cat = " << cat << std::endl;
    #endif

    QString *error = new QString();
    struct  field_info *fi = Vect_get_field( mMap, field); // should work also with field = 0

    // Read attributes
    if ( fi == NULL ) {
        #ifdef QGISDEBUG
	std::cerr << "No field info -> no attributes" << std::endl;
        #endif
	error->setLatin1( "Cannot get field info" );
	return error;
    }

    QString query;
    
    query.sprintf("insert into %s ( %s ) values ( %d )", fi->table, fi->key, cat );

    delete error;
    return executeSql ( field, query );
}

QString *QgsGrassProvider::deleteAttributes ( int field, int cat )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::deleteAttributes() field = " << field << " cat = " << cat << std::endl;
    #endif

    QString *error = new QString();
    struct  field_info *fi = Vect_get_field( mMap, field); // should work also with field = 0

    // Read attributes
    if ( fi == NULL ) {
        #ifdef QGISDEBUG
	std::cerr << "No field info -> no attributes" << std::endl;
        #endif
	error->setLatin1( "Cannot get field info" );
	return error;
    }

    QString query;
    
    query.sprintf("delete from %s where %s = %d", fi->table, fi->key, cat );

    delete error;
    return executeSql ( field, query );
}

QString *QgsGrassProvider::isOrphan ( int field, int cat, int *orphan)
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassProvider::isOrphan() field = " << field << " cat = " << cat << std::endl;
    #endif

    QString *error = new QString();

    // Check first if another line with such cat exists
    int fieldIndex = Vect_cidx_get_field_index ( mMap, field );
    if ( fieldIndex >= 0 ) 
    {
        int t, id;
        int ret = Vect_cidx_find_next ( mMap, fieldIndex, cat, 
                        GV_POINTS|GV_LINES, 0, &t, &id );
      
        if ( ret >= 0 ) {
           // Category exists
           *orphan = false;
           return error; 
        }
    }

    // Check if attribute exists
    struct  field_info *fi = Vect_get_field( mMap, field); // should work also with field = 0

    // Read attributes
    if ( fi == NULL ) {
        #ifdef QGISDEBUG
	std::cerr << "No field info -> no attributes" << std::endl;
        #endif
        *orphan = false;
        return error; 
    }

    #ifdef QGISDEBUG
    std::cerr << "Field info found -> open database" << std::endl;
    #endif
    QgsGrass::setMapset ( mGisdbase, mLocation, mMapset ); 
    dbDriver *driver = db_start_driver_open_database ( fi->driver, fi->database );

    if ( driver == NULL ) {
	std::cerr << "Cannot open database " << fi->database << " by driver " << fi->driver << std::endl;
	error->setAscii("Cannot open database");
	return error;
    }

    #ifdef QGISDEBUG
    std::cerr << "Database opened -> select record" << std::endl;
    #endif

    dbString dbstr; 
    db_init_string (&dbstr);
    QString query;
    
    query.sprintf("select %s from %s where %s = %d", fi->key, fi->table, fi->key, cat );
    db_set_string (&dbstr, (char *)query.latin1());
    
    #ifdef QGISDEBUG
    std::cerr << "SQL: " << db_get_string(&dbstr) << std::endl;
    #endif

    dbCursor cursor;
    if ( db_open_select_cursor(driver, &dbstr, &cursor, DB_SCROLL) != DB_OK )
    {
	db_close_database_shutdown_driver ( driver );
	error->setAscii("Cannot query database: " + query );
	return error;
    }
    int nRecords = db_get_num_rows ( &cursor );
    #ifdef QGISDEBUG
    std::cerr << "Number of records: " << nRecords << std::endl;
    #endif

    if ( nRecords > 0 ) { *orphan = true; }

    db_close_database_shutdown_driver ( driver );
    db_free_string(&dbstr);

    return error;
}


// -------------------------------------------------------------------------------

int QgsGrassProvider::cidxGetNumFields( ) 
{
    return ( Vect_cidx_get_num_fields(mMap) );
}

int QgsGrassProvider::cidxGetFieldNumber( int idx ) 
{
    return ( Vect_cidx_get_field_number(mMap, idx) );
}

int QgsGrassProvider::cidxGetMaxCat( int idx ) 
{
    int ncats = Vect_cidx_get_num_cats_by_index ( mMap, idx);

    int cat, type, id;
    Vect_cidx_get_cat_by_index ( mMap, idx, ncats-1, &cat, &type, &id );
    
    return ( cat );
}
    


QString QgsGrassProvider::name() const
{
    return GRASS_KEY;
} // QgsGrassProvider::name()



QString QgsGrassProvider::description() const
{
    return GRASS_DESCRIPTION;
} // QgsGrassProvider::description()


