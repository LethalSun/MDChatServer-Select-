#pragma once

#include <memory>
#include "../../Common/Packet.h"
#include "../../MDServerNetLib/MDServerNetLib/Define.h"
#include "../../Common/ErrorCode.h"
#include "ConnectedUserManager.h"
using ERROR_CODE = MDCommon::ERROR_CODE;

namespace MDServerNetLib
{
	class ILog;
	class ITCPNetWork;
}

namespace MDLogicLib
{
	class ConnectedUserManager;
	class UserManager;
	class LobbyManager;

	using ServerConfig = MDServerNetLib::ServerConfig;


#define CHECK_START  ERROR_CODE __result=ERROR_CODE::NONE;
#define CHECK_ERROR(f) __result=f; goto CHECK_ERR;//스코프드 엑시트

	class PacketProcessor
	{
		using PacketInfo = MDServerNetLib::RecvPacketInfo;
		typedef ERROR_CODE(PacketProcessor::*PacketFunc)(PacketInfo);
		PacketFunc PacketFuncArray[(int)MDCommon::PACKET_ID::MAX];

		using TcpNet = MDServerNetLib::ITCPNetWork;
		using ILog = MDServerNetLib::ILog;
	public:
		PacketProcessor() = default;
		~PacketProcessor() = default;
		void Init(TcpNet* pNetwork, UserManager* pUserMgr, LobbyManager* pLobbyMgr, ServerConfig* pConfig, ILog* pLogger);

		void Process(PacketInfo packetInfo);

		void StateCheck();
	private:
		ILog* m_pRefLogger;
		TcpNet* m_pRefNetwork;

		UserManager* m_pRefUserMgr;
		LobbyManager* m_pRefLobbyMgr;

		std::unique_ptr<ConnectedUserManager> m_pConnectedUserManager;

	private:
		ERROR_CODE NtfSysConnctSession(PacketInfo packetInfo);
		ERROR_CODE NtfSysCloseSession(PacketInfo packetInfo);

		ERROR_CODE Login(PacketInfo packetInfo);

		ERROR_CODE LobbyList(PacketInfo packetInfo);

		ERROR_CODE LobbyEnter(PacketInfo packetInfo);

		ERROR_CODE LobbyRoomList(PacketInfo packetInfo);

		ERROR_CODE LobbyUserList(PacketInfo packetInfo);

		ERROR_CODE LobbyChat(PacketInfo packetInfo);

		ERROR_CODE LobbyLeave(PacketInfo packetInfo);

		ERROR_CODE RoomEnter(PacketInfo packetInfo);

		ERROR_CODE RoomLeave(PacketInfo packetInfo);

		ERROR_CODE RoomChat(PacketInfo packetInfo);

		ERROR_CODE RoomMasterGameStart(PacketInfo packetInfo);

		ERROR_CODE RoomGameStart(PacketInfo packetInfo);



		ERROR_CODE DevEcho(PacketInfo packetInfo);
	};

}

