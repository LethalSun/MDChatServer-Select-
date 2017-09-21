#include "../../MDServerNetLib/MDServerNetLib/ILog.h"
#include "../../MDServerNetLib/MDServerNetLib/TCPNetWork.h"
#include "../../Common/ErrorCode.h"
#include  "../../Common/Packet.h"
#include "Lobby.h"
#include "LobbyManager.h"

using ERROR_CODE = MDCommon::ERROR_CODE;
using PACKET_ID = MDCommon::PACKET_ID;
namespace MDLogicLib
{
	void MDLogicLib::LobbyManager::Init(const LobbyManagerConfig config, TcpNet * pNetwork, ILog * pLogger)
	{
		m_pRefLogger = pLogger;
		m_pRefNetwork = pNetwork;

		for (int i = 0; i < config.MaxLobbyCount; ++i)
		{
			Lobby lobby;
			lobby.Init((short)i, (short)config.MaxLobbyUserCount, (short)config.MaxRoomCountByLobby, (short)config.MaxRoomUserCount);
			lobby.SetNetwork(m_pRefNetwork, m_pRefLogger);

			m_LobbyList.push_back(lobby);
		}
	}

	Lobby * MDLogicLib::LobbyManager::GetLobby(short lobbyId)
	{
		if (lobbyId < 0 || lobbyId >= (short)m_LobbyList.size()) {
			return nullptr;
		}

		return &m_LobbyList[lobbyId];
	}

	void MDLogicLib::LobbyManager::SendLobbyListInfo(const int sessionIndex)
	{
		MDCommon::PktLobbyListRes resPkt;
		resPkt.ErrorCode = (short)ERROR_CODE::NONE;
		resPkt.LobbyCount = static_cast<short>(m_LobbyList.size());

		int index = 0;
		for (auto& lobby : m_LobbyList)
		{
			resPkt.LobbyList[index].LobbyId = lobby.GetIndex();
			resPkt.LobbyList[index].LobbyUserCount = lobby.GetUserCount();

			++index;
		}

		// 보낼 데이터를 줄이기 위해 사용하지 않은 LobbyListInfo 크기는 빼고 보내도 된다.
		m_pRefNetwork->SendData(sessionIndex, (short)PACKET_ID::LOBBY_LIST_RES, sizeof(resPkt), (char*)&resPkt);

	}
}

