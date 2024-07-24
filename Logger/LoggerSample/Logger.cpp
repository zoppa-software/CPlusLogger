#include "Logger.h"
#include <xstring>

#define LOG_CACHE_SIZE 256

#define MAX_HISTRY_FILE 100

//----------------------------------------------------------------------
// ログ出力データ
//----------------------------------------------------------------------
/**
 * ログ出力データです。
 */
class LogData sealed
{
public:
	// ログ出力ファイル名
	char* fileName;

	// ファイル名の長さ
	size_t fileNameLength;

	// 行番号
	int line;

	// ログレベル
	LogLevel level;

	// メッセージ
	char* message;

	// メッセージの長さ
	size_t messageLength;

	// ログ出力時刻
	SYSTEMTIME localTime;

public:
	/**
	 * コンストラクタ。	
	 */
	LogData() :
		fileName(nullptr),
		fileNameLength(0),
		line(0),
		level(LogLevel::Info),
		message(nullptr),
		messageLength(0),
		localTime({ 0 })
	{}

	/**
	 * データを消去します。
	 * 
	 * @param fileNameLen ファイル名の長さ
	 * @param messageLen メッセージの長さ
	 */
	void Clear(size_t fileNameLen, size_t messageLen);

	/**
	 * メッセージをリサイズします。 
	 */
	void ResizeMessage(size_t writeLen, size_t bkSize);
};

// データを消去します。
void LogData::Clear(size_t fileNameLen, size_t messageLen)
{
	// ファイル名を初期化
	if (!this->fileName || this->fileNameLength < fileNameLen) {
		delete this->fileName;
		this->fileNameLength = fileNameLen;
		this->fileName = new char[this->fileNameLength];
	}
	memset(this->fileName, 0, this->fileNameLength);

	// 行番号を初期化
	this->line = 0;

	// ログレベルを初期化
	this->level = LogLevel::Info;

	// メッセージを初期化
	if (!this->message || this->messageLength < messageLen) {
		delete this->message;
		this->messageLength = messageLen;
		this->message = new char[this->messageLength];
	}
	memset(this->message, 0, this->messageLength);
}

// メッセージをリサイズします。
void LogData::ResizeMessage(size_t writeLen, size_t bkSize)
{
	writeLen = ((writeLen + bkSize / 2) / bkSize + 1)* bkSize;

	delete this->message;
	this->messageLength = writeLen;
	this->message = new char[this->messageLength];
}

//----------------------------------------------------------------------
// ログパラメータ
//----------------------------------------------------------------------
class LogParameter sealed
{
public:
	// ログファイルのハンドル
	HANDLE hFile;

	// ログファイルの最大サイズ
	long maxSize;

	// ログファイルの世代数
	int genLimit;

	// クリティカルセクション
	CRITICAL_SECTION cs;

	// ログデータの元データ
	LogData* dataSource;

	// ログデータのキャッシュ
	LogData** caches;

	// キャッシュポインタ
	size_t cachePtr;

	// 待機リスト
	LogData** waitList;

	// 待機件数
	size_t waitCount;

	// ログ出力スレッドハンドル
	HANDLE hThread;

	// ログ出力スレッドの実行フラグ
	bool isRunning;

	// ログレベル
	LogLevel outLogLevel;

public:
	/**
	 * コンストラクタ。
	 *
	 * @param hFile ログファイルのハンドル
	 * @param maxFileSize ログファイルの最大サイズ
	 * @param genLimit ログファイルの世代数
	 */
	LogParameter(HANDLE hFile, long maxFileSize, int genLimit);

	/**
	 * デストラクタ。
	 */
	~LogParameter();

	/**
	 * キャッシュからデータを取得します。
	 * 
	 * @param fileNameLen ファイル名の長さ
	 * @param messageLen メッセージの長さ
	 */
	LogData* GetCache(size_t fileNameLen, size_t messageLen);
};

// コンストラクタ。
LogParameter::LogParameter(HANDLE hFile, long maxFileSize, int genLimit) :
	hFile(hFile),
	maxSize(maxFileSize),
	genLimit(genLimit),
	hThread(nullptr),
	isRunning(false),
	outLogLevel(LogLevel::Debug)
{
	// クリティカルセクションの初期化
	InitializeCriticalSection(&this->cs);

	// ログデータの実体を生成
	this->dataSource = new LogData[LOG_CACHE_SIZE];

	// キャッシュを初期化
	this->caches = new LogData*[LOG_CACHE_SIZE];
	for (size_t i = 0; i < LOG_CACHE_SIZE; ++i) {
		this->caches[i] = &this->dataSource[i];
	}
	this->cachePtr = LOG_CACHE_SIZE;

	// 待機リストを初期化
	this->waitList = new LogData*[LOG_CACHE_SIZE];
	this->waitCount = 0;
}

// デストラクタ。
LogParameter::~LogParameter()
{
	// ログ出力スレッドを終了
	//
	// 1. 待機リストが空になるまで待機
	// 2. ログ出力スレッドの終了フラグを停止
	// 3. ログ出力スレッドの終了を待機
	// 4. ログ出力スレッドのハンドルをクローズ
	if (this->hThread != nullptr) {
		// 1. 待機リストが空になるまで待機
		size_t count;
		do {
			Sleep(300);
			EnterCriticalSection(&this->cs);
			count = this->waitCount;
			LeaveCriticalSection(&this->cs);
		} while (count);

		// 2. ログ出力スレッドの終了フラグを停止
		EnterCriticalSection(&this->cs);
		this->isRunning = false;
		LeaveCriticalSection(&this->cs);

		// 3. ログ出力スレッドの終了を待機
		WaitForSingleObject(this->hThread, INFINITE);

		// 4. ログ出力スレッドのハンドルをクローズ
		CloseHandle(this->hThread);
		this->hThread = nullptr;
	}

	// ファイルハンドルをクローズ
	if (this->hFile != nullptr) {
		CloseHandle(this->hFile);
		this->hFile = nullptr;
	}

	// 待機リストの削除
	delete this->waitList;

	// キャッシュの削除
	delete this->caches;
	for (size_t i = 0; i < LOG_CACHE_SIZE; ++i) {
		delete this->dataSource[i].fileName;
		delete this->dataSource[i].message;
	}

	// ログデータの実体を削除
	delete this->dataSource;

	// クリティカルセクションの削除
	DeleteCriticalSection(&this->cs);
}

// キャッシュからデータを取得します。
LogData* LogParameter::GetCache(size_t fileNameLen, size_t messageLen)
{
	if (this->cachePtr > 0) {
		// キャッシュの実体を取得
		LogData* res = this->caches[this->cachePtr - 1];
		this->cachePtr--;

		// データを消去して返す
		res->Clear(fileNameLen, messageLen);
		return res;
	}
	else {
		// キャッシュに実体がない場合は空を返す
		return nullptr;
	}
}

//----------------------------------------------------------------------
// ログ
//----------------------------------------------------------------------
// コンストラクタ。
Logger::Logger(const char* path, LogParameter* param) :
	logPath(path), param(param)
{}

// デストラクタ。
Logger::~Logger()
{
	// ログパラメータを削除
	delete this->param;

	// ログパスを削除
	delete this->logPath;
}

// ログファイルを作成します。
static HANDLE CreateLogFile(const char* path)
{
	HANDLE hFile = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		throw LogException("ログファイルの作成に失敗しました");
	}
	SetFilePointer(hFile, 0, NULL, FILE_END);
	return hFile;
}

// ログクラスを作成します。
Logger* Logger::Create(const char* path, long maxFileSize, int genLimit)
{
	// 履歴世代の最大数チェック
	if (genLimit < 0 || genLimit > MAX_HISTRY_FILE) {
		throw LogException("ログの世代数が1～100件の範囲外です");
	}

	// ファイルを作成し末尾に移動
	HANDLE hFile = CreateLogFile(path);

	// ログパラメータを作成
	LogParameter* param = new LogParameter(hFile, maxFileSize, genLimit);

	// ログクラスを作成
	size_t plen = strnlen_s(path, MAX_PATH) + 1;
	char * bpath = new char[plen + 1];
	strcpy_s(bpath, plen, path);
	Logger * logger = new Logger(bpath, param);

	// ログ出力スレッドを作成
	logger->param->hThread = CreateThread(NULL, 0, Logger::LoggingThread, logger, 0, NULL);
	if (logger->param->hThread == nullptr) {
		throw LogException("ログ出力スレッドの作成に失敗しました");
	}
		
	return logger;
}

// ログレベルを設定します。
void Logger::SetLevel(LogLevel lv)
{
	if (lv >= LogLevel::MinLevel && lv <= LogLevel::MaxLevel) {
		this->param->outLogLevel = lv;
	}
	else {
		throw LogException("ログレベルが範囲外です");
	}
}

// ファイル名からファイル名のポインタを取得します。
static const char * SplitPath(const char * path)
{
	const char* p = path;
	for (const char* i = p; *i; ++i) {
		if (*i == '\\') {
			p = i + 1;
		}
	}
	return p;
}

// 文字列の長さを計算します（ブロックを考慮して）
static size_t CalcLength(const char * str, size_t bkSize)
{
	size_t len = 0;
	for (const char* i = str; *i; ++i) {
		len++;
	}	
	return ((len + bkSize / 2) / bkSize + 1) * bkSize;
}

// ログレベルを表す文字列を取得します。
static const char* GetLevel(LogLevel lv)
{
	switch (lv)
	{
	case Warning:
		return "Warning";
	case Info:
		return "Info";
	case Debug:
		return "Debug";
	default:
		return "Error";
	}
}

/**
 * ログファイルの履歴を収集します。
 */
class HistoryFile
{
public:
	// ログファイルのリスト
	const char (*historyFiles)[MAX_PATH + 1];

	// ログファイルの数
	DWORD historyCount;

	// 新しいログファイル
	const char * newHistoryFile;

public:
	// デフォルトコンストラクタ
	HistoryFile() :
		historyFiles(nullptr),
		historyCount(0),
		newHistoryFile(nullptr)
	{}

	// コピーコンストラクタ
	HistoryFile(const HistoryFile& src) :
		historyFiles(src.historyFiles),
		historyCount(src.historyCount),
		newHistoryFile(src.newHistoryFile)
	{}

	// 確保したメモリを解放します。
	void Delete() {
		if (this->historyFiles) {
			delete[] this->historyFiles;
			this->historyFiles = nullptr;
		}
		this->historyCount = 0;
		if (this->newHistoryFile) {
			delete[] this->newHistoryFile;
			this->newHistoryFile = nullptr;
		}
	}
};


static HistoryFile CollectFistoryFiles(const char* logPath)
{
	char patternPath[MAX_PATH + 1] = { 0 };
	char* appendFile = new char[MAX_PATH + 1];
	strcpy_s(patternPath, MAX_PATH, logPath);
	memcpy_s(appendFile, MAX_PATH + 1, logPath, MAX_PATH + 1);

	size_t filePtr = 0;
	size_t extPtr = 0;
	size_t strLen = 0;
	for (const char* i = patternPath; *i; ++i, ++strLen) {
		switch (*i)
		{
		case '\\':
			filePtr = strLen;
			break;
		case '.':
			extPtr = strLen;
			break;
		default:
			break;
		}
	}
	SYSTEMTIME lt;
	GetLocalTime(&lt);
	char time[18];
	int hlen = sprintf_s(time, sizeof(time), "%04d%02d%02d%02d%02d%02d%03d",
		lt.wYear,
		lt.wMonth,
		lt.wDay,
		lt.wHour,
		lt.wMinute,
		lt.wSecond,
		lt.wMilliseconds
	);

	memmove_s(patternPath + extPtr + 1, MAX_PATH - extPtr, patternPath + extPtr, strLen - extPtr + 1);
	patternPath[extPtr] = '*';

	memmove_s(appendFile + extPtr + 17, MAX_PATH - extPtr, appendFile + extPtr, strLen - extPtr + 1);
	for (size_t i = 0; i < 17; ++i) {
		appendFile[extPtr + i] = time[i];
	}

	WIN32_FIND_DATAA* tempFiles = new WIN32_FIND_DATAA[MAX_HISTRY_FILE];
	memset(tempFiles, 0, sizeof(WIN32_FIND_DATAA) * MAX_HISTRY_FILE);

	WIN32_FIND_DATAA win32fd;
	HANDLE hFind = FindFirstFileA(patternPath, &win32fd);
	DWORD historyCount = 0;
	do {
		tempFiles[historyCount++] = win32fd;
	} while (FindNextFileA(hFind, &win32fd));
	FindClose(hFind);

	for (size_t i = 1; i < historyCount; ++i) {
		int cmp = strcmp(tempFiles[i - 1].cFileName, tempFiles[i].cFileName);

		if (cmp > 0) {
			size_t j = i;
			WIN32_FIND_DATAA tmp = tempFiles[i];
			do {
				tempFiles[j] = tempFiles[j - 1];
				j--;
			} while (j > 0 && strcmp(tempFiles[j - 1].cFileName, tmp.cFileName));
			tempFiles[j] = tmp;
		}
	}

	char(*historyFiles)[MAX_PATH + 1] = (char(*)[MAX_PATH + 1])new char[MAX_HISTRY_FILE * (MAX_PATH + 1)];
	memset(historyFiles, 0, MAX_HISTRY_FILE * (MAX_PATH + 1));

	patternPath[filePtr] = 0;
	for (size_t i = 1; i < historyCount; ++i) {
		sprintf_s(historyFiles[i - 1], MAX_PATH, "%s\\%s", patternPath, tempFiles[i].cFileName);
	}

	HistoryFile res;
	res.historyFiles = historyFiles;
	res.historyCount = historyCount;
	res.newHistoryFile = appendFile;
	return res;
}

void Logger::Log(const char* file, int line, LogLevel lv, const char* message, ...)
{
	va_list args;
	va_start(args, message);

	// キャッシュからデータを取得
	const char* fnptr = SplitPath(file);
	LogData* data = nullptr;
	do {
		EnterCriticalSection(&this->param->cs);

		data = this->param->GetCache(
			CalcLength(fnptr, 32),
			CalcLength(message, 256)
		);
		if (data) {
			strcpy_s(data->fileName, data->fileNameLength - 1, fnptr);
			data->line = line;
			data->level = lv;
			do {
				size_t wlen = vsnprintf_s(data->message, data->messageLength, data->messageLength - 1, message, args);
				if (wlen < data->messageLength - 1) {
					break;
				}
				else {
					data->ResizeMessage(wlen, 8);
				}
			} while (true);
			GetLocalTime(&data->localTime);

			// 書き込みリストに追加
			this->param->waitList[this->param->waitCount++] = data;
		}

		LeaveCriticalSection(&this->param->cs);

		if (!data) {
			Sleep(100);
		}
	} while (!data);

	va_end(args);
}

DWORD WINAPI Logger::LoggingThread(LPVOID loggerPtr)
{
	Logger* logger = (Logger*)loggerPtr;
	logger->param->isRunning = true;

	bool running = true;
	bool writed = false;
	do {
		long size = GetFileSize(logger->param->hFile, NULL);

		if (size > logger->param->maxSize) {
			HistoryFile finded = CollectFistoryFiles(logger->logPath);
			for (int i = 0; i < (int)finded.historyCount - logger->param->genLimit; ++i) {
				DeleteFileA(finded.historyFiles[i]);
			}

			FlushFileBuffers(logger->param->hFile);
			Sleep(500);
			CloseHandle(logger->param->hFile);
			MoveFileA(logger->logPath, finded.newHistoryFile);

			logger->param->hFile = CreateLogFile(logger->logPath);

			finded.Delete();	
		}

		bool needWrite = false;

		LogData* data = nullptr;
		EnterCriticalSection(&logger->param->cs);
		if (logger->param->waitCount > 0) {
			data = logger->param->waitList[0];

			if (logger->param->waitCount > 1) {
				memmove_s(
					logger->param->waitList,
					sizeof(LogData*) * LOG_CACHE_SIZE, 
					logger->param->waitList + 1,
					sizeof(LogData*) * (logger->param->waitCount - 1)
				);
			}
			logger->param->waitCount--;
			needWrite = true;
		}
		LeaveCriticalSection(&logger->param->cs);

		if (needWrite) {
			if (data->level <= logger->param->outLogLevel) {
				char time[32];
				int hlen = sprintf_s(time, sizeof(time), "[%04d-%02d-%02d %02d:%02d:%02d.%03d ",
					data->localTime.wYear,
					data->localTime.wMonth,
					data->localTime.wDay,
					data->localTime.wHour,
					data->localTime.wMinute,
					data->localTime.wSecond,
					data->localTime.wMilliseconds
				);
				WriteFile(logger->param->hFile, time, hlen, NULL, NULL);

				size_t fln = strnlen_s(data->fileName, data->fileNameLength - 1);
				WriteFile(logger->param->hFile, data->fileName, (DWORD)fln, NULL, NULL);

				char lnstr[32];
				int llen = sprintf_s(lnstr, sizeof(lnstr), ":%d %s] ", data->line, GetLevel(data->level));
				WriteFile(logger->param->hFile, lnstr, llen, NULL, NULL);

				size_t mln = strnlen_s(data->message, data->messageLength - 1);
				WriteFile(logger->param->hFile, data->message, (DWORD)mln, NULL, NULL);

				WriteFile(logger->param->hFile, "\r\n", 2, NULL, NULL);

				writed = true;

				EnterCriticalSection(&logger->param->cs);
				logger->param->caches[logger->param->cachePtr++] = data;
				LeaveCriticalSection(&logger->param->cs);
			}
			Sleep(1);
		}
		else {
			if (writed) {
				FlushFileBuffers(logger->param->hFile);
				writed = false;
			}

			Sleep(50);

			EnterCriticalSection(&logger->param->cs);
			running = logger->param->isRunning;
			LeaveCriticalSection(&logger->param->cs);
		}

	} while (running);

	return 0;
}

//----------------------------------------------------------------------
// ログ例外
//----------------------------------------------------------------------
/**
 * コンストラクタ。
 *
 * @param msg メッセージ
 */
LogException::LogException(const char* msg, ...)
{
	va_list args;
	va_start(args, msg);

	// メッセージを生成
	char* buffer = new char[2048];
	vsprintf_s(buffer, 2048, msg, args);
	this->message = buffer;

	va_end(args);
}

/**
 * デストラクタ。
 */
LogException::~LogException()
{
	delete this->message;
	this->message = nullptr;
}

/**
 * メッセージを取得します。
 *
 * @return メッセージ
 */
const char* LogException::ToString() const
{
	return this->message;
}
