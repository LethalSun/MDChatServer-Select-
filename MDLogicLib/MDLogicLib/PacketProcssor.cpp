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

		//��Ŷ�� �ش��ϴ� �Լ��� ���̵� ���ؼ� ���� �Ѵ�. ���� ���� �����Ƿ�
		//�迭�� ���� ������ ���� ����ϴ°͵� ����
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
		//��Ŷ�� ���̵� �̿��ؼ�
		auto packetId = packetInfo.PacketId;

		//�Լ��� �����Ѵ�.
		if (PacketFuncArray[packetId] == nullptr)
		{
			//TODO: �α� �����
			return;
		}

		(this->*PacketFuncArray[packetId])(packetInfo);
	}

	//�������� �α��� ���¸� �ֱ������� üũ �Ѵ�.
	void PacketProcessor::StateCheck()
	{
		m_pConnectedUserManager->LoginCheck();
	}

	//���� ��Ŷ ó��
	ERROR_CODE PacketProcessor::NtfSysConnctSession(PacketInfo packetInfo)
	{
		m_pConnectedUserManager->SetConnectSession(packetInfo.SessionIndex);
		return ERROR_CODE::NONE;
	}

	ERROR_CODE PacketProcessor::NtfSysCloseSession(PacketInfo packetInfo)
	{		
		//������ ���ؼ�
		auto pUser = std::get<1>(m_pRefUserMgr->GetUser(packetInfo.SessionIndex));

		//������ ������
		if (pUser)
		{
			//������ �κ� �����ϰ�
			auto pLobby = m_pRefLobbyMgr->GetLobby(pUser->GetLobbyIndex());

			//�κ� ������ �α����ϰ� �κ�� ���� �������� �� �����Ƿ�
			if (pLobby)
			{
				//������ ���� �����ϰ�
				auto pRoom = pLobby->GetRoom(pUser->GetRoomIndex());

				//�濡 ������
				if (pRoom)
				{
					//�濡�� ����, ���� �ٸ� �����鿡�� �˸��� �κ��� �ٸ� �����鿡�� ���� ��ȭ�� �˸���
					pRoom->LeaveUser(pUser->GetIndex());
					pRoom->NotifyLeaveUserInfo(pUser->GetID().c_str());
					pLobby->NotifyChangedRoomInfo(pRoom->GetIndex());
					//�α� ���
					m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | NtfSysCloseSesson. sessionIndex(%d). Room Out", __FUNCTION__, packetInfo.SessionIndex);
				}
				//�κ��� �ٸ� �����鿡�� �˸���
				pLobby->LeaveUser(pUser->GetIndex());

				//���࿡ �κ� �ִ� ���¸�
				if (pRoom == nullptr) {
					//�κ��� ����鿡�� �˸���.
					pLobby->NotifyLobbyLeaveUserInfo(pUser);
				}

				//�α� ���
				m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | NtfSysCloseSesson. sessionIndex(%d). Lobby Out", __FUNCTION__, packetInfo.SessionIndex);
			}

			//�׸��� ������ ������ �ش�.
			m_pRefUserMgr->RemoveUser(packetInfo.SessionIndex);
		}

		m_pConnectedUserManager->SetDisConnectSession(packetInfo.SessionIndex);

		m_pRefLogger->Write(LOG_TYPE::L_INFO, "%s | NtfSysCloseSesson. sessionIndex(%d)", __FUNCTION__, packetInfo.SessionIndex);
		return ERROR_CODE::NONE;
	}

	//���� ��������
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
