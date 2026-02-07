
#include "x2rotator.h"


X2Rotator::X2Rotator(const char* pszDriverSelection,
						const int& nInstanceIndex,
						SerXInterface					* pSerX, 
						TheSkyXFacadeForDriversInterface	* pTheSkyX, 
						SleeperInterface					* pSleeper,
						BasicIniUtilInterface			* pIniUtil,
						LoggerInterface					* pLogger,
						MutexInterface					* pIOMutex,
						TickCountInterface				* pTickCount)
{
	m_nInstanceIndex				= nInstanceIndex;
	m_pSerX							= pSerX;		
	m_pTheSkyXForMounts				= pTheSkyX;
	m_pSleeper						= pSleeper;
	m_pIniUtil						= pIniUtil;
	m_pLogger						= pLogger;	
	m_pIOMutex						= pIOMutex;
	m_pTickCount					= pTickCount;

	m_pIOMutex = pIOMutex;
	m_pSavedMutex = pIOMutex;

	m_pSavedSerX = pSerX;
	mRotator.SetSerxPointer(pSerX);

	m_nInstanceIndex = nInstanceIndex;
	m_bLinked = false;
	m_dPosition = 0;
	m_bCalibrating = false;
	m_dTargetPosition = 0;
	m_nGotoStartStamp = 0;
    
}

X2Rotator::~X2Rotator()
{

	if (GetTheSkyXFacadeForDrivers())
		delete GetTheSkyXFacadeForDrivers();
	if (GetSleeper())
		delete GetSleeper();
	if (GetSimpleIniUtil())
		delete GetSimpleIniUtil();
	if (GetLogger())
		delete GetLogger();
	if (GetTickCountInterface())
		delete GetTickCountInterface();

	if (m_pSavedSerX)
		delete m_pSavedSerX;
	if (m_pSavedMutex)
		delete m_pSavedMutex;

}

int	X2Rotator::queryAbstraction(const char* pszName, void** ppVal) 
{
	*ppVal = NULL;

	if (!strcmp(pszName, LinkInterface_Name))
		*ppVal = (LinkInterface*)this;

	else if (!strcmp(pszName, SerialPortParams2Interface_Name))
		*ppVal = dynamic_cast<SerialPortParams2Interface*>(this);

	else if (!strcmp(pszName, X2GUIEventInterface_Name))
        *ppVal = dynamic_cast<X2GUIEventInterface*>(this);

	else if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
        *ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);

	else if (!strcmp(pszName, MultiConnectionDeviceInterface_Name))
		*ppVal = dynamic_cast<MultiConnectionDeviceInterface*>(this);

	return SB_OK;
}


int X2Rotator::execModalSettingsDialog()
{
    int nErr = SB_OK;
    X2ModalUIUtil uiutil(this, GetTheSkyXFacadeForDrivers());
    X2GUIInterface*                    ui = uiutil.X2UI();
    X2GUIExchangeInterface*            dx = NULL;//Comes after ui is loaded
    bool bPressedOK = false;
    bool bReversed = false;
    bool bNewReversed = false;
    double dPos;
	std::string sHemisphere;

    if (NULL == ui)
        return ERR_POINTER;

    if ((nErr = ui->loadUserInterface("Arco.ui", deviceType(), m_nInstanceIndex)))
        return nErr;


    if (NULL == (dx = uiutil.X2DX()))
        return ERR_POINTER;

    X2MutexLocker ml(GetMutex());
    if(m_bLinked) {
        dx->setEnabled("pushButton", true);
        dx->setEnabled("pushButton_2", true);
        dx->setEnabled("doubleSpinBox", true);
        nErr = mRotator.getPosition(dPos);
        if(!nErr)
            dx->setPropertyDouble("doubleSpinBox", "value", dPos);
        dx->setEnabled("reverseDir", true);
        nErr = mRotator.getReverseEnable(bReversed);
        if(!nErr)
            dx->setChecked("reverseDir", bReversed);

		mRotator.getHemisphere(sHemisphere);
		if(sHemisphere == "northern")
			dx->setChecked("radioButton", 1);
		else
			dx->setChecked("radioButton_2", 1);
    }
    else {
        dx->setEnabled("pushButton", false);
        dx->setEnabled("pushButton_2", false);
        dx->setEnabled("doubleSpinBox", false);
        dx->setEnabled("reverseDir", false);
		dx->setEnabled("radioButton", false);
		dx->setEnabled("radioButton_2", false);
    }
	dx->setText("calibrationState", "");

    //Display the user interface
    if ((nErr = ui->exec(bPressedOK)))
        return nErr;

    //Retreive values from the user interface
    if (bPressedOK) {
        if(m_bLinked) {
            bNewReversed = dx->isChecked("reverseDir");
            if(bNewReversed != bReversed)
                mRotator.setReverseEnable(bNewReversed); // no need to set it if it didn't change
        }
    }

    return nErr;
}


void X2Rotator::uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
    double dNewPos;
    int nErr = SB_OK;
    std::string sErrMsg;
	bool bCalDone = false;

	if (!strcmp(pszEvent, "on_timer")) {
		if(m_bCalibrating) {
			mRotator.isCalibrationDone(bCalDone);
			if(bCalDone) {
				uiex->setText("pushButton", "Calibrate");
				uiex->setText("calibrationState", "Calibration done");
				m_bCalibrating = false;
			}
		}
	}

	if (!strcmp(pszEvent, "on_pushButton_clicked")) {
		if(!m_bCalibrating) {
			uiex->setText("pushButton", "Abort");
			mRotator.startCalibration();
			m_bCalibrating = true;
			uiex->setText("calibrationState", "Calibrating");

		}
		else {
			// abort
			uiex->setText("pushButton", "Calibrate");
			mRotator.stopCalibration();
			m_bCalibrating = false;
			uiex->setText("calibrationState", "");

		}
	}

    else if (!strcmp(pszEvent, "on_pushButton_2_clicked")) {
        uiex->propertyDouble("doubleSpinBox", "value", dNewPos);
        nErr = mRotator.syncMotorPosition(dNewPos);
        if(nErr) {
            sErrMsg = "Sync to new position failed : Error = " + std::to_string(nErr);
            uiex->messageBox("Sync failed", sErrMsg.c_str());
        }
    }
	else if (!strcmp(pszEvent, "on_radioButton_clicked")) {
		mRotator.setHemisphere("northern");
	}
	else if (!strcmp(pszEvent, "on_radioButton_2_clicked")) {
		mRotator.setHemisphere("southern");
	}
}


int	X2Rotator::establishLink(void)						
{
    int nErr = SB_OK;
    char szPort[SERIAL_BUFFER_SIZE];
	X2MutexLocker ml(GetMutex());
    // get serial port device name
    portNameOnToCharPtr(szPort,SERIAL_BUFFER_SIZE);
    nErr = mRotator.Connect(szPort);
    if(nErr) {
        m_bLinked = false;
    }
    else
        m_bLinked = true;

	return nErr;
}
int	X2Rotator::terminateLink(void)						
{
    int nErr = SB_OK;

	X2MutexLocker ml(GetMutex());
	// m_PegasusPPBAExtFoc.Disconnect(m_nInstanceCount);
	mRotator.Disconnect();
	// We're not connected, so revert to our saved interfaces
	mRotator.SetSerxPointer(m_pSavedSerX);
	m_pIOMutex = m_pSavedMutex;
    m_bLinked = false;
	return nErr;
}

bool X2Rotator::isLinked(void) const
{
	return m_bLinked;
}


void X2Rotator::deviceInfoNameShort(BasicStringInterface& str) const
{
	std::string sModelName;
	X2Rotator* pMe = (X2Rotator*)this;
	if(!m_bLinked) {
		str = "ARCO Rotator";
	}
	else {
		X2MutexLocker ml(pMe->GetMutex());
		pMe->mRotator.getModelName(sModelName);
		str = sModelName.c_str();
	}
}

void X2Rotator::deviceInfoNameLong(BasicStringInterface& str) const
{
	deviceInfoNameShort(str);
}

void X2Rotator::deviceInfoDetailedDescription(BasicStringInterface& str) const
{
	std::string sModelName;
	std::string sDesc;
	X2Rotator* pMe = (X2Rotator*)this;
	if(!m_bLinked) {
		str = "ARCO Rotator";
	}
	else {
		X2MutexLocker ml(pMe->GetMutex());
		pMe->mRotator.getModelName(sModelName);
		sDesc = "PrimaLuceLab ";
		sDesc.append(sModelName);
		str = sDesc.c_str();
	}
}

void X2Rotator::deviceInfoFirmwareVersion(BasicStringInterface& str)				
{
    if(m_bLinked) {
        std::string sFirmware;
        X2MutexLocker ml(GetMutex());
        mRotator.getFirmwareVersion(sFirmware);
        str = sFirmware.c_str();
    }
    else
        str = "N/A";
}

void X2Rotator::deviceInfoModel(BasicStringInterface& str)
{
	std::string sModelName;
	std::string sDesc;

	if(!m_bLinked) {
		str = "ARCO Rotator";
	}
	else {
		mRotator.getModelName(sModelName);
		sDesc = "Rotator ";
		sDesc.append(sModelName);
		str = sDesc.c_str();
	}
}

void X2Rotator::driverInfoDetailedInfo(BasicStringInterface& str) const
{
    str = str = "ARCO Rotator X2 plugin by Rodolphe Pineau";;
}
double X2Rotator::driverInfoVersion(void) const				
{
	return ARCO_PLUGIN_VERSION;
}

int	X2Rotator::position(double& dPosition)			
{
    int nErr = SB_OK;
    
    if(!m_bLinked)
        return ERR_NOLINK;

    X2MutexLocker ml(GetMutex());
    nErr = mRotator.getPosition(dPosition);

    return nErr;
}
int	X2Rotator::abort(void)							
{
    int nErr = SB_OK;
    
    if(!m_bLinked)
        return ERR_NOLINK;

    X2MutexLocker ml(GetMutex());
    nErr = mRotator.haltArco();

    return nErr;
}

int	X2Rotator::startRotatorGoto(const double& dTargetPosition)	
{
    int nErr;

    if(!m_bLinked)
        return ERR_NOLINK;

    X2MutexLocker ml(GetMutex());

    nErr = mRotator.gotoPosition(dTargetPosition);
    if(nErr)
		return nErr;

    else
        return SB_OK;
}
int	X2Rotator::isCompleteRotatorGoto(bool& bComplete) const	
{
    int nErr = SB_OK;

    if(!m_bLinked)
        return ERR_NOLINK;

    X2Rotator* pMe = (X2Rotator*)this;
    X2MutexLocker ml(pMe->GetMutex());
    nErr = pMe->mRotator.isGoToComplete(bComplete);

    if(nErr)
		return nErr;
    return nErr;

}
int	X2Rotator::endRotatorGoto(void)							
{
    int nErr = SB_OK;
    double dPosition;
    
    if(!m_bLinked)
        return ERR_NOLINK;

    X2MutexLocker ml(GetMutex());
    nErr = mRotator.getPosition(dPosition);
    if(nErr)
		return nErr;

    return nErr;
}


//
// SerialPortParams2Interface
//
#pragma mark - SerialPortParams2Interface

void X2Rotator::portName(BasicStringInterface& str) const
{
    char szPortName[SERIAL_BUFFER_SIZE];

    portNameOnToCharPtr(szPortName, SERIAL_BUFFER_SIZE);

    str = szPortName;

}

void X2Rotator::setPortName(const char* szPort)
{
    if (m_pIniUtil)
        m_pIniUtil->writeString(ARCO_PARENT_KEY, ARCO_CHILD_KEY_PORTNAME, szPort);
}


void X2Rotator::portNameOnToCharPtr(char* pszPort, const int& nMaxSize) const
{
    if (NULL == pszPort)
        return;

    snprintf(pszPort, nMaxSize,DEF_PORT_NAME);

    if (m_pIniUtil)
        m_pIniUtil->readString(ARCO_PARENT_KEY, ARCO_CHILD_KEY_PORTNAME, pszPort, pszPort, nMaxSize);
}

#pragma mark - MultiConnectionDeviceInterface

int X2Rotator::deviceIdentifier(BasicStringInterface &sIdentifier)
{
	sIdentifier = "ESATTO_ARCO";
	return SB_OK;
}

int X2Rotator::isConnectionPossible(const int &nPeerArraySize, MultiConnectionDeviceInterface **ppPeerArray, bool &bConnectionPossible)
{
	for (int nIndex = 0; nIndex < nPeerArraySize; ++nIndex)
	{
		X2Focuser *pRotatorPeer = dynamic_cast<X2Focuser*>(ppPeerArray[nIndex]);
		if (pRotatorPeer == NULL)
		{
			bConnectionPossible = false;
			return ERR_POINTER;
		}
	}

	bConnectionPossible = true;
	return SB_OK;
}

int X2Rotator::useResource(MultiConnectionDeviceInterface *pPeer)
{

	X2Focuser *pRotatorPeer = dynamic_cast<X2Focuser*>(pPeer);
	if (pRotatorPeer == NULL) {
		return ERR_POINTER; // Peer must be a focuser pointer
	}

	// Use the resources held by the specified peer
	m_pIOMutex = pRotatorPeer->m_pSavedMutex;
	mRotator.SetSerxPointer(pRotatorPeer->m_pSavedSerX);
	return SB_OK;

}

int X2Rotator::swapResource(MultiConnectionDeviceInterface *pPeer)
{

	X2Focuser *pRotatorPeer = dynamic_cast<X2Focuser*>(pPeer);
	if (pRotatorPeer == NULL) {
		return ERR_POINTER; //  Peer must be a focuser pointer
	}

	// Swap this driver instance's resources for the ones held by pPeer
	MutexInterface* pTempMutex = m_pSavedMutex;
	SerXInterface*  pTempSerX = m_pSavedSerX;

	m_pSavedMutex = pRotatorPeer->m_pSavedMutex;
	m_pSavedSerX = pRotatorPeer->m_pSavedSerX;

	pRotatorPeer->m_pSavedMutex = pTempMutex;
	pRotatorPeer->m_pSavedSerX = pTempSerX;

	return SB_OK;
}
