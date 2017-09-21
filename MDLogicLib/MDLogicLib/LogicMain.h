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
		//Init함수에서 서버 설정을 로드하는 함수.
		ERROR_CODE LoadConfig();
		//네트워크 인스턴스의 릴리즈 함수를 호출한다.
		void Release();
	private:
		bool m_IsRun = false;
		//std::unique_ptr<MDServerNetLib::Log> m_Logger; //로거 멤버
		//std::unique_ptr<MDServerNetLib::ServerConfig> m_ServerConfig; //서버의 속성을 json파일로 부터 읽어오는 멤버
		//TODO: 나중에 별도의 스레드로 분리
		//std::unique_ptr<MDServerNetLib::ITcpNetwork> m_Network; //네트워크 인스턴스 
		std::unique_ptr<MDServerNetLib::ServerConfig> m_pServerConfig;
		std::unique_ptr<MDServerNetLib::ILog> m_pLogger;

		std::unique_ptr<MDServerNetLib::ITCPNetWork> m_pNetwork;
		std::unique_ptr<PacketProcessor> m_pPacketProcessor;
		std::unique_ptr<LobbyManager> m_pLobbyManager;
		std::unique_ptr<UserManager> m_pUserManager;

		
		
	};


}
