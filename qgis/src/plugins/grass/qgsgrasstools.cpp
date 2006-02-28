/***************************************************************************
                              qgsgrasstools.cpp
                             -------------------
    begin                : March, 2005
    copyright            : (C) 2005 by Radim Blazek
    email                : blazek@itc.it
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include <iostream>

#include <qapplication.h>
#include <qdir.h>
#include <qfile.h>
#include <qsettings.h>
#include <qpixmap.h>
#include <q3listbox.h>
#include <qstringlist.h>
#include <qlabel.h>
#include <QComboBox>
#include <qspinbox.h>
#include <qmessagebox.h>
#include <qinputdialog.h>
#include <qsettings.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qpen.h>
#include <q3pointarray.h>
#include <qcursor.h>
#include <qnamespace.h>
#include <q3listview.h>
#include <qcolordialog.h>
#include <q3table.h>
#include <qstatusbar.h>
#include <qevent.h>
#include <qpoint.h>
#include <qsize.h>
#include <qdom.h>
#include <qtabwidget.h>
#include <qlayout.h>
#include <qcheckbox.h>
#include <q3process.h>
#include <qicon.h>
//Added by qt3to4:
#include <QCloseEvent>
#include <QTabBar>
#include <QListView>
#include <QProcess>
#include <QHeaderView>

#include "qgis.h"
#include "qgsapplication.h"
#include "qgsmapcanvas.h"
#include "qgsmaplayer.h"
#include "qgsvectorlayer.h"
#include "qgsdataprovider.h"
#include "qgsfield.h"
#include "qgsfeatureattribute.h"

extern "C" {
#include <grass/gis.h>
#include <grass/Vect.h>
}

#include "../../src/providers/grass/qgsgrass.h"
#include "../../src/providers/grass/qgsgrassprovider.h"
#include "qgsgrassattributes.h"
#include "qgsgrasstools.h"
#include "qgsgrassmodule.h"
#include "qgsgrassshell.h"
#include "qgsgrassmodel.h"
#include "qgsgrassbrowser.h"

QgsGrassToolsTabWidget::QgsGrassToolsTabWidget( QWidget * parent ): 
        QTabWidget(parent)
{
    // Default height seems to be too small for our purpose
    int height = (int)(1.5 * tabBar()->iconSize().height());
    // Max width (see QgsGrassModule::pixmap for hardcoded sizes)
    int width = 3*height + 28 + 29;
    tabBar()->setIconSize( QSize(width,height) );
}

QSize QgsGrassToolsTabWidget::iconSize()
{
    return tabBar()->iconSize();
}

QgsGrassToolsTabWidget::~QgsGrassToolsTabWidget() {}

QgsGrassTools::QgsGrassTools ( QgisApp *qgisApp, QgisIface *iface, 
	                     QWidget * parent, const char * name, Qt::WFlags f )
             :QDialog ( parent )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassTools()" << std::endl;
    #endif

   setWindowTitle ( "GRASS Tools" );
//    setupUi(this);

    mQgisApp = qgisApp;
    mIface = iface;
    mCanvas = mIface->getMapCanvas();

    mTabWidget = new QgsGrassToolsTabWidget (this);
    QVBoxLayout *layout1 = new QVBoxLayout(this);
    layout1->addWidget(mTabWidget);


    mModulesListView = new Q3ListView();
    mTabWidget->addTab( mModulesListView, "Modules" );
    mModulesListView->addColumn("col1",0);
    
    // Set list view
    mModulesListView->setColumnText(0,"Modules");
    mModulesListView->clear();
    mModulesListView->setSorting(-1);
    mModulesListView->setRootIsDecorated(true);
    mModulesListView->setResizeMode(Q3ListView::AllColumns);
    mModulesListView->header()->hide();

    connect( mModulesListView, SIGNAL(clicked(Q3ListViewItem *)), 
		         this, SLOT(moduleClicked( Q3ListViewItem *)) );

    QString title = "GRASS Tools: " + QgsGrass::getDefaultLocation()
                + "/" + QgsGrass::getDefaultMapset();
    setCaption(title);

    // Warning: QgsApplication initialized in main.cpp
    //          is not valid here (static libraries / linking)

#if defined(WIN32) || defined(Q_OS_MACX)
    mAppDir = qApp->applicationDirPath();
#else
    mAppDir = PREFIX;
#endif

    //QString conf = QgsApplication::pkgDataPath() + "/grass/config/default.qgc";
    QString conf = mAppDir + "/share/qgis/grass/config/default.qgc";

    restorePosition();

    // Show before loadConfig() so that user can see loading
    mModulesListView->show(); 

    loadConfig ( conf );
    //statusBar()->hide();

    // Add map browser 
    // Warning: if browser is on the first page modules are 
    // displayed over the browser
    QgsGrassBrowser *browser = new QgsGrassBrowser ( mIface, this );
    mTabWidget->addTab( browser, "Browser" );
}

void QgsGrassTools::moduleClicked( Q3ListViewItem * item )
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassTools::moduleClicked()" << std::endl;
    #endif
    if ( !item ) return;

    QString name = item->text(1);
    //std::cerr << "name = " << name << std::endl;
    
    if ( name.length() == 0 ) return;  // Section
    
    //QString path = QgsApplication::pkgDataPath() + "/grass/modules/" + name;
    QString path = mAppDir + "/share/qgis/grass/modules/" + name;
    #ifdef QGISDEBUG
    std::cerr << "path = " << path.ascii() << std::endl;
    #endif
    QWidget *m;
    QgsGrassShell *sh = 0;
    if ( name == "shell" )
    {
#ifdef WIN32
         // Run MSYS if available
         // Note: I was not able to run cmd.exe and command.com
         //       with QProcess

         QString msysPath = mAppDir + "/msys/msys.bat";
         QFile file ( msysPath );

         if ( !file.exists() ) 
         {
	     QMessageBox::warning( 0, "Warning",
                 "Cannot find MSYS (" + msysPath + ")" );
         } 
         else
         {
             QProcess *proc = new QProcess(this);
             proc->start (msysPath);
         }
         return;
#else 

    #ifdef HAVE_OPENPTY
        sh = new QgsGrassShell(this, mTabWidget);
        m = dynamic_cast<QWidget *> ( sh );
    #else
	QMessageBox::warning( 0, "Warning", "GRASS Shell is not compiled." );
    #endif // HAVE_OPENPTY

#endif // ! WIN32
    }
    else
    {
	m = dynamic_cast<QWidget *> ( new QgsGrassModule ( this, 
                                      mQgisApp, mIface, path, mTabWidget ) );
    }
    
    int height = mTabWidget->iconSize().height();
    QPixmap pixmap = QgsGrassModule::pixmap ( path, height ); 
    
    // Icon size in QT4 does not seem to be variable
    // -> put smaller icons in the middle
    QPixmap pixmap2 ( mTabWidget->iconSize() );
    QPalette pal;
    pixmap2.fill ( pal.color(QPalette::Window) );
    QPainter painter(&pixmap2);
    int x = (int) ( (mTabWidget->iconSize().width()-pixmap.width())/2 );
    painter.drawPixmap ( x, 0, pixmap );
    painter.end();

    QIcon is;
    is.addPixmap ( pixmap2 );
    mTabWidget->addTab ( m, is, "" );

   QgsGrassToolsTabWidget tw;
		
    mTabWidget->setCurrentPage ( mTabWidget->count()-1 );
     
    // We must call resize to reset COLUMNS enviroment variable
    // used by bash !!!
#ifndef WIN32
    if ( sh ) sh->resizeTerminal();
#endif
}

bool QgsGrassTools::loadConfig(QString filePath)
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassTools::loadConfig(): " << filePath.toLocal8Bit().data() << std::endl;
    #endif
    mModulesListView->clear();

    QFile file ( filePath );

    if ( !file.exists() ) {
	QMessageBox::warning( 0, "Warning", "The config file (" + filePath + ") not found." );
	return false;
    }
    if ( ! file.open( QIODevice::ReadOnly ) ) {
	QMessageBox::warning( 0, "Warning", "Cannot open config file (" + filePath + ")" );
	return false;
    }
    
    QDomDocument doc ( "qgisgrass" );
    QString err;
    int line, column;
    if ( !doc.setContent( &file,  &err, &line, &column ) ) {
	QString errmsg = "Cannot read config file (" + filePath + "):\n" + err + "\nat line "  
	                 + QString::number(line) + " column " + QString::number(column);
	std::cerr << errmsg.toLocal8Bit().data() << std::endl;
	QMessageBox::warning( 0, "Warning", errmsg );
	file.close();
	return false;
    }

    QDomElement docElem = doc.documentElement();
    QDomNodeList modulesNodes = docElem.elementsByTagName ( "modules" );

    if ( modulesNodes.count() == 0 ) {
	 file.close();
	 return false;
    }

    QDomNode modulesNode = modulesNodes.item(0);
    QDomElement modulesElem = modulesNode.toElement();
    
    // Go through the sections and modules and add them to the list view
    addModules ( 0, modulesElem );
    
    file.close();
}

void QgsGrassTools::addModules (  Q3ListViewItem *parent, QDomElement &element )
{
    QDomNode n = element.firstChild();

    Q3ListViewItem *item;
    Q3ListViewItem *lastItem = 0;
    while( !n.isNull() ) {
	QDomElement e = n.toElement();
	if( !e.isNull() ) {
	    //std::cout << "tag = " << e.tagName() << std::endl;

	    if ( e.tagName() == "section" && e.tagName() == "grass" ) {
		std::cout << "Unknown tag: " << e.tagName().toLocal8Bit().data() << std::endl;
		continue;
	    }
	    
	    if ( parent ) {
		item = new Q3ListViewItem( parent, lastItem );
	    } else {
		item = new Q3ListViewItem( mModulesListView, lastItem );
	    }

	    if ( e.tagName() == "section" ) {
		QString label = e.attribute("label");
	        std::cout << "label = " << label.toLocal8Bit().data() << std::endl;
		item->setText( 0, label );
		item->setOpen(true); // for debuging to spare one click

		addModules ( item, e );
		
		lastItem = item;
	    } else if ( e.tagName() == "grass" ) { // GRASS module
		QString name = e.attribute("name");
	        std::cout << "name = " << name.toLocal8Bit().data() << std::endl;

                //QString path = QgsApplication::pkgDataPath() + "/grass/modules/" + name;
                QString path = mAppDir + "/share/qgis/grass/modules/" + name;
                QString label = QgsGrassModule::label ( path );
		QPixmap pixmap = QgsGrassModule::pixmap ( path, 25 ); 

		item->setText( 0, label );
		item->setPixmap( 0, pixmap );
		item->setText( 1, name );
		lastItem = item;
	    }
            // Show items during loading
            mModulesListView->repaint();
	}
	n = n.nextSibling();
    }
}

void QgsGrassTools::mapsetChanged()
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassTools::mapsetChanged()" << std::endl;
    #endif

    QString title = "GRASS Tools: " + QgsGrass::getDefaultLocation()
                + "/" + QgsGrass::getDefaultMapset();
    setCaption(title);

    // TODO: Close opened tools (tabs) ?
}

QgsGrassTools::~QgsGrassTools()
{
    #ifdef QGISDEBUG
    std::cerr << "QgsGrassTools::~QgsGrassTools()" << std::endl;
    #endif
    saveWindowLocation();
}

QString QgsGrassTools::appDir(void)
{
    //return QgsApplication::applicationDirPath();
    return mAppDir;
}

void QgsGrassTools::close(void)
{
    saveWindowLocation();
    hide();
}

void QgsGrassTools::closeEvent(QCloseEvent *e)
{
    saveWindowLocation();
    e->accept();
}

void QgsGrassTools::restorePosition()
{
    QSettings settings("QuantumGIS", "qgis");
    int ww = settings.readNumEntry("/GRASS/windows/tools/w", 250);
    int wh = settings.readNumEntry("/GRASS/windows/tools/h", 300);
    int wx = settings.readNumEntry("/GRASS/windows/tools/x", 100);
    int wy = settings.readNumEntry("/GRASS/windows/tools/y", 100);
    resize(ww,wh);
    move(wx,wy);
    show();
}

void QgsGrassTools::saveWindowLocation()
{
    QSettings settings("QuantumGIS", "qgis");
    QPoint p = this->pos();
    QSize s = this->size();
    settings.writeEntry("/GRASS/windows/tools/x", p.x());
    settings.writeEntry("/GRASS/windows/tools/y", p.y());
    settings.writeEntry("/GRASS/windows/tools/w", s.width());
    settings.writeEntry("/GRASS/windows/tools/h", s.height());
}



