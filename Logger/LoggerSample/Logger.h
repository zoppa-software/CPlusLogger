#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <Windows.h>

/**
 * ���O�̃��x���ł��B  
 */
enum LogLevel
{
	/** ���O���x���̍ŏ��l�B */
	MinLevel = 0,

	/** �G���[���x���B */
	Error = MinLevel,

	/** �x�����x���B */
	Warning,

	/** �ʏ탌�x���B */
	Info,

	/** �f�o�b�O���x���B */
	Debug,

	/** ���O���x���̍ő�l�B */
	MaxLevel = Debug
};

/**
 * ���O�̃p�����[�^�ł��B  
 */
class LogParameter;

/**
 * ���O���o�͂��܂��B  
 */
class Logger sealed
{
private:
	// ���O�t�@�C���̃p�X
	const char * logPath;

	// ���O�t�@�C���̃p�����[�^
	LogParameter * param;

private:
	/**
	 * �R���X�g���N�^�B
	 */
	Logger(const char* path, LogParameter* param);

public:
	/**
	 * �f�X�g���N�^�B
	 */
	~Logger();

public:
	/**
	 * ���O�N���X���쐬���܂��B  
	 * 
	 * @param path ���O�t�@�C���̃p�X
	 * @param maxFileSize ���O�t�@�C���̍ő�T�C�Y
	 * @param genLimit ���O�t�@�C���̐��㐔
	 * @return ���O�N���X
	 */
	static Logger* Create(const char * path, long maxFileSize, int genLimit);

	/**
	 * ���O���x����ݒ肵�܂��B  
	 * 
	 * @param lv ���O���x��
	 */
	void SetLevel(LogLevel lv);

	/**
	 * ���O���o�͂��܂��B  
	 * 
	 * @param file �t�@�C����
	 * @param line �s�ԍ�
	 * @param lv ���O���x��
	 * @param message ���b�Z�[�W
	 */
	void Log(const char * file, int line, LogLevel lv, const char* message, ...);

private:
	/**
	 * ���O�o�͏����ł��i�ʃX���b�h�j 
	 */
	static DWORD WINAPI LoggingThread(LPVOID loggerPtr);
};

// �G���[���O���o�͂��܂��B
#define LOG_ERROR(logger, message, ...) logger->Log(__FILE__, __LINE__, LogLevel::Error, message, __VA_ARGS__)

// �x�����O���o�͂��܂��B
#define LOG_WARNING(logger, message, ...) logger->Log(__FILE__, __LINE__, LogLevel::Warning, message, __VA_ARGS__)

// �ʏ탍�O���o�͂��܂��B
#define LOG_INFO(logger, message, ...) logger->Log(__FILE__, __LINE__, LogLevel::Info, message, __VA_ARGS__)

// �f�o�b�O���O���o�͂��܂��B
#define LOG_DEBUG(logger, message, ...) logger->Log(__FILE__, __LINE__, LogLevel::Debug, message, __VA_ARGS__)

/**
 * ���O�̗�O�ł��B  
 */
class LogException sealed
{
private:
	// ���b�Z�[�W
	const char* message;

public:
	/**
	 * �R���X�g���N�^�B 
	 * 
	 * @param msg ���b�Z�[�W
	 */
	LogException(const char* msg, ...);
	
	/**
	 * �f�X�g���N�^�B
	 */
	~LogException();

public:
	/**
	 * ���b�Z�[�W���擾���܂��B  
	 * 
	 * @return ���b�Z�[�W
	 */
	const char* ToString() const;
};

#endif // !__LOGGER_H__

