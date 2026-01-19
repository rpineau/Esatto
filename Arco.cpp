//
//  falcon.cpp
//  Created by Rodolphe Pineau on 2020/11/26.
//  Pegasus Falcon Rotator X2 plugin
//


#include "Arco.h"


CArcoRotator::CArcoRotator()
{
    m_dTargetPos = 0;
    m_bAborted = false;
    
    m_pSerx = NULL;


#ifdef ARCO_PLUGIN_DEBUG
#if defined(SB_WIN_BUILD)
	m_sLogfilePath = getenv("HOMEDRIVE");
	m_sLogfilePath += getenv("HOMEPATH");
	m_sLogfilePath += "\\ArcoLog.txt";
	m_sPlatform = "Windows";
#elif defined(SB_LINUX_BUILD)
	m_sLogfilePath = getenv("HOME");
	m_sLogfilePath += "/ArcoLog.txt";
	m_sPlatform = "Linux";
#elif defined(SB_MAC_BUILD)
	m_sLogfilePath = getenv("HOME");
	m_sLogfilePath += "/ArcoLog.txt";
	m_sPlatform = "macOS";
#endif
	m_sLogFile.open(m_sLogfilePath, std::ios::out |std::ios::trunc);
#endif

#if defined ARCO_PLUGIN_DEBUG
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Version " << std::fixed << std::setprecision(2) << ARCO_PLUGIN_VERSION << " build " << __DATE__ << " " << __TIME__ << " on "<< m_sPlatform << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Constructor Called." << std::endl;
	m_sLogFile.flush();
#endif
	m_StatusTimer.Reset();
}

CArcoRotator::~CArcoRotator()
{
#if defined ARCO_PLUGIN_DEBUG
	// Close LogFile
	if(m_sLogFile.is_open())
		m_sLogFile.close();
#endif
}

int CArcoRotator::Connect(const char *pszPort)
{
	int nErr = R_PLUGIN_OK;
	std::string sModelName;
	std::string sDummy;

	if(!m_pSerx)
		return ERR_COMMNOLINK;

#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Called." << std::endl;
	m_sLogFile.flush();
#endif

	m_bIsConnected = false;

	nErr = m_pSerx->open(pszPort, 115200, SerXInterface::B_NOPARITY);
	if(nErr)
		return nErr;

	m_bIsConnected = true;

#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  connected to " << pszPort << std::endl;
	m_sLogFile.flush();
#endif

	// get status so we can figure out what device we are connecting to.
#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  getting device status." << std::endl;
	m_sLogFile.flush();
#endif
	if(!isAcroPresent())
		return ERR_NORESPONSE;

	nErr = getModelName(sModelName);
	if(nErr) {
		m_bIsConnected = false;
#if defined ARCO_PLUGIN_DEBUG
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  **** ERROR **** getting device model names : " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}

	nErr = getFirmwareVersion(sDummy);

#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  Done." << std::endl;
	m_sLogFile.flush();
#endif

	return nErr;
}

void CArcoRotator::Disconnect()
{
    if(m_bIsConnected && m_pSerx)
        m_pSerx->close();
 
	m_bIsConnected = false;
}

bool CArcoRotator::isAcroPresent()
{
	int nErr = R_PLUGIN_OK;
	std::string sResp;
	json jCmd;
	json jResp;
	int nPresent = 0;

	m_bArcoPresent = false;
	jCmd["req"]["get"]["ARCO"]="";
	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return false;
	jResp = json::parse(sResp);
	if(jResp.at("res").at("get").contains("ARCO")) {
		nPresent = jResp.at("res").at("get").at("ARCO").get<int>();
	}

	return (nPresent==1);
}


#pragma mark move commands
int CArcoRotator::haltArco()
{
	int nErr = R_PLUGIN_OK;
	std::string sResp;
	json jCmd;
	json jResp;

#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [haltFocuser] Called." << std::endl;
	m_sLogFile.flush();
#endif

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	jCmd["req"]["cmd"]["MOT2"]["MOT_ABORT"]="";

	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return nErr;
	if(!sResp.length()) {
		return ERR_CMDFAILED;
	}
	// parse output
	try {
		jResp = json::parse(sResp);
#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [haltFocuser] response : " << sResp << std::endl;
		m_sLogFile.flush();
#endif

		if(jResp.at("res").at("cmd").at("MOT2").at("MOT_ABORT") == "done") {
#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [haltFocuser] motor has stopped." << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [haltFocuser] m_nTargetPos : " << m_dTargetPos << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [haltFocuser] m_nCurPos    : " << m_dCurPos << std::endl;
			m_sLogFile.flush();
#endif
			m_bAborted = true;

		}
		else {
#if defined ARCO_PLUGIN_DEBUG
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] **** ERROR **** haltFocuser failed : " << nErr << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [Connect] **** ERROR **** haltFocuser response : " << sResp << std::endl;
			m_sLogFile.flush();
#endif
			return ERR_CMDFAILED;
		}
	}
	catch (json::exception& e) {
#if defined ARCO_PLUGIN_DEBUG
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [haltFocuser] json exception : " << e.what() << " - " << e.id << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [haltFocuser] json exception response : " << sResp << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}

	return nErr;
}

int CArcoRotator::gotoPosition(double dPosDeg)
{
	int nErr = R_PLUGIN_OK;
	std::string sResp;
	json jCmd;
	json jResp;

#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Called" << std::endl;
	m_sLogFile.flush();
#endif

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  goto position : " << dPosDeg << std::endl;
	m_sLogFile.flush();
#endif
#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  m_StatusTimer.GetElapsedSeconds() : " << m_StatusTimer.GetElapsedSeconds() << std::endl;
	m_sLogFile.flush();
#endif

	jCmd["req"]["cmd"]["MOT2"]["MOVE_ABS"]["DEG"]=dPosDeg;

#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  m_StatusTimer.GetElapsedSeconds() : " << m_StatusTimer.GetElapsedSeconds() << std::endl;
	m_sLogFile.flush();
#endif
	nErr = ctrlCommand(jCmd.dump(), sResp);
#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  m_StatusTimer.GetElapsedSeconds() : " << m_StatusTimer.GetElapsedSeconds() << std::endl;
	m_sLogFile.flush();
#endif

	if(nErr)
		return nErr;

	if(!sResp.length()) {
		return ERR_CMDFAILED;
	}

	try {
		jResp = json::parse(sResp);
#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  response : " << jResp.dump(2) << std::endl;
		m_sLogFile.flush();
#endif
		if(jResp.at("res").at("cmd").at("MOT2").at("DEG") == "done") {
			m_dTargetPos = dPosDeg;
#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] goto m_dTargetPos : " << m_dTargetPos << std::endl;
			m_sLogFile.flush();
#endif
		}
		else {
#if defined ARCO_PLUGIN_DEBUG
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  **** ERROR **** gotoPosition failed : " << nErr << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  **** ERROR **** gotoPosition response : " << sResp << std::endl;
			m_sLogFile.flush();
#endif
			m_dTargetPos = m_dCurPos;
			return ERR_CMDFAILED;
		}
	}
	catch (json::exception& e) {
#if defined ARCO_PLUGIN_DEBUG
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception : " << e.what() << " - " << e.id << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]  json exception response : " << sResp << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}

	m_bAborted = false;
	return nErr;
}
#pragma mark command complete functions

int CArcoRotator::isGoToComplete(bool &bComplete)
{
    int nErr = R_PLUGIN_OK;
    bool bIsMoving;
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;
    
    bComplete = false;
    // check if we're still moving
    isMotorMoving(bIsMoving);
    if(bIsMoving)
        return nErr;
    
    getPosition(m_dCurPos);
#if defined ARCO_PLUGIN_DEBUG
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_dCurPos    : " << m_dCurPos << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_dTargetPos : " << m_dTargetPos << std::endl;
	m_sLogFile.flush();

#endif
    if(m_bAborted) {
		bComplete = true;
		m_dTargetPos = m_dCurPos;
		m_bAborted = false;
	}
    
    
    else if ((m_dTargetPos <= m_dCurPos + 0.1) && (m_dTargetPos >= m_dCurPos - 0.1))
        bComplete = true;
    else
        bComplete = false;
    return nErr;
}

int CArcoRotator::isMotorMoving(bool &bMoving)
{
	int nErr = R_PLUGIN_OK;
	std::string sResp;
	json jCmd;
	json jResp;

	bMoving = false;
	jCmd["req"]["get"]["MOT2"]["STATUS"]="";
	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return false;
	// parse output
	try {
		jResp = json::parse(sResp);
		if(jResp.at("res").at("get").at("MOT2").contains("STATUS")) {
			bMoving = (jResp.at("res").at("get").at("MOT2").at("STATUS").at("MST").get<std::string>() != "stop");
		}
	}
	catch (json::exception& e) {
#if defined ARCO_PLUGIN_DEBUG
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception : " << e.what() << " - " << e.id << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception response : " << sResp << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}

	return nErr;
}

#pragma mark getters and setters

int CArcoRotator::getFirmwareVersion(std::string &sVersion)
{
    int nErr = R_PLUGIN_OK;
	std::string sResp;
	json jCmd;
	json jResp;

#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Called." << std::endl;
	m_sLogFile.flush();
#endif

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	if(m_sAppVer.size() && m_sWebVer.size()) {
		sVersion = m_sAppVer + " / " + m_sWebVer;
		return nErr;
	}

	jCmd["req"]["get"]["SWVERS"]="";

#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
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
#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] response : " << jResp.dump(2) << std::endl;
		m_sLogFile.flush();
#endif
		m_sAppVer = jResp.at("res").at("get").at("SWVERS").at("SWAPP").get<std::string>();
		m_sWebVer = jResp.at("res").at("get").at("SWVERS").at("SWWEB").get<std::string>();
	}
	catch (json::exception& e) {
#if defined ARCO_PLUGIN_DEBUG
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception : " << e.what() << " - " << e.id << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception response : " << sResp << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}
	sVersion = m_sAppVer + " / " + m_sWebVer;

	m_fFirmwareVersion = std::stof(m_sAppVer);

#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] szResp     : " << sResp << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] pszVersion : " << sVersion << std::endl;
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] m_fFirmwareVersion : " << m_fFirmwareVersion << std::endl;
	m_sLogFile.flush();
#endif
	return nErr;
}

int CArcoRotator::getModelName(std::string &sModelName)
{
	int nErr = R_PLUGIN_OK;
	std::string sResp;
	json jCmd;
	json jResp;

#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Called." << std::endl;
	m_sLogFile.flush();
#endif

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	if(m_sModelName.size()) {
		sModelName.assign(m_sModelName);
		return nErr;
	}
	jCmd["req"]["get"]["MOT2"]["SUBMODEL"]="";

#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
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
#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] response : " << jResp.dump(2) << std::endl;
		m_sLogFile.flush();
#endif
		m_sModelName = jResp.at("res").at("get").at("MOT2").at("SUBMODEL").get<std::string>();
	}
	catch (json::exception& e) {
#if defined ARCO_PLUGIN_DEBUG
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]json exception : " << e.what() << " - " << e.id << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception response : " << sResp << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}

	sModelName.assign(m_sModelName);
	return nErr;
}

int CArcoRotator::getPosition(double &dPosition)
{
    int nErr = R_PLUGIN_OK;
	std::string sResp;
	json jCmd;
	json jResp;

#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Called." << std::endl;
	m_sLogFile.flush();
#endif
	jCmd["req"]["get"]["MOT2"]["POSITION_DEG"]="";
	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return false;
	if(!sResp.length()) {
		return ERR_CMDFAILED;
	}
	try {
		jResp = json::parse(sResp);
		if(jResp.at("res").at("get").at("MOT2").contains("POSITION_DEG")) {
			m_dCurPos = jResp.at("res").at("get").at("MOT2").at("POSITION_DEG").get<double>();
		}
	}
	catch (json::exception& e) {
#if defined ARCO_PLUGIN_DEBUG
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "]json exception : " << e.what() << " - " << e.id << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception response : " << sResp << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}
	dPosition = m_dCurPos;
	return nErr;
}


int CArcoRotator::syncMotorPosition(double dPosDeg)
{
    int nErr = R_PLUGIN_OK;
	std::string sResp;
	json jCmd;
	json jResp;

#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Called." << std::endl;
	m_sLogFile.flush();
#endif

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	jCmd["req"]["set"]["MOT2"]["SYNC_POS"]["DEG"]=dPosDeg;
#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] jCmd : " << jCmd.dump() << std::endl;
	m_sLogFile.flush();
#endif
	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return nErr;
	// parse output
	try {
#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] response : " << std::endl << jResp.dump(2) << std::endl;
		m_sLogFile.flush();
#endif
		jResp = json::parse(sResp);
		if(jResp.at("res").at("set").at("MOT2").at("ABS_POS") != "done") {
#if defined ARCO_PLUGIN_DEBUG
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] **** ERROR **** syncMotorPosition failed   : " << nErr << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] **** ERROR **** syncMotorPosition response : " << sResp << std::endl;
			m_sLogFile.flush();
#endif
			return ERR_CMDFAILED;
		}
	}
	catch (json::exception& e) {
#if defined ARCO_PLUGIN_DEBUG
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception : " << e.what() << " - " << e.id << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception response : " << sResp << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}

	m_dCurPos = dPosDeg;
	return nErr;
}


int CArcoRotator::setReverseEnable(bool bEnabled)
{
	int nErr = R_PLUGIN_OK;
	std::string sResp;
	json jCmd;
	json jResp;

#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Called." << std::endl;
	m_sLogFile.flush();
#endif

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	jCmd["req"]["set"]["MOT2"]["REVERSE"]=(bEnabled?1:0);
#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] jCmd : " << jCmd.dump() << std::endl;
	m_sLogFile.flush();
#endif
	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return nErr;
	// parse output
	try {
#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] response : " << std::endl << jResp.dump(2) << std::endl;
		m_sLogFile.flush();
#endif
		jResp = json::parse(sResp);
		if(jResp.at("res").at("set").at("MOT2").at("REVERSE") != "done") {
#if defined ARCO_PLUGIN_DEBUG
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] **** ERROR **** failed   : " << nErr << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] **** ERROR **** response : " << sResp << std::endl;
			m_sLogFile.flush();
#endif
			return ERR_CMDFAILED;
		}
	}
	catch (json::exception& e) {
#if defined ARCO_PLUGIN_DEBUG
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception : " << e.what() << " - " << e.id << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception response : " << sResp << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}

	return nErr;
}

int CArcoRotator::getReverseEnable(bool &bEnabled)
{
	int nErr = R_PLUGIN_OK;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;
	std::string sResp;
	json jCmd;
	json jResp;
	int nReverse = 0;

#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Called." << std::endl;
	m_sLogFile.flush();
#endif
	bEnabled = false;
	jCmd["req"]["get"]["MOT2"]["REVERSE"]="";
	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return false;
	if(!sResp.length()) {
		return ERR_CMDFAILED;
	}
	try {
		jResp = json::parse(sResp);
		if(jResp.at("res").at("get").at("MOT2").contains("REVERSE")) {
			nReverse = jResp.at("res").at("get").at("MOT2").at("POSITION_DEG").get<int>();
		}
	}
	catch (json::exception& e) {
#if defined ARCO_PLUGIN_DEBUG
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception : " << e.what() << " - " << e.id << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception response : " << sResp << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}

	if(nReverse == 1)
		bEnabled = true;

	return nErr;
}


int CArcoRotator::setHemisphere(std::string sHemisphere)
{
	int nErr = R_PLUGIN_OK;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;
	std::string sResp;
	json jCmd;
	json jResp;

#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Called." << std::endl;
	m_sLogFile.flush();
#endif
	std::transform(sHemisphere.begin(), sHemisphere.end(), sHemisphere.begin(),  [](unsigned char c) -> unsigned char { return std::tolower(c); });
	jCmd["req"]["set"]["MOT2"]["HEMISPHERE"]=sHemisphere;
	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return false;
	if(!sResp.length()) {
		return ERR_CMDFAILED;
	}
	try {
		jResp = json::parse(sResp);
		if(jResp.at("res").at("set").at("MOT2").at("HEMISPHERE") != "done") {
			return ERR_CMDFAILED;
		}
	}
	catch (json::exception& e) {
#if defined ARCO_PLUGIN_DEBUG
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception : " << e.what() << " - " << e.id << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception response : " << sResp << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}
	return nErr;
}

int CArcoRotator::getHemisphere(std::string &sHemisphere)
{
	int nErr = R_PLUGIN_OK;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;
	std::string sResp;
	json jCmd;
	json jResp;

#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Called." << std::endl;
	m_sLogFile.flush();
#endif
	jCmd["req"]["get"]["MOT2"]["HEMISPHERE"]="";
	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return false;
	if(!sResp.length()) {
		return ERR_CMDFAILED;
	}
	try {
		jResp = json::parse(sResp);
		if(jResp.at("res").at("get").at("MOT2").contains("HEMISPHERE")) {
			sHemisphere.assign(jResp.at("res").at("get").at("MOT2").at("POSITION_DEG").get<std::string>());
		}
	}
	catch (json::exception& e) {
#if defined ARCO_PLUGIN_DEBUG
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception : " << e.what() << " - " << e.id << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception response : " << sResp << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}
	return nErr;
}

int CArcoRotator::startCalibration()
{
	int nErr = R_PLUGIN_OK;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;
	std::string sResp;
	json jCmd;
	json jResp;

#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Called." << std::endl;
	m_sLogFile.flush();
#endif
	jCmd["req"]["set"]["MOT2"]["CAL_STATUS"]="exec";
	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return false;
	if(!sResp.length()) {
		return ERR_CMDFAILED;
	}
	try {
		jResp = json::parse(sResp);
		if(jResp.at("res").at("set").at("MOT2").at("CAL_STATUS") != "done") {
			return ERR_CMDFAILED;
		}
	}
	catch (json::exception& e) {
#if defined ARCO_PLUGIN_DEBUG
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception : " << e.what() << " - " << e.id << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception response : " << sResp << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}
	return nErr;
}

int CArcoRotator::stopCalibration()
{
	int nErr = R_PLUGIN_OK;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;
	std::string sResp;
	json jCmd;
	json jResp;

#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Called." << std::endl;
	m_sLogFile.flush();
#endif
	jCmd["req"]["set"]["MOT2"]["CAL_STATUS"]="stop";
	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return false;
	if(!sResp.length()) {
		return ERR_CMDFAILED;
	}
	try {
		jResp = json::parse(sResp);
		if(jResp.at("res").at("set").at("MOT2").at("CAL_STATUS") != "done") {
			return ERR_CMDFAILED;
		}
	}
	catch (json::exception& e) {
#if defined ARCO_PLUGIN_DEBUG
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception : " << e.what() << " - " << e.id << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception response : " << sResp << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}
	return nErr;
}

int CArcoRotator::isCalibrationDone(bool bDone)
{
	int nErr = R_PLUGIN_OK;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;
	std::string sResp;
	json jCmd;
	json jResp;

#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] Called." << std::endl;
	m_sLogFile.flush();
#endif

	bDone = false;

	jCmd["req"]["get"]["MOT2"]["CAL_STATUS"]="";
	nErr = ctrlCommand(jCmd.dump(), sResp);
	if(nErr)
		return false;
	if(!sResp.length()) {
		return ERR_CMDFAILED;
	}
	try {
		jResp = json::parse(sResp);
		if(jResp.at("res").at("set").at("MOT2").at("HEMISPHERE") == "stop") {
			bDone = true;
		}
	}
	catch (json::exception& e) {
#if defined ARCO_PLUGIN_DEBUG
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception : " << e.what() << " - " << e.id << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [" << __func__ << "] json exception response : " << sResp << std::endl;
		m_sLogFile.flush();
#endif
		return ERR_CMDFAILED;
	}
	return nErr;
}



#pragma mark command and response functions

int CArcoRotator::ctrlCommand(const std::string sCmd, std::string &sResult, int nTimeout)
{
	int nErr = R_PLUGIN_OK;
	unsigned long  ulBytesWrite;
	int nBytesWaiting = 0 ;
	std::size_t startPos;
	std::size_t endPos;
	std::string sLocalResult;
	int nbTries = 0;
#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [ctrlCommand] Called." << std::endl;
	m_sLogFile.flush();
#endif
	sResult.clear();

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;
	nErr = m_pSerx->bytesWaitingRx(nBytesWaiting);
	if(nBytesWaiting) {
#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [ctrlCommand] calling purgeTxR"<< std::endl;
		m_sLogFile.flush();
#endif
		m_pSerx->purgeTxRx();
	}


#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [ctrlCommand] Sending : " << sCmd << std::endl;
	m_sLogFile.flush();
#endif

	nErr = m_pSerx->writeFile((void *) (sCmd.c_str()) , sCmd.length(), ulBytesWrite);
	m_pSerx->flushTx();
	if(nErr){
#if defined ARCO_PLUGIN_DEBUG
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [ctrlCommand] writeFile error  : " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		return nErr;
	}

	do {
		// read response
#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [ctrlCommand] Getting response." << std::endl;
		m_sLogFile.flush();
#endif

		nErr = readResponse(sLocalResult, nTimeout);
		if(nErr){
#if defined ARCO_PLUGIN_DEBUG
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [ctrlCommand] readResponse error  : " << nErr << std::endl;
			m_sLogFile.flush();
#endif
			return nErr;
		}

#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [ctrlCommand] response : " << sLocalResult << std::endl;
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

#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 2
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [ctrlCommand] final response : " << sResult << std::endl;
	m_sLogFile.flush();
#endif

	return nErr;
}


int CArcoRotator::readResponse(std::string &sResp, int nTimeout)
{
	int nErr = R_PLUGIN_OK;
	char pszBuf[SERIAL_BUFFER_SIZE];
	unsigned long ulBytesRead = 0;
	unsigned long ulTotalBytesRead = 0;
	char *pszBufPtr;
	int nBytesWaiting = 0 ;
	int nbTimeouts = 0;

#ifdef ARCO_PLUGIN_DEBUG
	std::string hexBuff;
#endif

	memset(pszBuf, 0, SERIAL_BUFFER_SIZE);
	pszBufPtr = pszBuf;

	do {
		nErr = m_pSerx->bytesWaitingRx(nBytesWaiting);
#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 3
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] nBytesWaiting      : " << nBytesWaiting << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] nBytesWaiting nErr : " << nErr << std::endl;
		m_sLogFile.flush();
#endif
		if(!nBytesWaiting) {
			nbTimeouts += MAX_READ_WAIT_TIMEOUT;
			if(nbTimeouts >= nTimeout) {
#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 3
				m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] bytesWaitingRx timeout, no data for " << nbTimeouts << " ms"<< std::endl;
				hexdump(pszBuf, int(ulBytesRead), hexBuff);
				m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] hexdump  " << hexBuff << std::endl;
				m_sLogFile.flush();
#endif
				nErr = R_COMMAND_TIMEOUT;
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
#if defined ARCO_PLUGIN_DEBUG
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] readFile error : " << nErr << std::endl;
			m_sLogFile.flush();
#endif
			return nErr;
		}
#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 3
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] readFile nBytesWaiting : " << nBytesWaiting << std::endl;
		m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] readFile ulBytesRead   : " << ulBytesRead << std::endl;
		m_sLogFile.flush();
#endif

		if (ulBytesRead != nBytesWaiting) { // timeout
#if defined ARCO_PLUGIN_DEBUG
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] rreadFile Timeout Error." << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] readFile nBytesWaiting : " << nBytesWaiting << std::endl;
			m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] readFile ulBytesRead   : " << ulBytesRead << std::endl;
			m_sLogFile.flush();
#endif
		}

		ulTotalBytesRead += ulBytesRead;
		pszBufPtr+=ulBytesRead;
	}  while (ulTotalBytesRead < SERIAL_BUFFER_SIZE  && *(pszBufPtr-1) != '\n');

	if(!ulTotalBytesRead) {
		nErr = R_COMMAND_TIMEOUT; // we didn't get an answer.. so timeout
	}
	else {
		if( *(pszBufPtr-1) == '\n')
			*(pszBufPtr-1) = 0; //remove the \n
		sResp.assign(pszBuf);
	}

#if defined ARCO_PLUGIN_DEBUG && ARCO_PLUGIN_DEBUG >= 3
	m_sLogFile << "["<<getTimeStamp()<<"]"<< " [readResponse] sResp : " << sResp << std::endl;
	m_sLogFile.flush();
#endif

	return nErr;
}

#ifdef ARCO_PLUGIN_DEBUG
void  CArcoRotator::hexdump( char *inputData, int inputSize,  std::string &outHex)
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

const std::string CArcoRotator::getTimeStamp()
{
	time_t     now = time(0);
	struct tm  tstruct;
	char       buf[80];
	tstruct = *localtime(&now);
	std::strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

	return buf;
}
#endif
