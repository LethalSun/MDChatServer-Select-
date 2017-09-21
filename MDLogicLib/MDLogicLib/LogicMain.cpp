
#include <thread>
#include <chrono>

#include "../../MDServerNetLib/MDServerNetLib/ServerNetErrorCode.h"
#include "../../MDServerNetLib/MDServerNetLib/Define.h"
#include "../../MDServerNetLib/MDServerNetLib/TCPNetWork.h"
#include "ConsoleLogger.h"
#include "LobbyManager.h"
#include "PacketProcssor.h"
#include "UserManager.h"
#include "LogicMain.h"

using LOG_TYPE = MDServerNetLib::LOG_TYPE;
using NET_ERROR_CODE = MDServerNetLib::NET_ERROR_CODE;

namespace MDLogicLib
{
	LogicMain::LogicMain()
	{
	}


	LogicMain::~LogicMain()
	{
		Release();
	}

	ERROR_CODE LogicMain::Init()
	{
		//로러 생성
		m_pLogger = std::make_unique<ConsoleLog>();

		//컨피그 로드
		LoadConfig();

		m_pNetwork = std::make_unique<MDServerNetLib::TCPNetWork>();//TODO: 1.분리 해야 될 부분
		//이것을 어디서든지 스레드를 이용해서 별도로 동작하고 큐를 통해서 서로 패킷을 주고 받게
		//고쳐야 한다

		auto result = m_pNetwork->Init(m_pServerConfig.get(), m_pLogger.get());

		if (result != NET_ERROR_CODE::NONE)
		{
			m_pLogger->Write(LOG_TYPE::L_ERROR, "%s | Init Fail. NetErrorCode(%s)", __FUNCTION__, (short)result);
			return ERROR_CODE::MAIN_INIT_NETWORK_INIT_FAIL;
		}

		m_pUserManager = std::make_unique<UserManager>();
		m_pUserManager->Init(m_pServerConfig->MaxClientCount);

		m_pLobbyManager = std::make_unique<LobbyManager>();
		m_pLobbyManager->Init(
			{m_pServerConfig->MaxLobbyCount,
			m_pServerConfig->MaxLobbyUserCount,
			m_pServerConfig->MaxRoomCountByLobby,
			m_pServerConfig->MaxRoomUserCount },
			m_pNetwork.get(),m_pLogger.get());

		m_pPacketProcessor = std::make_unique<PacketProcessor>();
		m_pPacketProcessor->Init(m_pNetwork.get(), m_pUserManager.get(), m_pLobbyManager.get(), m_pServerConfig.get(), m_pLogger.get());

		m_IsRun = true;

		m_pLogger->Write(LOG_TYPE::L_INFO, "%s | Init Success. Server Run", __FUNCTION__);
		return ERROR_CODE::NONE;

	}

	void LogicMain::Run()
	{
		while (m_IsRun)
		{
			m_pNetwork->Run();

			while (true)
			{
				auto packetInfo = m_pNetwork->GetPacketInfo();

				if (packetInfo.PacketId == 0)
				{
					break;
				}
				else
				{
					m_pPacketProcessor->Process(packetInfo);
				}
			}

			m_pPacketProcessor->StateCheck();
		}
	}

	void LogicMain::Stop()
	{
		m_IsRun = false;
	}

	ERROR_CODE LogicMain::LoadConfig()
	{
		m_pServerConfig = std::make_unique<MDServerNetLib::ServerConfig>();

		wchar_t sPath[MAX_PATH] = { 0, };
		::GetCurrentDirectory(MAX_PATH, sPath);

		wchar_t inipath[MAX_PATH] = { 0, };
		_snwprintf_s(inipath, _countof(inipath), _TRUNCATE, L"%s\\ServerConfig.ini", sPath);

		m_pServerConfig->Port = (unsigned short)GetPrivateProfileInt(L"Config", L"Port", 0, inipath);
		m_pServerConfig->BackLogCount = GetPrivateProfileInt(L"Config", L"BackLogCount", 0, inipath);
		m_pServerConfig->MaxClientCount = GetPrivateProfileInt(L"Config", L"MaxClientCount", 0, inipath);

		m_pServerConfig->MaxClientSocketOptRecvBufferSize = (short)GetPrivateProfileInt(L"Config", L"MaxClientSockOptRecvBufferSize", 0, inipath);
		m_pServerConfig->MaxClientSocketOptSendBufferSize = (short)GetPrivateProfileInt(L"Config", L"MaxClientSockOptSendBufferSize", 0, inipath);
		m_pServerConfig->MaxClientRecvBufferSize = (short)GetPrivateProfileInt(L"Config", L"MaxClientRecvBufferSize", 0, inipath);
		m_pServerConfig->MaxClientSendBufferSize = (short)GetPrivateProfileInt(L"Config", L"MaxClientSendBufferSize", 0, inipath);

		m_pServerConfig->IsLoginCheck = GetPrivateProfileInt(L"Config", L"IsLoginCheck", 0, inipath) == 1 ? true : false;

		m_pServerConfig->ExtraClientCount = GetPrivateProfileInt(L"Config", L"ExtraClientCount", 0, inipath);
		m_pServerConfig->MaxLobbyCount = GetPrivateProfileInt(L"Config", L"MaxLobbyCount", 0, inipath);
		m_pServerConfig->MaxLobbyUserCount = GetPrivateProfileInt(L"Config", L"MaxLobbyUserCount", 0, inipath);
		m_pServerConfig->MaxRoomCountByLobby = GetPrivateProfileInt(L"Config", L"MaxRoomCountByLobby", 0, inipath);
		m_pServerConfig->MaxRoomUserCount = GetPrivateProfileInt(L"Config", L"MaxRoomUserCount", 0, inipath);

		m_pLogger->Write(MDServerNetLib::LOG_TYPE::L_INFO, "%s | Port(%d), Backlog(%d)", __FUNCTION__, m_pServerConfig->Port, m_pServerConfig->BackLogCount);
		m_pLogger->Write(MDServerNetLib::LOG_TYPE::L_INFO, "%s | IsLoginCheck(%d)", __FUNCTION__, m_pServerConfig->IsLoginCheck);
		return ERROR_CODE::NONE;
	}

	void LogicMain::Release()
	{
		if (m_pNetwork) {
			m_pNetwork->Release();
		}
	}

}

