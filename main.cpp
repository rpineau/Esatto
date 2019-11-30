#include <stdio.h>
#include "../../licensedinterfaces/basicstringinterface.h"
#include "main.h"
#include "x2focuser.h"



extern "C" PlugInExport int sbPlugInName2(BasicStringInterface& str)
{
	str = PLUGIN_NAME;

	return 0;
}

extern "C" PlugInExport int sbPlugInFactory2(	const char* pszDisplayName, 
												const int& nInstanceIndex,
												SerXInterface					* pSerXIn, 
												TheSkyXFacadeForDriversInterface	* pTheSkyXIn, 
												SleeperInterface					* pSleeperIn,
												BasicIniUtilInterface			* pIniUtilIn,
												LoggerInterface					* pLoggerIn,
												MutexInterface					* pIOMutexIn,
												TickCountInterface				* pTickCountIn,
												void** ppObjectOut)
{
    *ppObjectOut = NULL;
	X2Focuser* gpMyImpl=NULL;

	if (NULL == gpMyImpl)
		gpMyImpl = new X2Focuser(	pszDisplayName, 
									nInstanceIndex,
									pSerXIn, 
									pTheSkyXIn, 
									pSleeperIn,
									pIniUtilIn,
									pLoggerIn,
									pIOMutexIn,
									pTickCountIn);


	*ppObjectOut = gpMyImpl;

	return 0;
}

