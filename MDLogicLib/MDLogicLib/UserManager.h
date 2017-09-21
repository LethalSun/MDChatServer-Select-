#pragma once
#include <unordered_map>
#include <deque>
#include <string>
#include "User.h"

namespace MDCommon
{
	enum class ERROR_CODE :short;
}
using ERROR_CODE = MDCommon::ERROR_CODE;

namespace MDLogicLib
{
	class User;

	class UserManager
	{
	public:
		UserManager() = default;
		virtual ~UserManager() = default;

		void Init(const int maxUserCount);

		ERROR_CODE AddUser(const int sessionIndex, const char* pszID);
		ERROR_CODE RemoveUser(const int sessionIndex);

		std::tuple<ERROR_CODE, User*> GetUser(const int sessionIndex);


	private:
		User* AllocUserObjPoolIndex();
		void ReleaseUserObjPoolIndex(const int index);

		User* FindUser(const int sessionIndex);
		User* FindUser(const char* pszID);

	private:
		//�ߺ��ڵ��̹Ƿ� ���� Ŭ������ ���� ���ø��� �̿��Ѵ�.
		std::vector<User> m_UserObjPool;
		std::deque<int> m_UserObjPoolIndex;

		//�ν�Ʈ ���̺귯���� ��Ƽ �ε��� ���.
		std::unordered_map<int, User*> m_UserSessionDic;
		std::unordered_map<const char*, User*> m_UserIDDic; //char*�� key�� ������

	};
}

