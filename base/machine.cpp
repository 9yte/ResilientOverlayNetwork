//                   In the name of GOD
/**
 * Partov is a simulation engine, supporting emulation as well,
 * making it possible to create virtual networks.
 *  
 * Copyright © 2009-2014 Behnam Momeni.
 * 
 * This file is part of the Partov.
 * 
 * Partov is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Partov is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Partov.  If not, see <http://www.gnu.org/licenses/>.
 *  
 */

#include "machine.h"

#include "interface.h"
#include "frame.h"
#include "cf.h"

Machine::Machine (const ClientFramework *cf, int count) {
	this->cf = cf;
	iface = new Interface[countOfInterfaces = count];
}

Machine::~Machine () {
	delete iface;
}

void Machine::initInterface (int index, uint64 mac, uint32 ip, uint32 mask) {
	if (0 <= index && index < countOfInterfaces) {
		iface[index].init (index, mac, ip, mask, cf);
	}
}

int Machine::getCountOfInterfaces () const {
	return countOfInterfaces;
}

void Machine::printInterfacesInformation () const {
	PRINT ("==========================================");
	for (int i = 0; i < countOfInterfaces; ++i) {
		if (i > 0) {
			PRINT ("---------------------");
		}
		PRINT ("Interface " << i << ":");
		iface[i].printInterfaceInformation ();
	}
	PRINT ("==========================================");
}

void Machine::setCustomInformation (const std::string &info) {
	customInfo = info;
}

const std::string &Machine::getCustomInformation () const {
	return customInfo;
}

bool Machine::sendFrame (Frame frame, int ifaceIndex) const {
	return cf->sendFrame (frame, ifaceIndex);
}

