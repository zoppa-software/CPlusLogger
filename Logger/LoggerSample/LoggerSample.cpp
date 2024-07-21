// LoggerSample.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

#include <iostream>
#include "Logger.h"

int main()
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    try {
        throw LogException("Test Exception");
    }
    catch (const LogException& e) {
		std::cout << e.ToString() << std::endl;
	}

    {
        Logger* logger = Logger::Create("logs\\log.txt", 8192, 10);
        logger->SetLevel(LogLevel::Info);

        for (size_t i = 0; i < 1000; ++i) {
            LOG_INFO(logger, "Hello, %s count(%d)", "world", i);
        }

        delete logger;
    }

    _CrtDumpMemoryLeaks();

    return 0;
}

