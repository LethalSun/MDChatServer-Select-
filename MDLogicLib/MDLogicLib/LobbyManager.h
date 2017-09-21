#pragma once
#include <vector>
#include <unordered_map>
#include "Lobby.h"
namespace MDServerNetLib
{
	class ITCPNetWork;
	class ILog;
}

namespace MDLogicLib
{
	struct LobbyManagerConfig
	{
		int MaxLobbyCount;
		int MaxLobbyUserCount;
		int MaxRoomCountByLobby;
		int MaxRoomUserCount;
	};

	struct LobbySmallInfo
	{
		short Num;
		short UserCount;
	};

	class Lobby;

	class LobbyManager
	{
		using TcpNet = MDServerNetLib::ITCPNetWork;
		using ILog = MDServerNetLib::ILog;

	public:
		LobbyManager() = default;
		virtual ~LobbyManager() = default;

		void Init(const LobbyManagerConfig config, TcpNet* pNetwork, ILog* pLogger);

		Lobby* GetLobby(short lobbyId);


	public:
		void SendLobbyListInfo(const int sessionIndex);





	private:
		ILog* m_pRefLogger;
		TcpNet* m_pRefNetwork;

		std::vector<Lobby> m_LobbyList;

	};
}