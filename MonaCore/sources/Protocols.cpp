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

#include "Mona/Protocols.h"

#include "Mona/RTMP/RTMProtocol.h"
#include "Mona/RTMFP/RTMFProtocol.h"
#include "Mona/HTTP/HTTProtocol.h"
#include "Mona/RTSP/RTSProtocol.h"
#include "Mona/Logs.h"

namespace Mona {
	
void Protocols::load(Sessions& sessions) {
	//NOTE("RTMFProtocol");
	//loadProtocol<RTMFProtocol>("RTMFP", 1935, sessions);
	NOTE("RTMProtocol");
	loadProtocol<RTMProtocol>("RTMP", 1935, sessions);
	//NOTE("HTTProtocol");
	//loadProtocol<HTTProtocol>("HTTP", 80, sessions);
	//NOTE("RTSProtocol");
	//loadProtocol<RTSProtocol>("RTSP", 554, sessions);
}


} // namespace Mona
