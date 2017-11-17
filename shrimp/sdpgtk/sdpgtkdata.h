#ifndef SDPGTKDATA_H
#define SDPGTKDATA_H

// SDPGTK Library
// Copyright � 1995-2003, Timothy M. Shead
//
// Contact: tshead@k-3d.com
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

/** \file
		\brief Declares the sdpGtkData class, which encapsulates a GtkData object
		\author Timothy M. Shead (tshead@k-3d.com)
*/

#include "sdpgtkobject.h"

////////////////////////////////////////////////////////////////////////////////////////
// sdpGtkData

/// Encapulates a GtkData object
class sdpGtkData : public sdpGtkObject
{
public:
	/// Standard constructor
	sdpGtkData();
	/// Attaches this instance to an existing GtkData object
	sdpGtkData(GtkData* Data);

	~sdpGtkData();

	/// Casts this instance to a GtkData pointer
	operator GtkData*() { return GTK_DATA(m_Object); }
};

#endif // SDPGTKDATA_H
