#pragma once

#include <memory>
#include "../../Common/Packet.h"
#include "../../Common/ErrorCode.h"
#include <thread>
#include <concurrent_queue.h>//

using ERROR_CODE = MDCommon::ERROR_CODE;

namespace MDServerNetLib
{
	struct ServerConfig;
	class ILog;
	class ITCPNetWork;
}

namespace MDLogicLib
{
	class UserManager;
	class LobbyManager;
	class PacketProcessor;

	class LogicMain
	{
	public:
		LogicMain();
		~LogicMain();

		ERROR_CODE Init();

		void Run();

		void Stop();
		//

	private:
		//Init�Լ����� ���� ������ �ε��ϴ� �Լ�.
		ERROR_CODE LoadConfig();
		//��Ʈ��ũ �ν��Ͻ��� ������ �Լ��� ȣ���Ѵ�.
		void Release();
	private:
		bool m_IsRun = false;
		//std::unique_ptr<MDServerNetLib::Log> m_Logger; //�ΰ� ���
		//std::unique_ptr<MDServerNetLib::ServerConfig> m_ServerConfig; //������ �Ӽ��� json���Ϸ� ���� �о���� ���
		//TODO: ���߿� ������ ������� �и�
		//std::unique_ptr<MDServerNetLib::ITcpNetwork> m_Network; //��Ʈ��ũ �ν��Ͻ� 
		std::unique_ptr<MDServerNetLib::ServerConfig> m_pServerConfig;
		std::unique_ptr<MDServerNetLib::ILog> m_pLogger;

		std::unique_ptr<MDServerNetLib::ITCPNetWork> m_pNetwork;
		std::unique_ptr<PacketProcessor> m_pPacketProcessor;
		std::unique_ptr<LobbyManager> m_pLobbyManager;
		std::unique_ptr<UserManager> m_pUserManager;

		
		
	};


}
