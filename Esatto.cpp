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

#ifdef PLUGIN_DEBUG
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

#if defined PLUGIN_DEBUG
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [CEsattoController] Version " << std::fixed << std::setprecision(2) << PLUGIN_VERSION << " build " << __DATE__ << " " << __TIME__ << " on "<< m_sPlatform << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [CEsattoController] Constructor Called." << std::endl;
    m_sLogFile.flush();
#endif

}

CEsattoController::~CEsattoController()
{
#ifdef    PLUGIN_DEBUG
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

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] Called." << std::endl;
    m_sLogFile.flush();
#endif

    m_bIsConnected = false;

    nErr = m_pSerx->open(pszPort, 115200, SerXInterface::B_NOPARITY);
    if(nErr)
        return nErr;

    m_bIsConnected = true;
    m_cmdDelayTimer.Reset();

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] connected to " << pszPort << std::endl;
    m_sLogFile.flush();
#endif

    // get status so we can figure out what device we are connecting to.
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] getting device status." << std::endl;
    m_sLogFile.flush();
#endif

    nErr = getModelName(sModelName);
    if(nErr) {
        m_bIsConnected = false;
#if defined PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] **** ERROR **** getting device model names : " << nErr << std::endl;
        m_sLogFile.flush();
#endif
        return nErr;
    }

    nErr = getFirmwareVersion(sDummy);

    nErr = getDeviceStatus();
    if(nErr) {
		m_bIsConnected = false;
#if defined PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] **** ERROR **** getting device status : " << nErr << std::endl;
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

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_RunSettings.runSpeed     : " << m_RunSettings.runSpeed << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_RunSettings.accSpeed     : " << m_RunSettings.accSpeed << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_RunSettings.decSpeed     : " << m_RunSettings.decSpeed << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_RunSettings.runCurrent   : " << m_RunSettings.runCurrent << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_RunSettings.accCurrent   : " << m_RunSettings.accCurrent << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_RunSettings.decCurrent   : " << m_RunSettings.decCurrent << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_RunSettings.holdCurrent  : " << m_RunSettings.holdCurrent << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] m_RunSettings.backlash     : " << m_RunSettings.backlash << std::endl;
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

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [haltFocuser] Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	jCmd["req"]["cmd"]["MOT1"]["MOT_ABORT"]="";

	nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr)
        return nErr;
	// parse output
	try {
		jResp = json::parse(sResp);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [haltFocuser] response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif

        if(jResp.at("res").at("cmd").at("MOT1").at("MOT_ABORT") == "done") {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [haltFocuser] motor has stopped." << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [haltFocuser] m_nTargetPos : " << m_nTargetPos << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [haltFocuser] m_nCurPos    : " << m_nCurPos << std::endl;
            m_sLogFile.flush();
#endif
			m_nTargetPos = m_nCurPos;
            m_bHalted = true;

        }
        else {
#if defined PLUGIN_DEBUG
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] **** ERROR **** haltFocuser failed : " << nErr << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] **** ERROR **** haltFocuser response : " << sResp << std::endl;
            m_sLogFile.flush();
#endif
			return ERR_CMDFAILED;
        }
	}
    catch (json::exception& e) {
#if defined PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [haltFocuser] json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [haltFocuser] json exception response : " << sResp << std::endl;
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

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [gotoPosition] Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
		return ERR_COMMNOLINK;


    if (m_bPosLimitEnabled && (nPos>m_nMaxPos || nPos < m_nMinPos))
        return ERR_LIMITSEXCEEDED;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [gotoPosition] goto position : " << nPos << std::endl;
    m_sLogFile.flush();
#endif

	jCmd["req"]["cmd"]["MOT1"]["GOTO"]=nPos;
    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr)
        return nErr;

	try {
		jResp = json::parse(sResp);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [haltFocuser] response : " << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif

		if(jResp.at("res").at("cmd").at("MOT1").at("GOTO") == "done") {
			m_nTargetPos = nPos;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [gotoPosition] goto m_nTargetPos : " << m_nTargetPos << std::endl;
            m_sLogFile.flush();
#endif
		}
		else {
#if defined PLUGIN_DEBUG
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] **** ERROR **** gotoPosition failed : " << nErr << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] **** ERROR **** gotoPosition response : " << sResp << std::endl;
            m_sLogFile.flush();
#endif
			m_nTargetPos = m_nCurPos;
			return ERR_CMDFAILED;
		}
	}
    catch (json::exception& e) {
#if defined PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [haltFocuser] json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [haltFocuser] json exception response : " << sResp << std::endl;
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

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [moveRelativeToPosision] Called." << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [moveRelativeToPosision] goto relative position : " << nSteps << std::endl;
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
	
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [isGoToComplete] Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	bComplete = false;

    if(m_bHalted) {
        bComplete = true;
        return nErr;
    }

    getDeviceStatus();
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [isGoToComplete] m_bMoving : " << (m_bMoving?"True":"False") << std::endl;
    m_sLogFile.flush();
#endif
	if(m_bMoving)
		return nErr;

	getDeviceStatus();
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [isGoToComplete] m_nCurPos    : " << m_nCurPos << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [isGoToComplete] m_nTargetPos : " << m_nTargetPos << std::endl;
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
    int nErr;
    std::string sResp;
	json jCmd;
	json jResp;
    bool bNeedExtraStatus = false;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getDeviceStatus] Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
		return ERR_COMMNOLINK;
	
	jCmd["req"]["get"]["MOT1"]="";

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getDeviceStatus] jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif

	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return nErr;
	// parse output
	try {
		jResp = json::parse(sResp);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getDeviceStatus] response : " << jResp.dump(2) << std::endl;
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

        if(jResp.at("res").at("get").at("MOT1").contains("STATUS")) {
            m_bMoving = (jResp.at("res").at("get").at("MOT1").at("STATUS").at("MST").get<std::string>() != "stop");
        }
        else {
            bNeedExtraStatus = true;
        }
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

        if(bNeedExtraStatus) {
            jCmd.clear();
            jCmd["req"]["get"]["MOT1"]["STATUS"]="";
            nErr = ctrlCommand(jCmd.dump(), sResp);
            if(nErr)
                return nErr;
            jResp = json::parse(sResp);
            if(jResp.at("res").at("get").at("MOT1").contains("STATUS")) {
                m_bMoving = (jResp.at("res").at("get").at("MOT1").at("STATUS").at("MST").get<std::string>() != "stop");
            }
        }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getDeviceStatus] m_nCurPos       : " << m_nCurPos << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getDeviceStatus] m_nMaxPos       : " << m_nMaxPos << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getDeviceStatus] m_nMinPos       : " << m_nMinPos << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getDeviceStatus] m_bMoving       : " << (m_bMoving?"True":"False") << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getDeviceStatus] m_nDir          : " << (m_nDir==NORMAL?"normal":"invert") << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getDeviceStatus] FnRUN_SPD       : " << m_RunSettings.runSpeed << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getDeviceStatus] FnRUN_ACC       : " << m_RunSettings.accSpeed << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getDeviceStatus] FnRUN_DEC       : " << m_RunSettings.decSpeed << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getDeviceStatus] FnRUN_CURR_SPD  : " << m_RunSettings.runCurrent << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getDeviceStatus] FnRUN_CURR_ACC  : " << m_RunSettings.accCurrent << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getDeviceStatus] FnRUN_CURR_DEC  : " << m_RunSettings.decCurrent << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getDeviceStatus] FnRUN_CURR_HOLD : " << m_RunSettings.holdCurrent << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getDeviceStatus] CAL_BKLASH      : " << m_RunSettings.backlash << std::endl;
        m_sLogFile.flush();
#endif
	}
    catch (json::exception& e) {
#if defined PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getDeviceStatus] json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getDeviceStatus] json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }
	return nErr;
}

int CEsattoController::getFirmwareVersion(std::string &sVersion)
{
	int nErr = PLUGIN_OK;
    std::string sResp;
	json jCmd;
	json jResp;


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFirmwareVersion] Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	if(m_sAppVer.size() && m_sWebVer.size()) {
        sVersion = m_sAppVer + " / " + m_sWebVer;
		return nErr;
	}

	jCmd["req"]["get"]["SWVERS"]="";

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFirmwareVersion] jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif
	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return nErr;
	// parse output
	try {
		jResp = json::parse(sResp);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFirmwareVersion] response : " << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif
		m_sAppVer = jResp.at("res").at("get").at("SWVERS").at("SWAPP").get<std::string>();
		m_sWebVer = jResp.at("res").at("get").at("SWVERS").at("SWWEB").get<std::string>();
	}
    catch (json::exception& e) {
#if defined PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFirmwareVersion] json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFirmwareVersion] json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }
    sVersion = m_sAppVer + " / " + m_sWebVer;

    m_fFirmwareVersion = std::stof(m_sAppVer);
    
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFirmwareVersion] szResp     : " << sResp << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFirmwareVersion] pszVersion : " << sVersion << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getFirmwareVersion] m_fFirmwareVersion : " << m_fFirmwareVersion << std::endl;
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

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getModelName] Called." << std::endl;
    m_sLogFile.flush();
#endif

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	if(m_sModelName.size()) {
        sModelName.assign(m_sModelName);
		return nErr;
	}

	jCmd["req"]["get"]["MODNAME"]="";
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getModelName] jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif

	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return nErr;
	// parse output
	try {
		jResp = json::parse(sResp);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getModelName] response : " << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif
		m_sModelName = jResp.at("res").at("get").at("MODNAME").get<std::string>();
	}
    catch (json::exception& e) {
#if defined PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getModelName] json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getModelName] json exception response : " << sResp << std::endl;
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

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getTemperature] Called." << std::endl;
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
	
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getTemperature] jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif

	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return nErr;
	// parse output
	try {
		jResp = json::parse(sResp);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getTemperature] response : " << jResp.dump(2) << std::endl;
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
#if defined PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getTemperature] json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getTemperature] json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }
	catch(std::invalid_argument& e){
#if defined PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getTemperature] stod invalid_argument exception : " << e.what() << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getTemperature] json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
	}
	catch(std::out_of_range& e){
#if defined PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getTemperature] stod out_of_range exception : " << e.what() << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getTemperature] json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
	}
    catch(const std::exception& e) {
#if defined PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getTemperature] conversion exception : " << e.what() << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getTemperature] json exception response : " << sResp << std::endl;
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

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getPosLimit] Called." << std::endl;
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

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPosLimit] Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;


    jCmd["req"]["set"]["MOT1"]["CAL_MINPOS"]=nMin;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPosLimit] setting new min pos to : " << nMin << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPosLimit] jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif

    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr)
        return nErr;
    // parse output
    try {
        jResp = json::parse(sResp);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPosLimit] response : " << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif
        if(jResp.at("res").at("set").at("MOT1").at("CAL_MINPOS") != "done") {
#if defined PLUGIN_DEBUG
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPosLimit] **** ERROR **** setPosLimit min failed : " << nErr << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPosLimit] **** ERROR **** setPosLimit response   : " << sResp << std::endl;
            m_sLogFile.flush();
#endif
            return ERR_CMDFAILED;
        }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPosLimit] response : " << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif
    }
    catch (json::exception& e) {
#if defined PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPosLimit] json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPosLimit] json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }
    m_nMinPos = nMin;

    jCmd.clear();
    jResp.clear();
    jCmd["req"]["set"]["MOT1"]["CAL_MAXPOS"]=nMax;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPosLimit] setting new max pos to : " << nMax << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPosLimit] jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif
    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr)
        return nErr;
    // parse output
    try {
        jResp = json::parse(sResp);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPosLimit] response : " << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif
        if(jResp.at("res").at("set").at("MOT1").at("CAL_MAXPOS") != "done") {
#if defined PLUGIN_DEBUG
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPosLimit] **** ERROR **** setPosLimit max failed   : " << nErr << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPosLimit] **** ERROR **** setPosLimit max response : " << sResp << std::endl;
            m_sLogFile.flush();
#endif
            return ERR_CMDFAILED;
        }
    }
    catch (json::exception& e) {
#if defined PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPosLimit] json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setPosLimit] json exception response : " << sResp << std::endl;
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

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setDirection] Called." << std::endl;
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

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setDirection] setting new max pos to : " << sDir << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setDirection] jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif
    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr)
        return nErr;
    // parse output
    try {
        jResp = json::parse(sResp);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setDirection] response : " << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif
        if(jResp.at("res").at("set").at("MOT1").at("CAL_DIR") != "done") {
#if defined PLUGIN_DEBUG
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setDirection] **** ERROR **** setDirection failed   : " << nErr << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setDirection] **** ERROR **** setDirection response : " << sResp << std::endl;
            m_sLogFile.flush();
#endif
            return ERR_CMDFAILED;
        }
    }
    catch (json::exception& e) {
#if defined PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setDirection] json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setDirection] json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

    return nErr;
}

int CEsattoController::getDirection(int &nDir)
{
    int nErr = PLUGIN_OK;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getDirection] Called." << std::endl;
    m_sLogFile.flush();
#endif
    nErr = getDeviceStatus();
    if(nErr)
        return nErr;

    nDir = m_nDir;
    return nErr;
}

int CEsattoController::getPosition(int &nPosition)
{
	int nErr = PLUGIN_OK;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getPosition] Called." << std::endl;
    m_sLogFile.flush();
#endif
	nErr = getDeviceStatus();
	if(nErr)
		return nErr;

	nPosition = m_nCurPos;
	return nErr;
}


int CEsattoController::getWiFiConfig(int &nMode, std::string &sSSID, std::string &sPWD)
{
    int nErr = PLUGIN_OK;
    std::string sResp;
    json jCmd;
    json jResp;
    std::string wifiMode;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getWiFiConfig] Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    // get lan mode  : {"req":{"get":{"LANCFG":""}}}
    // haven't found how to change mode
    // and only the password can be changed from my tests.
    // for now .. nMode = AP always
    nMode = AP;
    switch(nMode) {
        case AP :
            wifiMode = "WIFIAP";
            break;
        case STA :
            wifiMode = "WIFISTA";
            break;
        default:
            break;
    }
	jCmd["req"]["get"][wifiMode]="";

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getWiFiConfig] jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif
    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr)
        return nErr;
    // parse output
    try {
        jResp = json::parse(sResp);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getWiFiConfig] response : " << std::endl << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif

        sSSID = jResp.at("res").at("get").at(wifiMode).at("SSID").get<std::string>();
        sPWD = jResp.at("res").at("get").at(wifiMode).at("PWD").get<std::string>();
    }
    catch (json::exception& e) {
#if defined PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getWiFiConfig] json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getWiFiConfig] json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

    return nErr;
}

int CEsattoController::setWiFiConfig(int nMode, std::string sSSID, std::string sPWD)
{
    int nErr = PLUGIN_OK;
    std::string sResp;
    json jCmd;
    json jResp;
    std::string wifiMode;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setWiFiConfig] Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    // haven't found how to change mode
    // and only the password can be changed from my tests.
    // for now .. nMode = AP always
    nMode = AP;

    switch(nMode) {
        case AP :
            wifiMode = "WIFIAP";
            break;
        case STA :
            wifiMode = "WIFISTA";
            break;
        default:
            break;
    }


	jCmd["req"]["set"][wifiMode]["PWD"]=sPWD;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setWiFiConfig] jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif

    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr)
        return nErr;
    // parse output
    try {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setWiFiConfig] response : " << std::endl << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif
        jResp = json::parse(sResp);
    }
    catch (json::exception& e) {
#if defined PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setWiFiConfig] json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setWiFiConfig] json exception response : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return ERR_CMDFAILED;
    }

    return nErr;
}


int CEsattoController::syncMotorPosition(int nPos)
{
    int nErr = PLUGIN_OK;
    std::string sResp;
	json jCmd;
	json jResp;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [syncMotorPosition] Called." << std::endl;
    m_sLogFile.flush();
#endif

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;
        jCmd["req"]["set"]["MOT1"]["ABS_POS"]=nPos;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [syncMotorPosition] jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif
	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return nErr;
	// parse output
	try {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [syncMotorPosition] response : " << std::endl << jResp.dump(2) << std::endl;
        m_sLogFile.flush();
#endif
		jResp = json::parse(sResp);
        if(jResp.at("res").at("set").at("MOT1").at("ABS_POS") != "done") {
#if defined PLUGIN_DEBUG
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [syncMotorPosition] **** ERROR **** syncMotorPosition failed   : " << nErr << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [syncMotorPosition] **** ERROR **** syncMotorPosition response : " << sResp << std::endl;
            m_sLogFile.flush();
#endif
            return ERR_CMDFAILED;
        }
	}
	catch (json::exception& e) {
#if defined PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [syncMotorPosition] json exception : " << e.what() << " - " << e.id << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [syncMotorPosition] json exception response : " << sResp << std::endl;
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

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getMotorSettings] Called." << std::endl;
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


#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getMotorSettings] settings.runSpeed    :" << settings.runSpeed  << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getMotorSettings] settings.accSpeed    :" << settings.accSpeed  << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getMotorSettings] settings.decSpeed    :" << settings.decSpeed  << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getMotorSettings] settings.runCurrent  :" << settings.runCurrent  << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getMotorSettings] settings.accCurrent  :" << settings.accCurrent  << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getMotorSettings] settings.decCurrent  :" << settings.decCurrent  << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getMotorSettings] settings.holdCurrent :" << settings.holdCurrent  << std::endl;
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [getMotorSettings] settings.backlash    :" << settings.backlash  << std::endl;
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

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setMotorSettings] Called." << std::endl;
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

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setMotorSettings] jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif
    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr) {
#if defined PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setMotorSettings] **** ERROR **** setting motor settings failed : " << nErr << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [setMotorSettings] **** ERROR **** setMotorSettings response     : " << sResp << std::endl;
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

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [startCalibration] Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    jCmd["req"]["set"]["MOT1"]["CAL_FOCUSER"]="Init";
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [startCalibration] jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif
    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr) {
#if defined PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [startCalibration] **** ERROR **** error initializing calibartion: " << nErr << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [startCalibration] **** ERROR **** startCalibration response     : " << sResp << std::endl;
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

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [storeAsMinPosition] Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    syncMotorPosition(0); // we want the minimum position to be 0
    startCalibration();   // start calibration and save current position as minimum. The settings dialog process will ask the user to set the focuser all the way in
    jCmd["req"]["set"]["MOT1"]["CAL_FOCUSER"]="StoreAsMinPos";
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [storeAsMinPosition] jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif
    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr) {
#if defined PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [storeAsMinPosition] **** ERROR **** setting motor initial position failed : " << nErr << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [storeAsMinPosition] **** ERROR **** StoreAsMinPos response     : " << sResp << std::endl;
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

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [storeAsMaxPosition] Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    jCmd["req"]["set"]["MOT1"]["CAL_FOCUSER"]="StoreAsMaxPos";
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [storeAsMaxPosition] jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif
    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr) {
#if defined PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [storeAsMaxPosition] **** ERROR **** setting motor max position failed : " << nErr << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [storeAsMaxPosition] **** ERROR **** StoreAsMinPos response     : " << sResp << std::endl;
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

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [findMaxPos] Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    jCmd["req"]["set"]["MOT1"]["CAL_FOCUSER"]="GoOutToFindMaxPos";
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [findMaxPos] jCmd : " << jCmd.dump() << std::endl;
    m_sLogFile.flush();
#endif
    nErr = ctrlCommand(jCmd.dump(), sResp);
    if(nErr) {
#if defined PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [findMaxPos] **** ERROR **** sending motor to max position failed : " << nErr << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [findMaxPos] **** ERROR **** GoOutToFindMaxPos response     : " << sResp << std::endl;
        m_sLogFile.flush();
#endif
        return nErr;
    }
    return nErr;
}

bool CEsattoController::isFocuserMoving()
{
    getDeviceStatus();
    return m_bMoving;
}


#pragma mark command and response functions

int CEsattoController::ctrlCommand(const std::string sCmd, std::string &sResult, int nTimeout)
{
    int nErr = PLUGIN_OK;

    unsigned long  ulBytesWrite;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [ctrlCommand] Called." << std::endl;
    m_sLogFile.flush();
#endif

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    m_pSerx->purgeTxRx();
    interCommandPause();

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [ctrlCommand] Sending : " << sCmd << std::endl;
    m_sLogFile.flush();
#endif

    nErr = m_pSerx->writeFile((void *) (sCmd.c_str()) , sCmd.size(), ulBytesWrite);
    m_pSerx->flushTx();
    m_cmdDelayTimer.Reset();

    if(nErr){
#if defined PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [ctrlCommand] writeFile error  : " << nErr << std::endl;
        m_sLogFile.flush();
#endif

        return nErr;
    }

    // read response
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [ctrlCommand] Getting response." << std::endl;
    m_sLogFile.flush();
#endif

    nErr = readResponse(sResult, nTimeout);
    if(nErr){
#if defined PLUGIN_DEBUG
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [ctrlCommand] readResponse error  : " << nErr << std::endl;
        m_sLogFile.flush();
#endif
        return nErr;
    }

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [ctrlCommand] response : " << sResult << std::endl;
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

    memset(pszBuf, 0, SERIAL_BUFFER_SIZE);
    pszBufPtr = pszBuf;

    do {
        nErr = m_pSerx->bytesWaitingRx(nBytesWaiting);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] nBytesWaiting      : " << nBytesWaiting << std::endl;
        m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] nBytesWaiting nErr : " << nErr << std::endl;
        m_sLogFile.flush();
#endif
        if(!nBytesWaiting) {
            nbTimeouts += MAX_READ_WAIT_TIMEOUT;
            if(nbTimeouts >= nTimeout) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
                m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] bytesWaitingRx timeout, no data for " << nbTimeouts << " ms"<< std::endl;
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
#if defined PLUGIN_DEBUG
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] readFile error : " << nErr << std::endl;
            m_sLogFile.flush();
#endif
            return nErr;
        }

        if (ulBytesRead != nBytesWaiting) { // timeout
#if defined PLUGIN_DEBUG
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] rreadFile Timeout Error." << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] readFile nBytesWaiting : " << nBytesWaiting << std::endl;
            m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] readFile ulBytesRead   : " << ulBytesRead << std::endl;
            m_sLogFile.flush();
#endif
        }

        ulTotalBytesRead += ulBytesRead;
        pszBufPtr+=ulBytesRead;
    }  while (ulTotalBytesRead < SERIAL_BUFFER_SIZE  && *(pszBufPtr-1) != '\n');

    if(!ulTotalBytesRead)
        nErr = COMMAND_TIMEOUT; // we didn't get an answer.. so timeout
    else
        *(pszBufPtr-1) = 0; //remove the \n

    sResp.assign(pszBuf);

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
    m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] sResp : " << sResp << std::endl;
    m_sLogFile.flush();
#endif

    return nErr;
}


void CEsattoController::interCommandPause()
{
    int dDelayMs;
    // do we need to wait ?
    if(m_cmdDelayTimer.GetElapsedSeconds()<INTER_COMMAND_WAIT) {
        dDelayMs = INTER_COMMAND_WAIT - int(m_cmdDelayTimer.GetElapsedSeconds() *1000);
        if(dDelayMs>0)
            std::this_thread::sleep_for(std::chrono::milliseconds(dDelayMs)); // need to give time to the controller to process the commands
    }

}


#ifdef PLUGIN_DEBUG
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
