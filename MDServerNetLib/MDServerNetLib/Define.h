#pragma once

namespace MDServerNetLib
{
	struct ServerConfig
	{
		unsigned short Port;
		int BackLogCount;

		int MaxClientCount;//최대 수용 클라이언트
		int ExtraClientCount;//가능하면 로그인에서 짜르도록 MaxClientCount + 여유분을 준비한다. 패킷을 보내서 끊는다.

		short MaxClientSocketOptRecvBufferSize;//커널 버퍼
		short MaxClientSocketOptSendBufferSize;//커널 버퍼
		short MaxClientRecvBufferSize;//내가 사용하는 버퍼
		short MaxClientSendBufferSize;//내가 사용하는 버퍼

		bool IsLoginCheck; //연결 후 특정 시간 이내에 로그인 완료 여부 조사 악의적 접속 조사 여부

		int MaxLobbyCount;
		int MaxLobbyUserCount;
		int MaxRoomCountByLobby;
		int MaxRoomUserCount;

	};

	const int MAX_IP_LEN = 32; // IP 문자열 최대 길이
	const int MAX_PACKET_BODY_SIZE = 1024; // 최대 패킷 보디 크기

	struct ClientSession
	{
		bool IsConnected() 
		{ 
			return SocketFD != 0 ? true : false; 
		}

		void Clear()
		{
			Seq = 0;
			SocketFD = 0;
			IP[0] = '\0';
			RemainingDataSize = 0;
			PrevReadPosInRecvBuffer = 0;
			SendSize = 0;
		}

		int Index = 0;
		long long Seq = 0;
		unsigned long long SocketFD = 0;
		char IP[MAX_IP_LEN] = { 0, };

		char* pRecvBuffer = nullptr;
		int RemainingDataSize = 0;
		int PrevReadPosInRecvBuffer = 0;

		char* pSendBuffer = nullptr;
		int SendSize = 0;
	};

	struct RecvPacketInfo
	{
		int SessionIndex = 0;
		short PacketId = 0;
		short PacketBodySize = 0;
		char* pData;
	};

	enum class SOCKET_CLOSE_CASE :short
	{
		SESSION_POOL_EMPTY = 1,
		SELECT_ERROR = 2,
		SOCKET_RECV_ERROR = 3,
		SOCKET_RECV_BUFFER_PROCESS_ERROR = 4,
		SOCKET_SEND_ERROR = 5,
		FORCING_CLOSE = 6,
	};

	enum class PACKET_ID :short
	{
		NTF_SYS_CONNECT_SESSION = 2,
		NTF_SYS_CLOSE_SESSION = 3,
	};

#pragma pack(push,1)
	struct PacketHeader
	{
		short Id;
		short BodySize;
	};

	const int PACKET_HEADER_SIZE = sizeof(PacketHeader);

	struct PktNtfSysCloseSession : PacketHeader
	{
		int SockFD;
	};
#pragma pack(pop)
}