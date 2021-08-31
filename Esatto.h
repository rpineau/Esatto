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

// #define PLUGIN_DEBUG 2
#define DRIVER_VERSION      1.35


#define SERIAL_BUFFER_SIZE 8192
#define MAX_TIMEOUT 1000
#define MAX_READ_WAIT_TIMEOUT 25
#define NB_RX_WAIT 30

#define LOG_BUFFER_SIZE 4096

#define NB_MOTOR_SETTINGS 4

enum PLUGIN_Errors  {PLUGIN_OK = 0, NOT_CONNECTED, ND_CANT_CONNECT, PLUGIN_BAD_CMD_RESPONSE, COMMAND_FAILED, COMMAND_TIMEOUT};
enum MotorStatus    {IDLE = 0, MOVING};
enum WiFiModes      {AP=0, STA};
enum TempProbe      {EXT_T = 0, NTC_T};
enum Models         {ESATTO = 0, SESTO};
enum MotorDir       {NORMAL=0, INVERT};

typedef struct motorSettings {
    int runSpeed;
    int accSpeed;
    int decSpeed;
    int runCurrent;
    int accCurrent;
    int decCurrent;
    int holdCurrent;
} MotorSettings;


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
    int         getModel();
    int         getTemperature(double &dTemperature, int nTempProbe);
    int         getPosition(int &nPosition);
    int         syncMotorPosition(int nPos);

    int         getPosLimit(int &nMin, int &nMax);
    int         setPosLimit(int nMin, int nMax);

    int         getDirection(int &nDir);
    int         setDirection(int nDir);

    int         getWiFiConfig(int &nMode, std::string &sSSID, std::string &sPWD);
    int         setWiFiConfig(int nMode, std::string sSSID, std::string sPWD);

    int         getMotorSettings(MotorSettings &settings);
    int         setMotorSettings(MotorSettings &settings);
protected:

	int             ctrlCommand(const std::string sCmd, char *pszResult, int nResultMaxLen);
    int             readResponse(char *respBuffer, int nBufferLen, int nTimeout = MAX_TIMEOUT);
    SerXInterface   *m_pSerx;
    SleeperInterface    *m_pSleeper;

    bool            m_bDebugLog;
    bool            m_bIsConnected;

    int             m_nCurPos;
    int             m_nTargetPos;
	int				m_nMaxPos;
	int				m_nMinPos;
	double			m_dExtTemp;
    int             m_nDir;

	bool            m_bPosLimitEnabled;
    bool            m_bMoving;
    bool            m_bHalted;

	std::string		m_sAppVer;
	std::string		m_sWebVer;
	std::string		m_sModelName;
    int             m_nModel;

    MotorSettings   m_RunSettings;

#ifdef PLUGIN_DEBUG
	std::string m_sLogfilePath;
	// timestamp for logs
	char *timestamp;
	time_t ltime;
	FILE *Logfile;	  // LogFile
#endif

};

#endif //__PLUGIN__
