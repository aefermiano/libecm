////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022      Antonio Fermiano
// Copyright (C) 2015-2017 Maxime Gauduin
// Copyright (C) 2002-2011 Neill Corlett
//
// This file is part of libecm.
//
// libecm is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// libecm is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
////////////////////////////////////////////////////////////////////////////////

#include "common.h"

uint32_t get32lsb(const uint8_t* src) {
    return
        (((uint32_t)(src[0])) <<  0) |
        (((uint32_t)(src[1])) <<  8) |
        (((uint32_t)(src[2])) << 16) |
        (((uint32_t)(src[3])) << 24);
}

void put32lsb(uint8_t* dest, uint32_t value) {
    dest[0] = (uint8_t)(value      );
    dest[1] = (uint8_t)(value >>  8);
    dest[2] = (uint8_t)(value >> 16);
    dest[3] = (uint8_t)(value >> 24);
}
