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


#ifndef MACHINE_H_
#define MACHINE_H_

#include "partovdef.h"

class Interface;
class ClientFramework;
class Frame;

/* Abstract */ class Machine {
private:
	const ClientFramework *cf;
	std::string customInfo;

protected:
	int countOfInterfaces;
	Interface *iface;

public:
	Machine (const ClientFramework *cf, int count);
	virtual ~Machine ();

	virtual void initialize () = 0;
	virtual void run () = 0;

	void initInterface (int index, uint64 mac, uint32 ip, uint32 mask);
	void printInterfacesInformation () const;

	void setCustomInformation (const std::string &info);
	const std::string &getCustomInformation () const;

	int getCountOfInterfaces () const;

	bool /* Synchronized */ sendFrame (Frame frame, int ifaceIndex) const;
	virtual void processFrame (Frame frame, int ifaceIndex) = 0;
};

#endif /* machine.h */

