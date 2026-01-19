//
//  Esatto.h
//  PimaLuce Lan Esatto focuser X2 plugin
//
//  Created by Rodolphe Pineau on 10/11/2019.


#ifndef __ESATTO_PLUGIN__
#define __ESATTO_PLUGIN__
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
#include <chrono>
#include <thread>
#include <exception>
#include <typeinfo>
#include <stdexcept>
#include <chrono>
#include <mutex>
#include <iomanip>
#include <fstream>


#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/serxinterface.h"
#include "StopWatch.h"

#include "json.hpp"
using json = nlohmann::json;

// #define ESATTO_PLUGIN_DEBUG 2
#define ESATTO_PLUGIN_VERSION      1.51


#define SERIAL_BUFFER_SIZE 8192
#define MAX_TIMEOUT 250
#define MAX_READ_WAIT_TIMEOUT 25

#define INTER_COMMAND_WAIT    100

#define LOG_BUFFER_SIZE 4096

#define NB_MOTOR_SETTINGS 4

enum PLUGIN_Errors  {PLUGIN_OK = 0, NOT_CONNECTED, PLUGIN_CANT_CONNECT, PLUGIN_BAD_CMD_RESPONSE, COMMAND_FAILED, COMMAND_TIMEOUT};
enum MotorStatus    {IDLE = 0, MOVING};
enum WiFiModes      {AP=0, STA, AP_STA};
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
    int backlash;
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

    // move commands
    int         haltFocuser();
    int         gotoPosition(int nPos);
    int         moveRelativeToPosision(int nSteps);

    // command complete functions
    int         isGoToComplete(bool &bComplete);

    // getter and setter
    void        setDebugLog(bool bEnable) {m_bDebugLog = bEnable; };

    int         getDeviceStatus();

    int         getFirmwareVersion(std::string &sVersion);
	int         getModelName(std::string &sModelName);
    int         getModel();
    int         getTemperature(double &dTemperature, int nTempProbe);
    int         getPosition(int &nPosition);
    int         syncMotorPosition(int nPos);

    int         getPosLimit(int &nMin, int &nMax);
    int         setPosLimit(int nMin, int nMax);

    int         getDirection(int &nDir);
    int         setDirection(int nDir);

    int         isWifiEnabled(bool &bEnabled);
    int         enableWiFi(bool bEnable);
    int         getWiFiConfig(int &nMode, std::string &sSSID_AP, std::string &sPWD_AP, std::string &sSSID_STA, std::string &sPWD_STA);
    int         setWiFiConfig(int nMode, std::string sSSID_AP, std::string sPWD_AP, std::string sSSID_STA, std::string sPWD_STA);
    int         getSTAIpConfig(std::string &IpAddress, std::string &subnetMask, std::string &gatewayIpAddress, std::string &dnsIpAdress);
    int         setSTAIpConfig(std::string IpAddress, std::string subnetMask, std::string gatewayIpAddress, std::string dnsIpAdress);

    int         getMotorSettings(MotorSettings &settings);
    int         setMotorSettings(MotorSettings &settings);

    int         startCalibration();
    int         storeAsMinPosition();
    int         storeAsMaxPosition();
    int         findMaxPos();
    bool        isFocuserMoving();

	int         setLeds(std::string sLedState);
	int         getLeds(std::string &sLedState);


protected:

    int             ctrlCommand(const std::string sCmd, std::string &sResult, int nTimeout = MAX_TIMEOUT);
    int             readResponse(std::string &sResp, int nTimeout = MAX_TIMEOUT);
    void            interCommandPause();

    SerXInterface   *m_pSerx;

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
    float           m_fFirmwareVersion;

	CStopWatch		m_StatusTimer;

#ifdef ESATTO_PLUGIN_DEBUG
	void  hexdump( char *inputData, int inputSize,  std::string &outHex);

	// timestamp for logs
    const std::string getTimeStamp();
    std::ofstream m_sLogFile;
    std::string m_sPlatform;
    std::string m_sLogfilePath;
#endif


};

#endif //__ESATTO_PLUGIN__
