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

#include "Mona/FlashStream.h"
#include "Mona/Logs.h"
#include "Mona/MediaCodec.h"
#define VIDEO_BUFFER_SIZE     32768
#define NEED_TRANSCODE		1

using namespace std;

namespace Mona {

	FlashStream::FlashStream(UInt16 id, Invoker& invoker, Peer& peer) : id(id), invoker(invoker), peer(peer), _pPublication(NULL), _pListener(NULL), _bufferTime(0), running(0), queueSize(0),flag(1){
	video_buffer_first = new Buffer();
	DEBUG("FlashStream ",id," created")
}

FlashStream::~FlashStream() {
	
	disengage();
	DEBUG("FlashStream ",id," deleted")
}

void FlashStream::disengage(FlashWriter* pWriter) {
	// Stop the current  job
	if(_pPublication) {
		const string& name(_pPublication->name());
		if(pWriter)
			pWriter->writeAMFStatus("NetStream.Unpublish.Success",name + " is now unpublished");
		 // do after writeAMFStatus because can delete the publication, so corrupt name reference
		invoker.unpublish(peer,name);
		_pPublication = NULL;
	}
	if(_pListener) {
		const string& name(_pListener->publication.name());
		if (pWriter) {
			pWriter->writeAMFStatus("NetStream.Play.Stop", "Stopped playing " + name);
			OnStop::raise(id, *pWriter); // stream end
		}
		 // do after writeAMFStatus because can delete the publication, so corrupt publication name reference
		invoker.unsubscribe(peer,name);
		_pListener = NULL;
	}
}


bool FlashStream::process(AMF::ContentType type,UInt32 time,PacketReader& packet,FlashWriter& writer,double lostRate) {

	// if exception, it closes the connection, and print an ERROR message
	switch(type) {

		case AMF::AUDIO:
			{	
				audioHandler(time,packet, lostRate);    //音频处理
				break;
			}
		case AMF::VIDEO:
			{
				videoHandler(time, packet, lostRate);   //视频处理
				break;
			}
		case AMF::DATA_AMF3:
			packet.next();
		case AMF::DATA: {     //AMF::DATA
			AMFReader reader(packet);
			dataHandler(reader, lostRate);
			break;
		}

		case AMF::EMPTY:
			break;

		case AMF::INVOCATION_AMF3:
			packet.next();
		case AMF::INVOCATION: {    //AMF::INVOCATION   connect命令对应的type为INVOCATION
			DEBUG("process() do AMF Data type analysis then call messageHandler");
			string name;
			AMFReader reader(packet);
	
			reader.readString(name);   
			double number(0);
			reader.readNumber(number);
			writer.setCallbackHandle(number);
			reader.readNull();
			messageHandler(name,reader,writer);   
			break;
		}

		case AMF::RAW:
			rawHandler(packet.read16(), packet, writer);
			break;

		default:
			ERROR("Unpacking type '",Format<UInt8>("%02x",(UInt8)type),"' unknown");
	}

	writer.setCallbackHandle(0);
	return writer.state()!=Writer::CLOSED;
}


UInt32 FlashStream::bufferTime(UInt32 ms) {
	_bufferTime = ms;
	INFO("setBufferTime ", ms, "ms on stream ",id)
	if (_pListener)
		_pListener->setNumber("bufferTime", ms);
	return _bufferTime;
}


	//连接流上的命令
void FlashStream::messageHandler(const string& name, AMFReader& message, FlashWriter& writer) {
	DEBUG("Enter FlashStream::messageHandler");
	//INFO("%%%%%", name);
	if (name == "play") {
		INFO("FlashString handle RTMP Commabd message play");
		disengage(&writer);

		string publication;
		message.readString(publication);
		// TODO implements completly NetStream.play method, with possible NetStream.play.failed too!
		Exception ex;
		_pListener = invoker.subscribe(ex, peer, publication, writer); // ex already log displayed
		if (!_pListener) {
			writer.writeAMFStatus("NetStream.Play.Failed", ex.error());
			return;
		}
		
		OnStart::raise(id, writer); // stream begin
		writer.writeAMFStatus("NetStream.Play.Reset", "Playing and resetting " + publication); // for entiere playlist
		writer.writeAMFStatus("NetStream.Play.Start", "Started playing "+publication); // for item
		AMFWriter& amf(writer.writeAMFData("|RtmpSampleAccess"));
		amf.writeBoolean(true); // audioSampleAccess
		amf.writeBoolean(true); // videoSampleAccess

		if (_bufferTime > 0)
			_pListener->setNumber("bufferTime", _bufferTime);
		return;
	}
	
	if (name == "closeStream") {
		INFO("FlashString handle RTMP Commabd message closeStream");
		disengage(&writer);
		return;
	}
	
	//publishing content
	if (name == "publish") {
		INFO("FlashString handle RTMP Commabd message publish");
		disengage(&writer);

		string type, publication;
		message.readString(publication);
		size_t query = publication.find('?');
		if (query != string::npos)
			publication = publication.substr(0, query); // TODO use query in Util::UnpackQuery for publication options?
		if (message.available())
			message.readString(type); // TODO support "append" and "appendWithGap"

		Exception ex;
		DEBUG("Create and start Publishion");
		_pPublication = invoker.publish(ex, peer, publication, type == "record" ? Publication::RECORD : Publication::LIVE);
		if (ex) {
			writer.writeAMFStatus("NetStream.Publish.BadName", ex.error());
			_pPublication = NULL;
		} else
			writer.writeAMFStatus("NetStream.Publish.Start", publication + " is now published");
		return;
	}
	
	if (_pListener) {

		if(name == "receiveAudio") {
			INFO("FlashString handle RTMP Commabd message receiveAudio");
			message.readBoolean(_pListener->receiveAudio);
			return;
		}
		
		if (name == "receiveVideo") {
			INFO("FlashString handle RTMP Commabd message receiveVideo");
			message.readBoolean(_pListener->receiveVideo);
			return;
		}
		
		//客户端告知服务器端暂停或恢复播放
		if (name == "pause") {
			INFO("FlashString handle RTMP Commabd message pause");
			bool paused(true);
			message.readBoolean(paused);
			// TODO support pause for VOD
		
			if (paused) {
				// useless, client knows it when it calls NetStream::pause method
				// writer.writeAMFStatus("NetStream.Pause.Notify", _pListener->publication.name() + " paused");
			} else {
				double position;
				if (message.readNumber(position))
					_pListener->seek((UInt32)position);
				OnStart::raise(id, writer); // stream begin
				// useless, client knows it when it calls NetStream::resume method
				//	writer.writeAMFStatus("NetStream.Unpause.Notify", _pListener->publication.name() + " resumed");
			}
			return;
		}
		 
		//定位流位置
		if (name == "seek") {   
			INFO("FlashString handle RTMP Commabd message seek");
			double position;
			if (message.readNumber(position)) {
				_pListener->seek((UInt32)position);
				 // TODO support seek for VOD
				OnStart::raise(id, writer); // stream begin
				// useless, client knows it when it calls NetStream::seek method, and wait "NetStream.Seek.Complete" rather (raised by client side)
				// writer.writeAMFStatus("NetStream.Seek.Notify", _pListener->publication.name() + " seek operation");
			} else
				writer.writeAMFStatus("NetStream.Seek.InvalidTime", _pListener->publication.name() + " seek operation must pass in argument a milliseconds position time");
			return;
		}
	}

	ERROR("Message '",name,"' unknown on stream ",id);
}

void FlashStream::dataHandler(DataReader& data, double lostRate) {
	if(!_pPublication) {
		ERROR("a data packet has been received on a no publishing stream ",id,", certainly a publication currently closing");
		return;
	}

	// necessary AMF0 here!
	if (data.packet.available()>3 && *data.packet.current() == AMF_STRING && *(data.packet.current() + 3) == '@' && *(data.packet.current() + 1) == 0) {

		if (data.packet.available()>15 && *(data.packet.current() + 2) == 13 && memcmp(data.packet.current() + 3, EXPAND("@setDataFrame"))==0) {
			// @setDataFrame
			data.next();
			_pPublication->writeProperties(data);
			return;
		}

		if (data.packet.available()>17 && *(data.packet.current() + 2) == 15 && memcmp(data.packet.current() + 3, EXPAND("@clearDataFrame"))==0) {
			// @clearDataFrame
			_pPublication->clearProperties();
			return;
		}

	}

	_pPublication->pushData(data,peer.ping(),lostRate);
}

void FlashStream::rawHandler(UInt16 type, PacketReader& packet, FlashWriter& writer) {
	if(type==0x0022) { // TODO Here we receive RTMFP flow sync signal, useless to support it!
		//TRACE("Sync ",id," : ",data.read32(),"/",data.read32());
		return;
	}
	ERROR("Raw message ",Format<UInt16>("%.4x",type)," unknown on stream ",id);
}

void FlashStream::audioHandler(UInt32 time,PacketReader& packet, double lostRate) {
	if(!_pPublication) {
		WARN("an audio packet has been received on a no publishing stream ",id,", certainly a publication currently closing");
		return;
	}
	_pPublication->pushAudio(time,packet,peer.ping(),lostRate);
	//NOTE("receiveAudio");
}

void FlashStream::videoHandler(UInt32 time,PacketReader& packet, double lostRate) {
	if(!_pPublication) {
		WARN("a video packet has been received on a no publishing stream ",id,", certainly a publication currently closing");
		return;
	}
	if (NEED_TRANSCODE)
	{
		Buffer *video_buffer = new Buffer();

		char flvHeader[]= { 'F', 'L', 'V', 0x01,
			0x01,             //0x04代表有音频, 0x01代表有视频 
			0x00, 0x00, 0x00, 0x09,
			0x00, 0x00, 0x00, 0x00
		};
		char tagHeader[] = { 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, };

		char tagEnd[] = { 0x00, 0x00, 0x00, 0x00};

		Transcode::build_flv_message(tagHeader, tagEnd, packet.size(),time);			  //组flv头、videoTag 

		if (MediaCodec::H264::IsCodecInfos(packet)&&flag==1) {                                     //if AVCPacketType then add byte flv header and 4byte previoustime
			video_buffer_first->append(flvHeader, 13);
			video_buffer_first->append(tagHeader, 11);												  // add 11byte videoTag header
			video_buffer_first->append(packet.current(), packet.size());							  //add video data
			video_buffer_first->append(tagEnd, 4);
			flag = 2;
		}
		else
		{
			if (flag==2)
			{
				video_buffer_first->append(tagHeader, 11);												  // add 11byte videoTag header
				video_buffer_first->append(packet.current(), packet.size());							      //add video data
				video_buffer_first->append(tagEnd, 4);
				BinaryReader videoPacket(video_buffer_first->data(), video_buffer_first->size());
				DEBUG("size of videopacket", videoPacket.size())
				transcodeThread.receiveVideoPacket(videoPacket);
				if (!running)
				{
					Exception exWarn;
					running = transcodeThread.start(exWarn);
					DEBUG("start transcode thread")
					transcodeThread.setPublication(_pPublication);

				}
				flag=3;
			}
			else
			{
				video_buffer->append(tagHeader, 11);												  // add 11byte videoTag header
				video_buffer->append(packet.current(), packet.size());							      //add video data
				video_buffer->append(tagEnd, 4);													  //add 4byte previoustime
				BinaryReader videoPacket(video_buffer->data(), video_buffer->size());				  //构建videoPacket

				DEBUG("size of videopacket", videoPacket.size())
				transcodeThread.receiveVideoPacket(videoPacket);
				DEBUG("send videopacket to queue")
			}
		}
	}     
	else
	{
		//INFO("video packet's size",packet.size())
		INFO("receve packet size:",packet.size())
		_pPublication->pushVideo(time, packet, peer.ping(), lostRate);

	}
	
}

/*void FlashStream::videoHandler(UInt32 time, PacketReader& packet, double lostRate)
	{
		if (!_pPublication) {
			WARN("a video packet has been received on a no publishing stream ", id, ", certainly a publication currently closing");
			return;
		}
		char flvHeader[] = { 'F', 'L', 'V', 0x01,
			0x01,             // 0x04代表有音频, 0x01代表有视频 
			0x00, 0x00, 0x00, 0x09,
			0x00, 0x00, 0x00, 0x00
		};
		char tagHeader[] = { 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, };

		char tagEnd[] = { 0x00, 0x00, 0x00, 0x00 };

		Transcode::build_flv_message(tagHeader, tagEnd, packet.size(), time);			  //组flv头、videoTag 

		if (MediaCodec::H264::IsCodecInfos(packet)) {                                     //if AVCPacketType then add byte flv header and 4byte previoustime
			video_buffer.append(flvHeader, 13);
		}
		video_buffer.append(tagHeader, 11);												  // add 11byte videoTag header
		video_buffer.append(packet.current(), packet.size());							      //add video data
		video_buffer.append(tagEnd, 4);
		

			PacketReader videoPacket(video_buffer.data(), video_buffer.size());          //构建videoPacket
			INFO("The size of VideoPacket:", videoPacket.size());

			Buffer *transcodedVideoBuffer = transcodeThread.decode(videoPacket);
			//PacketReader transcodedVideo(transcodedVideoBuffer->data(),transcodedVideoBuffer->size());

			//_pPublication->pushVideo(time, transcodedVideo, peer.ping(), lostRate);

			video_buffer.clear();               //清理视频缓存
	
	}*/

} // namespace Mona
