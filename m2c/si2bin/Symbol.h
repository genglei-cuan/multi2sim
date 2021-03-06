/*
 *  Multi2Sim
 *  Copyright (C) 2012  Rafael Ubal (ubal@ece.neu.edu)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef M2C_SI2BIN_SYMBOL_H
#define M2C_SI2BIN_SYMBOL_H

#include <iostream>

namespace si2bin
{

class Symbol 
{
	std::string name;
	int value;

	/* True if the symbol definition has been found already. False when
	 * the symbol has been found as a forward declaration. */
	bool defined;

public:

	/* Constructor */
	Symbol(const std::string &name) : name(name), value(0),
			defined(false) { }

	/* Getters */
	const std::string &getName() { return name; }
	int getValue() { return value; }
	bool getDefined() { return defined; }

	/* Setters */
	void setValue(int value) { this->value = value; }
	void setDefined(bool defined) { this->defined = defined; }

	/* Dump Function */
	void Dump(std::ostream& os);
	friend std::ostream &operator<<(std::ostream &os, Symbol &symbol)
			{ symbol.Dump(os); return os; }
};

} /* Namespace S2B */


#endif

