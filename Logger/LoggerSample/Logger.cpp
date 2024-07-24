#include "Logger.h"
#include <xstring>

#define LOG_CACHE_SIZE 256

#define MAX_HISTRY_FILE 100

//----------------------------------------------------------------------
// ���O�o�̓f�[�^
//----------------------------------------------------------------------
/**
 * ���O�o�̓f�[�^�ł��B
 */
class LogData sealed
{
public:
	// ���O�o�̓t�@�C����
	char* fileName;

	// �t�@�C�����̒���
	size_t fileNameLength;

	// �s�ԍ�
	int line;

	// ���O���x��
	LogLevel level;

	// ���b�Z�[�W
	char* message;

	// ���b�Z�[�W�̒���
	size_t messageLength;

	// ���O�o�͎���
	SYSTEMTIME localTime;

public:
	/**
	 * �R���X�g���N�^�B	
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
	 * �f�[�^���������܂��B
	 * 
	 * @param fileNameLen �t�@�C�����̒���
	 * @param messageLen ���b�Z�[�W�̒���
	 */
	void Clear(size_t fileNameLen, size_t messageLen);

	/**
	 * ���b�Z�[�W�����T�C�Y���܂��B 
	 */
	void ResizeMessage(size_t writeLen, size_t bkSize);
};

// �f�[�^���������܂��B
void LogData::Clear(size_t fileNameLen, size_t messageLen)
{
	// �t�@�C������������
	if (!this->fileName || this->fileNameLength < fileNameLen) {
		delete this->fileName;
		this->fileNameLength = fileNameLen;
		this->fileName = new char[this->fileNameLength];
	}
	memset(this->fileName, 0, this->fileNameLength);

	// �s�ԍ���������
	this->line = 0;

	// ���O���x����������
	this->level = LogLevel::Info;

	// ���b�Z�[�W��������
	if (!this->message || this->messageLength < messageLen) {
		delete this->message;
		this->messageLength = messageLen;
		this->message = new char[this->messageLength];
	}
	memset(this->message, 0, this->messageLength);
}

// ���b�Z�[�W�����T�C�Y���܂��B
void LogData::ResizeMessage(size_t writeLen, size_t bkSize)
{
	writeLen = ((writeLen + bkSize / 2) / bkSize + 1)* bkSize;

	delete this->message;
	this->messageLength = writeLen;
	this->message = new char[this->messageLength];
}

//----------------------------------------------------------------------
// ���O�p�����[�^
//----------------------------------------------------------------------
class LogParameter sealed
{
public:
	// ���O�t�@�C���̃n���h��
	HANDLE hFile;

	// ���O�t�@�C���̍ő�T�C�Y
	long maxSize;

	// ���O�t�@�C���̐��㐔
	int genLimit;

	// �N���e�B�J���Z�N�V����
	CRITICAL_SECTION cs;

	// ���O�f�[�^�̌��f�[�^
	LogData* dataSource;

	// ���O�f�[�^�̃L���b�V��
	LogData** caches;

	// �L���b�V���|�C���^
	size_t cachePtr;

	// �ҋ@���X�g
	LogData** waitList;

	// �ҋ@����
	size_t waitCount;

	// ���O�o�̓X���b�h�n���h��
	HANDLE hThread;

	// ���O�o�̓X���b�h�̎��s�t���O
	bool isRunning;

	// ���O���x��
	LogLevel outLogLevel;

public:
	/**
	 * �R���X�g���N�^�B
	 *
	 * @param hFile ���O�t�@�C���̃n���h��
	 * @param maxFileSize ���O�t�@�C���̍ő�T�C�Y
	 * @param genLimit ���O�t�@�C���̐��㐔
	 */
	LogParameter(HANDLE hFile, long maxFileSize, int genLimit);

	/**
	 * �f�X�g���N�^�B
	 */
	~LogParameter();

	/**
	 * �L���b�V������f�[�^���擾���܂��B
	 * 
	 * @param fileNameLen �t�@�C�����̒���
	 * @param messageLen ���b�Z�[�W�̒���
	 */
	LogData* GetCache(size_t fileNameLen, size_t messageLen);
};

// �R���X�g���N�^�B
LogParameter::LogParameter(HANDLE hFile, long maxFileSize, int genLimit) :
	hFile(hFile),
	maxSize(maxFileSize),
	genLimit(genLimit),
	hThread(nullptr),
	isRunning(false),
	outLogLevel(LogLevel::Debug)
{
	// �N���e�B�J���Z�N�V�����̏�����
	InitializeCriticalSection(&this->cs);

	// ���O�f�[�^�̎��̂𐶐�
	this->dataSource = new LogData[LOG_CACHE_SIZE];

	// �L���b�V����������
	this->caches = new LogData*[LOG_CACHE_SIZE];
	for (size_t i = 0; i < LOG_CACHE_SIZE; ++i) {
		this->caches[i] = &this->dataSource[i];
	}
	this->cachePtr = LOG_CACHE_SIZE;

	// �ҋ@���X�g��������
	this->waitList = new LogData*[LOG_CACHE_SIZE];
	this->waitCount = 0;
}

// �f�X�g���N�^�B
LogParameter::~LogParameter()
{
	// ���O�o�̓X���b�h���I��
	//
	// 1. �ҋ@���X�g����ɂȂ�܂őҋ@
	// 2. ���O�o�̓X���b�h�̏I���t���O���~
	// 3. ���O�o�̓X���b�h�̏I����ҋ@
	// 4. ���O�o�̓X���b�h�̃n���h�����N���[�Y
	if (this->hThread != nullptr) {
		// 1. �ҋ@���X�g����ɂȂ�܂őҋ@
		size_t count;
		do {
			Sleep(300);
			EnterCriticalSection(&this->cs);
			count = this->waitCount;
			LeaveCriticalSection(&this->cs);
		} while (count);

		// 2. ���O�o�̓X���b�h�̏I���t���O���~
		EnterCriticalSection(&this->cs);
		this->isRunning = false;
		LeaveCriticalSection(&this->cs);

		// 3. ���O�o�̓X���b�h�̏I����ҋ@
		WaitForSingleObject(this->hThread, INFINITE);

		// 4. ���O�o�̓X���b�h�̃n���h�����N���[�Y
		CloseHandle(this->hThread);
		this->hThread = nullptr;
	}

	// �t�@�C���n���h�����N���[�Y
	if (this->hFile != nullptr) {
		CloseHandle(this->hFile);
		this->hFile = nullptr;
	}

	// �ҋ@���X�g�̍폜
	delete this->waitList;

	// �L���b�V���̍폜
	delete this->caches;
	for (size_t i = 0; i < LOG_CACHE_SIZE; ++i) {
		delete this->dataSource[i].fileName;
		delete this->dataSource[i].message;
	}

	// ���O�f�[�^�̎��̂��폜
	delete this->dataSource;

	// �N���e�B�J���Z�N�V�����̍폜
	DeleteCriticalSection(&this->cs);
}

// �L���b�V������f�[�^���擾���܂��B
LogData* LogParameter::GetCache(size_t fileNameLen, size_t messageLen)
{
	if (this->cachePtr > 0) {
		// �L���b�V���̎��̂��擾
		LogData* res = this->caches[this->cachePtr - 1];
		this->cachePtr--;

		// �f�[�^���������ĕԂ�
		res->Clear(fileNameLen, messageLen);
		return res;
	}
	else {
		// �L���b�V���Ɏ��̂��Ȃ��ꍇ�͋��Ԃ�
		return nullptr;
	}
}

//----------------------------------------------------------------------
// ���O
//----------------------------------------------------------------------
// �R���X�g���N�^�B
Logger::Logger(const char* path, LogParameter* param) :
	logPath(path), param(param)
{}

// �f�X�g���N�^�B
Logger::~Logger()
{
	// ���O�p�����[�^���폜
	delete this->param;

	// ���O�p�X���폜
	delete this->logPath;
}

// ���O�t�@�C�����쐬���܂��B
static HANDLE CreateLogFile(const char* path)
{
	HANDLE hFile = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		throw LogException("���O�t�@�C���̍쐬�Ɏ��s���܂���");
	}
	SetFilePointer(hFile, 0, NULL, FILE_END);
	return hFile;
}

// ���O�N���X���쐬���܂��B
Logger* Logger::Create(const char* path, long maxFileSize, int genLimit)
{
	// ���𐢑�̍ő吔�`�F�b�N
	if (genLimit < 0 || genLimit > MAX_HISTRY_FILE) {
		throw LogException("���O�̐��㐔��1�`100���͈̔͊O�ł�");
	}

	// �t�@�C�����쐬�������Ɉړ�
	HANDLE hFile = CreateLogFile(path);

	// ���O�p�����[�^���쐬
	LogParameter* param = new LogParameter(hFile, maxFileSize, genLimit);

	// ���O�N���X���쐬
	size_t plen = strnlen_s(path, MAX_PATH) + 1;
	char * bpath = new char[plen + 1];
	strcpy_s(bpath, plen, path);
	Logger * logger = new Logger(bpath, param);

	// ���O�o�̓X���b�h���쐬
	logger->param->hThread = CreateThread(NULL, 0, Logger::LoggingThread, logger, 0, NULL);
	if (logger->param->hThread == nullptr) {
		throw LogException("���O�o�̓X���b�h�̍쐬�Ɏ��s���܂���");
	}
		
	return logger;
}

// ���O���x����ݒ肵�܂��B
void Logger::SetLevel(LogLevel lv)
{
	if (lv >= LogLevel::MinLevel && lv <= LogLevel::MaxLevel) {
		this->param->outLogLevel = lv;
	}
	else {
		throw LogException("���O���x�����͈͊O�ł�");
	}
}

// �t�@�C��������t�@�C�����̃|�C���^���擾���܂��B
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

// ������̒������v�Z���܂��i�u���b�N���l�����āj
static size_t CalcLength(const char * str, size_t bkSize)
{
	size_t len = 0;
	for (const char* i = str; *i; ++i) {
		len++;
	}	
	return ((len + bkSize / 2) / bkSize + 1) * bkSize;
}

// ���O���x����\����������擾���܂��B
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
 * ���O�t�@�C���̗��������W���܂��B
 */
class HistoryFile
{
public:
	// ���O�t�@�C���̃��X�g
	const char (*historyFiles)[MAX_PATH + 1];

	// ���O�t�@�C���̐�
	DWORD historyCount;

	// �V�������O�t�@�C��
	const char * newHistoryFile;

public:
	// �f�t�H���g�R���X�g���N�^
	HistoryFile() :
		historyFiles(nullptr),
		historyCount(0),
		newHistoryFile(nullptr)
	{}

	// �R�s�[�R���X�g���N�^
	HistoryFile(const HistoryFile& src) :
		historyFiles(src.historyFiles),
		historyCount(src.historyCount),
		newHistoryFile(src.newHistoryFile)
	{}

	// �m�ۂ�����������������܂��B
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

	// �L���b�V������f�[�^���擾
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

			// �������݃��X�g�ɒǉ�
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
// ���O��O
//----------------------------------------------------------------------
/**
 * �R���X�g���N�^�B
 *
 * @param msg ���b�Z�[�W
 */
LogException::LogException(const char* msg, ...)
{
	va_list args;
	va_start(args, msg);

	// ���b�Z�[�W�𐶐�
	char* buffer = new char[2048];
	vsprintf_s(buffer, 2048, msg, args);
	this->message = buffer;

	va_end(args);
}

/**
 * �f�X�g���N�^�B
 */
LogException::~LogException()
{
	delete this->message;
	this->message = nullptr;
}

/**
 * ���b�Z�[�W���擾���܂��B
 *
 * @return ���b�Z�[�W
 */
const char* LogException::ToString() const
{
	return this->message;
}
