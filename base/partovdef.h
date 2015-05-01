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

#ifndef PARTOV_DEF_H_
#define PARTOV_DEF_H_

#include <iostream>

#include <string.h>
#include <stdint.h>

#define PRINT(X) std::cout << X << std::endl

#define TO_HEX(DEC_NUM) (((DEC_NUM) < 10) ? ((DEC_NUM) + '0') : ((DEC_NUM) - 10 + 'A'))

#define forever for (;;)

typedef uint8_t byte;

typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int32_t int32;

#endif /* partovdef.h */

