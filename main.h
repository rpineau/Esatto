#ifdef SB_WIN_BUILD
	#define PlugInExport __declspec(dllexport)
#else
	#define PlugInExport
#endif

#define PLUGIN_NAME "X2Focuser Essato"

#include "x2focuser.h"

extern "C" PlugInExport int sbPlugInName2(BasicStringInterface& str);

extern "C" PlugInExport int sbPlugInFactory2(	const char* pszDisplayName, 
												const int& nInstanceIndex,
												SerXInterface					* pSerXIn, 
												TheSkyXFacadeForDriversInterface	* pTheSkyXIn, 
												SleeperInterface					* pSleeperIn,
												BasicIniUtilInterface			* pIniUtilIn,
												LoggerInterface					* pLoggerIn,
												MutexInterface					* pIOMutexIn,
												TickCountInterface				* pTickCountIn,
												void** ppObjectOut);
