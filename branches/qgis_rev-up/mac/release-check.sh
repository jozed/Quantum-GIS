#!/bin/sh
# Display all paths to supporting libraries
# Output should be visually inspected for paths which haven't been made relative (such as /usr/local)

PREFIX=qgis0.9.1.app/Contents/MacOS

otool -L $PREFIX/qgis
otool -L $PREFIX/bin/qgis_help.app/Contents/MacOS/qgis_help
otool -L $PREFIX/bin/msexport.app/Contents/MacOS/msexport
#otool -L $PREFIX/bin/omgui

otool -L $PREFIX/lib/libqgis_core.dylib
otool -L $PREFIX/lib/libqgis_gui.dylib
otool -L $PREFIX/lib/libqgisgrass.dylib
otool -L $PREFIX/lib/qgis/libcopyrightlabelplugin.so 
otool -L $PREFIX/lib/qgis/libdelimitedtextplugin.so
otool -L $PREFIX/lib/qgis/libdelimitedtextprovider.so
otool -L $PREFIX/lib/qgis/libgeorefplugin.so 
otool -L $PREFIX/lib/qgis/libgpsimporterplugin.so
otool -L $PREFIX/lib/qgis/libgpxprovider.so
otool -L $PREFIX/lib/qgis/libgrassplugin.so 
otool -L $PREFIX/lib/qgis/libgrassprovider.so 
otool -L $PREFIX/lib/qgis/libgridmakerplugin.so 
otool -L $PREFIX/lib/qgis/libwfsprovider.so
otool -L $PREFIX/lib/qgis/libnortharrowplugin.so
otool -L $PREFIX/lib/qgis/libogrprovider.so
otool -L $PREFIX/lib/qgis/libpggeoprocessingplugin.so
otool -L $PREFIX/lib/qgis/libpostgresprovider.so
otool -L $PREFIX/lib/qgis/libscalebarplugin.so
otool -L $PREFIX/lib/qgis/libspitplugin.so
otool -L $PREFIX/lib/qgis/libwfsplugin.so
otool -L $PREFIX/lib/qgis/libwmsprovider.so
#otool -L $PREFIX/lib/qgis/libopenmodellerplugin.so 

otool -L $PREFIX/lib/Qt3Support.framework/Versions/4/Qt3Support
otool -L $PREFIX/lib/QtCore.framework/Versions/4/QtCore
otool -L $PREFIX/lib/QtGui.framework/Versions/4/QtGui
otool -L $PREFIX/lib/QtNetwork.framework/Versions/4/QtNetwork
otool -L $PREFIX/lib/QtSql.framework/Versions/4/QtSql
otool -L $PREFIX/lib/QtSvg.framework/Versions/4/QtSvg
otool -L $PREFIX/lib/QtXml.framework/Versions/4/QtXml
otool -L $PREFIX/../plugins/imageformats/libqjpeg.dylib

otool -L $PREFIX/lib/libgdal.1.11.4.dylib
otool -L $PREFIX/lib/gdalplugins/gdal_GRASS.so
otool -L $PREFIX/lib/gdalplugins/ogr_GRASS.so
otool -L $PREFIX/lib/libgeos.2.2.3.dylib
otool -L $PREFIX/lib/libgeos_c.1.1.1.dylib
otool -L $PREFIX/lib/libproj.0.5.2.dylib
otool -L $PREFIX/lib/libsqlite3.0.8.6.dylib
otool -L $PREFIX/lib/libxerces-c.28.0.dylib
otool -L $PREFIX/lib/libgif.4.1.4.dylib
otool -L $PREFIX/lib/libjpeg.62.0.0.dylib
otool -L $PREFIX/lib/libpng.3.1.2.8.dylib
otool -L $PREFIX/lib/libtiff.3.dylib
otool -L $PREFIX/lib/libgeotiff.1.2.3.dylib
otool -L $PREFIX/lib/libjasper-1.701.1.0.0.dylib
otool -L $PREFIX/lib/libexpat.1.5.2.dylib
otool -L $PREFIX/lib/libgsl.0.9.0.dylib
otool -L $PREFIX/lib/libgslcblas.0.0.0.dylib
#otool -L $PREFIX/lib/libopenmodeller.0.0.0.dylib
#otool -L $PREFIX/lib/openmodeller/libombioclim.0.0.0.dylib
#otool -L $PREFIX/lib/openmodeller/libombioclim_distance.0.0.0.dylib
#otool -L $PREFIX/lib/openmodeller/libomcsmbs.0.0.0.dylib
#otool -L $PREFIX/lib/openmodeller/libomdg_bs.0.0.0.dylib
#otool -L $PREFIX/lib/openmodeller/libomdistance_to_average.0.0.0.dylib
#otool -L $PREFIX/lib/openmodeller/libomminimum_distance.0.0.0.dylib
#otool -L $PREFIX/lib/openmodeller/libomoldgarp.0.0.0.dylib
otool -L $PREFIX/lib/libpq.5.0.dylib

for LIBGRASS in datetime dbmibase dbmiclient dgl dig2 form gis gmath gproj I linkm rtree shape vask vect
do
	otool -L $PREFIX/lib/grass/libgrass_$LIBGRASS.6.3.0RC3.dylib
done

otool -L $PREFIX/share/qgis/python/qgis/core.so
otool -L $PREFIX/share/qgis/python/qgis/gui.so
otool -L $PREFIX/share/qgis/python/sip.so
for LIBPYQT4 in Qt QtCore QtGui QtNetwork QtSql QtSvg QtXml QtAssistant QtDesigner QtOpenGL QtScript QtTest
do
	otool -L $PREFIX/share/qgis/python/PyQt4/$LIBPYQT4.so
done
