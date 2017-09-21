#pragma once

#define FD_SETSIZE 5096 // http://blog.naver.com/znfgkro1/220175848048 사이즈 재정의
//단 #include <winsock2.h>를 하기 전에 해 줘야 한다.

#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <ws2tcpip.h>

#include <vector>
#include <deque>
#include <unordered_map>
//#include "ServerNetErrorCode.h"
//#include "Define.h"
#include "ITCPNetWork.h"

namespace MDServerNetLib
{
	class TCPNetWork :public ITCPNetWork
	{
	public:
		TCPNetWork() = default;
		virtual ~TCPNetWork();

		NET_ERROR_CODE Init(const ServerConfig* pConfig, ILog* pLogger) override;

		NET_ERROR_CODE SendData(const int pSessionIndex, const short pPacketId, 
			const short pSize, const char* pMsg)override;

		void Run()override;
		
		RecvPacketInfo GetPacketInfo()override;

		void Release()override;

		int ClientSessionPoolSize()override
		{
			return static_cast<int>(clientSessionPool.size());
		}

		void ForcedClose(const int pSessionIndex);
	protected:

		//소켓 초기화
		NET_ERROR_CODE InitServerSocket();

		NET_ERROR_CODE BindAndListen(short pPort, int backlogCount);

		int AllocClientSessionIndex();
		void ReleaseSessionIndex(const int pIndex);
		
		int CreateSessionPool(const int pMaxClientCount);
		NET_ERROR_CODE NewSession();
		void SetSocketOption(const SOCKET pFD);
		void ConnectedSession(const int pSessionIndex, const SOCKET pFD,
			const char* pIP);//mntcn

		void CloseSession(const SOCKET_CLOSE_CASE pCloseCase, 
			const SOCKET pSocketFD, const int pSessionIndex);

		NET_ERROR_CODE RecvSocket(const int pSessionIndex);
		NET_ERROR_CODE RecvBufferProcess(const int pSessionIndex);
		void AddPacketQue(const int pSessionIndex, const short pPktId, 
			const short pBodySize, char* pDataPos);

		void RunProcessWrite(const int pSessionIndex, 
			const SOCKET pFD, fd_set& pWrite_set);
		NetError FlushSendBuff(const int pSessionIndex);
		NetError SendSocket(const SOCKET pFD, const char* pMsg, 
			const int pSize);

		bool RunCheckSelectResult(const int pResult);
		void RunCheckSelectClients(fd_set& pRead_set, fd_set& pWrite_set);
		bool RunProcessReceive(const int sessionIndex, const SOCKET pFD, 
			fd_set& pRead_set);

	protected:
		ServerConfig config;
		
		SOCKET serverSocketFd;

		fd_set ReadFds;
		size_t connectedSessionCount = 0;

		int64_t connectSeq = 0;

		std::vector<ClientSession> clientSessionPool;//클라이언트의풀
		std::deque<int> clientSessionPoolIndex;//비어있는 풀의 인덱스 끊어지면 덱에 넣고 필요할때는 인덱스를 팝해서 사용한다.

		std::deque<RecvPacketInfo> packetQue;

		ILog* refLogger;
	};


}
