/***************************************************************************
                         qgslogger.cpp  -  description
                             -------------------
    begin                : April 2006
    copyright            : (C) 2006 by Marco Hugentobler
    email                : marco.hugentobler at karto dot baug dot ethz dot ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include "qgslogger.h"

void QgsLogger::debug(const QString& msg, int debuglevel, const char* file, const char* function, int line)
{
  const char* dfile = debugFile();
  if(dfile) //exit if QGIS_DEBUG_FILE is set and the message comes from the wrong file
    {
      if(!file || strcmp(dfile, file) != 0)
	{
	  return;
	}
    }

  int dlevel = debugLevel();
  if(dlevel >= debuglevel && debuglevel > 0)
    {
      if(file == NULL)
	{
	  qDebug(msg.toLocal8Bit().data());
	}
      else if(function == NULL)
	{
	  qDebug("File: %s, Message: %s", file, msg.toLocal8Bit().data());
	}
      else if(line == -1)
	{
	  qDebug("File: %s, Function: %s, Message: %s", file, function, msg.toLocal8Bit().data());
	}
      else
	{
	  qDebug("File: %s, Function: %s, Line: %d, Message: %s", file, function, line, msg.toLocal8Bit().data());
	}
    }
}

void QgsLogger::debug(const QString& var, int val, int debuglevel, const char* file, const char* function, int line)
{
  const char* dfile = debugFile();
  if(dfile) //exit if QGIS_DEBUG_FILE is set and the message comes from the wrong file
    {
      if(!file || strcmp(dfile, file) != 0)
	{
	  return;
	}
    }

  int dlevel = debugLevel();
  if(dlevel >= debuglevel && debuglevel > 0)
    {
      if(file == NULL)
	{
	  qDebug("Variable: %s, Value: %d", var.toLocal8Bit().data(), val);
	}
      else if(function == NULL)
	{
	  qDebug("File: %s, Variable: %s, Value: %d", file, var.toLocal8Bit().data(), val);
	}
      else if(line == -1)
	{
	  qDebug("File: %s, Function: %s, Variable: %s, Value: %d", file, function, var.toLocal8Bit().data(), val);
	}
      else
	{
	  qDebug("File: %s, Function: %s, Line: %d, Variable: %s, Value: %d", file, function, line, var.toLocal8Bit().data(), val);
	}
    }
}

void QgsLogger::debug(const QString& var, double val, int debuglevel, const char* file, const char* function, int line)
{
  const char* dfile = debugFile();
  if(dfile) //exit if QGIS_DEBUG_FILE is set and the message comes from the wrong file
    {
      if(!file || strcmp(dfile, file) != 0)
	{
	  return;
	}
    }

  int dlevel = debugLevel();
  if(dlevel >= debuglevel && debuglevel > 0)
    {
      if(file == NULL)
	{
	  qDebug("Variable: %s, Value: %f", var.toLocal8Bit().data(), val);
	}
      else if(function == NULL)
	{
	  qDebug("File: %s, Variable: %s, Value: %f", file, var.toLocal8Bit().data(), val);
	}
      else if(line == -1)
	{
	  qDebug("File: %s, Function: %s, Variable: %s, Value: %f", file, function, var.toLocal8Bit().data(), val);
	}
      else
	{
	  qDebug("File: %s, Function: %s, Line: %d, Variable: %s, Value: %f", file, function, line, var.toLocal8Bit().data(), val);
	}
    }
}

void QgsLogger::warning(const QString& msg)
{
  qWarning(msg.toLocal8Bit().data());
}
  
void QgsLogger::critical(const QString& msg)
{
  qCritical(msg.toLocal8Bit().data());
}

void QgsLogger::fatal(const QString& msg)
{
  qFatal(msg.toLocal8Bit().data());
}

int QgsLogger::debugLevel()
{
  const char* dlevel = getenv("QGIS_DEBUG");
  if(dlevel == NULL) //environment variable not set
    {
#ifdef QGISDEBUG
      return 1; //1 is default value in debug mode
#else
      return 0;
#endif
    }
  int level = atoi(dlevel);
#ifdef QGISDEBUG
  if(level == 0)
    {
      level = 1;
    }
#endif
  return level;
}

const char* QgsLogger::debugFile()
{
  const char* dfile = getenv("QGIS_DEBUG_FILE");
  return dfile;
}
