/***************************************************************************
                               qgsattributeaction.cpp 

 A class that stores and controls the managment and execution of actions 
 associated. Actions are defined to be external programs that are run
 with user-specified inputs that can depend on the value of layer
 attributes. 

                             -------------------
    begin                : Oct 24 2004
    copyright            : (C) 2004 by Gavin Macaulay
    email                : gavin at macaulay dot co dot nz

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
/*  $Id$ */

#include <iostream>

#include <qstringlist.h>
#include <qmessagebox.h>
#include <qdom.h>

#include "qgsattributeaction.h"

static const char * const ident_ = "$Id$";

void QgsAttributeAction::addAction(QString name, QString action,
				   bool capture)
{
  mActions.push_back(QgsAction(name, action, capture));
}

void QgsAttributeAction::doAction(unsigned int index, const std::vector<std::pair<QString, QString> >& values,
				  int defaultValueIndex) 
{
  aIter action = retrieveAction(index);

  // A couple of extra options for running the action may be
  // useful. For example,
  // - run the action inside a terminal (on unix)
  // - capture the stdout from the process and display in a dialog
  //   box
  //
  // The capture stdout one is partially implemented. It just needs
  // the UI and the code in this function to select on the
  // action.capture() return value.

  if (action != end())
  {
    QString expanded_action = expandAction(action->action(), values, defaultValueIndex);
    std::cout << "Running command '" << expanded_action << "'.\n";
    QStringList args = QStringList::split(" ", expanded_action);

    // This is beginning to duplicate the functionality in the
    // Launcher plugin - perhaps that should be used instead?
    
    process = new QProcess();
    process->setArguments(args);
    if (!process->start())
    {
      QMessageBox::critical(0, "Unable to run command", 
			    "Unable to run the command \n" + expanded_action +
			    "\n", QMessageBox::Ok, QMessageBox::NoButton);
    }
    delete process;
  }
}

QgsAttributeAction::aIter QgsAttributeAction::retrieveAction(unsigned int index) const
{
  // This function returns an iterator so that it's easy to deal with
  // an invalid index begin given.
  aIter a_iter = end();

  if (index >= 0 && index < mActions.size())
  {
    a_iter = mActions.begin();
    for (int i = 0; i < index; ++i, ++a_iter)
      {}
  }
  return a_iter;
}

QString QgsAttributeAction::expandAction(QString action, const std::vector<std::pair<QString, QString> >& values, 
					 int clickedOnValue)
{
  // This function currently replaces all %% characters in the action
  // with the value from values[clickedOnValue].second, and then
  // searches for all strings that go %attribite_name, where
  // attribute_name is found in values[x].first, and replaces any that
  // it finds by values[s].second.

  // Additional substitutions could include symbols for $CWD, $HOME,
  // etc (and their OSX and Windows equivalents)

  // This function will potentially fall apart if any of the
  // substitutions produce text that could match another
  // substitution. May be better to adopt a two pass approach - identify
  // all matches and their substitutions and then do a second pass 
  // for the actual substitutions.

  QString expanded_action;
  if (clickedOnValue >= 0 && clickedOnValue < values.size())
    expanded_action = action.replace("%%", values[clickedOnValue].second);
  else
    expanded_action = action;

  for (int i = 0; i < values.size(); ++i)
    {
      QString to_replace = "%" + values[i].first;
      expanded_action = expanded_action.replace(to_replace, values[i].second);
    }

  return expanded_action;
}

bool QgsAttributeAction::writeXML(QDomNode& layer_node, QDomDocument& doc) const
{
  QDomElement aActions = doc.createElement("attributeactions");

  aIter a_iter = begin();

  for (; a_iter != end(); ++a_iter)
  {
    QDomElement actionSetting = doc.createElement("actionsetting");
    actionSetting.setAttribute("name", a_iter->name());
    actionSetting.setAttribute("action", a_iter->action());
    actionSetting.setAttribute("capture", a_iter->capture());
    aActions.appendChild(actionSetting);
  }
  layer_node.appendChild(aActions);

  return true;
}

bool QgsAttributeAction::readXML(QDomNode& layer_node)
{
  QDomNode aaNode = layer_node.namedItem("attributeactions");

  if (!aaNode.isNull())
  {
    QDomNodeList actionsettings = aaNode.childNodes();
    for (int i = 0; i < actionsettings.length(); ++i)
    {
      QDomElement setting = actionsettings.item(i).toElement();
      int capture = setting.attributeNode("capture").value().toInt();
      addAction(setting.attributeNode("name").value(),
		setting.attributeNode("action").value(),
		capture == 0 ? false : true);
    }
  }
  return true;
}
