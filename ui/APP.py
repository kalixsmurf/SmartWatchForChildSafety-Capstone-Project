# -*- coding: utf-8 -*-

# Form implementation generated from reading ui file 'APP.ui'
#
# Created by: PyQt5 UI code generator 5.15.9
#
# WARNING: Any manual changes made to this file will be lost when pyuic5 is
# run again.  Do not edit this file unless you know what you are doing.


from PyQt5 import QtCore, QtGui, QtWidgets


class Ui_ChildSafety(object):
    def setupUi(self, ChildSafety):
        ChildSafety.setObjectName("ChildSafety")
        ChildSafety.resize(320, 240)
        self.centralwidget = QtWidgets.QWidget(ChildSafety)
        self.centralwidget.setObjectName("centralwidget")
        self.tableWidget = QtWidgets.QTableWidget(self.centralwidget)
        self.tableWidget.setGeometry(QtCore.QRect(30, 10, 251, 191))
        self.tableWidget.setObjectName("tableWidget")
        self.tableWidget.setColumnCount(2)
        self.tableWidget.setRowCount(0)
        item = QtWidgets.QTableWidgetItem()
        self.tableWidget.setHorizontalHeaderItem(0, item)
        item = QtWidgets.QTableWidgetItem()
        self.tableWidget.setHorizontalHeaderItem(1, item)
        ChildSafety.setCentralWidget(self.centralwidget)
        self.statusbar = QtWidgets.QStatusBar(ChildSafety)
        self.statusbar.setObjectName("statusbar")
        ChildSafety.setStatusBar(self.statusbar)

        self.retranslateUi(ChildSafety)
        QtCore.QMetaObject.connectSlotsByName(ChildSafety)

    def retranslateUi(self, ChildSafety):
        _translate = QtCore.QCoreApplication.translate
        ChildSafety.setWindowTitle(_translate("ChildSafety", "Child Safety"))
        item = self.tableWidget.horizontalHeaderItem(0)
        item.setText(_translate("ChildSafety", "Time"))
        item = self.tableWidget.horizontalHeaderItem(1)
        item.setText(_translate("ChildSafety", "Prediction Result"))
