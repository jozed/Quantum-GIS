# -*- coding: utf-8 -*-
from PyQt4.QtCore import *
from PyQt4.QtGui import *
from qgis.core import *
from qgis.gui import *

from ui_widgetGrid import Ui_GdalToolsWidget as Ui_Widget
from widgetPluginBase import GdalToolsBasePluginWidget as BasePluginWidget
import GdalTools_utils as Utils

class GdalToolsDialog(QWidget, Ui_Widget, BasePluginWidget):

  def __init__(self, iface):
      QWidget.__init__(self)
      self.iface = iface
      self.canvas = self.iface.mapCanvas()
      self.algorithm = ('invdist', 'average', 'nearest', 'datametrics')
      self.datametrics = ('minimum', 'maximum', 'range')

      self.setupUi(self)
      BasePluginWidget.__init__(self, self.iface, "gdal_grid")

      self.outSelector.setType( self.outSelector.FILE )
      self.extentSelector.setCanvas(self.canvas)
      #self.extentSelector.stop()

      # set the default QSpinBoxes and QProgressBar value
      self.widthSpin.setValue(3000)
      self.heightSpin.setValue(3000)
      self.invdistPowerSpin.setValue(2.0)

      self.outputFormat = Utils.fillRasterOutputFormat()
      self.lastEncoding = Utils.getLastUsedEncoding()

      self.setParamsStatus(
        [
          (self.inSelector, SIGNAL("filenameChanged()")),
          (self.outSelector, SIGNAL("filenameChanged()")),
          (self.zfieldCombo, SIGNAL("currentIndexChanged(int)"), self.zfieldCheck),
          (self.algorithmCombo, SIGNAL("currentIndexChanged(int)"), self.algorithmCheck),
          (self.stackedWidget, None, self.algorithmCheck),
          ([self.invdistPowerSpin, self.invdistSmothingSpin, self.invdistRadius1Spin, self.invdistRadius2Spin, self.invdistAngleSpin, self.invdistNoDataSpin], SIGNAL("valueChanged(double)")),
          ([self.invdistMaxPointsSpin, self.invdistMinPointsSpin], SIGNAL("valueChanged(int)")),
          ([self.averageRadius1Spin, self.averageRadius2Spin, self.averageAngleSpin, self.averageNoDataSpin], SIGNAL("valueChanged(double)")),
          (self.averageMinPointsSpin, SIGNAL("valueChanged(int)")),
          ([self.nearestRadius1Spin, self.nearestRadius2Spin, self.nearestAngleSpin, self.nearestNoDataSpin], SIGNAL("valueChanged(double)")),
          (self.datametricsCombo, SIGNAL("currentIndexChanged(int)")),
          ([self.datametricsRadius1Spin, self.datametricsRadius2Spin, self.datametricsAngleSpin, self.datametricsNoDataSpin], SIGNAL("valueChanged(double)")),
          (self.datametricsMinPointsSpin, SIGNAL("valueChanged(int)")), 
          (self.extentSelector, [SIGNAL("selectionStarted()"), SIGNAL("newExtentDefined()")], self.extentGroup), 
          ( [self.widthSpin, self.heightSpin], SIGNAL( "valueChanged(int)" ), self.resizeGroupBox )
        ]
      )

      self.connect(self.inSelector, SIGNAL("selectClicked()"), self.fillInputFileEdit)
      self.connect(self.outSelector, SIGNAL("selectClicked()"), self.fillOutputFileEdit)
      self.connect(self.inSelector, SIGNAL("layerChanged()"), self.fillFieldsCombo)
      self.connect(self.extentGroup, SIGNAL("toggled(bool)"), self.onExtentCheckedChanged)


  def onClosing(self):
      self.extentSelector.stop()
      BasePluginWidget.onClosing(self)

  def onExtentCheckedChanged(self, enabled):
        self.extentSelector.start() if enabled else self.extentSelector.stop()

  def onLayersChanged(self):
      self.inSelector.setLayers( Utils.LayerRegistry.instance().getVectorLayers() )

  def fillFieldsCombo(self):
      if self.inSelector.layer() == None:
        return

      self.lastEncoding = self.inSelector.layer().dataProvider().encoding()
      self.loadFields( self.getInputFileName() )
      
  def fillInputFileEdit(self):
      lastUsedFilter = Utils.FileFilter.lastUsedVectorFilter()
      inputFile, encoding = Utils.FileDialog.getOpenFileName(self, self.tr( "Select the input file for Grid" ), Utils.FileFilter.allVectorsFilter(), lastUsedFilter, True)
      if inputFile.isEmpty():
        return
      Utils.FileFilter.setLastUsedVectorFilter(lastUsedFilter)

      self.inSelector.setFilename(inputFile)
      self.lastEncoding = encoding

      self.loadFields( inputFile )

  def fillOutputFileEdit(self):
      lastUsedFilter = Utils.FileFilter.lastUsedRasterFilter()
      outputFile = Utils.FileDialog.getSaveFileName(self, self.tr( "Select the raster file to save the results to" ), Utils.FileFilter.allRastersFilter(), lastUsedFilter )
      if outputFile.isEmpty():
        return
      Utils.FileFilter.setLastUsedRasterFilter(lastUsedFilter)

      self.outputFormat = Utils.fillRasterOutputFormat( lastUsedFilter, outputFile )
      self.outSelector.setFilename(outputFile)

  def getArguments(self):
      arguments = QStringList()
      if self.zfieldCheck.isChecked() and self.zfieldCombo.currentIndex() >= 0:
        arguments << "-zfield"
        arguments << self.zfieldCombo.currentText()
      inputFn = self.getInputFileName()
      if not inputFn.isEmpty():
        arguments << "-l"
        arguments << QFileInfo( inputFn ).baseName()
      if self.extentGroup.isChecked():
        rect = self.extentSelector.getExtent()
        if rect != None:
          arguments << "-txe"
          arguments << str(rect.xMinimum())
          arguments << str(rect.xMaximum())
          arguments << "-tye"
          arguments << str(rect.yMaximum())
          arguments << str(rect.yMinimum())
      if self.algorithmCheck.isChecked() and self.algorithmCombo.currentIndex() >= 0:
        arguments << "-a"
        arguments << self.algorithmArguments(self.algorithmCombo.currentIndex())
      if self.resizeGroupBox.isChecked():
        arguments << "-outsize"
        arguments << str( self.widthSpin.value() )
        arguments << str( self.heightSpin.value() )
      outputFn = self.getOutputFileName()
      if not outputFn.isEmpty():
        arguments << "-of"
        arguments << self.outputFormat
      arguments << inputFn
      arguments << outputFn
      return arguments

  def getInputFileName(self):
      return self.inSelector.filename()

  def getOutputFileName(self):
      return self.outSelector.filename()

  def addLayerIntoCanvas(self, fileInfo):
      self.iface.addRasterLayer(fileInfo.filePath())

  def algorithmArguments(self, index):
      algorithm = self.algorithm[index]
      arguments = QStringList()
      if algorithm == "invdist":
        arguments.append(algorithm)
        arguments.append("power=" + str(self.invdistPowerSpin.value()))
        arguments.append("smothing=" + str(self.invdistSmothingSpin.value()))
        arguments.append("radius1=" + str(self.invdistRadius1Spin.value()))
        arguments.append("radius2=" + str(self.invdistRadius2Spin.value()))
        arguments.append("angle=" + str(self.invdistAngleSpin.value()))
        arguments.append("max_points=" + str(self.invdistMaxPointsSpin.value()))
        arguments.append("min_points=" + str(self.invdistMinPointsSpin.value()))
        arguments.append("nodata=" + str(self.invdistNoDataSpin.value()))
      elif algorithm == "average":
        arguments.append(algorithm)
        arguments.append("radius1=" + str(self.averageRadius1Spin.value()))
        arguments.append("radius2=" + str(self.averageRadius2Spin.value()))
        arguments.append("angle=" + str(self.averageAngleSpin.value()))
        arguments.append("min_points=" + str(self.averageMinPointsSpin.value()))
        arguments.append("nodata=" + str(self.averageNoDataSpin.value()))
      elif algorithm == "nearest":
        arguments.append(algorithm)
        arguments.append("radius1=" + str(self.nearestRadius1Spin.value()))
        arguments.append("radius2=" + str(self.nearestRadius2Spin.value()))
        arguments.append("angle=" + str(self.nearestAngleSpin.value()))
        arguments.append("nodata=" + str(self.nearestNoDataSpin.value()))
      else:
        arguments.append(self.datametrics[self.datametricsCombo.currentIndex()])
        arguments.append("radius1=" + str(self.datametricsRadius1Spin.value()))
        arguments.append("radius2=" + str(self.datametricsRadius2Spin.value()))
        arguments.append("angle=" + str(self.datametricsAngleSpin.value()))
        arguments.append("min_points=" + str(self.datametricsMinPointsSpin.value()))
        arguments.append("nodata=" + str(self.datametricsNoDataSpin.value()))
      return arguments.join(":")

  def loadFields(self, vectorFile = QString()):
      self.zfieldCombo.clear()

      if vectorFile.isEmpty():
        return

      try:
        (fields, names) = Utils.getVectorFields(vectorFile)
      except Utils.UnsupportedOGRFormat, e:
        QErrorMessage(self).showMessage( e.args[0] )
        self.inSelector.setLayer( None )
        return

      ncodec = QTextCodec.codecForName(self.lastEncoding)
      for name in names:
        self.zfieldCombo.addItem( ncodec.toUnicode(name) )

