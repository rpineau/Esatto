//
//  Esatto.cpp
//  PimaLuce Lan Esatto focuser X2 plugin
//
//  Created by Rodolphe Pineau on 10/11/2019.


#include "Esatto.h"

CEsattoController::CEsattoController()
{
    m_pSerx = NULL;
    m_bDebugLog = false;
    m_bIsConnected = false;

    m_nCurPos = 0;
    m_nTargetPos = 0;
    m_nMaxPos = 0;
	m_nMinPos = 0;
    m_bPosLimitEnabled = 0;
    m_bMoving = false;
    m_bHalted = false;

	m_sAppVer.clear();
	m_sWebVer.clear();
	m_sModelName.clear();
    m_nModel = ESATTO;

#ifdef ESATTO_PLUGIN_DEBUG
#if defined(SB_WIN_BUILD)
    m_sLogfilePath = getenv("HOMEDRIVE");
    m_sLogfilePath += getenv("HOMEPATH");
    m_sLogfilePath += "\\EsattoLog.txt";
    m_sPlatform = "Windows";
#elif defined(SB_LINUX_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/EsattoLog.txt";
    m_sPlatform = "Linux";
#elif defined(SB_MAC_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/EsattoLog.txt";
    m_sPlatform = "macOS";
#endif
    m_sLogFile.open(m_sLogfilePath, std::ios::out |std::ios::trunc);
#endif

#if defined ESATTO_PLUGIN_DEBUG
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Version " << std::fixed << std::setprecision(2) << ESATTO_PLUGIN_VERSION << " build " << __DATE__ << " " << __TIME__ << " on "<< m_sPlatform << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Constructor Called." << std::endl;
    m_sLogFile.flush();
#endif
	m_StatusTimer.Reset();
}

CEsattoController::~CEsattoController()
{
#ifdef    ESATTO_PLUGIN_DEBUG
    // Close LogFile
    if(m_sLogFile.is_open())
        m_sLogFile.close();
#endif
}

int CEsattoController::Connect(const char *pszPort)
{
    int nErr = PLUGIN_OK;
    std::string sModelName;
    std::string sDummy;

    if(!m_pSerx)
        return ERR_COMMNOLINK;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Called." << std::endl;
    m_sLogFile.flush();
#endif

    m_bIsConnected = false;

    nErr = m_pSerx->open(pszPort, 115200, SerXInterface::B_NOPARITY);
    if(nErr)
        return nErr;

    m_bIsConnected = true;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] connected to " << pszPort << std::endl;
    m_sLogFile.flush();
#endif

    // get status so we can figure out what device we are connecting to.
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] getting device status." << std::endl;
    m_sLogFile.flush();
#endif

    nErr = getModelName(sModelName);
    if(nErr) {
        m_bIsConnected = false;
#if defined ESATTO_PLUGIN_DEBUG
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] **** ERROR **** getting device model names : " << nErr << std::endl;
        m_sLogFile.flush();
#endif
        return nErr;
    }

    nErr = getFirmwareVersion(sDummy);

    nErr = getDeviceStatus();
    if(nErr) {
		m_bIsConnected = false;
#if defined ESATTO_PLUGIN_DEBUG
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] **** ERROR **** getting device status : " << nErr << std::endl;
        m_sLogFile.flush();
#endif

        return nErr;
    }
    if(m_nMaxPos == 0) {
        setPosLimit(0, 1000000);
    }

    // find what settings we're on
    MotorSettings tmpSettings;
    getMotorSettings(tmpSettings);

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_RunSettings.runSpeed     : " << m_RunSettings.runSpeed << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_RunSettings.accSpeed     : " << m_RunSettings.accSpeed << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_RunSettings.decSpeed     : " << m_RunSettings.decSpeed << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_RunSettings.runCurrent   : " << m_RunSettings.runCurrent << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_RunSettings.accCurrent   : " << m_RunSettings.accCurrent << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_RunSettings.decCurrent   : " << m_RunSettings.decCurrent << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_RunSettings.holdCurrent  : " << m_RunSettings.holdCurrent << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_RunSettings.backlash     : " << m_RunSettings.backlash << std::endl;
    m_sLogFile.flush();
#endif

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Done." << std::endl;
	m_sLogFile.flush();
#endif

	return nErr;
}

void CEsattoController::Disconnect()
{
    if(m_bIsConnected && m_pSerx)
        m_pSerx->close();
 
	m_bIsConnected = false;
}

#pragma mark move commands
int CEsattoController::haltFocuser()
{
    int nErr;
    std::string sResp;
	json jCmd;
	json jResp;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	jCmd["req"]["cmd"]["MOT1"]["MOT_ABORT"]="";

	nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr)
        return nErr;
	if(!sResp.length()) {
		return ERR_CMDFAILED;
	}
	// parse output
	try {
		jResp = json::parse(sResp);
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif

        if(jResp.at("res").at("cmd").at("MOT1").at("MOT_ABORT") == "done") {
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] motor has stopped." << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_nTargetPos : " << m_nTargetPos << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_nCurPos    : " << m_nCurPos << std::endl;
            m_sLogFile.flush();
#endif
			m_nTargetPos = m_nCurPos;
            m_bHalted = true;

        }
        else {
#if defined ESATTO_PLUGIN_DEBUG
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] **** ERROR **** haltFocuser failed : " << nErr << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] **** ERROR **** haltFocuser response : " << sResp << std::endl;
            m_sLogFile.flush();
#endif
			return ERR_CMDFAILED;
        }
	}
    catch (json::exception& e) {
#if defined ESATTO_PLUGIN_DEBUG
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception : " << e.what() << " - " << e.id << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

    return nErr;
}

int CEsattoController::gotoPosition(int nPos)
{
    int nErr;
    std::string sResp;
	json jCmd;
	json jResp;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
		return ERR_COMMNOLINK;


    if (m_bPosLimitEnabled && (nPos>m_nMaxPos || nPos < m_nMinPos))
        return ERR_LIMITSEXCEEDED;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] goto position : " << nPos << std::endl;
    m_sLogFile.flush();
#endif
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_StatusTimer.GetElapsedSeconds() : " << m_StatusTimer.GetElapsedSeconds() << std::endl;
	m_sLogFile.flush();
#endif


	jCmd["req"]["cmd"]["MOT1"]["MOVE_ABS"]["STEP"]=nPos;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_StatusTimer.GetElapsedSeconds() : " << m_StatusTimer.GetElapsedSeconds() << std::endl;
	m_sLogFile.flush();
#endif
	nErr = ctrlCommand(jCmd.dump(), sResp);
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_StatusTimer.GetElapsedSeconds() : " << m_StatusTimer.GetElapsedSeconds() << std::endl;
	m_sLogFile.flush();
#endif

	if(nErr)
        return nErr;

	if(!sResp.length()) {
		return ERR_CMDFAILED;
	}

	try {
		jResp = json::parse(sResp);
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] response : " << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif

		if(jResp.at("res").at("cmd").at("MOT1").at("STEP") == "done") {
			m_nTargetPos = nPos;
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] goto m_nTargetPos : " << m_nTargetPos << std::endl;
            m_sLogFile.flush();
#endif
		}
		else {
#if defined ESATTO_PLUGIN_DEBUG
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] **** ERROR **** gotoPosition failed : " << nErr << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] **** ERROR **** gotoPosition response : " << sResp << std::endl;
            m_sLogFile.flush();
#endif
			m_nTargetPos = m_nCurPos;
			return ERR_CMDFAILED;
		}
	}
    catch (json::exception& e) {
#if defined ESATTO_PLUGIN_DEBUG
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception : " << e.what() << " - " << e.id << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

    m_bHalted = false;
    return nErr;
}

int CEsattoController::moveRelativeToPosision(int nSteps)
{
    int nErr;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Called." << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] goto relative position : " << nSteps << std::endl;
    m_sLogFile.flush();
#endif

    m_nTargetPos = m_nCurPos + nSteps;
    nErr = gotoPosition(m_nTargetPos);
    return nErr;
}

#pragma mark command complete functions

int CEsattoController::isGoToComplete(bool &bComplete)
{
    int nErr = PLUGIN_OK;
	
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	bComplete = false;

    if(m_bHalted) {
        bComplete = true;
        return nErr;
    }

	isFocuserMoving();
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_bMoving : " << (m_bMoving?"True":"False") << std::endl;
    m_sLogFile.flush();
#endif
	if(m_bMoving)
		return nErr;

	getPosition(m_nCurPos);
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_nCurPos    : " << m_nCurPos << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_nTargetPos : " << m_nTargetPos << std::endl;
    m_sLogFile.flush();
#endif

    if(m_nCurPos == m_nTargetPos)
        bComplete = true;
	else {
		// not at the proper position yet.
        bComplete = false;
 	}
    return nErr;
}

#pragma mark getters and setters
int CEsattoController::getDeviceStatus()
{
	int nErr = PLUGIN_OK;
    std::string sResp;
	json jCmd;
	json jResp;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	if(m_StatusTimer.GetElapsedSeconds()<1)
		return nErr;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_StatusTimer.GetElapsedSeconds() : " << m_StatusTimer.GetElapsedSeconds() << std::endl;
	m_sLogFile.flush();
#endif

	m_StatusTimer.Reset();

	jCmd["req"]["get"]["MOT1"]="";

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif

	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return nErr;
	if(!sResp.length()) {
		return ERR_CMDFAILED;
	}

	// parse output
	try {
		jResp = json::parse(sResp);
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] response : " << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif

        if(jResp.at("res").at("get").at("MOT1").contains("ABS_POS")) {
            m_nCurPos = jResp.at("res").at("get").at("MOT1").at("ABS_POS").get<int>();
        }
        else if(jResp.at("res").at("get").at("MOT1").contains("ABS_POS_STEP")) {
            m_nCurPos = jResp.at("res").at("get").at("MOT1").at("ABS_POS_STEP").get<int>();
        }
        else if(jResp.at("res").at("get").at("MOT1").contains("POSITION_STEP")) {
            m_nCurPos = jResp.at("res").at("get").at("MOT1").at("POSITION_STEP").get<int>();
        }
        else if(jResp.at("res").at("get").at("MOT1").contains("POSITION")) {
            m_nCurPos = jResp.at("res").at("get").at("MOT1").at("POSITION").get<int>();
        }

		m_nMaxPos = jResp.at("res").at("get").at("MOT1").at("CAL_MAXPOS").get<int>();
		m_nMinPos = jResp.at("res").at("get").at("MOT1").at("CAL_MINPOS").get<int>();

        if(jResp.at("res").at("get").at("MOT1").contains("CAL_DIR")) {
            if(jResp.at("res").at("get").at("MOT1").at("CAL_DIR").get<std::string>().find("normal")!=-1)
                m_nDir = NORMAL;
            else if(jResp.at("res").at("get").at("MOT1").at("CAL_DIR").get<std::string>().find("invert")!=-1)
                m_nDir = INVERT;
            else
                m_nDir = NORMAL; // just in case.
        }
        else
            m_nDir = NORMAL; // just in case.


        if(m_nModel == SESTO) {
            m_RunSettings.runSpeed = jResp.at("res").at("get").at("MOT1").at("FnRUN_SPD").get<int>();
            m_RunSettings.accSpeed = jResp.at("res").at("get").at("MOT1").at("FnRUN_ACC").get<int>();
            m_RunSettings.decSpeed = jResp.at("res").at("get").at("MOT1").at("FnRUN_DEC").get<int>();
            m_RunSettings.runCurrent = jResp.at("res").at("get").at("MOT1").at("FnRUN_CURR_SPD").get<int>();
            m_RunSettings.accCurrent = jResp.at("res").at("get").at("MOT1").at("FnRUN_CURR_ACC").get<int>();
            m_RunSettings.decCurrent = jResp.at("res").at("get").at("MOT1").at("FnRUN_CURR_DEC").get<int>();
            m_RunSettings.holdCurrent = jResp.at("res").at("get").at("MOT1").at("FnRUN_CURR_HOLD").get<int>();
        }

        if(jResp.at("res").at("get").at("MOT1").contains("CAL_BKLASH")) {
            m_RunSettings.backlash = jResp.at("res").at("get").at("MOT1").at("CAL_BKLASH").get<int>();
        }
        else
            m_RunSettings.backlash = 0;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  m_nCurPos       : " << m_nCurPos << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  m_nMaxPos       : " << m_nMaxPos << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  m_nMinPos       : " << m_nMinPos << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  m_nDir          : " << (m_nDir==NORMAL?"normal":"invert") << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  FnRUN_SPD       : " << m_RunSettings.runSpeed << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  FnRUN_ACC       : " << m_RunSettings.accSpeed << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  FnRUN_DEC       : " << m_RunSettings.decSpeed << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  FnRUN_CURR_SPD  : " << m_RunSettings.runCurrent << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  FnRUN_CURR_ACC  : " << m_RunSettings.accCurrent << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  FnRUN_CURR_DEC  : " << m_RunSettings.decCurrent << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  FnRUN_CURR_HOLD : " << m_RunSettings.holdCurrent << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  CAL_BKLASH      : " << m_RunSettings.backlash << std::endl;
        m_sLogFile.flush();
#endif
	}
    catch (json::exception& e) {
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  Done." << std::endl;
	m_sLogFile.flush();
#endif

	return nErr;
}

int CEsattoController::getFirmwareVersion(std::string &sVersion)
{
	int nErr = PLUGIN_OK;
    std::string sResp;
	json jCmd;
	json jResp;


#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	if(m_sAppVer.size() && m_sWebVer.size()) {
        sVersion = m_sAppVer + " / " + m_sWebVer;
		return nErr;
	}

	jCmd["req"]["get"]["SWVERS"]="";

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif
	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return nErr;
	if(!sResp.length()) {
		return ERR_CMDFAILED;
	}
	// parse output
	try {
		jResp = json::parse(sResp);
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  response : " << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif
		m_sAppVer = jResp.at("res").at("get").at("SWVERS").at("SWAPP").get<std::string>();
		m_sWebVer = jResp.at("res").at("get").at("SWVERS").at("SWWEB").get<std::string>();
	}
    catch (json::exception& e) {
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }
    sVersion = m_sAppVer + " / " + m_sWebVer;

    m_fFirmwareVersion = std::stof(m_sAppVer);
    
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  szResp     : " << sResp << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  pszVersion : " << sVersion << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  m_fFirmwareVersion : " << m_fFirmwareVersion << std::endl;
    m_sLogFile.flush();
#endif
	return nErr;
}


int CEsattoController::getModelName(std::string &sModelName)
{
	int nErr = PLUGIN_OK;
    std::string sResp;
	json jCmd;
	json jResp;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  Called." << std::endl;
    m_sLogFile.flush();
#endif

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	if(m_sModelName.size()) {
        sModelName.assign(m_sModelName);
		return nErr;
	}

	jCmd["req"]["get"]["MODNAME"]="";
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif

	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return nErr;
	if(!sResp.length()) {
		return ERR_CMDFAILED;
	}
	// parse output
	try {
		jResp = json::parse(sResp);
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  response : " << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif
		m_sModelName = jResp.at("res").at("get").at("MODNAME").get<std::string>();
	}
    catch (json::exception& e) {
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

    sModelName.assign(m_sModelName);
    if(m_sModelName.find("ESATTO") !=-1)
        m_nModel = ESATTO;
    else if(m_sModelName.find("SESTO") !=-1)
        m_nModel = SESTO;
    else
        m_nModel = ESATTO;
	return nErr;
}

int CEsattoController::getModel()
{
    return m_nModel;
}

int CEsattoController::getTemperature(double &dTemperature, int nTempProbe)
{
    int nErr = PLUGIN_OK;
    std::string sResp;
	json jCmd;
	json jResp;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    switch (nTempProbe) {
        case EXT_T:
            jCmd["req"]["get"]["EXT_T"]="";
            break;
        case NTC_T:
            jCmd["req"]["get"]["MOT1"]["NTC_T"]="";
            break;
        default:
            jCmd["req"]["get"]["MOT1"]["NTC_T"]="";
            break;
    }
	
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif

	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return nErr;
	if(!sResp.length()) {
		return ERR_CMDFAILED;
	}
	// parse output
	try {
		jResp = json::parse(sResp);
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  response : " << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif
        switch (nTempProbe) {
            case EXT_T:
                dTemperature = std::stod(jResp.at("res").at("get").at("EXT_T").get<std::string>());
                break;
            case NTC_T:
                dTemperature = std::stod(jResp.at("res").at("get").at("MOT1").at("NTC_T").get<std::string>());
                break;
            default:
                dTemperature = std::stod(jResp.at("res").at("get").at("MOT1").at("NTC_T").get<std::string>());
                break;
        }

	}
    catch (json::exception& e) {
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }
	catch(std::invalid_argument& e){
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  stod invalid_argument exception : " << e.what() << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
	}
	catch(std::out_of_range& e){
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  stod out_of_range exception : " << e.what() << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
	}
    catch(const std::exception& e) {
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  conversion exception : " << e.what() << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }
	return nErr;
}

int CEsattoController::getPosLimit(int &nMin, int &nMax)
{
    int nErr = PLUGIN_OK;
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  Called." << std::endl;
    m_sLogFile.flush();
#endif
	getDeviceStatus();
	nMin = m_nMinPos;
	nMax = m_nMaxPos;

    return nErr;
}

int CEsattoController::setPosLimit(int nMin, int nMax)
{
    int nErr = PLUGIN_OK;
    std::string sResp;
    json jCmd;
    json jResp;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;


    jCmd["req"]["set"]["MOT1"]["CAL_MINPOS"]=nMin;
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  setting new min pos to : " << nMin << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif

    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr)
        return nErr;
	if(!sResp.length()) {
		return ERR_CMDFAILED;
	}
    // parse output
    try {
        jResp = json::parse(sResp);
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  response : " << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif
        if(jResp.at("res").at("set").at("MOT1").at("CAL_MINPOS") != "done") {
#if defined ESATTO_PLUGIN_DEBUG
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  **** ERROR **** setPosLimit min failed : " << nErr << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  **** ERROR **** setPosLimit response   : " << sResp << std::endl;
            m_sLogFile.flush();
#endif
            return ERR_CMDFAILED;
        }

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  response : " << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif
    }
    catch (json::exception& e) {
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }
    m_nMinPos = nMin;

    jCmd.clear();
    jResp.clear();
    jCmd["req"]["set"]["MOT1"]["CAL_MAXPOS"]=nMax;
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  setting new max pos to : " << nMax << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif
    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr)
        return nErr;
	if(!sResp.length()) {
		return ERR_CMDFAILED;
	}
    // parse output
    try {
        jResp = json::parse(sResp);
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  response : " << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif
        if(jResp.at("res").at("set").at("MOT1").at("CAL_MAXPOS") != "done") {
#if defined ESATTO_PLUGIN_DEBUG
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  **** ERROR **** setPosLimit max failed   : " << nErr << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  **** ERROR **** setPosLimit max response : " << sResp << std::endl;
            m_sLogFile.flush();
#endif
            return ERR_CMDFAILED;
        }
    }
    catch (json::exception& e) {
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

    m_nMaxPos = nMax;
    return nErr;
}

int CEsattoController::setDirection(int nDir)
{
    int nErr = PLUGIN_OK;
    std::string sResp;
    json jCmd;
    json jResp;
    std::string sDir;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    switch (nDir) {
        case NORMAL:
            sDir="normal";
            break;
        case INVERT:
            sDir="invert";
            break;
        default:
            sDir="normal";
            break;
    }

    jCmd["req"]["set"]["MOT1"]["CAL_DIR"]=sDir;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  setting new max pos to : " << sDir << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif
    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr)
        return nErr;
	if(!sResp.length()) {
		return ERR_CMDFAILED;
	}
    // parse output
    try {
        jResp = json::parse(sResp);
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  response : " << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif
        if(jResp.at("res").at("set").at("MOT1").at("CAL_DIR") != "done") {
#if defined ESATTO_PLUGIN_DEBUG
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  **** ERROR **** setDirection failed   : " << nErr << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  **** ERROR **** setDirection response : " << sResp << std::endl;
            m_sLogFile.flush();
#endif
            return ERR_CMDFAILED;
        }
    }
    catch (json::exception& e) {
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

    return nErr;
}

int CEsattoController::getDirection(int &nDir)
{
    int nErr = PLUGIN_OK;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  Called." << std::endl;
    m_sLogFile.flush();
#endif
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = getDeviceStatus();
    if(nErr)
        return nErr;

    nDir = m_nDir;
    return nErr;
}

int CEsattoController::getPosition(int &nPosition)
{
	int nErr = PLUGIN_OK;
	std::string sResp;
	json jCmd;
	json jResp;

	if(!m_bIsConnected) {
		nPosition = 0;
		return ERR_COMMNOLINK;
	}
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  Called." << std::endl;
    m_sLogFile.flush();
#endif

	jCmd["req"]["get"]["MOT1"]["POSITION_STEP"]="";
	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return false;
	if(!sResp.length()) {
		return ERR_CMDFAILED;
	}
	jResp = json::parse(sResp);
	if(jResp.at("res").at("get").at("MOT1").contains("POSITION_STEP")) {
		m_nCurPos = jResp.at("res").at("get").at("MOT1").at("POSITION_STEP").get<int>();
	}

	nPosition = m_nCurPos;
	return nErr;
}

int CEsattoController::isWifiEnabled(bool &bEnabled)
{
    int nErr = PLUGIN_OK;
    std::string sResp;
    json jCmd;
    json jResp;
    std::string sLanStatus;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    jCmd["req"]["get"]["LANSTATUS"]="";
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif
    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr)
        return nErr;
    // parse output
    try {
        jResp = json::parse(sResp);
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  response : " << std::endl << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif

        sLanStatus = jResp.at("res").at("get").at("LANCFG").get<std::string>();
    }
    catch (json::exception& e) {
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

    if(sLanStatus.find("disconnected")!=-1)
        bEnabled = false;
    else
        bEnabled = true;

#if defined ESATTO_PLUGIN_DEBUG
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  sLanStatus  : " << sLanStatus << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  bEnabled   : " << (bEnabled?"Yes":"No") << std::endl;
    m_sLogFile.flush();
#endif

    return nErr;
}

int CEsattoController::enableWiFi(bool bEnable)
{
    int nErr = PLUGIN_OK;
    std::string sResp;
    json jCmd;
    json jResp;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    jCmd["req"]["cmd"]["AP_SET_STATUS"]= (bEnable?"on":"off");

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif

    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr)
        return nErr;
    // parse output
    try {
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  response : " << std::endl << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif
        jResp = json::parse(sResp);
    }
    catch (json::exception& e) {
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

    return nErr;
}


int CEsattoController::getWiFiConfig(int &nMode, std::string &sSSID_AP, std::string &sPWD_AP, std::string &sSSID_STA, std::string &sPWD_STA)
{
    int nErr = PLUGIN_OK;
    std::string sResp;
    json jCmd;
    json jResp;
    std::string wifiMode;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    jCmd["req"]["get"]["LANCFG"]="";
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif
    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr)
        return nErr;
    // parse output
    try {
        jResp = json::parse(sResp);
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  response : " << std::endl << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif

        wifiMode = jResp.at("res").at("get").at("LANCFG").get<std::string>();
    }
    catch (json::exception& e) {
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }


    if(wifiMode.find("ap+sta")!=-1){
        nMode = AP_STA;
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  nMode AP_STA"<< std::endl;
        m_sLogFile.flush();
#endif
    }
    else if(wifiMode.find("ap")!=-1){
        nMode = AP;
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  nMode AP"<< std::endl;
        m_sLogFile.flush();
#endif
    }
    else if(wifiMode.find("sta")!=-1){
        nMode = STA;
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  nMode STA"<< std::endl;
        m_sLogFile.flush();
#endif
    }

    jCmd.clear();
	jCmd["req"]["get"]["WIFIAP"]="";

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif
    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr)
        return nErr;
    // parse output
    try {
        jResp = json::parse(sResp);
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  response : " << std::endl << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif

        sSSID_AP= jResp.at("res").at("get").at("WIFIAP").at("SSID").get<std::string>();
        sPWD_AP = jResp.at("res").at("get").at("WIFIAP").at("PWD").get<std::string>();
    }
    catch (json::exception& e) {
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

    jCmd.clear();
    jCmd["req"]["get"]["WIFISTA"]="";

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif
    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr)
        return nErr;
    // parse output
    try {
        jResp = json::parse(sResp);
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  response : " << std::endl << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif

        sSSID_STA= jResp.at("res").at("get").at("WIFISTA").at("SSID").get<std::string>();
        sPWD_STA = jResp.at("res").at("get").at("WIFISTA").at("PWD").get<std::string>();
    }
    catch (json::exception& e) {
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

#if defined ESATTO_PLUGIN_DEBUG
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  sSSID_AP  : " << sSSID_AP << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  sPWD_AP   : " << sPWD_AP << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  sSSID_STA : " << sSSID_STA << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  sPWD_STA  : " << sPWD_STA << std::endl;
    m_sLogFile.flush();
#endif

    return nErr;
}

int CEsattoController::setWiFiConfig(int nMode, std::string sSSID_AP, std::string sPWD_AP, std::string sSSID_STA, std::string sPWD_STA)
{
    int nErr = PLUGIN_OK;
    std::string sResp;
    json jCmd;
    json jResp;
    std::string wifiMode;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    if(nMode == AP || nMode == AP_STA) {
        // not sure we can change the SSID
        jCmd["req"]["set"]["WIFIAP"]["PWD"]=sPWD_AP;
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] jCmd : " << jCmd.dump() << std::endl;
        m_sLogFile.flush();
#endif

        nErr = ctrlCommand(jCmd.dump(), sResp);
        if(nErr)
            return nErr;
        // parse output
        try {
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] response : " << std::endl << jResp.dump(2) << std::endl;
            m_sLogFile.flush();
#endif
            jResp = json::parse(sResp);
        }
        catch (json::exception& e) {
#if defined ESATTO_PLUGIN_DEBUG
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception : " << e.what() << " - " << e.id << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception response : " << sResp << std::endl;
            m_sLogFile.flush();
#endif
            return ERR_CMDFAILED;
        }
    }

    if(nMode == STA || nMode == AP_STA) {
        jCmd["req"]["set"]["WIFISTA"]["PWD"]=sPWD_STA;
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] jCmd : " << jCmd.dump() << std::endl;
        m_sLogFile.flush();
#endif

        nErr = ctrlCommand(jCmd.dump(), sResp);
        if(nErr)
            return nErr;
        // parse output
        try {
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] response : " << std::endl << jResp.dump(2) << std::endl;
            m_sLogFile.flush();
#endif
            jResp = json::parse(sResp);
        }
        catch (json::exception& e) {
#if defined ESATTO_PLUGIN_DEBUG
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception : " << e.what() << " - " << e.id << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception response : " << sResp << std::endl;
            m_sLogFile.flush();
#endif
            return ERR_CMDFAILED;
        }
    }

    return nErr;
}

int CEsattoController::getSTAIpConfig(std::string &IpAddress, std::string &subnetMask, std::string &gatewayIpAddress, std::string &dnsIpAdress)
{
    int nErr = PLUGIN_OK;
    std::string sResp;
    json jCmd;
    json jResp;

    jCmd["req"]["get"]["WIFISTA"]="";

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif
    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr)
        return nErr;
    // parse output
    try {
        jResp = json::parse(sResp);
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  response : " << std::endl << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif

        IpAddress= jResp.at("res").at("get").at("WIFISTA").at("IP").get<std::string>();
        subnetMask = jResp.at("res").at("get").at("WIFISTA").at("NM").get<std::string>();
        gatewayIpAddress = jResp.at("res").at("get").at("WIFISTA").at("GW").get<std::string>();
        dnsIpAdress = jResp.at("res").at("get").at("WIFISTA").at("DNS").get<std::string>();
    }
    catch (json::exception& e) {
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

#if defined ESATTO_PLUGIN_DEBUG
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  IpAddress        : " << IpAddress << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  subnetMask       : " << subnetMask << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  gatewayIpAddress : " << gatewayIpAddress << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  dnsIpAdress      : " << dnsIpAdress << std::endl;
    m_sLogFile.flush();
#endif

    return nErr;
}

int CEsattoController::setSTAIpConfig(std::string IpAddress, std::string subnetMask, std::string gatewayIpAddress, std::string dnsIpAdress)
{
    int nErr = PLUGIN_OK;
    std::string sResp;
    json jCmd;
    json jResp;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    jCmd["req"]["set"]["WIFISTA"]["IP"]=IpAddress;
    jCmd["req"]["set"]["WIFISTA"]["NM"]=subnetMask;
    jCmd["req"]["set"]["WIFISTA"]["GW"]=gatewayIpAddress;
    jCmd["req"]["set"]["WIFISTA"]["DNS"]=dnsIpAdress;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif
    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr) {
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  **** ERROR **** setting Station network settings failed : " << nErr << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  **** ERROR **** response     : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return nErr;
    }

    return nErr;
}


int CEsattoController::syncMotorPosition(int nPos)
{
    int nErr = PLUGIN_OK;
    std::string sResp;
	json jCmd;
	json jResp;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  Called." << std::endl;
    m_sLogFile.flush();
#endif

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    jCmd["req"]["set"]["MOT1"]["ABS_POS"]=nPos;
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif
	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return nErr;
	// parse output
	try {
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  response : " << std::endl << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif
		jResp = json::parse(sResp);
        if(jResp.at("res").at("set").at("MOT1").at("ABS_POS") != "done") {
#if defined ESATTO_PLUGIN_DEBUG
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  **** ERROR **** syncMotorPosition failed   : " << nErr << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  **** ERROR **** syncMotorPosition response : " << sResp << std::endl;
            m_sLogFile.flush();
#endif
            return ERR_CMDFAILED;
        }
	}
	catch (json::exception& e) {
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}

	m_nCurPos = nPos;
    return nErr;
}



int CEsattoController::getMotorSettings(MotorSettings &settings)
{
    int nErr = PLUGIN_OK;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    nErr = getDeviceStatus();
    if(nErr)
        return nErr;

    settings.runSpeed =  m_RunSettings.runSpeed;
    settings.accSpeed =  m_RunSettings.accSpeed;
    settings.decSpeed =  m_RunSettings.decSpeed;
    settings.runCurrent =  m_RunSettings.runCurrent;
    settings.accCurrent =  m_RunSettings.accCurrent;
    settings.decCurrent =  m_RunSettings.decCurrent;
    settings.holdCurrent =  m_RunSettings.holdCurrent;
    settings.backlash =  m_RunSettings.backlash;


#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  settings.runSpeed    :" << settings.runSpeed  << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  settings.accSpeed    :" << settings.accSpeed  << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  settings.decSpeed    :" << settings.decSpeed  << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  settings.runCurrent  :" << settings.runCurrent  << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  settings.accCurrent  :" << settings.accCurrent  << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  settings.decCurrent  :" << settings.decCurrent  << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  settings.holdCurrent :" << settings.holdCurrent  << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  settings.backlash    :" << settings.backlash  << std::endl;
    m_sLogFile.flush();
#endif

    return nErr;
}

int CEsattoController::setMotorSettings(MotorSettings &settings)
{
    int nErr = PLUGIN_OK;
    std::string sPreset;
    std::string sResp;
    json jCmd;
    json jResp;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    jCmd["req"]["set"]["MOT1"]["FnRUN_SPD"]=settings.runSpeed;
    jCmd["req"]["set"]["MOT1"]["FnRUN_ACC"]=settings.accSpeed;
    jCmd["req"]["set"]["MOT1"]["FnRUN_DEC"]=settings.decSpeed;
    jCmd["req"]["set"]["MOT1"]["FnRUN_CURR_SPD"]=settings.runCurrent;
    jCmd["req"]["set"]["MOT1"]["FnRUN_CURR_ACC"]=settings.accCurrent;
    jCmd["req"]["set"]["MOT1"]["FnRUN_CURR_DEC"]=settings.decCurrent;
    jCmd["req"]["set"]["MOT1"]["FnRUN_CURR_HOLD"]=settings.holdCurrent;
    jCmd["req"]["set"]["MOT1"]["CAL_BKLASH"]=settings.backlash;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif
    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr) {
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  **** ERROR **** setting motor settings failed : " << nErr << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  **** ERROR **** setMotorSettings response     : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return nErr;
    }
    m_RunSettings.runSpeed =  settings.runSpeed;
    m_RunSettings.accSpeed =  settings.accSpeed;
    m_RunSettings.decSpeed =  settings.decSpeed;
    m_RunSettings.runCurrent =  settings.runCurrent;
    m_RunSettings.accCurrent =  settings.accCurrent;
    m_RunSettings.decCurrent =  settings.decCurrent;
    m_RunSettings.holdCurrent =  settings.holdCurrent;
    m_RunSettings.backlash =  settings.backlash;
    return nErr;
}


int CEsattoController::startCalibration()
{
    int nErr = PLUGIN_OK;
    std::string sResp;
    json jCmd;
    json jResp;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    jCmd["req"]["set"]["MOT1"]["CAL_FOCUSER"]="Init";
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif
    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr) {
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  **** ERROR **** error initializing calibartion: " << nErr << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  **** ERROR **** startCalibration response     : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return nErr;
    }

    return nErr;
}

int CEsattoController::storeAsMinPosition()
{
    int nErr = PLUGIN_OK;
    std::string sResp;
    json jCmd;
    json jResp;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    syncMotorPosition(0); // we want the minimum position to be 0
    startCalibration();   // start calibration and save current position as minimum. The settings dialog process will ask the user to set the focuser all the way in
    jCmd["req"]["set"]["MOT1"]["CAL_FOCUSER"]="StoreAsMinPos";
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif
    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr) {
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  **** ERROR **** setting motor initial position failed : " << nErr << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  **** ERROR **** StoreAsMinPos response     : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return nErr;
    }

    return nErr;
}

int CEsattoController::storeAsMaxPosition()
{
    int nErr = PLUGIN_OK;
    std::string sResp;
    json jCmd;
    json jResp;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    jCmd["req"]["set"]["MOT1"]["CAL_FOCUSER"]="StoreAsMaxPos";
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif
    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr) {
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  **** ERROR **** setting motor max position failed : " << nErr << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  **** ERROR **** StoreAsMinPos response     : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return nErr;
    }

    return nErr;
}

int CEsattoController::findMaxPos()
{
    int nErr = PLUGIN_OK;
    std::string sResp;
    json jCmd;
    json jResp;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    jCmd["req"]["set"]["MOT1"]["CAL_FOCUSER"]="GoOutToFindMaxPos";
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif
    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr) {
#if defined ESATTO_PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  **** ERROR **** sending motor to max position failed : " << nErr << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  **** ERROR **** GoOutToFindMaxPos response     : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return nErr;
    }
    return nErr;
}

bool CEsattoController::isFocuserMoving()
{
	int nErr = PLUGIN_OK;
	std::string sResp;
	json jCmd;
	json jResp;

	jCmd["req"]["get"]["MOT1"]["STATUS"]="";
	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return false;
	jResp = json::parse(sResp);
	if(jResp.at("res").at("get").at("MOT1").contains("STATUS")) {
		m_bMoving = (jResp.at("res").at("get").at("MOT1").at("STATUS").at("MST").get<std::string>() != "stop");
	}

	return m_bMoving;
}


int CEsattoController::setLeds(std::string sLedState)
{
	int nErr = PLUGIN_OK;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;
	std::string sResp;
	json jCmd;
	json jResp;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Called." << std::endl;
	m_sLogFile.flush();
#endif
	std::transform(sLedState.begin(), sLedState.end(), sLedState.begin(),  [](unsigned char c) -> unsigned char { return std::tolower(c); });
	jCmd["req"]["cmd"]["DIMLEDS"]=sLedState;
	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return false;
	if(!sResp.length()) {
		return ERR_CMDFAILED;
	}
	try {
		jResp = json::parse(sResp);
		if(jResp.at("res").at("cmd").at("cmd") != "done") {
			return ERR_CMDFAILED;
		}
	}
	catch (json::exception& e) {
#if defined ESATTO_PLUGIN_DEBUG
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception : " << e.what() << " - " << e.id << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception response : " << sResp << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}
	return nErr;
}

int CEsattoController::getLeds(std::string &sLedState)
{
	int nErr = PLUGIN_OK;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;
	std::string sResp;
	json jCmd;
	json jResp;

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Called." << std::endl;
	m_sLogFile.flush();
#endif
	jCmd["req"]["get"]["DIMLEDS"]="";
	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return false;
	if(!sResp.length()) {
		return ERR_CMDFAILED;
	}
	try {
		jResp = json::parse(sResp);
		if(jResp.at("res").at("get").contains("DIMLEDS")) {
			sLedState.assign(jResp.at("res").at("get").at("DIMLEDS").get<std::string>());
		}
	}
	catch (json::exception& e) {
#if defined ESATTO_PLUGIN_DEBUG
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception : " << e.what() << " - " << e.id << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception response : " << sResp << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}

#if defined ESATTO_PLUGIN_DEBUG
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Leds : " << sLedState << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
}



#pragma mark command and response functions

int CEsattoController::ctrlCommand(const std::string sCmd, std::string &sResult, int nTimeout)
{
    int nErr = PLUGIN_OK;
    unsigned long  ulBytesWrite;
	int nBytesWaiting = 0 ;
	std::size_t startPos;
	std::size_t endPos;
	std::string sLocalResult;
	int nbTries = 0;
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  Called." << std::endl;
    m_sLogFile.flush();
#endif
	sResult.clear();

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;
	nErr = m_pSerx->bytesWaitingRx(nBytesWaiting);
	if(nBytesWaiting) {
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  calling purgeTxR"<< std::endl;
		m_sLogFile.flush();
#endif
		m_pSerx->purgeTxRx();
	}


#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  Sending : " << sCmd << std::endl;
	m_sLogFile.flush();
#endif

	nErr = m_pSerx->writeFile((void *) (sCmd.c_str()) , sCmd.length(), ulBytesWrite);
	m_pSerx->flushTx();
	if(nErr){
#if defined ESATTO_PLUGIN_DEBUG
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  writeFile error  : " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}

	do {
		// read response
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  Getting response." << std::endl;
		m_sLogFile.flush();
#endif

		nErr = readResponse(sLocalResult, nTimeout);
		if(nErr){
#if defined ESATTO_PLUGIN_DEBUG
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  readResponse error  : " << nErr << std::endl;
			m_sLogFile.flush();
#endif
			return nErr;
		}

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  response : " << sLocalResult << std::endl;
		m_sLogFile.flush();
#endif
		if(sLocalResult.find("{") != std::string::npos)
			break;
		nbTries++;
	} while (nbTries < 5);

	//cleanup response
	if(sLocalResult.find("{") != std::string::npos) {
		startPos = sLocalResult.find_first_of("{");
		endPos = sLocalResult.find_last_of("}");
		sResult.assign(sLocalResult.substr(startPos,endPos+1));

	}

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  final response : " << sResult << std::endl;
	m_sLogFile.flush();
#endif

	return nErr;
}


int CEsattoController::readResponse(std::string &sResp, int nTimeout)
{
    int nErr = PLUGIN_OK;
    char pszBuf[SERIAL_BUFFER_SIZE];
    unsigned long ulBytesRead = 0;
    unsigned long ulTotalBytesRead = 0;
    char *pszBufPtr;
    int nBytesWaiting = 0 ;
    int nbTimeouts = 0;

#ifdef ESATTO_PLUGIN_DEBUG
	std::string hexBuff;
#endif

    memset(pszBuf, 0, SERIAL_BUFFER_SIZE);
    pszBufPtr = pszBuf;

    do {
        nErr = m_pSerx->bytesWaitingRx(nBytesWaiting);
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 3
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  nBytesWaiting      : " << nBytesWaiting << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  nBytesWaiting nErr : " << nErr << std::endl;
        m_sLogFile.flush();
#endif
        if(!nBytesWaiting) {
            nbTimeouts += MAX_READ_WAIT_TIMEOUT;
            if(nbTimeouts >= nTimeout) {
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 3
                m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  bytesWaitingRx timeout, no data for " << nbTimeouts << " ms"<< std::endl;
				hexdump(pszBuf, int(ulBytesRead), hexBuff);
				m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  hexdump  " << hexBuff << std::endl;
				m_sLogFile.flush();
#endif
                nErr = COMMAND_TIMEOUT;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(MAX_READ_WAIT_TIMEOUT));
            continue;
        }
        nbTimeouts = 0;
        if(ulTotalBytesRead + nBytesWaiting <= SERIAL_BUFFER_SIZE)
            nErr = m_pSerx->readFile(pszBufPtr, nBytesWaiting, ulBytesRead, nTimeout);
        else {
            nErr = ERR_RXTIMEOUT;
            break; // buffer is full.. there is a problem !!
        }
        if(nErr) {
#if defined ESATTO_PLUGIN_DEBUG
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  readFile error : " << nErr << std::endl;
            m_sLogFile.flush();
#endif
            return nErr;
        }
#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 3
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  readFile nBytesWaiting : " << nBytesWaiting << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  readFile ulBytesRead   : " << ulBytesRead << std::endl;
		m_sLogFile.flush();
#endif

        if (ulBytesRead != nBytesWaiting) { // timeout
#if defined ESATTO_PLUGIN_DEBUG
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  rreadFile Timeout Error." << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  readFile nBytesWaiting : " << nBytesWaiting << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  readFile ulBytesRead   : " << ulBytesRead << std::endl;
            m_sLogFile.flush();
#endif
        }

        ulTotalBytesRead += ulBytesRead;
        pszBufPtr+=ulBytesRead;
    }  while (ulTotalBytesRead < SERIAL_BUFFER_SIZE  && *(pszBufPtr-1) != '\n');

	if(!ulTotalBytesRead) {
		nErr = COMMAND_TIMEOUT; // we didn't get an answer.. so timeout
	}
	else {
		if( *(pszBufPtr-1) == '\n')
			*(pszBufPtr-1) = 0; //remove the \n
		sResp.assign(pszBuf);
	}

#if defined ESATTO_PLUGIN_DEBUG && ESATTO_PLUGIN_DEBUG >= 3
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  sResp : " << sResp << std::endl;
    m_sLogFile.flush();
#endif

    return nErr;
}

#ifdef ESATTO_PLUGIN_DEBUG
void CEsattoController::log(std::string sLogString)
{
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] " << sLogString << std::endl;
	m_sLogFile.flush();

}


void  CEsattoController::hexdump( char *inputData, int inputSize,  std::string &outHex)
{
	int idx=0;
	std::stringstream ssTmp;

	outHex.clear();
	for(idx=0; idx<inputSize; idx++){
		if((idx%16) == 0 && idx>0)
			ssTmp << std::endl;
		ssTmp << "0x" << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << (int)inputData[idx] <<" ";
	}
	outHex.assign(ssTmp.str());
}

const std::string CEsattoController::getTimeStamp()
{
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    std::strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}
#endif
