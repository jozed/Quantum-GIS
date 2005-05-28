/***************************************************************************
                            main.cpp  -  description
                              -------------------
              begin                : Fri Jun 21 10:48:28 AKDT 2002
              copyright            : (C) 2002 by Gary E.Sherman
              email                : sherman at mrcc.com
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

#include <qgsconfig.h>

#include <iostream>
#include <cstdio>
#ifndef WIN32
#include <getopt.h>
#endif

#include <qapplication.h>
#include <qfont.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qmessagebox.h>
#include <qstring.h>
#include <qtextcodec.h>
#include <qtranslator.h>
#include <qstyle.h>
#include <qpixmap.h>
#include <qstringlist.h> 

#ifdef Q_OS_MACX
#include <ApplicationServices/ApplicationServices.h>
#endif

#include "qgisapp.h"
#include "qgsexception.h"
#include "qgsproject.h"

static const char * const ident_ = "$Id$";

/** print usage text
 */
void usage( std::string const & appName )
{
  std::cerr << "Quantum GIS - " << VERSION << " 'Simon'\n" 
      << "Quantum GIS (QGIS) is a viewer for spatial data sets, including\n" 
      << "raster and vector data.\n"  
      << "Usage: " << appName <<  " [options] [FILES]\n"  
      << "  options:\n"
      << "\t[--snapshot filename]\temit snapshot of loaded datasets to given file\n"
      << "\t[--lang language]\tuse language for interface text\n"
      << "\t[--project projectfile]\tload the given QGIS project\n"
      << "\t[--help]\t\tthis text\n\n"
      << "  FILES:\n"  
      << "    Files specified on the command line can include rasters,\n"  
      << "    vectors, and QGIS project files (.qgs): \n"  
      << "     1. Rasters - Supported formats include GeoTiff, DEM \n"  
      << "        and others supported by GDAL\n"  
      << "     2. Vectors - Supported formats include ESRI Shapefiles\n"  
      << "        and others supported by OGR and PostgreSQL layers using\n"  
            << "        the PostGIS extension\n"  ;

} // usage()


/////////////////////////////////////////////////////////////////
// Command line options 'behaviour' flag setup
////////////////////////////////////////////////////////////////

// These two are global so that they can be set by the OpenDocuments
// AppleEvent handler as well as by the main routine argv processing

// This behaviour will cause QGIS to autoload a project
static QString myProjectFileName="";

// This is the 'leftover' arguments collection
static QStringList myFileList;


#ifdef Q_OS_MACX
/* Mac OS OpenDocuments AppleEvent handler called when files are double-clicked.
 * May be called at startup before application is initialized as well as
 * at any time while the application is running.
 */
short openDocumentsAEHandler(const AppleEvent *event, AppleEvent *reply, long refCon)
{
  AEDescList docs;
  if (AEGetParamDesc(event, keyDirectObject, typeAEList, &docs) == noErr)
  {
    // Get count of files to open
    long count = 0;
    AECountItems(&docs, &count);

    // Examine files and load first project file followed by all other non-project files
    myProjectFileName.truncate(0);
    myFileList.clear();
    for (int i = 0; i < count; i++)
    {
      FSRef ref;
      UInt8 strBuffer[256];
      if (AEGetNthPtr(&docs, i + 1, typeFSRef, 0, 0, &ref, sizeof(ref), 0) == noErr &&
          FSRefMakePath(&ref, strBuffer, 256) == noErr)
      {
        QString fileName(QString::fromUtf8(reinterpret_cast<char *>(strBuffer)));
        if (fileName.endsWith(".qgs"))
        {
          // Load first project file and ignore all other project files
          if (myProjectFileName.isEmpty())
          {
            myProjectFileName = fileName;
          }
        }
        else
        {
           // Load all non-project files
           myFileList.append(fileName);
        }
      }
    }

    // Open files now if application has been initialized
    QgisApp *qgis = dynamic_cast<QgisApp *>(qApp->mainWidget());
    if (qgis)
    {
      if (!myProjectFileName.isEmpty())
      {
        qgis->openProject(myProjectFileName);
      }
      for (QStringList::Iterator myIterator = myFileList.begin();
           myIterator != myFileList.end(); ++myIterator ) 
      {
        QString fileName = *myIterator;
        qgis->openLayer(fileName);
      }
    }
  }
  return noErr;
}
#endif


/* Test to determine if this program was started on Mac OS X by double-clicking
 * the application bundle rather then from a command line. If clicked, argv[1]
 * contains a process serial number in the form -psn_0_1234567. Don't process
 * the command line arguments in this case because argv[1] confuses the processing.
 */
bool bundleclicked(int argc, char *argv[])
{
  return ( argc > 1 && memcmp(argv[1], "-psn_", 5) == 0 );
}


int main(int argc, char *argv[])
{
  
  /////////////////////////////////////////////////////////////////
  // Command line options 'behaviour' flag setup
  ////////////////////////////////////////////////////////////////

  //
  // Parse the command line arguments, looking to see if the user has asked for any 
  // special behaviours. Any remaining non command arguments will be kept aside to
  // be passed as a list of layers and / or a project that should be loaded.
  //

  // This behaviour is used to load the app, snapshot the map,
  // save the image to disk and then exit
  QString mySnapshotFileName="";

  // This behaviour will allow you to force the use of a translation file
  // which is useful for testing
  QString myTranslationFileName="";

#ifndef WIN32
  if ( !bundleclicked(argc, argv) )
  {

  //////////////////////////////////////////////////////////////// 
  // USe the GNU Getopts utility to parse cli arguments
  // Invokes ctor `GetOpt (int argc, char **argv,  char *optstring);'
  ///////////////////////////////////////////////////////////////
  int optionChar;
  while (1)
  {
    static struct option long_options[] =
    {
    /* These options set a flag. */
    {"help", no_argument, 0, 'h'},
    /* These options don't set a flag.
     *  We distinguish them by their indices. */
    {"snapshot", required_argument, 0, 's'},
    {"lang",     required_argument, 0, 'l'},
    {"project",  required_argument, 0, 'p'},
    {0, 0, 0, 0}
    };

    /* getopt_long stores the option index here. */
    int option_index = 0;

    optionChar = getopt_long (argc, argv, "slp",
        long_options, &option_index);

    /* Detect the end of the options. */
    if (optionChar == -1)
    break;

    switch (optionChar)
    {
    case 0:
      /* If this option set a flag, do nothing else now. */
      if (long_options[option_index].flag != 0)
      break;
      printf ("option %s", long_options[option_index].name);
      if (optarg)
      printf (" with arg %s", optarg);
      printf ("\n");
      break;

    case 's':
      mySnapshotFileName = optarg;
      break;

    case 'l':
      myTranslationFileName = optarg;
      break;

    case 'p':
      myProjectFileName = optarg;
      break;

    case 'h':
    case '?':
      usage( argv[0] );
      return 2;   // XXX need standard exit codes
      break;

    default:
      std::cerr << argv[0] << ": getopt returned character code " << optionChar << "\n";
      return 1;   // XXX need standard exit codes
    }
  }

  // Add any remaining args to the file list - we will attempt to load them 
  // as layers in the map view further down....
#ifdef QGISDEBUG
  std::cout << "Files specified on command line: " << optind << std::endl;
#endif
  if (optind < argc)
  {
    while (optind < argc)
    {
#ifdef QGISDEBUG
    int idx = optind;
    std::cout << idx << ": " << argv[idx] << std::endl;
#endif
    myFileList.append(argv[optind++]);
    }
  }
  }
#endif //WIN32

  /////////////////////////////////////////////////////////////////////
  // Now we have the handlers for the different behaviours...
  ////////////////////////////////////////////////////////////////////

  /////////////////////////////////////////////////////////////////////
  // Initialise the application and the translation stuff
  /////////////////////////////////////////////////////////////////////

#ifdef Q_WS_X11
  bool myUseGuiFlag = getenv( "DISPLAY" ) != 0;
#else
  bool myUseGuiFlag = TRUE;
#endif
  if (!myUseGuiFlag) 
  {
    std::cerr << "QGIS starting in non-interactive mode not supported.\n You are seeing this message most likely because you have no DISPLAY environment variable set." << std::endl;
    exit(1); //exit for now until a version of qgis is capabable of running non interactive
  }
  QApplication a(argc, argv, myUseGuiFlag );

#ifdef Q_OS_MACX
  // Install OpenDocuments AppleEvent handler after application object is initialized
  // but before any other event handling (including dialogs or splash screens) occurs.
  // If an OpenDocuments event has been created before the application was launched,
  // it must be handled before some other event handler runs and dismisses it as unknown.
  // If run at startup, the handler will set either or both of myProjectFileName and myFileList.
  AEInstallEventHandler(kCoreEventClass, kAEOpenDocuments, openDocumentsAEHandler, 0, false);
#endif

  // Check to see if qgis was started from the source directory. 
  // This is done by looking for Makefile in the directory where qgis was
  // started from. If running from the src directory, exit gracefully

  // Get the application path. This method is required to support qt 3.1.2
  // which does not support the applicationFilePath and applicationDirPath
  // functions. We assume that OS X and Win32 systems will be using at least
  // Qt 3.2 and therefore support the required functions.
#if defined(Q_OS_MACX) || defined(WIN32)
  QString appPath = qApp->applicationFilePath();
  QString appDir = qApp->applicationDirPath();
  QString testFile = "Makefile";
#else
  QString appPath = argv[0];
  QString appDir = appPath.left(appPath.findRev("/"));
  QString testFile = "lt-qgis";
#endif

  if(appPath.contains("/src/"))
  {
    // check to see if configure is present in the directory
    
    QFileInfo fi(appDir + "/" + testFile);
    if(fi.exists())
    {
      QMessageBox::critical(0,"QGIS Not Installed",
          "You appear to be running QGIS from the source directory.\n"
          "You must install QGIS using make install and run it from the "
          "installed directory.");
      exit(1);
    }
  }

  
  // a.setFont(QFont("helvetica", 11));

#if defined(Q_OS_MACX) || defined(WIN32)
  QString PKGDATAPATH = qApp->applicationDirPath() + "/share/qgis";
#endif

  QTranslator tor(0);

  // For WIN32, get the locale
  if (myTranslationFileName!="")
  {
    QString translation = "qgis_" + myTranslationFileName;
    tor.load(translation, QString(PKGDATAPATH) + "/i18n");
  } 
  else
  {
#ifdef QGISDEBUG
    std::cout << "Setting translation to " 
      << PKGDATAPATH << "/i18n/qgis_" << QTextCodec::locale() << std::endl; 
#endif
    tor.load(QString("qgis_") + QTextCodec::locale(), QString(PKGDATAPATH) + "/i18n");
  }

  //tor.load("qgis_go", "." );
  a.installTranslator(&tor);
  /* uncomment the following line, if you want a Windows 95 look */
  //a.setStyle("Windows");

  QgisApp *qgis = new QgisApp; // "QgisApp" used to find canonical instance
  qgis->setName( "QgisApp" );

  a.setMainWidget(qgis);

  /////////////////////////////////////////////////////////////////////
  // If no --project was specified, parse the args to look for a     //
  // .qgs file and set myProjectFileName to it. This allows loading  //
  // of a project file by clicking on it in various desktop managers //
  // where an appropriate mime-type has been set up.                 //
  /////////////////////////////////////////////////////////////////////
  if(myProjectFileName.isEmpty())
  {
    // check for a .qgs
    for(int i = 0; i < argc; i++)
    {
      QString arg = argv[i];
      if(arg.contains(".qgs"))
      {
        myProjectFileName = argv[i];
        break;
      }
    }
  }

  /////////////////////////////////////////////////////////////////////
  // Load a project file if one was specified
  /////////////////////////////////////////////////////////////////////
  if( ! myProjectFileName.isEmpty() )
  {
    qgis->openProject(myProjectFileName);
  }


  /////////////////////////////////////////////////////////////////////
  // autoload any filenames that were passed in on the command line
  /////////////////////////////////////////////////////////////////////
#ifdef QGISDEBUG
  std::cout << "Number of files in myFileList: " << myFileList.count() << std::endl;
#endif
  for ( QStringList::Iterator myIterator = myFileList.begin(); myIterator != myFileList.end(); ++myIterator ) 
  {


#ifdef QGISDEBUG
    std::cout << "Trying to load file : " << *myIterator << std::endl;
#endif
    QString myLayerName = *myIterator;
    // don't load anything with a .qgs extension - these are project files
    if(!myLayerName.contains(".qgs"))
    {
      qgis->openLayer(myLayerName);
    }
  }



  /////////////////////////////////////////////////////////////////////
  // Take a snapshot of the map view then exit if snapshot mode requested
  /////////////////////////////////////////////////////////////////////
  if(mySnapshotFileName!="")
  {

    /*You must have at least one paintEvent() delivered for the window to be
      rendered properly.

      It looks like you don't run the event loop in non-interactive mode, so the
      event is never occuring.

      To achieve this without runing the event loop: show the window, then call
      qApp->processEvents(), grab the pixmap, save it, hide the window and exit.
      */
    //qgis->show();
    a.processEvents();
    QPixmap * myQPixmap = new QPixmap(800,600);
    myQPixmap->fill();
    qgis->saveMapAsImage(mySnapshotFileName,myQPixmap);
    a.processEvents();
    qgis->hide();
    return 1;
  }


  /////////////////////////////////////////////////////////////////////
  // Continue on to interactive gui...
  /////////////////////////////////////////////////////////////////////
  qgis->show();
  a.connect(&a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()));

  return a.exec();

}
