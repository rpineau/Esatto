#include <stdio.h>
#include <string.h>
#include "x2focuser.h"

#include "../../licensedinterfaces/theskyxfacadefordriversinterface.h"
#include "../../licensedinterfaces/sleeperinterface.h"
#include "../../licensedinterfaces/loggerinterface.h"
#include "../../licensedinterfaces/basiciniutilinterface.h"
#include "../../licensedinterfaces/mutexinterface.h"
#include "../../licensedinterfaces/basicstringinterface.h"
#include "../../licensedinterfaces/tickcountinterface.h"
#include "../../licensedinterfaces/serxinterface.h"
#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/serialportparams2interface.h"

X2Focuser::X2Focuser(const char* pszDisplayName,
												const int& nInstanceIndex,
												SerXInterface						* pSerXIn,
												TheSkyXFacadeForDriversInterface	* pTheSkyXIn,
												SleeperInterface					* pSleeperIn,
												BasicIniUtilInterface				* pIniUtilIn,
												LoggerInterface						* pLoggerIn,
												MutexInterface						* pIOMutexIn,
												TickCountInterface					* pTickCountIn)

{
	m_pSerX							= pSerXIn;
	m_pTheSkyXForMounts				= pTheSkyXIn;
	m_pSleeper						= pSleeperIn;
	m_pIniUtil						= pIniUtilIn;
	m_pLogger						= pLoggerIn;
	m_pIOMutex						= pIOMutexIn;
	m_pTickCount					= pTickCountIn;

	m_bLinked = false;
	m_nPosition = 0;
    m_fLastTemp = -273.15f; // aboslute zero :)

    // Read in settings
    if (m_pIniUtil) {
    }
	m_pIOMutex = pIOMutexIn;
	m_pSavedMutex = pIOMutexIn;

	m_pSavedSerX = pSerXIn;
	m_Esatto.SetSerxPointer(pSerXIn);
}

X2Focuser::~X2Focuser()
{
    //Delete objects used through composition
	if (GetTheSkyXFacadeForDrivers())
		delete GetTheSkyXFacadeForDrivers();
	if (GetSleeper())
		delete GetSleeper();
	if (GetSimpleIniUtil())
		delete GetSimpleIniUtil();
	if (GetLogger())
		delete GetLogger();

	if (m_pSavedSerX)
		delete m_pSavedSerX;
	if (m_pSavedMutex)
		delete m_pSavedMutex;
}

#pragma mark - DriverRootInterface

int	X2Focuser::queryAbstraction(const char* pszName, void** ppVal)
{
    *ppVal = NULL;

    if (!strcmp(pszName, LinkInterface_Name))
        *ppVal = (LinkInterface*)this;

    else if (!strcmp(pszName, FocuserGotoInterface2_Name))
        *ppVal = (FocuserGotoInterface2*)this;

    else if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
        *ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);

    else if (!strcmp(pszName, X2GUIEventInterface_Name))
        *ppVal = dynamic_cast<X2GUIEventInterface*>(this);

    else if (!strcmp(pszName, FocuserTemperatureInterface_Name))
        *ppVal = dynamic_cast<FocuserTemperatureInterface*>(this);

    else if (!strcmp(pszName, SerialPortParams2Interface_Name))
        *ppVal = dynamic_cast<SerialPortParams2Interface*>(this);

	else if (!strcmp(pszName, MultiConnectionDeviceInterface_Name))
		*ppVal = dynamic_cast<MultiConnectionDeviceInterface*>(this);

    return SB_OK;
}

#pragma mark - DriverInfoInterface
void X2Focuser::driverInfoDetailedInfo(BasicStringInterface& str) const
{
        str = "Focuser X2 plugin by Rodolphe Pineau";
}

double X2Focuser::driverInfoVersion(void) const
{
	return ESATTO_PLUGIN_VERSION;
}

void X2Focuser::deviceInfoNameShort(BasicStringInterface& str) const
{
    std::string sModelName;
	X2Focuser* pMe = (X2Focuser*)this;
	if(!m_bLinked) {
		str="NA";
	}
	else {
		X2MutexLocker ml(pMe->GetMutex());
		pMe->m_Esatto.getModelName(sModelName);
		str = sModelName.c_str();
	}
}

void X2Focuser::deviceInfoNameLong(BasicStringInterface& str) const
{
    deviceInfoNameShort(str);
}

void X2Focuser::deviceInfoDetailedDescription(BasicStringInterface& str) const
{
    std::string sModelName;
	std::string sDesc;
	X2Focuser* pMe = (X2Focuser*)this;
    if(!m_bLinked) {
		str="NA";
	}
	else {
        X2MutexLocker ml(pMe->GetMutex());
        pMe->m_Esatto.getModelName(sModelName);
		sDesc = "PrimaLuceLab ";
		sDesc.append(sModelName);
		str = sDesc.c_str();
	}
}

void X2Focuser::deviceInfoFirmwareVersion(BasicStringInterface& str)
{
    if(!m_bLinked) {
        str="NA";
    }
    else {
        X2MutexLocker ml(GetMutex());
        // get firmware version
        std::string sFirmware;
        m_Esatto.getFirmwareVersion(sFirmware);
        str = sFirmware.c_str();
    }
}

void X2Focuser::deviceInfoModel(BasicStringInterface& str)
{
    deviceInfoNameShort(str);
}

#pragma mark - LinkInterface
int	X2Focuser::establishLink(void)
{
    char szPort[DRIVER_MAX_STRING];
    int nErr;

    X2MutexLocker ml(GetMutex());
    // get serial port device name
    portNameOnToCharPtr(szPort,DRIVER_MAX_STRING);
    nErr = m_Esatto.Connect(szPort);
    if(nErr)
        m_bLinked = false;
    else
        m_bLinked = true;

    return nErr;
}

int	X2Focuser::terminateLink(void)
{
    if(!m_bLinked)
        return SB_OK;

	X2MutexLocker ml(GetMutex());
	// m_PegasusPPBAExtFoc.Disconnect(m_nInstanceCount);
	m_Esatto.haltFocuser();
	m_Esatto.Disconnect();
	// We're not connected, so revert to our saved interfaces
	m_Esatto.SetSerxPointer(m_pSavedSerX);
	m_pIOMutex = m_pSavedMutex;
	m_bLinked = false;
	return SB_OK;
}

bool X2Focuser::isLinked(void) const
{
	return m_bLinked;
}

#pragma mark - ModalSettingsDialogInterface
int	X2Focuser::initModalSettingsDialog(void)
{
    return SB_OK;
}


int	X2Focuser::execModalSettingsDialog(void)
{
    int nErr = SB_OK;
    X2ModalUIUtil uiutil(this, GetTheSkyXFacadeForDrivers());
    X2GUIInterface*					ui = uiutil.X2UI();
    X2GUIExchangeInterface*			dx = NULL;//Comes after ui is loaded
    char szBuffer[LOG_BUFFER_SIZE];
    bool bPressedOK = false;
	int nPosition = 0;
    int minPos = 0;
    int maxPos = 0;
    mUiEnabled = false;
    int nWiFiMode = AP;
    std::string sSSID_AP;
    std::string sPWD_AP;
    std::string sSSID_STA;
    std::string sPWD_STA;
	std::string sLedState;
	int nLedStateIndex = 0;
    MotorSettings motorSettings;
    int nDir;
    int nModel = ESATTO;
    bool bEnable;

    if (NULL == ui)
        return ERR_POINTER;

    if ((nErr = ui->loadUserInterface("Esatto.ui", deviceType(), m_nPrivateMulitInstanceIndex)))
        return nErr;

    if (NULL == (dx = uiutil.X2DX()))
        return ERR_POINTER;

    X2MutexLocker ml(GetMutex());
	// set controls values
    if(m_bLinked) {
        nModel = m_Esatto.getModel();
        // new position (set to current )
        nErr = m_Esatto.getPosition(nPosition);
        if(nErr)
            return nErr;
        dx->setEnabled("newPos", true);
        dx->setEnabled("pushButton", true);
        dx->setPropertyInt("newPos", "value", nPosition);
        nErr = m_Esatto.getPosLimit(minPos, maxPos);
        if(nModel==SESTO) {
            dx->setEnabled("maxPos", true);
            dx->setEnabled("pushButton_3", true);
        }
        else { // don't change the limits on the Esatto.
            dx->setEnabled("maxPos", false);
            dx->setEnabled("pushButton_3", false);
        }
        dx->setPropertyInt("maxPos", "value", maxPos);
        snprintf(szBuffer, LOG_BUFFER_SIZE, "Current position : %d", nPosition);
        dx->setText("curPosLabel",szBuffer);
        nErr = m_Esatto.getDirection(nDir);
        switch (nDir) {
            case NORMAL:
                dx->setChecked("radioButton", 1);
                break;
            case INVERT:
                dx->setChecked("radioButton_2", 1);
                break;
            default:
                break;
        }

        if(nModel==SESTO) {
            m_Esatto.getMotorSettings(motorSettings);
            dx->setPropertyInt("runSpeed", "value", motorSettings.runSpeed);
            dx->setPropertyInt("accSpeed", "value", motorSettings.accSpeed);
            dx->setPropertyInt("decSpeed", "value", motorSettings.decSpeed);
            dx->setPropertyInt("runCurrent", "value", motorSettings.runCurrent);
            dx->setPropertyInt("accCurrent", "value", motorSettings.accCurrent);
            dx->setPropertyInt("decCurrent", "value", motorSettings.decCurrent);
            dx->setPropertyInt("holdCurrent", "value", motorSettings.holdCurrent);
            dx->setPropertyInt("backlash", "value", motorSettings.backlash);
        }
        else {
            dx->setEnabled("runSpeed", false);
            dx->setEnabled("accSpeed", false);
            dx->setEnabled("decSpeed", false);
            dx->setEnabled("runCurrent", false);
            dx->setEnabled("accCurrent", false);
            dx->setEnabled("decCurrent", false);
            dx->setEnabled("holdCurrent", false);
            dx->setEnabled("backlash", false);
        }

        nErr = m_Esatto.getWiFiConfig(nWiFiMode, sSSID_AP, sPWD_AP, sSSID_STA, sPWD_STA);
        if(!nErr) {
            dx->setText("sSSID", sSSID_AP.c_str());
            dx->setText("sPWD", sPWD_AP.c_str());
            dx->setText("StaSSID", sSSID_STA.c_str());
            dx->setText("StaPWD", sPWD_STA.c_str());
        }
        else {
            dx->setText("sSSID", "not available");
            dx->setEnabled("sPWD", false);
            dx->setEnabled("StaSSID", false);
            dx->setEnabled("StaPWD", false);
            dx->setEnabled("pushButton_2", false);
        }

        nErr = m_Esatto.isWifiEnabled(bEnable);
        if(bEnable) {
            dx->setChecked("checkBox",1);
            dx->setEnabled("pushButton_2", true);
        }
        else {
            dx->setChecked("checkBox",0);
            dx->setEnabled("sPWD", false);
            dx->setEnabled("StaSSID", false);
            dx->setEnabled("StaPWD", false);
            dx->setEnabled("MACAddress", false);
            dx->setEnabled("IPAddress", false);
            dx->setEnabled("SubnetMask", false);
            dx->setEnabled("GatewayIP", false);
            dx->setEnabled("pushButton_2", true);
        }
		nErr = m_Esatto.getLeds(sLedState);
		if(sLedState == "on")
			nLedStateIndex = 0;
		else if(sLedState == "low")
			nLedStateIndex = 1;
		else if(sLedState == "middle")
			nLedStateIndex = 2;
		else if(sLedState == "off")
			nLedStateIndex = 3;
		dx->setCurrentIndex("comboBox_2", nLedStateIndex);
    }
    else {
        // disable all controls
        dx->setEnabled("newPos", false);
        dx->setPropertyInt("newPos", "value", 0);
        dx->setEnabled("pushButton", false);
        dx->setEnabled("radioButton", false);
        dx->setEnabled("radioButton_2", false);
        dx->setEnabled("runSpeed", false);
        dx->setEnabled("accSpeed", false);
        dx->setEnabled("decSpeed", false);
        dx->setEnabled("runCurrent", false);
        dx->setEnabled("accCurrent", false);
        dx->setEnabled("decCurrent", false);
        dx->setEnabled("holdCurrent", false);
        dx->setEnabled("backlash", false);
		dx->setEnabled("comboBox", false);

		dx->setEnabled("checkBox", false);

		dx->setEnabled("sSSID", false);
        dx->setEnabled("sPWD", false);
        dx->setEnabled("StaSSID", false);
        dx->setEnabled("StaPWD", false);
        dx->setText("MACAddress", "");
        dx->setEnabled("MACAddress", false);
        dx->setText("IPAddress", "");
        dx->setEnabled("IPAddress", false);
        dx->setText("SubnetMask", "");
        dx->setEnabled("SubnetMask", false);
        dx->setText("GatewayIP", "");
        dx->setEnabled("GatewayIP", false);
        dx->setEnabled("pushButton_2", false);
        dx->setEnabled("maxPos", false);
        dx->setEnabled("pushButton_3", false);
        dx->setText("curPosLabel","");
		dx->setEnabled("comboBox_2", false);
		dx->setEnabled("pushButton_4", false);
    }

    //Display the user interface
    mUiEnabled = true;
    if ((nErr = ui->exec(bPressedOK)))
        return nErr;
    mUiEnabled = false;

    //Retrieve values from the user interface
    if (bPressedOK) {
        if(dx->isChecked("radioButton"))
            m_Esatto.setDirection(NORMAL);
        else
            m_Esatto.setDirection(INVERT);
        if(nModel==SESTO) {
            dx->propertyInt("runSpeed", "value", motorSettings.runSpeed);
            dx->propertyInt("accSpeed", "value", motorSettings.accSpeed);
            dx->propertyInt("decSpeed", "value", motorSettings.decSpeed);
            dx->propertyInt("runCurrent", "value", motorSettings.runCurrent);
            dx->propertyInt("accCurrent", "value", motorSettings.accCurrent);
            dx->propertyInt("decCurrent", "value", motorSettings.decCurrent);
            dx->propertyInt("holdCurrent", "value", motorSettings.holdCurrent);
            dx->propertyInt("backlash", "value", motorSettings.backlash);
            m_Esatto.setMotorSettings(motorSettings);
        }

        nErr = SB_OK;
    }

    return nErr;
}

void X2Focuser::uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
    int nErr = SB_OK;
    int nTmpVal;
    char szBuffer[LOG_BUFFER_SIZE];

    if(!mUiEnabled)
        return;

    if (!strcmp(pszEvent, "on_timer")) {
        nErr = m_Esatto.getPosition(nTmpVal);
        if(!nErr) {
            snprintf(szBuffer, LOG_BUFFER_SIZE, "Current position : %d", nTmpVal);
            uiex->setText("curPosLabel",szBuffer);
        }
    }
    // new position
    if (!strcmp(pszEvent, "on_pushButton_clicked")) {
        uiex->propertyInt("newPos", "value", nTmpVal);
        nErr = m_Esatto.syncMotorPosition(nTmpVal);
        if(nErr) {
            snprintf(szBuffer, LOG_BUFFER_SIZE, "Error setting new position : Error %d", nErr);
            uiex->messageBox("Set New Position", szBuffer);
            return;
        }
        nErr = m_Esatto.getPosition(nTmpVal);
        if(!nErr) {
            snprintf(szBuffer, LOG_BUFFER_SIZE, "Current position : %d", nTmpVal);
            uiex->setText("curPosLabel",szBuffer);
        }
    } else if (!strcmp(pszEvent, "on_pushButton_2_clicked")) {
        std::string sSSID_AP;
        std::string sPWD_AP;
        std::string sSSID_STA;
        std::string sPWD_STA;
        int nWifiMode;
        char dummy[256];
        uiex->text("sSSID", dummy, 256);
        sSSID_AP.assign(dummy);
        uiex->text("sPWD", dummy, 256);
        sPWD_AP.assign(dummy);
        nWifiMode = uiex->currentIndex("comboBox");
        nErr = m_Esatto.setWiFiConfig(nWifiMode, sSSID_AP, sPWD_AP, sSSID_STA, sPWD_STA);
        if(nErr){
            snprintf(szBuffer, LOG_BUFFER_SIZE, "Error setting new WiFi parameters : Error %d", nErr);
            uiex->messageBox("Set WiFi Configuration", szBuffer);
            return;
        }
    }

    else if (!strcmp(pszEvent, "on_pushButton_3_clicked")) {
        uiex->propertyInt("maxPos", "value", nTmpVal);
        nErr = m_Esatto.setPosLimit(0, nTmpVal);
        if(nErr) {
            snprintf(szBuffer, LOG_BUFFER_SIZE, "Error setting max position : Error %d", nErr);
            uiex->messageBox("Set Max Position", szBuffer);
            return;
        }
    }
	else if (!strcmp(pszEvent, "on_comboBox_2_currentIndexChanged")) {
		uiex->propertyString("comboBox_2", "currentText", szBuffer, LOG_BUFFER_SIZE);
		m_Esatto.setLeds(std::string(szBuffer));
	}
}

#pragma mark - FocuserGotoInterface2
int	X2Focuser::focPosition(int& nPosition)
{
    int nErr;

    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());

    nErr = m_Esatto.getPosition(nPosition);
    m_nPosition = nPosition;
    return nErr;
}

int	X2Focuser::focMinimumLimit(int& nMinLimit)
{
	int nMax;

	if(!m_bLinked)
        return NOT_CONNECTED;

	X2MutexLocker ml(GetMutex());
	m_Esatto.getPosLimit(nMinLimit, nMax);
	return SB_OK;
}

int	X2Focuser::focMaximumLimit(int& nMaxLimit)
{
	int nMin;

    if(!m_bLinked)
        return NOT_CONNECTED;

	X2MutexLocker ml(GetMutex());
	m_Esatto.getPosLimit(nMin, nMaxLimit);
	return SB_OK;
}

int	X2Focuser::focAbort()
{   int nErr;

    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());
    nErr = m_Esatto.haltFocuser();
    return nErr;
}

int	X2Focuser::startFocGoto(const int& nRelativeOffset)
{
    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());
    m_Esatto.moveRelativeToPosision(nRelativeOffset);
    return SB_OK;
}

int	X2Focuser::isCompleteFocGoto(bool& bComplete) const
{
    int nErr;

    if(!m_bLinked)
        return NOT_CONNECTED;

    X2Focuser* pMe = (X2Focuser*)this;
    X2MutexLocker ml(pMe->GetMutex());
	nErr = pMe->m_Esatto.isGoToComplete(bComplete);

    return nErr;
}

int	X2Focuser::endFocGoto(void)
{
    int nErr;
    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());
    nErr = m_Esatto.getPosition(m_nPosition);
    return nErr;
}

int X2Focuser::amountCountFocGoto(void) const
{
	return 9;
}

int	X2Focuser::amountNameFromIndexFocGoto(const int& nZeroBasedIndex, BasicStringInterface& strDisplayName, int& nAmount)
{
	switch (nZeroBasedIndex)
	{
		case 0: strDisplayName="5 steps"; nAmount=5;break;
		case 1: strDisplayName="10 steps"; nAmount=10;break;
		case 2: strDisplayName="20 steps"; nAmount=20;break;
		case 3: strDisplayName="25 steps"; nAmount=25;break;
        case 4: strDisplayName="50 steps"; nAmount=50;break;
        case 5: strDisplayName="100 steps"; nAmount=100;break;
        case 6: strDisplayName="250 steps"; nAmount=250;break;
        case 7: strDisplayName="500 steps"; nAmount=500;break;
        case 8: strDisplayName="1000 steps"; nAmount=1000;break;
        default: strDisplayName="50 steps"; nAmount=50;break;
	}
	return SB_OK;
}

int	X2Focuser::amountIndexFocGoto(void)
{
	return 0;
}

#pragma mark - FocuserTemperatureInterface
int X2Focuser::focTemperature(double &dTemperature)
{
    int nErr = SB_OK;
    double dSavedTemp;

    if(!m_bLinked) {
        dTemperature = -100.0;
        return NOT_CONNECTED;
    }
    X2MutexLocker ml(GetMutex());

    // Taken from Richard's Robofocus plugin code.
    // this prevent us from asking the temperature too often
    static CStopWatch timer;
    if(timer.GetElapsedSeconds() > 30.0f || m_fLastTemp < -99.0f) {
        dSavedTemp = m_fLastTemp;
        nErr = m_Esatto.getTemperature(m_fLastTemp, EXT_T);
        if(nErr) {
            m_fLastTemp = dSavedTemp;
            nErr = SB_OK;
        }
        else if(m_fLastTemp == -127.00f) {
            nErr = m_Esatto.getTemperature(m_fLastTemp, NTC_T);
            if(m_fLastTemp == -127.00f)
                m_fLastTemp = -100.00f;
        }
        timer.Reset();
    }

    dTemperature = m_fLastTemp;

    return nErr;
}

#pragma mark - SerialPortParams2Interface

void X2Focuser::portName(BasicStringInterface& str) const
{
    char szPortName[DRIVER_MAX_STRING];

    portNameOnToCharPtr(szPortName, DRIVER_MAX_STRING);

    str = szPortName;
}

void X2Focuser::setPortName(const char* pszPort)
{
    if (m_pIniUtil)
        m_pIniUtil->writeString(PARENT_KEY, CHILD_KEY_PORTNAME, pszPort);
}


void X2Focuser::portNameOnToCharPtr(char* pszPort, const int& nMaxSize) const
{
    if (NULL == pszPort)
        return;

    snprintf(pszPort, nMaxSize, DEF_PORT_NAME);

    if (m_pIniUtil)
        m_pIniUtil->readString(PARENT_KEY, CHILD_KEY_PORTNAME, pszPort, pszPort, nMaxSize);
}




int X2Focuser::deviceIdentifier(BasicStringInterface &sIdentifier)
{
	sIdentifier = "ESATTO_ARCO";
	return SB_OK;
}

int X2Focuser::isConnectionPossible(const int &nPeerArraySize, MultiConnectionDeviceInterface **ppPeerArray, bool &bConnectionPossible)
{
	for (int nIndex = 0; nIndex < nPeerArraySize; ++nIndex)
	{
		X2Rotator *pFocuserPeer = dynamic_cast<X2Rotator*>(ppPeerArray[nIndex]);
		if (pFocuserPeer == NULL)
		{
			bConnectionPossible = false;
			return ERR_POINTER;
		}
	}

	bConnectionPossible = true;
	return SB_OK;

}

int X2Focuser::useResource(MultiConnectionDeviceInterface *pPeer)
{
	X2Rotator *pFocuserPeer = dynamic_cast<X2Rotator*>(pPeer);
	if (pFocuserPeer == NULL) {
		return ERR_POINTER; // Peer must be a power control  pointer
	}

	// Use the resources held by the specified peer
	m_pIOMutex = pFocuserPeer->m_pSavedMutex;
	m_Esatto.SetSerxPointer(pFocuserPeer->m_pSavedSerX);
	return SB_OK;

}

int X2Focuser::swapResource(MultiConnectionDeviceInterface *pPeer)
{

	X2Rotator *pFocuserPeer = dynamic_cast<X2Rotator*>(pPeer);
	if (pFocuserPeer == NULL) {
		return ERR_POINTER; //  Peer must be a power control  pointer
	}

	// Swap this driver instance's resources for the ones held by pPeer
	MutexInterface* pTempMutex = m_pSavedMutex;
	SerXInterface*  pTempSerX = m_pSavedSerX;

	m_pSavedMutex = pFocuserPeer->m_pSavedMutex;
	m_pSavedSerX = pFocuserPeer->m_pSavedSerX;

	pFocuserPeer->m_pSavedMutex = pTempMutex;
	pFocuserPeer->m_pSavedSerX = pTempSerX;

	return SB_OK;
}
