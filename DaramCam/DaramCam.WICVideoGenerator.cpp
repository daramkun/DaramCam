#include "DaramCam.h"

DCWICVideoGenerator::DCWICVideoGenerator ()
{
	gifContainerGUID = GUID_ContainerFormatGif;
	CoCreateInstance ( CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, ( LPVOID* ) &piFactory );
}

DCWICVideoGenerator::~DCWICVideoGenerator ()
{
	if ( piFactory )
		piFactory->Release ();
}

void DCWICVideoGenerator::Begin ( IStream * stream, DCCapturer * capturer )
{

}

void DCWICVideoGenerator::End ()
{

}
