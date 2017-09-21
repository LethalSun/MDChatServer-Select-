#include "../../Common/Packet.h"
#include "../../MDServerNetLib/MDServerNetLib/TCPNetWork.h"
#include "../../Common/ErrorCode.h"

#include "User.h"
#include "UserManager.h"
#include "Lobby.h"
#include "LobbyManager.h"
#include "PacketProcssor.h"

using PACKET_ID = MDCommon::PACKET_ID;

namespace MDLogicLib
{
	ERROR_CODE PacketProcessor::LobbyEnter(PacketInfo packetInfo)
	{
	CHECK_START
		// ���� ��ġ ���´� �α����� �³�?
		// �κ� ����.
		// ���� �κ� �ִ� ������� �� ����� ���Դٰ� �˷��ش�

		auto reqPkt = (MDCommon::PktLobbyEnterReq*)packetInfo.pData;
		MDCommon::PktLobbyEnterRes resPkt;

		auto pUserRet = m_pRefUserMgr->GetUser(packetInfo.SessionIndex);
		auto errorCode = std::get<0>(pUserRet);

		if (errorCode != ERROR_CODE::NONE) {
			CHECK_ERROR(errorCode);
		}

		auto pUser = std::get<1>(pUserRet);

		if (pUser->IsCurDomainInLogIn() == false) {
			CHECK_ERROR(ERROR_CODE::LOBBY_ENTER_INVALID_DOMAIN);
		}

		auto pLobby = m_pRefLobbyMgr->GetLobby(reqPkt->LobbyId);
		if (pLobby == nullptr) {
			CHECK_ERROR(ERROR_CODE::LOBBY_ENTER_INVALID_LOBBY_INDEX);
		}

		auto enterRet = pLobby->EnterUser(pUser);
		if (enterRet != ERROR_CODE::NONE) {
			CHECK_ERROR(enterRet);
		}

		pLobby->NotifyLobbyEnterUserInfo(pUser);

		resPkt.MaxUserCount = pLobby->MaxUserCount();
		resPkt.MaxRoomCount = pLobby->MaxRoomCount();
		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::LOBBY_ENTER_RES, sizeof(MDCommon::PktLobbyEnterRes), (char*)&resPkt);
		return ERROR_CODE::NONE;
		//�������� ����Ʈ
	CHECK_ERR:
		resPkt.SetError(__result);
		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::LOBBY_ENTER_RES, sizeof(MDCommon::PktLobbyEnterRes), (char*)&resPkt);
		return (ERROR_CODE)__result;
	}
	ERROR_CODE PacketProcessor::LobbyRoomList(PacketInfo packetInfo)
	{
	CHECK_START
		// ���� �κ� �ִ��� �����Ѵ�.
		// �� ����Ʈ�� �����ش�.

		auto pUserRet = m_pRefUserMgr->GetUser(packetInfo.SessionIndex);
		auto errorCode = std::get<0>(pUserRet);

		if (errorCode != ERROR_CODE::NONE) {
			CHECK_ERROR(errorCode);
		}

		auto pUser = std::get<1>(pUserRet);

		if (pUser->IsCurDomainInLobby() == false) {
			CHECK_ERROR(ERROR_CODE::LOBBY_ROOM_LIST_INVALID_DOMAIN);
		}

		auto pLobby = m_pRefLobbyMgr->GetLobby(pUser->GetLobbyIndex());
		if (pLobby == nullptr) {
			CHECK_ERROR(ERROR_CODE::LOBBY_ROOM_LIST_INVALID_LOBBY_INDEX);
		}

		auto reqPkt = (MDCommon::PktLobbyRoomListReq*)packetInfo.pData;

		pLobby->SendRoomList(pUser->GetSessioIndex(), reqPkt->StartRoomIndex);

		return ERROR_CODE::NONE;
	CHECK_ERR:
		MDCommon::PktLobbyRoomListRes resPkt;
		resPkt.SetError(__result);
		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::LOBBY_ENTER_ROOM_LIST_RES, sizeof(MDCommon::PktBase), (char*)&resPkt);
		return (ERROR_CODE)__result;
	}
	ERROR_CODE PacketProcessor::LobbyUserList(PacketInfo packetInfo)
	{
	CHECK_START
		// ���� �κ� �ִ��� �����Ѵ�.
		// ���� ����Ʈ�� �����ش�.

		auto pUserRet = m_pRefUserMgr->GetUser(packetInfo.SessionIndex);
		auto errorCode = std::get<0>(pUserRet);

		if (errorCode != ERROR_CODE::NONE) {
			CHECK_ERROR(errorCode);
		}

		auto pUser = std::get<1>(pUserRet);

		if (pUser->IsCurDomainInLobby() == false) {
			CHECK_ERROR(ERROR_CODE::LOBBY_USER_LIST_INVALID_DOMAIN);
		}

		auto pLobby = m_pRefLobbyMgr->GetLobby(pUser->GetLobbyIndex());
		if (pLobby == nullptr) {
			CHECK_ERROR(ERROR_CODE::LOBBY_USER_LIST_INVALID_LOBBY_INDEX);
		}

		auto reqPkt = (MDCommon::PktLobbyUserListReq*)packetInfo.pData;

		pLobby->SendUserList(pUser->GetSessioIndex(), reqPkt->StartUserIndex);

		return ERROR_CODE::NONE;
	CHECK_ERR:
		MDCommon::PktLobbyUserListRes resPkt;
		resPkt.SetError(__result);
		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::LOBBY_ENTER_USER_LIST_RES, sizeof(MDCommon::PktBase), (char*)&resPkt);
		return (ERROR_CODE)__result;
	}
	ERROR_CODE PacketProcessor::LobbyChat(PacketInfo packetInfo)
	{
	CHECK_START
		auto reqPkt = (MDCommon::PktLobbyChatReq*)packetInfo.pData;
		MDCommon::PktLobbyChatRes resPkt;

		auto pUserRet = m_pRefUserMgr->GetUser(packetInfo.SessionIndex);
		auto errorCode = std::get<0>(pUserRet);

		if (errorCode != ERROR_CODE::NONE) {
			CHECK_ERROR(errorCode);
		}

		auto pUser = std::get<1>(pUserRet);

		if (pUser->IsCurDomainInLobby() == false) {
			CHECK_ERROR(ERROR_CODE::LOBBY_CHAT_INVALID_DOMAIN);
		}

		auto lobbyIndex = pUser->GetLobbyIndex();
		auto pLobby = m_pRefLobbyMgr->GetLobby(lobbyIndex);
		if (pLobby == nullptr) {
			CHECK_ERROR(ERROR_CODE::LOBBY_CHAT_INVALID_LOBBY_INDEX);
		}

		pLobby->NotifyChat(pUser->GetSessioIndex(), pUser->GetID().c_str(), reqPkt->Msg);

		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::LOBBY_CHAT_RES, sizeof(resPkt), (char*)&resPkt);
		return ERROR_CODE::NONE;

	CHECK_ERR:
		resPkt.SetError(__result);
		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::LOBBY_CHAT_RES, sizeof(resPkt), (char*)&resPkt);
		return __result;
	}
	ERROR_CODE PacketProcessor::LobbyLeave(PacketInfo packetInfo)
	{
	CHECK_START
		// ���� �κ� �ִ��� �����Ѵ�.
		// �κ񿡼� ������
		// ���� �κ� �ִ� ������� ������ ����� �ִٰ� �˷��ش�.
		MDCommon::PktLobbyLeaveRes resPkt;

		auto pUserRet = m_pRefUserMgr->GetUser(packetInfo.SessionIndex);
		auto errorCode = std::get<0>(pUserRet);

		if (errorCode != ERROR_CODE::NONE) {
			CHECK_ERROR(errorCode);
		}

		auto pUser = std::get<1>(pUserRet);

		if (pUser->IsCurDomainInLobby() == false) {
			CHECK_ERROR(ERROR_CODE::LOBBY_LEAVE_INVALID_DOMAIN);
		}

		auto pLobby = m_pRefLobbyMgr->GetLobby(pUser->GetLobbyIndex());
		if (pLobby == nullptr) {
			CHECK_ERROR(ERROR_CODE::LOBBY_LEAVE_INVALID_LOBBY_INDEX);
		}

		auto enterRet = pLobby->LeaveUser(pUser->GetIndex());
		if (enterRet != ERROR_CODE::NONE) {
			CHECK_ERROR(enterRet);
		}

		pLobby->NotifyLobbyLeaveUserInfo(pUser);

		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::LOBBY_LEAVE_RES, sizeof(MDCommon::PktLobbyLeaveRes), (char*)&resPkt);

		return ERROR_CODE::NONE;
	CHECK_ERR:
		resPkt.SetError(__result);
		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::LOBBY_LEAVE_RES, sizeof(MDCommon::PktLobbyLeaveRes), (char*)&resPkt);
		return (ERROR_CODE)__result;
	}

}