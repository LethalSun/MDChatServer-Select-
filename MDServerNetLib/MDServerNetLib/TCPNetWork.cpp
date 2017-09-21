#include "TCPNetWork.h"

namespace MDServerNetLib
{
	TCPNetWork::~TCPNetWork()
	{
		for (auto& client : clientSessionPool)
		{
			//����ü ������ ���۸� ���� �������� ������ �Ҹ��ڿ� ���ؼ� ������.
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
		//���� ���� ����
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
		FD_SET(serverSocketFd, &ReadFds);//������ ���� �¿� �ִ´�.

		auto sessionPoolSize = CreateSessionPool(config.MaxClientCount + config.ExtraClientCount);
		refLogger->Write(LOG_TYPE::L_INFO, "%s | Session Pool Size: %d", __FUNCTION__, sessionPoolSize);
		return NET_ERROR_CODE::NONE;
	}

	NET_ERROR_CODE TCPNetWork::SendData(const int pSessionIndex, const short pPacketId, const short pSize, const char * pMsg)
	{
		//��Ŷ�� ���� ���Ǳ���ä�� ���� ���ۿ� ����´�.
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
		//�ΰ��� ���Ƶ� �ɱ�? ���Ƶ� ��� ���� ������ Ŀ�� ���ۿ� ���� ���̹Ƿ�
		auto read_set = ReadFds;
		auto write_set = ReadFds;
		//������ ����Ʈ �̺�Ʈ�� üũ�� ���� �ʴٰ� ���߿� �����ʿ��� ����� ũ��� ������ ���� ũ�Ⱑ �ٸ� �� �� �ٽ�����
		//�ʾ��� ���� �� ������ ����� �״ٰ� ����Ʈ �̺�Ʈ�� Ȯ���ϰ� ���� ������ ����. �ֳĸ� ���� ��� ������ ������ ���� �ִ�
		//�����̱⶧���̴�.
		timeval timeout{ 0, 1000 }; //���ʴ�� ��, ����ũ�� ��
		auto selectResult = select(0, &read_set, &write_set, 0, &timeout);//�аų� ���� �����Ѽ����� ������ �� ������ŭ ��ȯ
		//���⼭ ���� ����Ʈ�� ���� ������ �����带 �纸�ϴ� ó���� �� �ʿ䰡 ������ 
		//�����带 ������ ���ٸ� ����0�� 
		//����Ʈ�� ��Ƽ ������Ʈ�� ����ؼ� ��Ŷ�� ������ 
		//�̺�Ʈ�� ����� ó���� �ϵ��� �ϸ� �ȴ�.

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
		//� ������ ���� ��Ŷ�� ���� Ŭ���̾�Ʈ�� ���� �ð� ���� ���� ������ ������ ���´�.
		//SO_LINGER �ɼ�!
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
		auto tryCount = 0; // �ʹ� ���� accept�� �õ����� �ʵ��� �Ѵ�.

		do
		{
			++tryCount;

			SOCKADDR_IN client_addr;
			auto client_len = static_cast<int>(sizeof(client_addr));
			//���Ʈ
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

			//�Ҵ�
			auto newSessionIndex = AllocClientSessionIndex();
			if (newSessionIndex < 0)
			{
				refLogger->Write(LOG_TYPE::L_WARN, "%s | client_sockfd(%I64u)  >= MAX_SESSION", __FUNCTION__, client_sockfd);

				// �� �̻� ������ �� �����Ƿ� �ٷ� ¥����.
				CloseSession(SOCKET_CLOSE_CASE::SESSION_POOL_EMPTY, client_sockfd, -1);
				return NET_ERROR_CODE::ACCEPT_MAX_SESSION_COUNT;
			}


			char clientIP[MAX_IP_LEN] = { 0, };
			inet_ntop(AF_INET, &(client_addr.sin_addr), clientIP, MAX_IP_LEN - 1);

			//�ٷ�¥�� ���ֵ��� Ŭ���̾�Ʈ�� ����ϴ� ������ �ɼ��� ����
			SetSocketOption(client_sockfd);

			//������ ���ϼ¿� ����Ѵ�.
			FD_SET(client_sockfd, &ReadFds);
			//m_pRefLogger->Write(LOG_TYPE::L_DEBUG, "%s | client_sockfd(%I64u)", __FUNCTION__, client_sockfd);
			ConnectedSession(newSessionIndex, client_sockfd, clientIP);

		} while (tryCount < FD_SETSIZE);//������ ���� ���

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
		//�б� ���� �ϸ� �Ʒ��� ����.
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
			//���� Ȯ��
			return NetError(NET_ERROR_CODE::CLIENT_FLUSH_SEND_BUFF_REMOTE_CLOSE);
		}

		//���ۿ� �ִ� �����͸� ������ ��ŭ ������.
		auto result = SendSocket(fd, session.pSendBuffer, session.SendSize);

		if (result.Error != NET_ERROR_CODE::NONE) {
			//������ ������ ���� ���� ���� �ϰ��̹Ƿ� �Ʒ��� �����ϸ� �ȵȴ�.
			return result;
		}


		auto sendSize = result.Value;
		//���� ����� �������� ������� ������
		if (sendSize < session.SendSize)
		{
			//���ۿ��� �̵��� ��Ų��.
			memmove(&session.pSendBuffer[0],
				&session.pSendBuffer[sendSize],
				session.SendSize - sendSize);

			session.SendSize -= sendSize;
		}
		else
		{
			//������ �Ұ� ���� ���� �������� �����Ƿ� ������ ���� ������ ������ �Ҹ�ŭ ���°�
			//�׷��Ƿ� 0
			session.SendSize = 0;
		}
		return result;
	}

	NetError TCPNetWork::SendSocket(const SOCKET pFD, const char * pMsg, const int pSize)
	{
		NetError result(NET_ERROR_CODE::NONE);

		// ���� �Ǿ� �ִ��� �Ǵ� ���� �����Ͱ� �ִ���
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
			// TODO:�α� �����
			return false;
		}

		return true;
	}

	void TCPNetWork::RunCheckSelectClients(fd_set & pRead_set, fd_set & pWrite_set)
	{
		for (auto& session : clientSessionPool)//����Ǯ�� ��� ���ǿ� ���ؼ�
		{
			if (session.IsConnected() == false) 
			{
				continue;//������ ������ �ȵ������� �Ʒ� ����
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
		//����¿� ������ ������ �Ʒ�����
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

		//������ ���� �ȵ������� �Ʒ� ����
		if (session.IsConnected() == false)
		{
			return NET_ERROR_CODE::RECV_PROCESS_NOT_CONNECTED;
		}


		int recvPos = 0;

		//�� ���ۿ� �����Ͱ� ���������� ���� ��� �������� ��Ŷ�� �Դٴ��� �������� ��Ŷ�� �Դµ� ������ �����Ͱ� �߷ȴٴ���.
		if (session.RemainingDataSize > 0)
		{
			//�� ���ۿ� �����Ͱ� ���� ������ ������ ��ܿ´�.
			memcpy(session.pRecvBuffer, &session.pRecvBuffer[session.PrevReadPosInRecvBuffer], session.RemainingDataSize);
			//�׸��� �� �ڿ� �����͸� ������ �ֵ��� ��ġ ���� 
			recvPos += session.RemainingDataSize;
		}

		//�����͸� �޴´�. ���ۿ� ���� �ִ� ��ġ���� �޾ƿ´�. �о� ���� �ִ� ������ �ʹ� ũ�� ������? 
		//���Ǳ׿��� ���� ���� ���� �ϴ� ���۴�
		//���� �ƽ� ��Ŷ ũ���� 3�������� �����ϰ� �ٵ� ������� 2�踦 �о� ���� ������� �Ѵ�.
		auto recvSize = recv(fd, &session.pRecvBuffer[recvPos], (MAX_PACKET_BODY_SIZE * 2), 0);

		if (recvSize == 0)
		{
			//���� ����
			return NET_ERROR_CODE::RECV_REMOTE_CLOSE;
		}

		if (recvSize < 0)
		{
			//���� ó��
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
		//���� ���ۿ� �����ִ� ������ ����
		session.RemainingDataSize += recvSize;
		return NET_ERROR_CODE::NONE;
	}

	NET_ERROR_CODE TCPNetWork::RecvBufferProcess(const int pSessionIndex)
	{
		auto& session = clientSessionPool[pSessionIndex];

		auto readPos = 0;
		//���⼭ RemainingDataSize ���� �����ۿ� ���� ������ ������.
		const auto dataSize = session.RemainingDataSize;
		PacketHeader* pPktHeader;

		//�����ۿ� ��� �����Ͱ� ����� �� �޾ƿ����� ������ ����
		while ((dataSize - readPos) >= PACKET_HEADER_SIZE)
		{
			pPktHeader = (PacketHeader*)&session.pRecvBuffer[readPos];
			readPos += PACKET_HEADER_SIZE;

			if (pPktHeader->BodySize > 0)
			{
				if (pPktHeader->BodySize > (dataSize - readPos))
				{
					//�ٵ� �� �޾ƿ��� ���ϸ� �극��ũ �׸��� ���� ��ġ�� �ٽ� �ǵ���
					readPos -= PACKET_HEADER_SIZE;
					break;
				}

				if (pPktHeader->BodySize > MAX_PACKET_BODY_SIZE)
				{
					//�̻��� �ٵ� �������� ���� ģ����.
					// �� �̻� �� ���ǰ��� �۾��� ���� ���� ����. Ŭ���̾�Ʈ ���� ������� �ϴ��� ���� ©��� �Ѵ�.
					return NET_ERROR_CODE::RECV_CLIENT_MAX_PACKET;
				}
			}

			//�ٵ� �� �޾ƿ����� ���⸦ ����.
			AddPacketQue(pSessionIndex, pPktHeader->Id, pPktHeader->BodySize, &session.pRecvBuffer[readPos]);
			readPos += pPktHeader->BodySize;
		}

		//����� �ٸ��޾ƿ��� �����ۿ� ����ִ� �����ʹ� �������. ������ �޾ƿ� ��ġ�� 0.
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
