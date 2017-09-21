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
		//중복코드이므로 따로 클래스로 빼고 템플릿을 이용한다.
		std::vector<User> m_UserObjPool;
		std::deque<int> m_UserObjPoolIndex;

		//부스트 라이브러리의 멀티 인덱스 사용.
		std::unordered_map<int, User*> m_UserSessionDic;
		std::unordered_map<const char*, User*> m_UserIDDic; //char*는 key로 사용못함

	};
}

