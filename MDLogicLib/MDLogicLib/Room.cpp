#include <algorithm>


#include "../../MDServerNetLib/MDServerNetLib/ILog.h"
#include "../../MDServerNetLib/MDServerNetLib/TCPNetWork.h"
#include "../../Common/ErrorCode.h"
#include  "../../Common/Packet.h"

#include "User.h"
#include "Room.h"

using PACKET_ID = MDCommon::PACKET_ID;

namespace MDLogicLib
{
	MDLogicLib::Room::~Room()
	{
		if (m_pGame != nullptr) {
			delete m_pGame;
		}
	}
	void MDLogicLib::Room::Init(const short index, const short maxUserCount)
	{
		m_Index = index;
		m_MaxUserCount = maxUserCount;

		m_pGame = nullptr;//TODO:new Game;
	}

	void MDLogicLib::Room::SetNetwork(TcpNet * pNetwork, ILog * pLogger)
	{
		m_pRefLogger = pLogger;
		m_pRefNetwork = pNetwork;
	}

	void MDLogicLib::Room::Clear()
	{
		m_IsUsed = false;
		m_Title = L"";
		m_UserList.clear();
	}

	Game* Room::GetGameObj()
	{
		return m_pGame;
	}

	ERROR_CODE MDLogicLib::Room::CreateRoom(const wchar_t * pRoomTitle)
	{
		if (m_IsUsed) {
			return ERROR_CODE::ROOM_ENTER_CREATE_FAIL;
		}

		m_IsUsed = true;
		m_Title = pRoomTitle;

		return ERROR_CODE::NONE;
	}

	ERROR_CODE MDLogicLib::Room::EnterUser(User * pUser)
	{
		if (m_IsUsed == false) {
			return ERROR_CODE::ROOM_ENTER_NOT_CREATED;
		}

		if (m_UserList.size() == m_MaxUserCount) {
			return ERROR_CODE::ROOM_ENTER_MEMBER_FULL;
		}

		m_UserList.push_back(pUser);
		return ERROR_CODE::NONE;
	}

	bool MDLogicLib::Room::IsMaster(const short userIndex)
	{
		return m_UserList[0]->GetIndex() == userIndex ? true : false;
	}

	ERROR_CODE MDLogicLib::Room::LeaveUser(const short userIndex)
	{
		if (m_IsUsed == false) {
			return ERROR_CODE::ROOM_ENTER_NOT_CREATED;
		}

		auto iter = std::find_if(std::begin(m_UserList), std::end(m_UserList), [userIndex](auto pUser) { return pUser->GetIndex() == userIndex; });
		if (iter == std::end(m_UserList)) {
			return ERROR_CODE::ROOM_LEAVE_NOT_MEMBER;
		}

		m_UserList.erase(iter);

		if (m_UserList.empty())
		{
			Clear();
		}

		return ERROR_CODE::NONE;
	}

	void MDLogicLib::Room::SendToAllUser(const short packetId, const short dataSize, char * pData, const int passUserindex)
	{
		for (auto pUser : m_UserList)
		{
			if (pUser->GetIndex() == passUserindex) {
				continue;
			}

			m_pRefNetwork->SendData(pUser->GetSessioIndex(), packetId, dataSize, pData);
		}
	}

	void MDLogicLib::Room::NotifyEnterUserInfo(const int userIndex, const char * pszUserID)
	{
		MDCommon::PktRoomEnterUserInfoNtf pkt;
		strncpy_s(pkt.UserID, _countof(pkt.UserID), pszUserID, MDCommon::MAX_USER_ID_SIZE);

		SendToAllUser((short)PACKET_ID::ROOM_ENTER_USER_NTF, sizeof(pkt), (char*)&pkt, userIndex);
	}

	void MDLogicLib::Room::NotifyLeaveUserInfo(const char * pszUserID)
	{
		if (m_IsUsed == false) {
			return;
		}

		MDCommon::PktRoomLeaveUserInfoNtf pkt;
		strncpy_s(pkt.UserID, _countof(pkt.UserID), pszUserID, MDCommon::MAX_USER_ID_SIZE);

		SendToAllUser((short)PACKET_ID::ROOM_LEAVE_USER_NTF, sizeof(pkt), (char*)&pkt);
	}

	void MDLogicLib::Room::NotifyChat(const int sessionIndex, const char * pszUserID, const wchar_t * pszMsg)
	{
		MDCommon::PktRoomChatNtf pkt;
		strncpy_s(pkt.UserID, _countof(pkt.UserID), pszUserID, MDCommon::MAX_USER_ID_SIZE);
		wcsncpy_s(pkt.Msg, MDCommon::MAX_ROOM_CHAT_MSG_SIZE + 1, pszMsg, MDCommon::MAX_ROOM_CHAT_MSG_SIZE);

		SendToAllUser((short)PACKET_ID::ROOM_CHAT_NTF, sizeof(pkt), (char*)&pkt, sessionIndex);
	}

	void MDLogicLib::Room::Update()
	{
		//게임 로직 부분
	}
}

