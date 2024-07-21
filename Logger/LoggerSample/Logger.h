#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <Windows.h>

/**
 * ログのレベルです。  
 */
enum LogLevel
{
	/** ログレベルの最小値。 */
	MinLevel = 0,

	/** エラーレベル。 */
	Error = MinLevel,

	/** 警告レベル。 */
	Warning,

	/** 通常レベル。 */
	Info,

	/** デバッグレベル。 */
	Debug,

	/** ログレベルの最大値。 */
	MaxLevel = Debug
};

/**
 * ログのパラメータです。  
 */
class LogParameter;

/**
 * ログを出力します。  
 */
class Logger sealed
{
private:
	// ログファイルのパス
	const char * logPath;

	// ログファイルのパラメータ
	LogParameter * param;

private:
	/**
	 * コンストラクタ。
	 */
	Logger(const char* path, LogParameter* param);

public:
	/**
	 * デストラクタ。
	 */
	~Logger();

public:
	/**
	 * ログクラスを作成します。  
	 * 
	 * @param path ログファイルのパス
	 * @param maxFileSize ログファイルの最大サイズ
	 * @param genLimit ログファイルの世代数
	 * @return ログクラス
	 */
	static Logger* Create(const char * path, long maxFileSize, int genLimit);

	/**
	 * ログレベルを設定します。  
	 * 
	 * @param lv ログレベル
	 */
	void SetLevel(LogLevel lv);

	/**
	 * ログを出力します。  
	 * 
	 * @param file ファイル名
	 * @param line 行番号
	 * @param lv ログレベル
	 * @param message メッセージ
	 */
	void Log(const char * file, int line, LogLevel lv, const char* message, ...);

private:
	/**
	 * ログ出力処理です（別スレッド） 
	 */
	static DWORD WINAPI LoggingThread(LPVOID loggerPtr);
};

// エラーログを出力します。
#define LOG_ERROR(logger, message, ...) logger->Log(__FILE__, __LINE__, LogLevel::Error, message, __VA_ARGS__)

// 警告ログを出力します。
#define LOG_WARNING(logger, message, ...) logger->Log(__FILE__, __LINE__, LogLevel::Warning, message, __VA_ARGS__)

// 通常ログを出力します。
#define LOG_INFO(logger, message, ...) logger->Log(__FILE__, __LINE__, LogLevel::Info, message, __VA_ARGS__)

// デバッグログを出力します。
#define LOG_DEBUG(logger, message, ...) logger->Log(__FILE__, __LINE__, LogLevel::Debug, message, __VA_ARGS__)

/**
 * ログの例外です。  
 */
class LogException sealed
{
private:
	// メッセージ
	const char* message;

public:
	/**
	 * コンストラクタ。 
	 * 
	 * @param msg メッセージ
	 */
	LogException(const char* msg, ...);
	
	/**
	 * デストラクタ。
	 */
	~LogException();

public:
	/**
	 * メッセージを取得します。  
	 * 
	 * @return メッセージ
	 */
	const char* ToString() const;
};

#endif // !__LOGGER_H__

