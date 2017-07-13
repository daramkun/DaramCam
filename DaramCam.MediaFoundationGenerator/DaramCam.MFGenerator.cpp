#include "DaramCam.MediaFoundationGenerator.h"

#pragma comment ( lib, "DaramCam.lib" )

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfplay.h>
#include <mftransform.h>

#pragma comment ( lib, "mf.lib" )
#pragma comment ( lib, "mfplat.lib" )
#pragma comment ( lib, "mfuuid.lib" )
#pragma comment ( lib, "mfreadwrite.lib" )

DARAMCAMMEDIAFOUNDATIONGENERATOR_EXPORTS void DCMFStartup ()
{
	MFStartup ( MF_VERSION );
}

DARAMCAMMEDIAFOUNDATIONGENERATOR_EXPORTS void DCMFShutdown ()
{
	MFShutdown ();
}