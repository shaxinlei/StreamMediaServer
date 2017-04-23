/*
Copyright 2014 Mona
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License received along this program for more
details (or else see http://www.gnu.org/licenses/).

This file is a part of Mona.
*/

#pragma once

#include "Script.h"
#include "Mona/File.h"

class LUAFile {
public:
	static void Init(lua_State *pState, Mona::File& file) {}
	static void	Clear(lua_State* pState, Mona::File& file) {}
	static void	Delete(lua_State* pState, Mona::File& file) {delete &file;}
	static int	Get(lua_State *pState);
	static int	Set(lua_State *pState);
};
