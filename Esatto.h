//
//  Esatto.h
//  PimaLuce Lan Esatto focuser X2 plugin
//
//  Created by Rodolphe Pineau on 10/11/2019.


#ifndef __PLUGIN__
#define __PLUGIN__
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <memory.h>
#include <string.h>
#ifdef SB_MAC_BUILD
#include <unistd.h>
#endif
#ifdef SB_WIN_BUILD
#include <time.h>
#endif

#include <math.h>
#include <string.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

#include <exception>
#include <typeinfo>
#include <stdexcept>

#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/serxinterface.h"
#include "../../licensedinterfaces/loggerinterface.h"
#include "../../licensedinterfaces/sleeperinterface.h"

#include "json.hpp"
using json = nlohmann::json;

#define PLUGIN_DEBUG 2

#ifdef PLUGIN_DEBUG
#if defined(SB_WIN_BUILD)
#define PLUGIN_LOGFILENAME "C:\\EsattoLog.txt"
#elif defined(SB_LINUX_BUILD)
#define PLUGIN_LOGFILENAME "/tmp/EsattoLog.txt"
#elif defined(SB_MAC_BUILD)
#define PLUGIN_LOGFILENAME "/tmp/EsattoLog.txt"
#endif
#endif

#define DRIVER_VERSION      1.0


#define SERIAL_BUFFER_SIZE 4096
#define MAX_TIMEOUT 1000
#define LOG_BUFFER_SIZE 4096

enum PLUGIN_Errors    {PLUGIN_OK = 0, NOT_CONNECTED, ND_CANT_CONNECT, PLUGIN_BAD_CMD_RESPONSE, COMMAND_FAILED};
enum MotorStatus    {IDLE = 0, MOVING};


class CEsattoController
{
public:
    CEsattoController();
    ~CEsattoController();

    int         Connect(const char *pszPort);
    void        Disconnect(void);
    bool        IsConnected(void) { return m_bIsConnected; };

    void        SetSerxPointer(SerXInterface *p) { m_pSerx = p; };
    void        setSleeper(SleeperInterface *pSleeper) { m_pSleeper = pSleeper; };

    // move commands
    int         haltFocuser();
    int         gotoPosition(int nPos);
    int         moveRelativeToPosision(int nSteps);

    // command complete functions
    int         isGoToComplete(bool &bComplete);

    // getter and setter
    void        setDebugLog(bool bEnable) {m_bDebugLog = bEnable; };

    int         getDeviceStatus();

    int         getFirmwareVersion(char *pszVersion, int nStrMaxLen);
	int         getModelName(char *pszModelName, int nStrMaxLen);
    int         getTemperature(double &dTemperature);
    int         getPosition(int &nPosition);
    int         syncMotorPosition(int nPos);
    int         getPosLimit(int &nMin, int &nMax);

    int         getWiFiConfig(int &nMode, std::string &sSSID, std::string &sPWD);
    int         setWiFiConfig(int nMode, std::string sSSID, std::string sPWD);

protected:

	int             ctrlCommand(const std::string sCmd, char *pszResult, int nResultMaxLen);
    int             readResponse(char *pszRespBuffer, int nBufferLen);

    SerXInterface   *m_pSerx;
    SleeperInterface    *m_pSleeper;

    bool            m_bDebugLog;
    bool            m_bIsConnected;

    int             m_nCurPos;
    int             m_nTargetPos;
	int				m_nMaxPos;
	int				m_nMinPos;
	double			m_dExtTemp;

	bool            m_bPosLimitEnabled;
    bool            m_bMoving;
	std::string		m_sAppVer;
	std::string		m_sWebVer;
	std::string		m_sModelName;

#ifdef PLUGIN_DEBUG
	std::string m_sLogfilePath;
	// timestamp for logs
	char *timestamp;
	time_t ltime;
	FILE *Logfile;	  // LogFile
#endif

};

#endif //__PLUGIN__
