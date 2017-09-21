#include <iostream>
#include <thread>
#include "../../MDLogicLib/MDLogicLib/LogicMain.h"
//네트워크 라이브러리 인클루드
//로직 라이브러리 인클루드

int main()
{

	MDLogicLib::LogicMain logicMain;
	logicMain.Init();

	std::thread logicThread([&]() {
		logicMain.Run(); }//이것을 윈도우 서비스로 돌아게해보기.
	);

	std::cout << "press any key to exit...";
	getchar();

	logicMain.Stop();
	logicThread.join();

	return 0;
}