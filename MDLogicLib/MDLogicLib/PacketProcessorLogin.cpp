#include "../../Common/Packet.h"
#include "../../MDServerNetLib/MDServerNetLib/TCPNetWork.h"
#include "../../Common/ErrorCode.h"
#include "ConnectedUserManager.h"
#include "User.h"
#include "UserManager.h"
#include "LobbyManager.h"
#include "PacketProcssor.h"

using PACKET_ID = MDCommon::PACKET_ID;

namespace MDLogicLib
{
	ERROR_CODE PacketProcessor::Login(PacketInfo packetInfo)
	{
	CHECK_START
		//TODO: ���� �����Ͱ� PktLogInReq ũ�⸸ŭ���� �����ؾ� �Ѵ�.
		// �н������ ������ pass ���ش�.
		// ID �ߺ��̶�� ���� ó���Ѵ�.

		MDCommon::PktLogInRes resPkt;
		auto reqPkt = (MDCommon::PktLogInReq*)packetInfo.pData;

		auto addRet = m_pRefUserMgr->AddUser(packetInfo.SessionIndex, reqPkt->szID);

		if (addRet != ERROR_CODE::NONE) {
			CHECK_ERROR(addRet);
		}

		m_pConnectedUserManager->SetLogin(packetInfo.SessionIndex);

		resPkt.ErrorCode = (short)addRet;
		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::LOGIN_IN_RES, sizeof(MDCommon::PktLogInRes), (char*)&resPkt);

		return ERROR_CODE::NONE;

	CHECK_ERR:
		resPkt.SetError(__result);
		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::LOGIN_IN_RES, sizeof(MDCommon::PktLogInRes), (char*)&resPkt);
		return (ERROR_CODE)__result;
	}

	ERROR_CODE PacketProcessor::LobbyList(PacketInfo packetInfo)
	{
	CHECK_START
		// ���� ���� �����ΰ�?
		// ���� �κ� ���� ���� �����ΰ�?

		auto pUserRet = m_pRefUserMgr->GetUser(packetInfo.SessionIndex);
		auto errorCode = std::get<0>(pUserRet);

		if (errorCode != ERROR_CODE::NONE) {
			CHECK_ERROR(errorCode);
		}

		auto pUser = std::get<1>(pUserRet);

		if (pUser->IsCurDomainInLogIn() == false) {
			CHECK_ERROR(ERROR_CODE::LOBBY_LIST_INVALID_DOMAIN);
		}

		m_pRefLobbyMgr->SendLobbyListInfo(packetInfo.SessionIndex);

		return ERROR_CODE::NONE;

	CHECK_ERR:
		MDCommon::PktLobbyListRes resPkt;
		resPkt.SetError(__result);
		m_pRefNetwork->SendData(packetInfo.SessionIndex, (short)PACKET_ID::LOBBY_LIST_RES, sizeof(resPkt), (char*)&resPkt);
		return (ERROR_CODE)__result;
	}

}