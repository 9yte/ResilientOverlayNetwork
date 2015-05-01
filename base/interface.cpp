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

#include "interface.h"

#include "cf.h"

Interface::Interface () {
  memset (mac, 0, MAC_ADDRESS_LENGTH);
  ip = 0;
  mask = 0;
}

void Interface::init (int index, uint64 mac, uint32 ip, uint32 mask,
    const ClientFramework *cf) {
  memcpy (this->mac, &mac, MAC_ADDRESS_LENGTH);
  this->index = index;
  this->ip = ip;
  this->mask = mask;
  this->cf = cf;
}

void Interface::printInterfaceInformation () const {
  PRINT ("-- Type: Ethernet interface");
  char str[3 * 6];
  for (int j = 0; j < 6; ++j) {
    int b1 = (mac[j] >> 4) & 0x0F;
    int b2 = (mac[j]) & 0x0F;
    str[3 * j] = TO_HEX (b1);
    str[3 * j + 1] = TO_HEX (b2);
    str[3 * j + 2] = ':';
  }
  str[3 * 5 + 2] = '\0';
  PRINT ("-- MAC address: " << str);
  PRINT ("-- IP address: " << (ip >> 24) << "." << ((ip >> 16) & 0xFF) << "."
      << ((ip >> 8) & 0xFF) << "." << (ip & 0xFF));
  PRINT ("-- Network mask: " << (mask >> 24) << "." << ((mask >> 16) & 0xFF) << "."
      << ((mask >> 8) & 0xFF) << "." << (mask & 0xFF));
}

uint32 Interface::getIp () const {
  return ip;
}

bool Interface::setIp (uint32 ip) {
  if (this->ip != ip) {
    if (cf->notifyChangeOfIPAddress (ip, index)) {
      this->ip = ip;
      return true;
    }
  }
  return false;
}

uint32 Interface::getMask () const {
  return mask;
}

bool Interface::setMask (uint32 mask) {
  if (this->mask != mask) {
    if (cf->notifyChangeOfNetmask (mask, index)) {
      this->mask = mask;
      return true;
    }
  }
  return false;
}
