/******************************************************
  climatedataprocessor.h  -  description
  -------------------
begin                : Thu May 15 2003
copyright            : (C) 2003 by Tim Sutton
email                : t.sutton@reading.ac.uk
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/



#ifndef CLIMATEDATAPROCESSOR_H
#define CLIMATEDATAPROCESSOR_H

#include "dataprocessor.h"
#include "filewriter.h"
#include "filereader.h"
#include "filegroup.h"
#include <qmap.h>
#include <qstring.h>

/**
* This struct is simple container used in the 'run' method.
* @todo Remove this if possible
*/
struct FileWriterStruct
{
    /** A filewriter pointer */
    FileWriter * structFileWriter;
    /** The fill path and file name of the file refenced */
    QString structFullFileName;
};


/**The ClimateDataProcessor calculates specific climate variables using
 *DataProcessor functions.
 *@author Tim Sutton
 */

class ClimateDataProcessor {
    public:
        ClimateDataProcessor();

        /*
        ClimateDataProcessor(
                int theFileStartYear,
                int theJobStartYear,
                int theJobEndYear,
                QString theInputFileTypeString,
                QString theOutputFileTypeString
                );
        */
        ~ClimateDataProcessor();



        // Getters and setters

        /** Write property of QString meanTempFileGroup. */
        void setMeanTempFileName ( QString theFileNameString);
        /** Read property of QString meanTempFileGroup. */
        const QString  getMeanTempFileName ();

        /** Write property of QString minTempFileGroup. */
        void setMinTempFileName ( QString theFileNameString);
        /** Read property of QString minTempFileGroup. */
        const QString  getMinTempFileName ();

        /** Write property of QString maxTempFileGroup. */
        void setMaxTempFileName ( QString theFileNameString);
        /** Read property of QString maxTempFileGroup. */
        const QString  getMaxTempFileName ();

        /** Write property of QString diurnalTempFileGroup. */
        void setDiurnalTempFileName ( QString theFileNameString);
        /** Read property of QString diurnalTempFileGroup. */
        const QString  getDiurnalTempFileName ();

        /** Write property of QString meanPrecipFileGroup. */
        void setMeanPrecipFileName ( QString theFileNameString);
        /** Read property of QString meanPrecipFileGroup. */
        const QString  getMeanPrecipFileName ();

        /** Write property of QString frostDaysFileGroup. */
        void setFrostDaysFileName ( QString theFileNameString);
        /** Read property of QString frostDaysFileGroup. */
        const QString  getFrostDaysFileName ();

        /** Write property of QString totalSolarRadFileGroup. */
        void setTotalSolarRadFileName ( QString theFileNameString);
        /** Read property of QString totalSolarRadFileGroup. */
        const QString  getTotalSolarRadFileName ();

        /** Write property of QString windSpeedFileGroup. */
        void setWindSpeedFileName ( QString theFileNameString);
        /** Read property of QString windSpeedFileGroup. */
        const QString  getWindSpeedFileName ();

        /** Write property of QString * outputFilePathString. */
        void setOutputFilePathString( QString theFilePathString);
        /** Read property of QString * outputFilePathString. */
        const QString getOutputFilePathString();

        /** Write property of int fileStartYearInt. */
        void setFileStartYearInt( const int theYearInt);
        /** Read property of int fileStartYearInt. */
        const int getFileStartYearInt();

        /** Write property of int jobStartYearInt. */
        void setJobStartYearInt( const int theYearInt);
        /** Read property of int jobStartYearInt. */
        const int getJobStartYearInt();

        /** Write property of int jobEndYearInt. */
        void setJobEndYearInt( const int theYearInt);
        /** Read property of int jobEndYearInt. */
        const int getJobEndYearInt();

        /** Write property of FileReader::FileType inputFileType. */
        void setInputFileType( const FileReader::FileTypeEnum theInputFileType);
        /** Overloaded version of above that taks a string and looks up the enum */
        void setInputFileType( const QString theInputFileTypeString);
        /** Read property of FileReader::FileType inputFileType. */
        const FileReader::FileTypeEnum getInputFileType();

        /** Write property of FileWriter::FileType outputFileType. */
        void setOutputFileType( const FileWriter::FileTypeEnum theOutputFileType);
        /** Overloaded version of above that takes a string and looks up the enum */
        void setOutputFileType( const QString theOutputFileTypeString);
        /** Read property of FileWriter::FileType outputFileType. */
        const FileWriter::FileTypeEnum getOutputFileType();


        /**  Set up the filegroups for each filename that has been registered */
        bool makeFileGroups(int theStartYearInt);
        /** Set up an individual file group (called by makeFileGroups for
         *   each filegroup that needs to be initialised) */
        FileGroup * initialiseFileGroup(QString theFileNameString,int theStartYearInt);
        /**  Build a list of which calculations can be performed given the input files
         *    that have been registered. The boolean field indicates whether the user actually
         *    want to perform this calculation
         *    @see addUserCalculation */
        bool  makeAvailableCalculationsMap();
        /** Get the list of available calculations */
        QMap <QString, bool > getAvailableCalculationsMap();

        /**  Add a calculation to the list of those requested to be carried out by the user */
        bool addUserCalculation(QString theCalculationNameString);

        /** Start the data analysis process. When everything else is set up, this is the method to call! */
        bool run();

        /** get a Description of the ClimateDataProcessor vars. */
        QString getDescription();

        /** Write property of bool filesInSeriesFlag. */
        void setFilesInSeriesFlag( const bool theFlagl);
        /** Read property of bool filesInSeriesFlag. */
        const bool getFilesInSeriesFlag();

        /** Write property of QString outputHeaderString. */
        void setOutputHeaderString( const QString& theOutputHeaderString);
        /** Read property of QString outputHeaderString. */
        const QString getOutputHeaderString();

    private:

        // Private methods
        /** This method is intended for debugging purposes only */
        void printVectorAndResult(QValueVector<float> theVector, float theResultFloat);

        /**This is a private method. It is a simple method to populate the
         * inputFileTypeMap attribute - this will usually be called by the
         * constructor(s). All keys (file type strings) will be  stored in upper case.*/
        bool makeInputFileTypeMap();

        /**This is a private method. It is a simple method to populate the
         * outputFileTypeMap attribute - this will usually be called by the
         * constructor(s). All keys (file type strings) will be  stored in upper case.*/
        bool makeOutputFileTypeMap();

        /** Little utility method to convert from int to string */
        QString intToString(int theInt);

        // Private attributes
        /** The directory where the processed results will be stored. */
        QString outputFilePathString;
        /** This is the FILE START year (must be common to all files used!)
         *   in the files provided to the climate data processor. */
        int fileStartYearInt;
        /** This is the START year that should actually be processed
         *   (must be common to all files used!) in the files provided
         *   to the climate data proccessor. */
        int jobStartYearInt;
        /** This is the END year that should actually be processed
         *   (must be common to all files used!) in the files provided
         *   to the climate data proccessor. */

        int jobEndYearInt;
        /** The type of input files to be processed by the climate date processor. */
        FileReader::FileTypeEnum inputFileType;

        /** The type of output files to be produced by the climate date processor. */
        FileWriter::FileTypeEnum outputFileType;

        /** This is a map (associative array) that stores the key/value pairs
         * for the INPUT filetype. The key is the verbose name for the file type
         * (as will typically appear in the user interface, and the value
         * is the FileReader::FileTypeEnum equivalent.
         * @see makeInputFileTypeMap()
         * @see makeOutputFileTypeMap()
         */
        QMap <QString, FileReader::FileTypeEnum > inputFileTypeMap;


        /** This is a map (associative array) that stores the key/value pairs
         * for the OUTPUT filetype. The key is the verbose name for the file type
         * (as will typically appear in the user interface, and the value
         * is the FileWriter::FileTypeEnum equivalent.
         * @see makeInputFileTypeMap()
         * @see makeOutputFileTypeMap()
         */
        QMap <QString, FileWriter::FileTypeEnum > outputFileTypeMap;

        /** This is a map (associative array) that stores which calculations can be performed
         *   given the input files that have been registered with this climatedataprocessor.
         *   The boolean flag will be used to indicate whether the user actually wants to
         *   perform the calculation on the input dataset(s).
         *   @see makeAvailableCalculationsMap
         *   @see addUserCalculation
         */

        QMap <QString, bool > availableCalculationsMap;


        /** A filegroup containing files with mean temperature data. */
        FileGroup *  meanTempFileGroup;
        /** The file name that contains mean temp data. If the file type is
        * one where the data is stored in series (a single file per month),
        * this member will store the name of the first file in the series. */
        QString meanTempFileNameString;

        /** A filegroup containing files with minimum temperature data. */
        FileGroup * minTempFileGroup;
        /** The file name that contains min temp data. If the file type is
        * one where the data is stored in series (a single file per month),
        * this member will store the name of the first file in the series. */
        QString minTempFileNameString;

        /** A filegroup containing files with maximum temperature data. */
        FileGroup *  maxTempFileGroup;
        /** The file name that contains max temp data. If the file type is
        * one where the data is stored in series (a single file per month),
        * this member will store the name of the first file in the series. */
        QString maxTempFileNameString;

        /** A filegroup containing files with diurnal temperature data. */
        FileGroup *  diurnalTempFileGroup;
        /** The file name that contains diurnal temp data. If the file type is
        * one where the data is stored in series (a single file per month),
        * this member will store the name of the first file in the series. */
        QString diurnalTempFileNameString;

        /** A filegroup containing files with mean precipitation data. */
        FileGroup *  meanPrecipFileGroup;
        /** The file name that contains mean precipitation data. If the file type is
        * one where the data is stored in series (a single file per month),
        * this member will store the name of the first file in the series. */
        QString meanPrecipFileNameString;

        /** A filegroup containing files with number of frost days data. */
        FileGroup *  frostDaysFileGroup;
        /** The file name that contains mean frost data. If the file type is
        * one where the data is stored in series (a single file per month),
        * this member will store the name of the first file in the series. */
        QString frostDaysFileNameString;

        /** A filegroup containing files with solar radiation data. */
        FileGroup *  totalSolarRadFileGroup;
        /** The file name that contains mean solar radiation data. If the file type is
        * one where the data is stored in series (a single file per month),
        * this member will store the name of the first file in the series. */
        QString totalSolarRadFileNameString;

        /** A filegroup containing files with wind speed data. */
        FileGroup *  windSpeedFileGroup;
        /** The file name that contains wind speed data. If the file type is
        * one where the data is stored in series (a single file per month),
        * this member will store the name of the first file in the series. */
        QString windSpeedFileNameString;

        /** For certain input types (notably cres, arcinfo and Reading paleoclimate),
         * each months data is stored in a discrete file. Files should be numbered
         * e.g. meantemp01.asc, meantemp2.asc...meantemp12.asc for each month.
         * This flag lets us know whether data is in a series of seperate files for each month
         * or can all be found in the same file. */
        bool filesInSeriesFlag;

        /** This is a standard header (e.g. arc/info header) that will be appended to any output grids. */
        QString outputHeaderString;
};

#endif
/*
   bool meanPrecipOverDriestQ
   bool meanTempOverWarmestQ
   bool meanPrecipOverWettestQ
   bool meanTempOverCoolestM
   bool lowestTempOverCoolestM
   bool meanPrecipOverDriestM
   bool meanTempOverWarmestM
   bool highestTempOverWarmestM
   bool meanPrecipOverWettestM
   bool meanTemp
   bool meanPrecip
   bool meanDiurnal
   bool meanFrostDays
   bool meanRadiation
   bool meanWindSpeed
   bool stdevMeanTemp
   bool stdevMeanPrecip
   bool meanPrecipOverCoolestM
   bool meanDiurnalOverCoolestM
   bool meanRadiationOverCoolestM
   bool meanRadiationOverDriestM
   bool meanPrecipOverWarmestM
   bool meanDiurnalOverWarmestM
   bool meanRadiationOverWarmestM
   bool meanRadiationOverWettestM
   bool meanPrecipOverCoolestQ
   bool meanRadiationOverCoolestQ
   bool meanRadiationOverDriestQ
   bool meanPrecipOverWarmestQ
   bool meanRadiationOverWarmestQ
   bool meanRadiationOverWettestQ
   bool annualTempRange
   bool meanTempOverFrostFreeM
   bool meanPrecipOverFrostFreeM
   bool monthCountAboveFreezing
   */
