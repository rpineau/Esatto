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
	fprintf(Logfile, "[%s] [CEsattoController::CEsattoController] Version %3.2f build 2019_10_15_0925.\n", timestamp, DRIVER_VERSION);
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

    // 9600 8N1
    nErr = m_pSerx->open(pszPort, 9600, SerXInterface::B_NOPARITY, "-DTR_CONTROL 1");
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

	jCmd = {"req",{"cmd",{"MOT1" , {"MOT_ABORT",""}}}};
	nErr = ctrlCommand(jCmd.dump(), szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
	// parse output
	try {
		jResp = json::parse(szResp);
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

	jCmd = {"req",{"cmd",{"MOT1" ,{"GOTO", nPos}}}};
    nErr = ctrlCommand(jCmd.dump(), szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

	try {
		jResp = json::parse(szResp);
		if(jResp.at("res").at("cmd").at("MOT1").at("GOTO") == "done")
			m_nTargetPos = nPos;
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
	if(m_bMoving)
		return nErr;

    if(m_nCurPos == m_nTargetPos)
        bComplete = true;
	else {
		// not moving and not at position. command failed
        bComplete = false;
		nErr = ERR_CMDFAILED;
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
	
	jCmd = {"req",{"get",{"MOT1",""}}};

	nErr = ctrlCommand(jCmd.dump(), szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;
	// parse output
	try {
		jResp = json::parse(szResp);
		m_nCurPos = jResp.at("res").at("get").at("MOT1").at("ABS_POS").get<int>();
		m_nMaxPos = jResp.at("res").at("get").at("MOT1").at("CAL_MAXPOS").get<int>();
		m_nMinPos = jResp.at("res").at("get").at("MOT1").at("CAL_MINPOS").get<int>();
		m_bMoving = (jResp.at("res").at("get").at("MOT1").at("STATUS").at("MST").get<std::string>() == "stop");
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

	jCmd = {"req",{"get",{"SWVERS",""}}};

	nErr = ctrlCommand(jCmd.dump(), szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;
	// parse output
	try {
		jResp = json::parse(szResp);
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

	jCmd = {"req",{"get",{"MODNAME",""}}};

	nErr = ctrlCommand(jCmd.dump(), szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;
	// parse output
	try {
		jResp = json::parse(szResp);
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
	return nErr;
}


int CEsattoController::getTemperature(double &dTemperature)
{
    int nErr = PLUGIN_OK;
    char szResp[SERIAL_BUFFER_SIZE];
	json jCmd;
	json jResp;


	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	jCmd = {"req",{"get",{"EXT_T",""}}};

	nErr = ctrlCommand(jCmd.dump(), szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;
	// parse output
	try {
		jResp = json::parse(szResp);
		dTemperature = jResp.at("res").at("get").at("EXT_T").get<double>();
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

	return nErr;
}

int CEsattoController::getPosLimit(int &nMax, int &nMin)
{
    int nErr = PLUGIN_OK;
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	getDeviceStatus();
	nMax = m_nMaxPos;
	nMin = m_nMinPos;

    return nErr;
}


int CEsattoController::getPosition(int &nPosition)
{
	int nErr = PLUGIN_OK;

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


    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    jCmd = {"req",{"get",{"WIFI",""}}};

    nErr = ctrlCommand(jCmd.dump(), szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    // parse output
    try {
        jResp = json::parse(szResp);
        nMode = jResp.at("res").at("get").at("WIFI").at("CFG").get<int>();
        sSSID = jResp.at("res").at("get").at("WIFI").at("SSID").get<std::string>();
        sPWD = jResp.at("res").at("get").at("WIFI").at("PWD").get<std::string>();
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


    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    jCmd = {"req",{"set",{"WIFI",{{"CFG",nMode},{"SSID",sSSID.c_str()},{"PWD",sPWD.c_str()}}}}};
    
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


int CEsattoController::syncMotorPosition(int nPos)
{
    int nErr = PLUGIN_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];
	json jCmd;
	json jResp;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	jCmd = {"req",{"set",{"MOT1",{"ABS_POS", nPos}}}};
    printf("setting new pos to %d [ %s ]\n",nPos, szCmd);
    
	nErr = ctrlCommand(jCmd.dump(), szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;
	// parse output
	try {
		jResp = json::parse(szResp);
		if(jResp.at("res").at("set").at("MOT1").at("ABS_POS") != "done")
			return ERR_CMDFAILED;

#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CEsattoController::getTemperature] response : %s\n", timestamp, szResp);
		fflush(Logfile);
#endif
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

int CEsattoController::readResponse(char *pszRespBuffer, int nBufferLen)
{
    int nErr = PLUGIN_OK;
    unsigned long ulBytesRead = 0;
    unsigned long ulTotalBytesRead = 0;
    char *pszBufPtr;
	
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    memset(pszRespBuffer, 0, (size_t) nBufferLen);
    pszBufPtr = pszRespBuffer;

    do {
        nErr = m_pSerx->readFile(pszBufPtr, 1, ulBytesRead, MAX_TIMEOUT);
        if(nErr) {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			ltime = time(NULL);
			timestamp = asctime(localtime(&ltime));
			timestamp[strlen(timestamp) - 1] = 0;
			fprintf(Logfile, "[%s] [CEsattoController::readResponse] readFile Error : %d\n", timestamp, nErr);
			fflush(Logfile);
#endif
            return nErr;
        }

        if (ulBytesRead !=1) {// timeout
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
			ltime = time(NULL);
			timestamp = asctime(localtime(&ltime));
			timestamp[strlen(timestamp) - 1] = 0;
			fprintf(Logfile, "[%s] [CEsattoController::readResponse] readFile Timeout\n", timestamp);
			fflush(Logfile);
#endif
            nErr = ERR_NORESPONSE;
            break;
        }
        ulTotalBytesRead += ulBytesRead;
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CEsattoController::readResponse] ulBytesRead : %lu\\n", timestamp, ulBytesRead);
		fflush(Logfile);
#endif
    } while (*pszBufPtr++ != '\n' && ulTotalBytesRead < nBufferLen );

    if(ulTotalBytesRead)
        *(pszBufPtr-1) = 0; //remove the \n

    return nErr;
}
