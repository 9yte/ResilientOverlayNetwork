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

#ifndef INTERFACE_H_
#define INTERFACE_H_

#include "partovdef.h"

class ClientFramework;

class Interface {
public:
  static const int MAC_ADDRESS_LENGTH = 6;

  byte mac[MAC_ADDRESS_LENGTH];

private:
  int index;
  uint32 ip; // in host byte-order
  uint32 mask; // in host byte-order

protected:
  const ClientFramework *cf;

public:
  Interface ();

  void
  init (int index, uint64 mac, uint32 ip, uint32 mask,
      const ClientFramework *cf);

  void printInterfaceInformation () const;

  uint32 getIp () const;
  // return true if and only if the IP changed successfully.
  bool setIp (uint32 ip);

  uint32 getMask () const;
  // return true if and only if the network mask changed successfully.
  bool setMask (uint32 mask);
};

#endif /* interface.h */

