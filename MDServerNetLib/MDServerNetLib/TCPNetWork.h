#pragma once

#define FD_SETSIZE 5096 // http://blog.naver.com/znfgkro1/220175848048 ������ ������
//�� #include <winsock2.h>�� �ϱ� ���� �� ��� �Ѵ�.

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

		//���� �ʱ�ȭ
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

		std::vector<ClientSession> clientSessionPool;//Ŭ���̾�Ʈ��Ǯ
		std::deque<int> clientSessionPoolIndex;//����ִ� Ǯ�� �ε��� �������� ���� �ְ� �ʿ��Ҷ��� �ε����� ���ؼ� ����Ѵ�.

		std::deque<RecvPacketInfo> packetQue;

		ILog* refLogger;
	};


}
