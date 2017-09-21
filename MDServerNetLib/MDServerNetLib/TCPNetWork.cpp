#include "TCPNetWork.h"

namespace MDServerNetLib
{
	TCPNetWork::~TCPNetWork()
	{
		for (auto& client : clientSessionPool)
		{
			//구조체 내부의 버퍼를 정리 나머지는 각자의 소멸자에 의해서 정리됨.
			if (client.pRecvBuffer)
			{
				delete[] client.pRecvBuffer;
			}

			if (client.pSendBuffer)
			{
				delete[] client.pSendBuffer;
			}
		}
	}

	NET_ERROR_CODE TCPNetWork::Init(const ServerConfig * pConfig, ILog * pLogger)
	{
		//서버 설정 복사
		memcpy(&config, pConfig, sizeof(ServerConfig));

		refLogger = pLogger;

		auto errorCode = InitServerSocket();
		if (errorCode != NET_ERROR_CODE::NONE)
		{
			return static_cast<NET_ERROR_CODE>(errorCode);
		}

		errorCode = BindAndListen(config.Port, config.BackLogCount);
		if (errorCode != NET_ERROR_CODE::NONE)
		{
			return static_cast<NET_ERROR_CODE>(errorCode);
		}
		FD_ZERO(&ReadFds);//init all fd set's bits to 0
		FD_SET(serverSocketFd, &ReadFds);//소켓을 소켓 셋에 넣는다.

		auto sessionPoolSize = CreateSessionPool(config.MaxClientCount + config.ExtraClientCount);
		refLogger->Write(LOG_TYPE::L_INFO, "%s | Session Pool Size: %d", __FUNCTION__, sessionPoolSize);
		return NET_ERROR_CODE::NONE;
	}

	NET_ERROR_CODE TCPNetWork::SendData(const int pSessionIndex, const short pPacketId, const short pSize, const char * pMsg)
	{
		//패킷을 만들어서 세션구조채의 센드 버퍼에 써놓는다.
		auto& session = clientSessionPool[pSessionIndex];

		auto pos = session.SendSize;

		if ((pos + pSize + PACKET_HEADER_SIZE) > config.MaxClientSendBufferSize) {
			return NET_ERROR_CODE::CLIENT_SEND_BUFFER_FULL;
		}

		PacketHeader pktHeader{ pPacketId, pSize };
		memcpy(&session.pSendBuffer[pos], (char*)&pktHeader, PACKET_HEADER_SIZE);
		memcpy(&session.pSendBuffer[pos + PACKET_HEADER_SIZE], pMsg, pSize);
		session.SendSize += (pSize + PACKET_HEADER_SIZE);

		return NET_ERROR_CODE::NONE;
	}

	void TCPNetWork::Release()
	{
		WSACleanup();
	}

	void TCPNetWork::Run()
	{
		//두개가 같아도 될까? 같아도 상관 없다 어차피 커널 버퍼에 쓰는 것이므로
		auto read_set = ReadFds;
		auto write_set = ReadFds;
		//보통은 라이트 이벤트는 체크를 하지 않다가 나중에 쓰기쪽에서 써야할 크기와 실제로 써진 크기가 다를 때 즉 다써지지
		//않았을 때만 그 세션을 기록해 뒀다가 라이트 이벤트를 확인하고 쓸쑤 있으면 쓴다. 왜냐면 보통 모든 소켓이 언제나 쓸수 있는
		//상태이기때문이다.
		timeval timeout{ 0, 1000 }; //차례대로 초, 마이크로 초
		auto selectResult = select(0, &read_set, &write_set, 0, &timeout);//읽거나 쓰기 가능한소켓이 있으면 그 개수만큼 반환
		//여기서 비지 웨이트를 막기 때문에 스레드를 양보하는 처리를 할 필요가 없지만 
		//스레드를 별도로 뺀다면 슬립0나 
		//웨이트포 멀티 오브젝트를 사용해서 패킷이 왔을때 
		//이벤트를 만들면 처리를 하도록 하면 된다.

		auto isFDSetChanged = RunCheckSelectResult(selectResult);
		if (isFDSetChanged == false)//selectResult = -1, i.e. SOCKETERROR
		{
			return;
		}

		// Accept
		if (FD_ISSET(serverSocketFd, &read_set))
		{
			NewSession();
		}

		RunCheckSelectClients(read_set, write_set);
	}

	RecvPacketInfo TCPNetWork::GetPacketInfo()
	{
		RecvPacketInfo packetInfo;

		if (packetQue.empty() == false)
		{
			packetInfo = packetQue.front();
			packetQue.pop_front();
		}

		return packetInfo;
	}

	void TCPNetWork::ForcedClose(const int pSessionIndex)
	{
		//어떤 이유로 종료 패킷을 보낸 클라이언트가 일정 시간 동안 끊지 않으면 강제로 끊는다.
		//SO_LINGER 옵션!
		if (clientSessionPool[pSessionIndex].IsConnected() == false)
		{
			return;
		}

		CloseSession(SOCKET_CLOSE_CASE::FORCING_CLOSE, clientSessionPool[pSessionIndex].SocketFD, pSessionIndex);
	}

	NET_ERROR_CODE TCPNetWork::InitServerSocket()
	{
		WORD wVersionRequested = MAKEWORD(2, 2);
		WSADATA wsaData;
		WSAStartup(wVersionRequested, &wsaData);

		serverSocketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (serverSocketFd < 0)
		{
			return NET_ERROR_CODE::SERVER_SOCKET_CREATE_FAIL;
		}

		auto n = 1;
		if (setsockopt(serverSocketFd, SOL_SOCKET, SO_REUSEADDR, (char*)&n, sizeof(n)) < 0)
		{
			return NET_ERROR_CODE::SERVER_SOCKET_SO_REUSEADDR_FAIL;
		}

		return NET_ERROR_CODE::NONE;
	}

	NET_ERROR_CODE TCPNetWork::BindAndListen(short pPort, int backlogCount)
	{
		SOCKADDR_IN server_addr;
		ZeroMemory(&server_addr, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		server_addr.sin_port = htons(pPort);

		if (bind(serverSocketFd, (SOCKADDR*)&server_addr, sizeof(server_addr)) < 0)
		{
			return NET_ERROR_CODE::SERVER_SOCKET_BIND_FAIL;
		}

		unsigned long mode = 1;
		if (ioctlsocket(serverSocketFd, FIONBIO, &mode) == SOCKET_ERROR)
		{
			return NET_ERROR_CODE::SERVER_SOCKET_FIONBIO_FAIL;
		}

		if (listen(serverSocketFd, backlogCount) == SOCKET_ERROR)
		{
			return NET_ERROR_CODE::SERVER_SOCKET_LISTEN_FAIL;
		}

		refLogger->Write(LOG_TYPE::L_INFO, "%s | Listen. ServerSockfd(%I64u)", __FUNCTION__, serverSocketFd);
		return NET_ERROR_CODE::NONE;
	}

	int TCPNetWork::AllocClientSessionIndex()
	{
		if (clientSessionPoolIndex.empty()) {
			return -1;
		}

		int index = clientSessionPoolIndex.front();
		clientSessionPoolIndex.pop_front();
		return index;
	}

	void TCPNetWork::ReleaseSessionIndex(const int pIndex)
	{
		clientSessionPoolIndex.push_back(pIndex);
		clientSessionPool[pIndex].Clear();
	}

	int TCPNetWork::CreateSessionPool(const int pMaxClientCount)
	{
		for (int i = 0; i < pMaxClientCount; ++i)
		{
			ClientSession session;
			ZeroMemory(&session, sizeof(session));
			session.Index = i;
			session.pRecvBuffer = new char[config.MaxClientRecvBufferSize];
			session.pSendBuffer = new char[config.MaxClientSendBufferSize];

			clientSessionPool.push_back(session);
			clientSessionPoolIndex.push_back(session.Index);
		}

		return pMaxClientCount;
	}

	NET_ERROR_CODE TCPNetWork::NewSession()
	{
		auto tryCount = 0; // 너무 많이 accept를 시도하지 않도록 한다.

		do
		{
			++tryCount;

			SOCKADDR_IN client_addr;
			auto client_len = static_cast<int>(sizeof(client_addr));
			//억셉트
			auto client_sockfd = accept(serverSocketFd, (SOCKADDR*)&client_addr, &client_len);
			//m_pRefLogger->Write(LOG_TYPE::L_DEBUG, "%s | client_sockfd(%I64u)", __FUNCTION__, client_sockfd);
			if (client_sockfd == INVALID_SOCKET)
			{
				if (WSAGetLastError() == WSAEWOULDBLOCK)
				{
					return NET_ERROR_CODE::ACCEPT_API_WSAEWOULDBLOCK;
				}

				refLogger->Write(LOG_TYPE::L_ERROR, "%s | Wrong socket cannot accept", __FUNCTION__);
				return NET_ERROR_CODE::ACCEPT_API_ERROR;
			}

			//할당
			auto newSessionIndex = AllocClientSessionIndex();
			if (newSessionIndex < 0)
			{
				refLogger->Write(LOG_TYPE::L_WARN, "%s | client_sockfd(%I64u)  >= MAX_SESSION", __FUNCTION__, client_sockfd);

				// 더 이상 수용할 수 없으므로 바로 짜른다.
				CloseSession(SOCKET_CLOSE_CASE::SESSION_POOL_EMPTY, client_sockfd, -1);
				return NET_ERROR_CODE::ACCEPT_MAX_SESSION_COUNT;
			}


			char clientIP[MAX_IP_LEN] = { 0, };
			inet_ntop(AF_INET, &(client_addr.sin_addr), clientIP, MAX_IP_LEN - 1);

			//바로짜를 수있도록 클라이언트를 담당하는 소켓의 옵션을 설정
			SetSocketOption(client_sockfd);

			//소켓을 소켓셋에 등록한다.
			FD_SET(client_sockfd, &ReadFds);
			//m_pRefLogger->Write(LOG_TYPE::L_DEBUG, "%s | client_sockfd(%I64u)", __FUNCTION__, client_sockfd);
			ConnectedSession(newSessionIndex, client_sockfd, clientIP);

		} while (tryCount < FD_SETSIZE);//악의적 공격 방어

		return NET_ERROR_CODE::NONE;
	}

	void TCPNetWork::SetSocketOption(const SOCKET pFD)
	{
		linger ling;
		ling.l_onoff = 0;
		ling.l_linger = 0;
		setsockopt(pFD, SOL_SOCKET, SO_LINGER, (char*)&ling, sizeof(ling));

		int size1 = config.MaxClientSocketOptRecvBufferSize;
		int size2 = config.MaxClientSocketOptSendBufferSize;
		setsockopt(pFD, SOL_SOCKET, SO_RCVBUF, (char*)&size1, sizeof(size1));
		setsockopt(pFD, SOL_SOCKET, SO_SNDBUF, (char*)&size2, sizeof(size2));
	}

	void TCPNetWork::ConnectedSession(const int pSessionIndex, const SOCKET pFD, const char * pIP)
	{
		++connectSeq;

		auto& session = clientSessionPool[pSessionIndex];
		session.Seq = connectSeq;
		session.SocketFD = pFD;
		memcpy(session.IP, pIP, MAX_IP_LEN - 1);

		++connectedSessionCount;

		AddPacketQue(pSessionIndex, (short)PACKET_ID::NTF_SYS_CONNECT_SESSION, 0, nullptr);

		refLogger->Write(LOG_TYPE::L_INFO, "%s | New Session. FD(%I64u), m_ConnectSeq(%d), IP(%s)", __FUNCTION__, pFD, connectSeq, pIP);

	}

	void TCPNetWork::CloseSession(const SOCKET_CLOSE_CASE pCloseCase, const SOCKET pSocketFD, const int pSessionIndex)
	{
		if (pCloseCase == SOCKET_CLOSE_CASE::SESSION_POOL_EMPTY)
		{
			closesocket(pSocketFD);
			FD_CLR(pSocketFD, &ReadFds);
			return;
		}

		if (clientSessionPool[pSessionIndex].IsConnected() == false) {
			return;
		}

		closesocket(pSocketFD);
		FD_CLR(pSocketFD, &ReadFds);

		clientSessionPool[pSessionIndex].Clear();
		--connectedSessionCount;
		ReleaseSessionIndex(pSessionIndex);

		AddPacketQue(pSessionIndex, (short)PACKET_ID::NTF_SYS_CLOSE_SESSION, 0, nullptr);
	}

	void TCPNetWork::RunProcessWrite(const int pSessionIndex, const SOCKET pFD, fd_set & pWrite_set)
	{
		//읽기 가능 하면 아래를 수행.
		if (!FD_ISSET(pFD, &pWrite_set))
		{
			return;
		}

		auto retsend = FlushSendBuff(pSessionIndex);
		if (retsend.Error != NET_ERROR_CODE::NONE)
		{
			CloseSession(SOCKET_CLOSE_CASE::SOCKET_SEND_ERROR, pFD, pSessionIndex);
		}
	}

	NetError TCPNetWork::FlushSendBuff(const int pSessionIndex)
	{
		auto& session = clientSessionPool[pSessionIndex];
		auto fd = static_cast<SOCKET>(session.SocketFD);

		if (session.IsConnected() == false)
		{
			//연결 확인
			return NetError(NET_ERROR_CODE::CLIENT_FLUSH_SEND_BUFF_REMOTE_CLOSE);
		}

		//버퍼에 있는 데이터를 사이즈 만큼 보낸다.
		auto result = SendSocket(fd, session.pSendBuffer, session.SendSize);

		if (result.Error != NET_ERROR_CODE::NONE) {
			//에러가 있으면 리턴 값이 음수 일것이므로 아래를 수행하면 안된다.
			return result;
		}


		auto sendSize = result.Value;
		//보낸 사이즈가 보내야할 사이즈보다 작으면
		if (sendSize < session.SendSize)
		{
			//버퍼에서 이동을 시킨다.
			memmove(&session.pSendBuffer[0],
				&session.pSendBuffer[sendSize],
				session.SendSize - sendSize);

			session.SendSize -= sendSize;
		}
		else
		{
			//보내야 할것 보다 많이 보낸리가 없으므로 나머지 경우는 무조건 보내야 할만큼 보냈고
			//그러므로 0
			session.SendSize = 0;
		}
		return result;
	}

	NetError TCPNetWork::SendSocket(const SOCKET pFD, const char * pMsg, const int pSize)
	{
		NetError result(NET_ERROR_CODE::NONE);

		// 접속 되어 있는지 또는 보낼 데이터가 있는지
		if (pSize <= 0)
		{
			return result;
		}

		result.Value = send(pFD, pMsg, pSize, 0);

		if (result.Value <= 0)
		{
			result.Error = NET_ERROR_CODE::SEND_SIZE_ZERO;
		}

		return result;
	}

	bool TCPNetWork::RunCheckSelectResult(const int pResult)
	{
		if (pResult == 0)
		{
			return false;
		}
		else if (pResult == -1)
		{
			// TODO:로그 남기기
			return false;
		}

		return true;
	}

	void TCPNetWork::RunCheckSelectClients(fd_set & pRead_set, fd_set & pWrite_set)
	{
		for (auto& session : clientSessionPool)//세션풀의 모든 세션에 대해서
		{
			if (session.IsConnected() == false) 
			{
				continue;//세션이 연결이 안되있으면 아래 무시
			}

			SOCKET fd = session.SocketFD;
			auto sessionIndex = session.Index;

			// check read
			auto retReceive = RunProcessReceive(sessionIndex, fd, pRead_set);
			if (retReceive == false) {
				continue;
			}

			// check write
			RunProcessWrite(sessionIndex, fd, pWrite_set);
		}
	}

	bool TCPNetWork::RunProcessReceive(const int sessionIndex, const SOCKET pFD, fd_set & pRead_set)
	{
		//리드셋에 소켓이 있으면 아래실행
		if (!FD_ISSET(pFD, &pRead_set))
		{
			return true;
		}


		auto ret = RecvSocket(sessionIndex);
		if (ret != NET_ERROR_CODE::NONE)
		{
			CloseSession(SOCKET_CLOSE_CASE::SOCKET_RECV_ERROR, pFD, sessionIndex);
			return false;
		}

		ret = RecvBufferProcess(sessionIndex);
		if (ret != NET_ERROR_CODE::NONE)
		{
			CloseSession(SOCKET_CLOSE_CASE::SOCKET_RECV_BUFFER_PROCESS_ERROR, pFD, sessionIndex);
			return false;
		}

		return true;
	}

	NET_ERROR_CODE TCPNetWork::RecvSocket(const int pSessionIndex)
	{
		auto& session = clientSessionPool[pSessionIndex];
		auto fd = static_cast<SOCKET>(session.SocketFD);

		//세션이 연결 안돼있으면 아래 무시
		if (session.IsConnected() == false)
		{
			return NET_ERROR_CODE::RECV_PROCESS_NOT_CONNECTED;
		}


		int recvPos = 0;

		//내 버퍼에 데이터가 남아있으면 예를 들면 여러개의 패킷이 왔다던가 여러개의 패킷이 왔는데 마지막 데이터가 잘렸다던가.
		if (session.RemainingDataSize > 0)
		{
			//내 버퍼에 데이터가 남아 있으면 앞으로 당겨온다.
			memcpy(session.pRecvBuffer, &session.pRecvBuffer[session.PrevReadPosInRecvBuffer], session.RemainingDataSize);
			//그리고 그 뒤에 데이터를 받을수 있도록 위치 변경 
			recvPos += session.RemainingDataSize;
		}

		//데이터를 받는다. 버퍼에 남아 있는 위치부터 받아온다. 읽어 오는 최대 범위가 너무 크지 않을까? 
		//컨피그에서 보통 내가 관리 하는 버퍼는
		//실제 맥스 패킷 크기의 3배정도로 설정하고 바디 사이즈는 2배를 읽어 오는 방식으로 한다.
		auto recvSize = recv(fd, &session.pRecvBuffer[recvPos], (MAX_PACKET_BODY_SIZE * 2), 0);

		if (recvSize == 0)
		{
			//연결 종료
			return NET_ERROR_CODE::RECV_REMOTE_CLOSE;
		}

		if (recvSize < 0)
		{
			//오류 처리
			auto error = WSAGetLastError();
			if (error != WSAEWOULDBLOCK)
			{
				return NET_ERROR_CODE::RECV_API_ERROR;
			}
			else
			{
				return NET_ERROR_CODE::NONE;
			}
		}
		//현재 버퍼에 남아있는 데이터 갱신
		session.RemainingDataSize += recvSize;
		return NET_ERROR_CODE::NONE;
	}

	NET_ERROR_CODE TCPNetWork::RecvBufferProcess(const int pSessionIndex)
	{
		auto& session = clientSessionPool[pSessionIndex];

		auto readPos = 0;
		//여기서 RemainingDataSize 현재 내버퍼에 남은 데이터 사이즈.
		const auto dataSize = session.RemainingDataSize;
		PacketHeader* pPktHeader;

		//내버퍼에 담긴 데이터가 헤더를 다 받아왔으면 블럭안을 수행
		while ((dataSize - readPos) >= PACKET_HEADER_SIZE)
		{
			pPktHeader = (PacketHeader*)&session.pRecvBuffer[readPos];
			readPos += PACKET_HEADER_SIZE;

			if (pPktHeader->BodySize > 0)
			{
				if (pPktHeader->BodySize > (dataSize - readPos))
				{
					//바디를 다 받아오지 못하면 브레이크 그리고 읽은 위치를 다시 되돌림
					readPos -= PACKET_HEADER_SIZE;
					break;
				}

				if (pPktHeader->BodySize > MAX_PACKET_BODY_SIZE)
				{
					//이상한 바디를 보냈으면 나쁜 친구임.
					// 더 이상 이 세션과는 작업을 하지 않을 예정. 클라이언트 보고 나가라고 하던가 직접 짤라야 한다.
					return NET_ERROR_CODE::RECV_CLIENT_MAX_PACKET;
				}
			}

			//바디를 다 받아왔을때 여기를 수행.
			AddPacketQue(pSessionIndex, pPktHeader->Id, pPktHeader->BodySize, &session.pRecvBuffer[readPos]);
			readPos += pPktHeader->BodySize;
		}

		//헤더를 다못받아오면 내버퍼에 담겨있는 데이터는 변경안함. 이전에 받아온 위치도 0.
		session.RemainingDataSize -= readPos;
		session.PrevReadPosInRecvBuffer = readPos;

		return NET_ERROR_CODE::NONE;
	}

	void TCPNetWork::AddPacketQue(const int pSessionIndex, const short pPktId, const short pBodySize, char * pDataPos)
	{
		RecvPacketInfo packetInfo;
		packetInfo.SessionIndex = pSessionIndex;
		packetInfo.PacketId = pPktId;
		packetInfo.PacketBodySize = pBodySize;
		packetInfo.pData = pDataPos;

		packetQue.push_back(packetInfo);
	}
}
