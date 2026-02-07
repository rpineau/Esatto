//
//  Arco.h
//  Created by Rodolphe Pineau on 2020/11/26.
//  Pegasus Arco Rotator X2 plugin
//

#ifndef __ARCO__
#define __ARCO__
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
#include <algorithm>

#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/serxinterface.h"
#include "StopWatch.h"

#include "json.hpp"
using json = nlohmann::json;

// #define ARCO_PLUGIN_DEBUG 2

#define ARCO_PLUGIN_VERSION	1.00

#define SERIAL_BUFFER_SIZE 8192
#define MAX_TIMEOUT 250
#define MAX_READ_WAIT_TIMEOUT 25

#define INTER_COMMAND_WAIT    100

#define LOG_BUFFER_SIZE 4096


enum PLUGIN_Rotator_Errors  {R_PLUGIN_OK = 0, R_NOT_CONNECTED, R_PLUGIN_CANT_CONNECT, R_PLUGIN_BAD_CMD_RESPONSE, R_COMMAND_FAILED, R_COMMAND_TIMEOUT};
enum RotorStatus    {R_IDLE = 0, R_MOVING};
enum RotorDir       {R_NORMAL = 0 , R_REVERSE};

class CArcoRotator
{
public:
    CArcoRotator();
    ~CArcoRotator();

    int         Connect(const char *pszPort);
    void        Disconnect(void);
    bool        IsConnected(void) { return m_bIsConnected; };

    void        SetSerxPointer(SerXInterface *p) { m_pSerx = p; };

	bool		isAcroPresent();
    // move commands
    int         haltArco();
    int         gotoPosition(double dPosDeg);

    // command complete functions
    int         isGoToComplete(bool &bComplete);
    int         isMotorMoving(bool &bMoving);

    // getter and setter
	int         getFirmwareVersion(std::string &sVersion);
	int         getModelName(std::string &sModelName);

	int         getPosition(double &dPosition);
    int         syncMotorPosition(double dPos);

    int         setReverseEnable(bool bEnabled);
    int         getReverseEnable(bool &bEnabled);

	int         setHemisphere(std::string sHemisphere);
	int         getHemisphere(std::string &sHemisphere);

	int			startCalibration();
	int			stopCalibration();
	int			isCalibrationDone(bool &getbDone);
#ifdef ARCO_PLUGIN_DEBUG
	void 		log(std::string sLogString);
#endif

protected:
	int             ctrlCommand(const std::string sCmd, std::string &sResult, int nTimeout = MAX_TIMEOUT);
	int             readResponse(std::string &sResp, int nTimeout = MAX_TIMEOUT);

	SerXInterface   *m_pSerx;

    bool            m_bIsConnected;
	bool			m_bArcoPresent;

	std::string		m_sAppVer;
	std::string		m_sWebVer;
	std::string		m_sModelName;
	float           m_fFirmwareVersion;

	CStopWatch		m_StatusTimer;

    double          m_dTargetPos;
	double			m_dCurPos;
	bool			m_bAborted;
	

#ifdef ARCO_PLUGIN_DEBUG
	void  hexdump( char *inputData, int inputSize,  std::string &outHex);

	// timestamp for logs
	const std::string getTimeStamp();
	std::ofstream m_sLogFile;
	std::string m_sPlatform;
	std::string m_sLogfilePath;
#endif

};

#endif //__ARCO__
