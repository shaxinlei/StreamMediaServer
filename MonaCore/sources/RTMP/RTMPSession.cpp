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

#include "Mona/RTMP/RTMPSession.h"
#include "Mona/Util.h"
#include "Mona/RTMP/RTMPSender.h"
#include "math.h"

using namespace std;


namespace Mona {


RTMPSession::RTMPSession(const SocketAddress& peerAddress, SocketFile& file, Protocol& protocol, Invoker& invoker) : _mainStream(invoker,peer),_unackBytes(0),_readBytes(0),_decrypted(0), _chunkSize(RTMP::DEFAULT_CHUNKSIZE), _winAckSize(RTMP::DEFAULT_WIN_ACKSIZE),_handshaking(0), _pWriter(NULL), TCPSession(peerAddress,file, protocol, invoker),
		onStreamStart([this](UInt16 id, FlashWriter& writer) {
			// Stream Begin signal
			(_pController ? (FlashWriter&)*_pController : writer).writeRaw().write16(0).write32(id);
		}),
		onStreamStop([this](UInt16 id, FlashWriter& writer) {
			// Stream EOF signal
			(_pController ? (FlashWriter&)*_pController : writer).writeRaw().write16(1).write32(id);
		}) {
	
	_mainStream.OnStart::subscribe(onStreamStart);
	_mainStream.OnStop::subscribe(onStreamStop);

	dumpJustInDebug = true;
}

void RTMPSession::kill(UInt32 type) {
	if (died)
		return;
	// TODO if(shutdown)
	// writeAMFError("Connect.AppShutdown","server is stopping");
	//_writer.close(...);

	_mainStream.disengage(); // disengage FlashStreams because the writers "engaged" will be deleted

	_writers.clear();
	_pWriter = NULL;
	_pController.reset();

	Session::kill(type); // at the end to "unpublish or unsubscribe" before "onDisconnection"!

	_mainStream.OnStart::unsubscribe(onStreamStart);
	_mainStream.OnStop::unsubscribe(onStreamStop);
}

void RTMPSession::readKeys() {
	if (!_pHandshaker || _pHandshaker->failed)
		return;
	_pEncryptKey = _pHandshaker->pEncryptKey;
	_pDecryptKey = _pHandshaker->pDecryptKey;
	_pHandshaker.reset();
}


UInt32 RTMPSession::onData(PoolBuffer& pBuffer) {
	//DEBUG("Enter RTMPSsession::onData********hand1:",_handshaking);
	// first hanshake
	if (!_handshaking) {         //第一次RTMP握手（handshaking==0）
		UInt32 size(pBuffer->size());
		if (size < 1537)
			return 0;
		if (size > 1537) {
			ERROR("RTMP Handshake unknown");
			kill(PROTOCOL_DEATH);
			return size;
		}

		UInt8 handshakeType(*pBuffer->data());   //handshakeType为handshake状态（握手进行到哪一步）
		if(handshakeType!=3 && handshakeType!=6) {
			ERROR("RTMP Handshake type '",handshakeType,"' unknown");
			kill(PROTOCOL_DEATH);
			return size;
		}

		if(!TCPSession::receive(pBuffer))
			return size;

		_unackBytes += 1537;
		++_handshaking;    //handshaking =1

		Exception ex;
		_pHandshaker.reset(new RTMPHandshaker(handshakeType==6, peer.address, pBuffer));


		if (_pHandshaker->encrypted)
			((string&)peer.protocol) = "RTMPE";

		send<RTMPHandshaker>(ex, _pHandshaker, NULL); // threaded!    
		if (ex) {
			ERROR("RTMP Handshake, ", ex.error())
			kill(PROTOCOL_DEATH);
		}
		return size;
	}

	const UInt8* data(pBuffer->data());
	const UInt8* end(pBuffer->data()+pBuffer->size());
	while(end-data) {
		BinaryReader packet(data, end-data);
		///INFO("packet.current()", packet.current())
		///INFO("packet.available()", packet.available())
		///DEBUG("data:", data)
		///INFO("===");
		if (!buildPacket(packet))
			break;
	    //INFO("packet.current()", packet.current())
		//INFO("packet.available()", packet.available())
		data = packet.current()+packet.available(); // next data
		//INFO("packet.current()", packet.current())
		//INFO("packet.available()", packet.available())
	//	DEBUG("data:",data)
		receive(packet);
		//INFO("***");
		//INFO("packet.current()", packet.current())
		//INFO("packet.available()", packet.available())
		//DEBUG("data:", data)

	}
	//DEBUG(*pBuffer->data());
	//DEBUG("flush");
	if (data!=pBuffer->data())
		flush();//发包
	//DEBUG(data - pBuffer->data());
	return data-pBuffer->data(); // consumed
}

bool RTMPSession::buildPacket(BinaryReader& packet) {
	//INFO("In to buildpacket packet.available()", packet.available())
	//DEBUG("Enter RTMPSession::buildPacket");
	if (pDecryptKey() && packet.available()>_decrypted) {      //是否进行加密处理
		RC4(pDecryptKey().get(),packet.available()-_decrypted,packet.current()+_decrypted,(UInt8*)packet.current()+_decrypted);   //RC4加密算法
		_decrypted = packet.available();
	}
	//INFO("packet.available1()", packet.available())
	
	if(_handshaking==1) {                 //第一次握手
		if (packet.available() < 1536)
			return false;
		if (_decrypted>=1536)
			_decrypted -= 1536;
		packet.shrink(1536);
		return true;
	}

	if (!_pController)
		_pController.reset(new RTMPWriter(2, *this,_pSender,pEncryptKey()));

	dumpJustInDebug = false;

	//Logs::Dump(packet.current(), 16);
	
	UInt8 headerSize = packet.read8();     //向后读1字节，取出Basic header
	//读取客户端发来的connect请求的包时，fmt=0(第一类块消息头，含有11字节)，chunk stream ID=3
	UInt32 idWriter = headerSize & 0x3F;
	
	headerSize = 12 - (headerSize>>6)*4;     //计算包头的长度
	
	if(headerSize==0)
		headerSize=1;

	// if idWriter==0 id is encoded on the following byte, if idWriter==1 id is encoded on the both following byte
	if (idWriter < 2)
		headerSize += idWriter+1;
	//INFO("packet.available3()", packet.available())
	if (packet.available() < headerSize) // want read in first the header!
		return false;

	if (idWriter < 2)
		idWriter = (idWriter==0 ? packet.read8() : packet.read16()) + 64;
		
	RTMPWriter* pWriter;
	if (idWriter != 2) {
		MAP_FIND_OR_EMPLACE(_writers, it, idWriter, idWriter,*this,_pSender, pEncryptKey());
		pWriter = &it->second;
	} else
		pWriter = _pController.get();


	RTMPChannel& channel(pWriter->channel);
	bool isRelative(true);
	if(headerSize>=4) {     //读packet中的Message Header
		
		// TIME
		channel.time = packet.read24();    //向后读3字节，取出

		if(headerSize>=8) {
			// SIZE
			channel.bodySize = packet.read24();      //向后读3字节     RTMP chunk中body大小
			//NOTE("channel.bodySize", channel.bodySize)
			// TYPE
			channel.type = (AMF::ContentType)packet.read8();       //读取message type id
			if(headerSize>=12) {
				isRelative = false;
				// STREAM
				UInt32 streamId(packet.read8());
				streamId += packet.read8() << 8;
				streamId += packet.read8() << 16;
				streamId += packet.read8() << 24;
				if (!_mainStream.getStream(streamId, channel.pStream) && streamId) {
					ERROR("RTMPSession ",name()," indicates a non-existent ",streamId," FlashStream");
					kill(PROTOCOL_DEATH);
					return false;
				}

			}
		}
	}

	// extended timestamp (can be present for headerSize=1!)   //扩展时间戳
	//只有当块消息头中的普通时间戳设置为0x00ffffff时，本字段才被传送。如果普通时间戳的值小于0x00ffffff，那么本字段一定不能出现
	bool wasExtendedTime(false);
	if (channel.time >= 0xFFFFFF) {     
		headerSize += 4;
		if (packet.available() < 4)
			return false;
		channel.time = packet.read32();
		wasExtendedTime = true;
	}

  //  TRACE("Writer ",pWriter->id," absolute time ",channel.absoluteTime)
	//INFO("channel.bodySize", channel.bodySize)
	UInt32 total(channel.bodySize);
	if (!channel.pBuffer.empty()) {          //如果pBuffer非空,第一次进入时，不符合条件
		if (channel.pBuffer->size() > total) {      
			ERROR("RTMPSession ",name()," with a chunked message which doesn't match the bodySize indicated");
			kill(PROTOCOL_DEATH);
			return false;
		}
		total -= channel.pBuffer->size();
	}

	if(_chunkSize && total>_chunkSize)  //rtmp.h中 DEFAULT_CHUNKSIZE =	128
		total = _chunkSize;

	if (packet.available() < total)
		return false;

	//// resolve absolute time on new packet!
	if (channel.pBuffer.empty()) {
		if (isRelative)
			channel.absoluteTime += channel.time; // relative
		else
			channel.absoluteTime = channel.time; // absolute
	}
	if (wasExtendedTime) // reset channel.time
		channel.time = 0xFFFFFF;

	//// data consumed now!
	packet.shrink(total);
	total += headerSize;
//	INFO("total:", total)
	//INFO("_decrypted", _decrypted)
	if (_decrypted>=total)
		_decrypted -= total;

	_pWriter = pWriter;
	return true;
}


void RTMPSession::receive(BinaryReader& packet) {
	//DEBUG("Enter RTMPSession::receive");
	if (!TCPSession::receive(packet))
		return;

	_unackBytes += packet.position() + packet.available();
	//DEBUG("RTMPSession::receive*******handshaking2:", _handshaking);
	if (_handshaking < 2) {
		++_handshaking;
		return;
	}
	if (_handshaking == 2) {
		++_handshaking;
		// client settings
		_pController->writeProtocolSettings();     //std::unique_ptr<RTMPWriter>  _pController
	}

	// ack if required
	if (_unackBytes >= _winAckSize) {
		_readBytes += _unackBytes;
		DEBUG("Sending ACK : ", _readBytes, " bytes (_unackBytes: ", _unackBytes, ")")
		_pController->writeAck(_readBytes);
		_unackBytes = 0;
	}

	if(!_pWriter) {
		ERROR("Packet received on session ",name()," without channel indication");
        return;
    }

	// Process the packet
	RTMPChannel& channel(_pWriter->channel);
	
	// unchunk (build)
 	if (!channel.pBuffer.empty()) {    //第一次进入 empty()返回true
		channel.pBuffer->append(packet.current(), packet.available());
		//INFO("memcpy buffer2:", channel.pBuffer->size())
	//	INFO("channel.pBuffer->size()", channel.pBuffer->size())
		if (channel.bodySize > channel.pBuffer->size())
			return; // wait the next piece
	} else if (channel.bodySize > packet.available()) {
		channel.pBuffer->resize(packet.available(), false);
		
		memcpy(channel.pBuffer->data(),packet.current(),packet.available());   //将packet中的数据拷贝到buffer中
		//INFO("memcpy buffer1:", channel.pBuffer->size())
		return; // wait the next piece
	}

	PacketReader reader(channel.pBuffer.empty() ? packet.current() : channel.pBuffer->data(),channel.bodySize);
	
	switch(channel.type) {
		case AMF::ABORT:
			channel.reset(_pWriter);
			break;
		case AMF::CHUNKSIZE:
			_chunkSize = reader.read32();
			break;
		case AMF::BANDWITH:
			// send a win_acksize message for accept this change
			_pController->writeWinAckSize(reader.read32());
			break;
		case AMF::WIN_ACKSIZE:
			_winAckSize = reader.read32();
			break;
		case AMF::ACK:
			// nothing to do, a ack message says about how many bytes have been gotten by the peer
			// RTMFP has a complexe ack mechanism and RTMP is TCP based, ack mechanism is in system layer => so useless
			break;
		default: {
			if (!channel.pStream) {
				//NOTE("receive() call function process ***");
				if(_mainStream.process(channel.type,channel.absoluteTime, reader,*_pWriter) && peer.connected)
					_pWriter->isMain = true;
				else if (!died)
					kill(REJECTED_DEATH);
			} else{
				//NOTE("receive() call function process receive Audio AND Video" );
				channel.pStream->process(channel.type, channel.absoluteTime, reader, *_pWriter);
			}
				
		}
	}

	if (_pWriter) {
		channel.pBuffer.release();
		_pWriter = NULL;
	}
}

void RTMPSession::manage() {
	if (_pHandshaker && _pHandshaker->failed)
		kill(PROTOCOL_DEATH);
	else if (peer.connected && _pController && peer.ping(30000)) // 30 sec
		_pController->writePing();
	TCPSession::manage();
}


} // namespace Mona
