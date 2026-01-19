#ifndef __MAIN__
#define __MAIN__
#ifdef SB_WIN_BUILD
	#define PlugInExport __declspec(dllexport)
#else
	#define PlugInExport
#endif
#include <stdio.h>
#include "x2rotator.h"
#include "../../licensedinterfaces/basicstringinterface.h"

extern "C" PlugInExport int sbPlugInDisplayName(BasicStringInterface& str);

extern "C" PlugInExport int sbPlugInFactory(	const char* pszDisplayName, 
												const int& nInstanceIndex,
												SerXInterface					* pSerXIn, 
												TheSkyXFacadeForDriversInterface* pTheSkyXIn, 
												SleeperInterface		* pSleeperIn,
												BasicIniUtilInterface  * pIniUtilIn,
												LoggerInterface			* pLoggerIn,
												MutexInterface			* pIOMutexIn,
												TickCountInterface		* pTickCountIn,
												void** ppObjectOut);
#endif
