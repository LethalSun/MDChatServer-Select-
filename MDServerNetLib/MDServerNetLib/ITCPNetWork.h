#pragma once

#include "Define.h"
#include "ILog.h"
#include "ServerNetErrorCode.h"

namespace MDServerNetLib
{
	class ITCPNetWork
	{
	public:
		ITCPNetWork(){}

		virtual ~ITCPNetWork(){}

		virtual NET_ERROR_CODE Init(const ServerConfig* pConfig, ILog* pLogger) 
		{ 
			return NET_ERROR_CODE::NONE; 
		}

		virtual NET_ERROR_CODE SendData(const int pSessionIndex, const short pPacketId, const short pSize, const char* pMsg)
		{
			return NET_ERROR_CODE::NONE;
		}

		virtual void Run()
		{}

		virtual RecvPacketInfo GetPacketInfo()
		{
			return RecvPacketInfo();
		}

		virtual void Release()
		{}

		virtual int ClientSessionPoolSize()
		{
			return 0;
		}

		virtual void ForcedClose(const int pSessionIndex)
		{}
	};
}