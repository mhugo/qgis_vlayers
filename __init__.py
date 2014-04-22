# -*- coding: utf-8 -*-
"""
/***************************************************************************
 VTableSupport
                                 A QGIS plugin
 Virtual table support experimentation
                             -------------------
        begin                : 2014-03-31
        copyright            : (C) 2014 by Hugo Mercier/Oslandia
        email                : hugo.mercier@oslandia.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
 This script initializes the plugin, making it known to QGIS.
"""

def classFactory(iface):
    # load VTableSupport class from file VTableSupport
    from vtablesupport import VTableSupport
    return VTableSupport(iface)
