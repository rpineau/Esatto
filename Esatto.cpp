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

	m_sAppVer.clear();
	m_sWebVer.clear();
	m_sModelName.clear();
    m_nModel = ESATTO;
    
#ifdef PLUGIN_DEBUG
#if defined(SB_WIN_BUILD)
	m_sLogfilePath = getenv("HOMEDRIVE");
	m_sLogfilePath += getenv("HOMEPATH");
	m_sLogfilePath += "\\EsattoLog.txt";
#elif defined(SB_LINUX_BUILD)
	m_sLogfilePath = getenv("HOME");
	m_sLogfilePath += "/EsattoLog.txt";
#elif defined(SB_MAC_BUILD)
	m_sLogfilePath = getenv("HOME");
	m_sLogfilePath += "/EsattoLog.txt";
#endif
	Logfile = fopen(m_sLogfilePath.c_str(), "w");
#endif

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CEsattoController::CEsattoController] Version %3.2f build 2021_07_05_0945.\n", timestamp, DRIVER_VERSION);
	fprintf(Logfile, "[%s] [CEsattoController] Constructor Called.\n", timestamp);
	fflush(Logfile);
#endif

}

CEsattoController::~CEsattoController()
{
#ifdef	PLUGIN_DEBUG
    // Close LogFile
    if (Logfile) fclose(Logfile);
#endif
}

int CEsattoController::Connect(const char *pszPort)
{
    int nErr = PLUGIN_OK;

    if(!m_pSerx)
        return ERR_COMMNOLINK;

#ifdef PLUGIN_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CEsattoController::Connect Called %s\n", timestamp, pszPort);
	fflush(Logfile);
#endif

    nErr = m_pSerx->open(pszPort, 115200, SerXInterface::B_NOPARITY, "-DTR_CONTROL 1");
    if( nErr == 0)
        m_bIsConnected = true;
    else
        m_bIsConnected = false;

    if(!m_bIsConnected)
        return nErr;

    m_pSleeper->sleep(2000);

#ifdef PLUGIN_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CEsattoController::Connect connected to %s\n", timestamp, pszPort);
	fflush(Logfile);
#endif

    // get status so we can figure out what device we are connecting to.
#ifdef PLUGIN_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CEsattoController::Connect getting device status\n", timestamp);
	fflush(Logfile);
#endif
    nErr = getDeviceStatus();
    if(nErr) {
		m_bIsConnected = false;
#ifdef PLUGIN_DEBUG
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CEsattoController::Connect **** ERROR **** getting device status\n", timestamp);
		fflush(Logfile);
#endif
        return nErr;
    }
    if(m_nMaxPos == 0) {
        setPosLimit(0, 1000000);
    }
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
    char szResp[SERIAL_BUFFER_SIZE];
    // char szTmpBuf[SERIAL_BUFFER_SIZE];
	json jCmd;
	json jResp;

    if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	jCmd["req"]["cmd"]["MOT1"]["MOT_ABORT"]="";

	nErr = ctrlCommand(jCmd.dump(), szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
	// parse output
	try {
		jResp = json::parse(szResp);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CEsattoController::haltFocuser] response :\n%s\n", timestamp, jResp.dump(2).c_str());
        fflush(Logfile);
#endif
		if(jResp.at("res").at("cmd").at("MOT1").at("MOT_ABORT") == "done")
			m_nTargetPos = m_nCurPos;
		else
			return ERR_CMDFAILED;
	}
    catch (json::exception& e) {
        #if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CEsattoController::haltFocuser] json exception : %s - %d\n", timestamp, e.what(), e.id);
            fflush(Logfile);
        #endif
        return ERR_CMDFAILED;
    }

    return nErr;
}

int CEsattoController::gotoPosition(int nPos)
{
    int nErr;
    char szResp[SERIAL_BUFFER_SIZE];
    // char szTmpBuf[SERIAL_BUFFER_SIZE];
	json jCmd;
	json jResp;
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;


    if (m_bPosLimitEnabled && (nPos>m_nMaxPos || nPos < m_nMinPos))
        return ERR_LIMITSEXCEEDED;

#ifdef PLUGIN_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CEsattoController::gotoPosition goto position  : %d\n", timestamp, nPos);
    fflush(Logfile);
#endif

	jCmd["req"]["cmd"]["MOT1"]["GOTO"]=nPos;
    nErr = ctrlCommand(jCmd.dump(), szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

	try {
		jResp = json::parse(szResp);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CEsattoController::gotoPosition] response :\n%s\n", timestamp, jResp.dump(2).c_str());
        fflush(Logfile);
#endif
		if(jResp.at("res").at("cmd").at("MOT1").at("GOTO") == "done") {
			m_nTargetPos = nPos;
			#ifdef PLUGIN_DEBUG
				ltime = time(NULL);
				timestamp = asctime(localtime(&ltime));
				timestamp[strlen(timestamp) - 1] = 0;
				fprintf(Logfile, "[%s] CEsattoController::gotoPosition goto m_nTargetPos =  %d\n", timestamp, m_nTargetPos);
				fflush(Logfile);
			#endif
		}
		else {
			m_nTargetPos = m_nCurPos;
			return ERR_CMDFAILED;
		}
	}
    catch (json::exception& e) {
        #if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CEsattoController::gotoPosition] json exception : %s - %d\n", timestamp, e.what(), e.id);
            fflush(Logfile);
        #endif
        return ERR_CMDFAILED;
    }

    return nErr;
}

int CEsattoController::moveRelativeToPosision(int nSteps)
{
    int nErr;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

#ifdef PLUGIN_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CEsattoController::gotoPosition goto relative position  : %d\n", timestamp, nSteps);
    fflush(Logfile);
#endif

    m_nTargetPos = m_nCurPos + nSteps;
    nErr = gotoPosition(m_nTargetPos);
    return nErr;
}

#pragma mark command complete functions

int CEsattoController::isGoToComplete(bool &bComplete)
{
    int nErr = PLUGIN_OK;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	bComplete = false;
	getDeviceStatus();
	#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CEsattoController::isGoToComplete] m_bMoving = %s\n", timestamp, m_bMoving?"True":"False");
		fflush(Logfile);
	#endif
	if(m_bMoving)
		return nErr;

	getDeviceStatus();
	#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CEsattoController::isGoToComplete] m_nCurPos = %d\n", timestamp, m_nCurPos);
		fprintf(Logfile, "[%s] [CEsattoController::isGoToComplete] m_nTargetPos = %d\n", timestamp, m_nTargetPos);
		fflush(Logfile);
	#endif

	if(m_nCurPos == m_nTargetPos)
        bComplete = true;
	else {
		// not at the propernposition yet.
        bComplete = false;
 	}
    return nErr;
}

#pragma mark getters and setters
int CEsattoController::getDeviceStatus()
{
    int nErr;
    char szResp[SERIAL_BUFFER_SIZE];
	json jCmd;
	json jResp;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;
	
	jCmd["req"]["get"]["MOT1"]="";
	#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CEsattoController::getDeviceStatus] jCmd : %s\n", timestamp, jCmd.dump().c_str());
		fflush(Logfile);
	#endif

	nErr = ctrlCommand(jCmd.dump(), szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;
	// parse output
	try {
		jResp = json::parse(szResp);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CEsattoController::getDeviceStatus] response :\n%s\n", timestamp, jResp.dump(2).c_str());
        fflush(Logfile);
#endif
		m_nCurPos = jResp.at("res").at("get").at("MOT1").at("ABS_POS").get<int>();
		m_nMaxPos = jResp.at("res").at("get").at("MOT1").at("CAL_MAXPOS").get<int>();
		m_nMinPos = jResp.at("res").at("get").at("MOT1").at("CAL_MINPOS").get<int>();
		m_bMoving = (jResp.at("res").at("get").at("MOT1").at("STATUS").at("MST").get<std::string>() != "stop");
        if(jResp.at("res").at("get").at("MOT1").at("CAL_DIR").get<std::string>().find("normal")!=-1)
            m_nDir = NORMAL;
        else if(jResp.at("res").at("get").at("MOT1").at("CAL_DIR").get<std::string>().find("invert")!=-1)
            m_nDir = INVERT;
        else
            m_nDir = NORMAL; // just in case.

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			ltime = time(NULL);
			timestamp = asctime(localtime(&ltime));
			timestamp[strlen(timestamp) - 1] = 0;
			fprintf(Logfile, "[%s] [CEsattoController::getDeviceStatus] m_nCurPos : %d\n", timestamp, m_nCurPos);
			fprintf(Logfile, "[%s] [CEsattoController::getDeviceStatus] m_nMaxPos : %d\n", timestamp, m_nMaxPos);
			fprintf(Logfile, "[%s] [CEsattoController::getDeviceStatus] m_nMinPos : %d\n", timestamp, m_nMinPos);
			fprintf(Logfile, "[%s] [CEsattoController::getDeviceStatus] m_bMoving : %s\n", timestamp, m_bMoving?"True":"False");
            fprintf(Logfile, "[%s] [CEsattoController::getDeviceStatus] m_nDir    : %s\n", timestamp, (m_nDir==NORMAL)?"normal":"invert");
			fflush(Logfile);
#endif
	}
    catch (json::exception& e) {
        #if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CEsattoController::getDeviceStatus] json exception : %s - %d\n", timestamp, e.what(), e.id);
            fflush(Logfile);
        #endif
        return ERR_CMDFAILED;
    }
	return nErr;
}

int CEsattoController::getFirmwareVersion(char *pszVersion, int nStrMaxLen)
{
	int nErr = PLUGIN_OK;
	char szResp[SERIAL_BUFFER_SIZE];
	json jCmd;
	json jResp;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	if(m_sAppVer.size() && m_sWebVer.size()) {
		strncpy(pszVersion, (m_sAppVer + " " + m_sWebVer).c_str(), nStrMaxLen);
		return nErr;
	}

	jCmd["req"]["get"]["SWVERS"]="";

	#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CEsattoController::getFirmwareVersion] jCmd : %s\n", timestamp, jCmd.dump().c_str());
		fflush(Logfile);
	#endif

	nErr = ctrlCommand(jCmd.dump(), szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;
	// parse output
	try {
		jResp = json::parse(szResp);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CEsattoController::getFirmwareVersion] response :\n%s\n", timestamp, jResp.dump(2).c_str());
        fflush(Logfile);
#endif
		m_sAppVer = jResp.at("res").at("get").at("SWVERS").at("SWAPP").get<std::string>();
		m_sWebVer = jResp.at("res").at("get").at("SWVERS").at("SWWEB").get<std::string>();
	}
    catch (json::exception& e) {
        #if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CEsattoController::getFirmwareVersion] json exception : %s - %d\n", timestamp, e.what(), e.id);
            fflush(Logfile);
        #endif
        return ERR_CMDFAILED;
    }

    strncpy(pszVersion, (m_sAppVer + " " + m_sWebVer).c_str(), nStrMaxLen);
#ifdef PLUGIN_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CEsattoController::getFirmwareVersion szResp : %s\n", timestamp, szResp);
	fprintf(Logfile, "[%s] CEsattoController::getFirmwareVersion pszVersion : %s\n", timestamp, pszVersion);
	fflush(Logfile);
#endif

	return nErr;
}


int CEsattoController::getModelName(char *pszModelName, int nStrMaxLen)
{
	int nErr = PLUGIN_OK;
	char szResp[SERIAL_BUFFER_SIZE];
	json jCmd;
	json jResp;


	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	if(m_sModelName.size()) {
		strncpy(pszModelName, m_sModelName.c_str(), nStrMaxLen);
		return nErr;
	}

	jCmd["req"]["get"]["MODNAME"]="";
	#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CEsattoController::getModelName] jCmd : %s\n", timestamp, jCmd.dump().c_str());
		fflush(Logfile);
	#endif

	nErr = ctrlCommand(jCmd.dump(), szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;
	// parse output
	try {
		jResp = json::parse(szResp);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CEsattoController::getModelName] response :\n%s\n", timestamp, jResp.dump(2).c_str());
        fflush(Logfile);
#endif
		m_sModelName = jResp.at("res").at("get").at("MODNAME").get<std::string>();
	}
    catch (json::exception& e) {
        #if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CEsattoController::getModelName] json exception : %s - %d\n", timestamp, e.what(), e.id);
            fflush(Logfile);
        #endif
        return ERR_CMDFAILED;
    }

    strncpy(pszModelName, m_sModelName.c_str(), nStrMaxLen);
    if(m_sModelName.find("ESATTO") !=-1)
        m_nModel = ESATTO;
    else if(m_sModelName.find("SESTO") !=-1)
        m_nModel = SESTO;

	return nErr;
}

int CEsattoController::getModel()
{
    return m_nModel;
}

int CEsattoController::getTemperature(double &dTemperature, int nTempProbe)
{
    int nErr = PLUGIN_OK;
    char szResp[SERIAL_BUFFER_SIZE];
	json jCmd;
	json jResp;

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
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CEsattoController::getTemperature] jCmd : %s\n", timestamp, jCmd.dump().c_str());
		fflush(Logfile);
	#endif

	nErr = ctrlCommand(jCmd.dump(), szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;
	// parse output
	try {
		jResp = json::parse(szResp);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CEsattoController::getTemperature] response :\n%s\n", timestamp, jResp.dump(2).c_str());
        fflush(Logfile);
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
        #if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CEsattoController::getTemperature] json exception : %s - %d\n", timestamp, e.what(), e.id);
            fflush(Logfile);
        #endif
        return ERR_CMDFAILED;
    }
	catch(std::invalid_argument& e){
        #if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CEsattoController::getTemperature] stoi invalid_argument exception : %s\n", timestamp, e.what());
            fflush(Logfile);
        #endif
	}
	catch(std::out_of_range& e){
        #if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CEsattoController::getTemperature] stoi out_of_range exception : %s\n", timestamp, e.what());
            fflush(Logfile);
        #endif
	}
	catch(...) {
		nErr = ERR_CMDFAILED;
	}
	return nErr;
}

int CEsattoController::getPosLimit(int &nMin, int &nMax)
{
    int nErr = PLUGIN_OK;
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CEsattoController::getPosLimit]\n", timestamp);
		fflush(Logfile);
	#endif
	getDeviceStatus();
	nMin = m_nMinPos;
	nMax = m_nMaxPos;

    return nErr;
}

int CEsattoController::setPosLimit(int nMin, int nMax)
{
    int nErr = PLUGIN_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    json jCmd;
    json jResp;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;


    jCmd["req"]["set"]["MOT1"]["CAL_MINPOS"]=nMin;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CEsattoController::setPosLimit] setting new min pos to %d [ %s ]\n",timestamp, nMin, jCmd.dump().c_str());
    fprintf(Logfile, "[%s] [CEsattoController::setPosLimit] jCmd : %s\n", timestamp, jCmd.dump().c_str());
    fflush(Logfile);
#endif

    nErr = ctrlCommand(jCmd.dump(), szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    // parse output
    try {
        jResp = json::parse(szResp);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CEsattoController::setPosLimit] response :\n%s\n", timestamp, jResp.dump(2).c_str());
        fflush(Logfile);
#endif
        if(jResp.at("res").at("set").at("MOT1").at("CAL_MINPOS") != "done")
            return ERR_CMDFAILED;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CEsattoController::setPosLimit] response : %s\n", timestamp, szResp);
        fflush(Logfile);
#endif
    }
    catch (json::exception& e) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CEsattoController::setPosLimit] json exception : %s - %d\n", timestamp, e.what(), e.id);
        fflush(Logfile);
#endif
        return ERR_CMDFAILED;
    }
    m_nMinPos = nMin;

    jResp.clear();
    jCmd["req"]["set"]["MOT1"]["CAL_MAXPOS"]=nMax;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CEsattoController::setPosLimit] setting new max pos to %d [ %s ]\n",timestamp, nMax, jCmd.dump().c_str());
    fprintf(Logfile, "[%s] [CEsattoController::setPosLimit] jCmd : %s\n", timestamp, jCmd.dump().c_str());
    fflush(Logfile);
#endif

    nErr = ctrlCommand(jCmd.dump(), szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    // parse output
    try {
        jResp = json::parse(szResp);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CEsattoController::setPosLimit] response :\n%s\n", timestamp, jResp.dump(2).c_str());
        fflush(Logfile);
#endif
        if(jResp.at("res").at("set").at("MOT1").at("CAL_MAXPOS") != "done")
            return ERR_CMDFAILED;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CEsattoController::setPosLimit] response : %s\n", timestamp, szResp);
        fflush(Logfile);
#endif
    }
    catch (json::exception& e) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CEsattoController::setPosLimit] json exception : %s - %d\n", timestamp, e.what(), e.id);
        fflush(Logfile);
#endif
        return ERR_CMDFAILED;
    }

    m_nMaxPos = nMax;
    return nErr;
}

int CEsattoController::setDirection(int nDir)
{
    int nErr = PLUGIN_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    json jCmd;
    json jResp;
    std::string sDir;

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
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CEsattoController::setDirection] setting direction to %s\n",timestamp, sDir.c_str());
    fprintf(Logfile, "[%s] [CEsattoController::setDirection] jCmd : %s\n", timestamp, jCmd.dump().c_str());
    fflush(Logfile);
#endif

    nErr = ctrlCommand(jCmd.dump(), szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    // parse output
    try {
        jResp = json::parse(szResp);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CEsattoController::setDirection] response :\n%s\n", timestamp, jResp.dump(2).c_str());
        fflush(Logfile);
#endif
        if(jResp.at("res").at("set").at("MOT1").at("CAL_DIR") != "done")
            return ERR_CMDFAILED;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CEsattoController::setDirection] response : %s\n", timestamp, szResp);
        fflush(Logfile);
#endif
    }
    catch (json::exception& e) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CEsattoController::setDirection] json exception : %s - %d\n", timestamp, e.what(), e.id);
        fflush(Logfile);
#endif
        return ERR_CMDFAILED;
    }

    return nErr;

}

int CEsattoController::getDirection(int &nDir)
{
    int nErr = PLUGIN_OK;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CEsattoController::getPosition]\n", timestamp);
    fflush(Logfile);
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
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CEsattoController::getPosition]\n", timestamp);
		fflush(Logfile);
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
    char szResp[SERIAL_BUFFER_SIZE];
    json jCmd;
    json jResp;
    std::string wifiMode;

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
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CEsattoController::getWiFiConfig] jCmd : %s\n", timestamp, jCmd.dump().c_str());
		fflush(Logfile);
	#endif

    nErr = ctrlCommand(jCmd.dump(), szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    // parse output
    try {
        jResp = json::parse(szResp);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CEsattoController::getWiFiConfig] response :\n%s\n", timestamp, jResp.dump(2).c_str());
        fflush(Logfile);
#endif
        sSSID = jResp.at("res").at("get").at(wifiMode).at("SSID").get<std::string>();
        sPWD = jResp.at("res").at("get").at(wifiMode).at("PWD").get<std::string>();
    }
    catch (json::exception& e) {
        #if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CEsattoController::getWiFiConfig] json exception : %s - %d\n", timestamp, e.what(), e.id);
            fflush(Logfile);
        #endif
        return ERR_CMDFAILED;
    }

    return nErr;
}

int CEsattoController::setWiFiConfig(int nMode, std::string sSSID, std::string sPWD)
{
    int nErr = PLUGIN_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    json jCmd;
    json jResp;
    std::string wifiMode;

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


	// jCmd["req"]["set"][wifiMode]["SSID"]=sSSID.c_str();
	jCmd["req"]["set"][wifiMode]["PWD"]=sPWD.c_str();

    nErr = ctrlCommand(jCmd.dump(), szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    // parse output
    try {
        #if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
                ltime = time(NULL);
                timestamp = asctime(localtime(&ltime));
                timestamp[strlen(timestamp) - 1] = 0;
                fprintf(Logfile, "[%s] [CEsattoController::setWiFiConfig] response : %s\n", timestamp, szResp);
                fflush(Logfile);
        #endif
        jResp = json::parse(szResp);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CEsattoController::setWiFiConfig] response :\n%s\n", timestamp, jResp.dump(2).c_str());
        fflush(Logfile);
#endif
    }
    catch (json::exception& e) {
        #if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CEsattoController::setWiFiConfig] json exception : %s - %d\n", timestamp, e.what(), e.id);
            fflush(Logfile);
        #endif
        return ERR_CMDFAILED;
    }

    return nErr;

}


int CEsattoController::syncMotorPosition(int nPos)
{
    int nErr = PLUGIN_OK;
    char szResp[SERIAL_BUFFER_SIZE];
	json jCmd;
	json jResp;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	jCmd["req"]["set"]["MOT1"]["ABS_POS"]=nPos;
	#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CEsattoController::syncMotorPosition] setting new pos to %d [ %s ]\n",timestamp, nPos, jCmd.dump().c_str());
		fprintf(Logfile, "[%s] [CEsattoController::syncMotorPosition] jCmd : %s\n", timestamp, jCmd.dump().c_str());
		fflush(Logfile);
	#endif

	nErr = ctrlCommand(jCmd.dump(), szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;
	// parse output
	try {
		jResp = json::parse(szResp);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CEsattoController::syncMotorPosition] response :\n%s\n", timestamp, jResp.dump(2).c_str());
        fflush(Logfile);
#endif
		if(jResp.at("res").at("set").at("MOT1").at("ABS_POS") != "done")
			return ERR_CMDFAILED;

	}
	catch (json::exception& e) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CEsattoController::syncMotorPosition] json exception : %s - %d\n", timestamp, e.what(), e.id);
		fflush(Logfile);
#endif
		return ERR_CMDFAILED;
	}

	m_nCurPos = nPos;
    return nErr;
}


#pragma mark command and response functions

int CEsattoController::ctrlCommand(const std::string sCmd, char *pszResult, int nResultMaxLen)
{
    int nErr = PLUGIN_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    unsigned long  ulBytesWrite;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    m_pSerx->purgeTxRx();
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CEsattoController::ctrlCommand] Sending : %s\n", timestamp, sCmd.c_str());
	fflush(Logfile);
#endif

    nErr = m_pSerx->writeFile((void *) (sCmd.c_str()) , sCmd.size(), ulBytesWrite);
    m_pSerx->flushTx();

    if(nErr){
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CEsattoController::ctrlCommand] writeFile Error : %d\n", timestamp, nErr);
		fflush(Logfile);
#endif
        return nErr;
    }

    if(pszResult) {
        // read response
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CEsattoController::ctrlCommand] Getting response\n", timestamp);
		fflush(Logfile);
#endif

		nErr = readResponse(szResp, SERIAL_BUFFER_SIZE);
        if(nErr){
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			ltime = time(NULL);
			timestamp = asctime(localtime(&ltime));
			timestamp[strlen(timestamp) - 1] = 0;
			fprintf(Logfile, "[%s] [CEsattoController::ctrlCommand] readResponse Error : %d\n", timestamp, nErr);
			fflush(Logfile);
#endif
			return nErr;
		}
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CEsattoController::ctrlCommand response : '%s'\n", timestamp, szResp);
		fflush(Logfile);
#endif
        strncpy(pszResult, szResp, nResultMaxLen);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] CEsattoController::ctrlCommand response copied to pszResult : \"%s\"\n", timestamp, pszResult);
		fflush(Logfile);
#endif
    }
    return nErr;
}

int CEsattoController::readResponse(char *szRespBuffer, int nBufferLen, int nTimeout)
{
    int nErr = PLUGIN_OK;
    unsigned long ulBytesRead = 0;
    unsigned long ulTotalBytesRead = 0;
    char *pszBufPtr;
    int nBytesWaiting = 0 ;
    int nbTimeouts = 0;

    memset(szRespBuffer, 0, (size_t) nBufferLen);
    pszBufPtr = szRespBuffer;

    do {
        nErr = m_pSerx->bytesWaitingRx(nBytesWaiting);
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CRTIDome::readResponse] nBytesWaiting = %d\n", timestamp, nBytesWaiting);
        fprintf(Logfile, "[%s] [CRTIDome::readResponse] nBytesWaiting nErr = %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        if(!nBytesWaiting) {
            if(nbTimeouts++ >= NB_RX_WAIT) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
                ltime = time(NULL);
                timestamp = asctime(localtime(&ltime));
                timestamp[strlen(timestamp) - 1] = 0;
                fprintf(Logfile, "[%s] [CRTIDome::readResponse] bytesWaitingRx timeout, no data for %d loops\n", timestamp, NB_RX_WAIT);
                fflush(Logfile);
#endif
                nErr = ERR_RXTIMEOUT;
                break;
            }
            m_pSleeper->sleep(MAX_READ_WAIT_TIMEOUT);
            continue;
        }
        nbTimeouts = 0;
        if(ulTotalBytesRead + nBytesWaiting <= nBufferLen)
            nErr = m_pSerx->readFile(pszBufPtr, nBytesWaiting, ulBytesRead, nTimeout);
        else {
            nErr = ERR_RXTIMEOUT;
            break; // buffer is full.. there is a problem !!
        }
        if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CRTIDome::readResponse] readFile error.\n", timestamp);
            fflush(Logfile);
#endif
            return nErr;
        }

        if (ulBytesRead != nBytesWaiting) { // timeout
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CRTIDome::readResponse] readFile Timeout Error\n", timestamp);
            fprintf(Logfile, "[%s] [CRTIDome::readResponse] readFile nBytesWaiting = %d\n", timestamp, nBytesWaiting);
            fprintf(Logfile, "[%s] [CRTIDome::readResponse] readFile ulBytesRead = %lu\n", timestamp, ulBytesRead);
            fflush(Logfile);
#endif
        }

        ulTotalBytesRead += ulBytesRead;
        pszBufPtr+=ulBytesRead;
    } while (ulTotalBytesRead < nBufferLen  && *(pszBufPtr-1) != '\n');

    if(!ulTotalBytesRead)
        nErr = COMMAND_TIMEOUT; // we didn't get an answer.. so timeout
    else
        *(pszBufPtr-1) = 0; //remove the #

    return nErr;
}

