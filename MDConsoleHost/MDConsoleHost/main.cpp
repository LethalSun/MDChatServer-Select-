#include <iostream>
#include <thread>
#include "../../MDLogicLib/MDLogicLib/LogicMain.h"
//��Ʈ��ũ ���̺귯�� ��Ŭ���
//���� ���̺귯�� ��Ŭ���

int main()
{

	MDLogicLib::LogicMain logicMain;
	logicMain.Init();

	std::thread logicThread([&]() {
		logicMain.Run(); }//�̰��� ������ ���񽺷� ���ư��غ���.
	);

	std::cout << "press any key to exit...";
	getchar();

	logicMain.Stop();
	logicThread.join();

	return 0;
}