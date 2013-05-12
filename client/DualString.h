/*
 * Copyright (C) 2013 AirDC++ Project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef DCPLUSPLUS_DUALSTRING
#define DCPLUSPLUS_DUALSTRING

#include <string>

#include "typedefs.h"
#include "Text.h"

using std::string;

/* Class for storing a string as lowercase and normal with minimum memory usage overhead. Optimized for accessing the lowercase representation. */

class DualString : private string {
public:
	DualString(const string& aStr);

	const string& getLower() const { return *this; }
	string getNormal() const;

	size_t size() const noexcept { return std::string::size(); }

	bool hasUpperCase() const;
private:
	uint32_t charSizes;
};

#endif