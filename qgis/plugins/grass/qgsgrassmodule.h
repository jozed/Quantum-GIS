/***************************************************************************
                              qgsgrasstools.h 
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
#ifndef QGSGRASSMODULE_H
#define QGSGRASSMODULE_H

class QCloseEvent;
class QString;
class QStringList;
class QGroupBox;
class QFrame;
class QListView;
class QDomNode;
class QDomElement;
class QComboBox;
class QLineEdit;

#include <vector>
#include <qgroupbox.h>
#include <qcheckbox.h>
#include <qprocess.h>

// Must be here, so that it is included to moc file
#include "../../src/qgisapp.h"
#include "../../src/qgisiface.h"

class QgsGrassProvider;
class QgsGrassTools;
class QgsGrassModuleItem;
#include "qgsgrassmodulebase.h"

/*! \class QgsGrassModule
 *  \brief Interface to GRASS modules.
 *
 */
class QgsGrassModule: public QgsGrassModuleBase 
{
    Q_OBJECT;

public:
    //! Constructor
    QgsGrassModule ( QgsGrassTools *tools, QgisApp *qgisApp, QgisIface *iface,  
	           QString path, QWidget * parent = 0, const char * name = 0, WFlags f = 0 );

    //! Destructor
    ~QgsGrassModule();

    //! Returns module label for module description path
    static QString label ( QString path );

    //! Find element in GRASS module description by key, if not found, returned element is null
    static QDomNode nodeByKey ( QDomElement gDocElem, QString key );

    //! Returns pointer to QGIS interface 
    QgisIface *qgisIface();

    //! Returns pointer to QGIS application
    QgisApp *qgisApp();

public slots:
    //! Run the module with current options
    void run ();

    //! Close the module tab
    void close ();

    //! Running process finished
    void finished ();

    //! Read module's standard output
    void readStdout();

    //! Read module's standard error
    void readStderr();

private:
    //! QGIS application
    QgisApp *mQgisApp;

    //! Pointer to the QGIS interface object
    QgisIface *mIface;

    //! Pointer to canvas
    QgsMapCanvas *mCanvas;

    //! Pointer to GRASS Tools 
    QgsGrassTools *mTools;

    //! Module definition file path (.qgm file)
    QString mPath;

    //! Name of module executable 
    QString mXName;

    //! Path to module executable 
    QString mXPath;

    //! Parent widget
    QWidget *mParent;

    //! Option items
    std::vector<QgsGrassModuleItem*> mItems;

    //! Running GRASS module
    QProcess mProcess;

    //! QGIS directory
    QString mAppDir;
};

/*! \class QgsGrassModuleItem
 *  \brief GRASS module option
 */
class QgsGrassModuleItem
{
public:
    /*! \brief Constructor
     * \param qdesc option element in QGIS module description XML file
     * \param gdesc GRASS module XML description file
     */
    QgsGrassModuleItem ();

    //! Destructor
    virtual ~QgsGrassModuleItem();

    //! Pointer to GRASS module
    QgsGrassModule *mModule;

    //! Option key, for flags without '-'
    QString mKey;

    //! GRASS description
    QString mDescription;

    //! Hidden option or displayed
    bool mHidden;

    //! Is the item hidden
    bool hidden();

    //! Retruns list of options which will be passed to module
    virtual QStringList options(); 
private:

};

/*! \class QgsGrassModuleOption
 *  \brief  GRASS option 
 */
class QgsGrassModuleOption: public QFrame, public QgsGrassModuleItem
{
    Q_OBJECT;

public:
    /*! \brief Constructor
     * \param qdesc option element in QGIS module description XML file
     * \param gdesc GRASS module XML description file
     */
    QgsGrassModuleOption ( QgsGrassModule *module, 
	                  QDomElement &qdesc, QDomElement &gdesc,
                          QWidget * parent = 0 );

    //! Destructor
    ~QgsGrassModuleOption();

    //! Control option
    enum ControlType { LineEdit, ComboBox, SpinBox, CheckBox };
    
    //! Retruns list of options which will be passed to module
    virtual QStringList options(); 
    

private:
    //! Control type
    ControlType mControlType;
    
    //! Combobox
    QComboBox *mComboBox;
    
    //! Combobox
    QLineEdit *mLineEdit;

    //! Predefined answer from config
    QString mAnswer;
};

/*! \class QgsGrassModuleFlag
 *  \brief  GRASS flag
 */
class QgsGrassModuleFlag: public QCheckBox, public QgsGrassModuleItem
{
    Q_OBJECT;

public:
    /*! \brief Constructor
     * \param qdesc option element in QGIS module description XML file
     * \param gdesc GRASS module XML description file
     */
    QgsGrassModuleFlag ( QgsGrassModule *module, 
	                  QDomElement &qdesc, QDomElement &gdesc,
                          QWidget * parent = 0 );

    //! Destructor
    ~QgsGrassModuleFlag();
    //! Retruns list of options which will be passed to module
    virtual QStringList options(); 
private:
    //! Predefined answer from config
    bool mAnswer;
};

/*! \class QgsGrassModuleInput
 *  \brief Class representing raster or vector module input
 */
class QgsGrassModuleInput: public QGroupBox, public QgsGrassModuleItem
{
    Q_OBJECT;

public:
    /*! \brief Constructor
     * \param qdesc option element in QGIS module description XML file
     * \param gdesc GRASS module XML description file
     */
    QgsGrassModuleInput ( QgsGrassModule *module, 
	                  QDomElement &qdesc, QDomElement &gdesc,
                          QWidget * parent = 0 );

    //! Destructor
    ~QgsGrassModuleInput();

    enum Type { Vector, Raster };

    //! Retruns list of options which will be passed to module
    virtual QStringList options(); 

public slots:
    //! Fill combobox with currently available maps in QGIS canvas
    void updateQgisLayers();

private:
    //! Input type
    Type mType;
    
    //! Combobox for QGIS layers
    QComboBox *mLayerComboBox;

    //! Vector of map@mapset in the combobox
    std::vector<QString> mMaps;
};
#endif // QGSGRASSMODULE_H
