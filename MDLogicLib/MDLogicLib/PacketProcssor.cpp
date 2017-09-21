#include "../../MDServerNetLib/MDServerNetLib/TCPNetWork.h"
#include "../../MDServerNetLib/MDServerNetLib/ILog.h"

#include "ConnectedUserManager.h"
#include "User.h"
#include "UserManager.h"
#include "Room.h"
#include "Lobby.h"
#include "LobbyManager.h"
#include "PacketProcssor.h"

using LOG_TYPE = MDServerNetLib::LOG_TYPE;
using ServerConfig = MDServerNetLib::ServerConfig;

namespace MDLogicLib
{
	void PacketProcessor::Init(TcpNet * pNetwork, UserManager * pUserMgr, LobbyManager * pLobbyMgr, ServerConfig * pConfig, ILog * pLogger)
	{
		m_pRefLogger = pLogger;
		m_pRefNetwork = pNetwork;
		m_pRefUserMgr = pUserMgr;
		m_pRefLobbyMgr = pLobbyMgr;

		m_pConnectedUserManager = std::make_unique<ConnectedUserManager>();
		m_pConnectedUserManager->Init(pNetwork->ClientSessionPoolSize(), pNetwork, pConfig, pLogger);

		using netLibPacketId = MDServerNetLib::PACKET_ID;
		using commonPacketId = MDCommon::PACKET_ID;
		for (int i = 0; i < (int)commonPacketId::MAX; ++i)
		{
			PacketFuncArray[i] = nullptr;
		}

		//패킷에 해당하는 함수를 아이디에 대해서 저장 한다. 양이 많지 않으므로
		//배열로 관리 언오디드 맵을 사용하는것도 가능
		PacketFuncArray[(int)netLibPacketId::NTF_SYS_CONNECT_SESSION] = &PacketProcessor::NtfSysConnctSession;
		PacketFuncArray[(int)netLibPacketId::NTF_SYS_CLOSE_SESSION] = &PacketProcessor::NtfSysCloseSession;
		PacketFuncArray[(int)commonPacketId::LOGIN_IN_REQ] = &PacketProcessor::Login;
		PacketFuncArray[(int)commonPacketId::LOBBY_LIST_REQ] = &PacketProcessor::LobbyList;
		PacketFuncArray[(int)commonPacketId::LOBBY_ENTER_REQ] = &PacketProcessor::LobbyEnter;
		PacketFuncArray[(int)commonPacketId::LOBBY_ENTER_ROOM_LIST_REQ] = &PacketProcessor::LobbyRoomList;
		PacketFuncArray[(int)commonPacketId::LOBBY_ENTER_USER_LIST_REQ] = &PacketProcessor::LobbyUserList;
		PacketFuncArray[(int)commonPacketId::LOBBY_CHAT_REQ] = &PacketProcessor::LobbyChat;
		PacketFuncArray[(int)commonPacketId::LOBBY_LEAVE_REQ] = &PacketProcessor::LobbyLeave;
		PacketFuncArray[(int)commonPacketId::ROOM_ENTER_REQ] = &PacketProcessor::RoomEnter;
		PacketFuncArray[(int)commonPacketId::ROOM_LEAVE_REQ] = &PacketProcessor::RoomLeave;
		PacketFuncArray[(int)commonPacketId::ROOM_CHAT_REQ] = &PacketProcessor::RoomChat;
		PacketFuncArray[(int)commonPacketId::ROOM_MASTER_GAME_START_REQ] = &PacketProcessor::RoomMasterGameStart;
		PacketFuncArray[(int)commonPacketId::ROOM_GAME_START_REQ] = &PacketProcessor::RoomGameStart;


		PacketFuncArray[(int)commonPacketId::DEV_ECHO_REQ] = &PacketProcessor::DevEcho;
	}

	void PacketProcessor::Process(PacketInfo packetInfo)
	{
		//패킷에 아이디를 이용해서
		auto packetId = packetInfo.PacketId;

		//함수를 실행한다.
		if (PacketFuncArray[packetId] == nullptr)
		{
			//TODO: 로그 남긴다
			return;
		}

		(this->*PacketFuncArray[packetId])(packetInfo);
	}

	//유저들의 로그인 상태를 주기적으로 체크 한다.
	void PacketProcessor::StateCheck()
	{
		m_pConnectedUserManager->LoginCheck();
	}

	//내부 패킷 처리
	ERROR_CODE PacketProcessor::NtfSysConnctSession(PacketInfo packetInfo)
	{
		m_pConnectedUserManager->SetConnectSession(packetInfo.SessionIndex);
		return ERROR_CODE::NONE;
	}

	ERROR_CODE PacketProcessor::NtfSysCloseSession(PacketInfo packetInfo)
	{		
		//유저에 대해서
		auto pUser = std::get<1>(m_pRefUserMgr->GetUser(packetInfo.SessionIndex));

		//유저가 있으면
		if (pUser)
		{
			//유저의 로비를 조사하고
			auto pLobby = m_pRefLobbyMgr->GetLobby(pUser->GetLobbyIndex());

			//로비에 있으면 로그인하고 로비는 선택 안했을수 도 있으므로
			if (pLobby)
			{
				//유저의 방을 조사하고
				auto pRoom = pLobby->GetRoom(pUser->GetRoomIndex());

				//방에 있으면
				if (pRoom)
				{
					//방에서 제거, 방의 다른 유저들에게 알리고 로비의 다른 유저들에게 방의 변화를 알린다
					pRoom->LeaveUser(pUser->GetIndex());
					pRoom->NotifyLeaveUserInfo(pUser->GetID().c_str());
					pLobby->NotifyChangedRoomInfo(pRoom->GetIndex());
					//로그 출력
					m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | NtfSysCloseSesson. sessionIndex(%d). Room Out", __FUNCTION__, packetInfo.SessionIndex);
				}
				//로비의 다른 유저들에게 알리고
				pLobby->LeaveUser(pUser->GetIndex());

				//만약에 로비에 있던 상태면
				if (pRoom == nullptr) {
					//로비의 사람들에게 알린다.
					pLobby->NotifyLobbyLeaveUserInfo(pUser);
				}

				//로그 출력
				m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | NtfSysCloseSesson. sessionIndex(%d). Lobby Out", __FUNCTION__, packetInfo.SessionIndex);
			}

			//그리고 유저를 제거해 준다.
			m_pRefUserMgr->RemoveUser(packetInfo.SessionIndex);
		}

		m_pConnectedUserManager->SetDisConnectSession(packetInfo.SessionIndex);

		m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | NtfSysCloseSesson. sessionIndex(%d)", __FUNCTION__, packetInfo.SessionIndex);
		return ERROR_CODE::NONE;
	}

	//에코 서버역할
	ERROR_CODE PacketProcessor::DevEcho(PacketInfo packetInfo)
	{
		auto reqPkt = (MDCommon::PktDevEchoReq*)packetInfo.pData;

		MDCommon::PktDevEchoRes resPkt;
		resPkt.ErrorCode = (short)ERROR_CODE::NONE;
		resPkt.DataSize = reqPkt->DataSize;
		CopyMemory(resPkt.Datas, reqPkt->Datas, reqPkt->DataSize);

		auto sendSize = sizeof(MDCommon::PktDevEchoRes) - (MDCommon::DEV_ECHO_DATA_MAX_SIZE - reqPkt->DataSize);
		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)MDCommon::PACKET_ID::DEV_ECHO_RES, (short)sendSize, (char*)&resPkt);

		return ERROR_CODE::NONE;
	}

}
