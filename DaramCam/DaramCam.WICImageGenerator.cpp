#include "DaramCam.h"
#include <wincodecsdk.h>
#pragma comment ( lib, "windowscodecs.lib" )

DCWICImageGenerator::DCWICImageGenerator ( DCWICImageType imageType )
{
	switch ( imageType )
	{
	case DCWICImageType_BMP: containerGUID = GUID_ContainerFormatBmp; break;
	case DCWICImageType_JPEG: containerGUID = GUID_ContainerFormatJpeg; break;
	case DCWICImageType_PNG: containerGUID = GUID_ContainerFormatPng; break;
	case DCWICImageType_TIFF: containerGUID = GUID_ContainerFormatTiff; break;
	}

	CoCreateInstance ( CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, ( LPVOID* ) &piFactory );
}

DCWICImageGenerator::~DCWICImageGenerator ()
{
	if ( piFactory )
		piFactory->Release ();
}

void DCWICImageGenerator::Generate ( IStream * stream, DCBitmap * bitmap )
{
	IWICStream * piStream = nullptr;
	piFactory->CreateStream ( &piStream );

	piStream->InitializeFromIStream ( stream );

	IWICBitmapEncoder * piEncoder;
	piFactory->CreateEncoder ( containerGUID, NULL, &piEncoder );
	piEncoder->Initialize ( piStream, WICBitmapEncoderNoCache );

	IWICBitmapFrameEncode *piBitmapFrame = NULL;
	IPropertyBag2 *pPropertybag = NULL;
	piEncoder->CreateNewFrame ( &piBitmapFrame, &pPropertybag );
	piBitmapFrame->Initialize ( pPropertybag );

	piBitmapFrame->SetSize ( bitmap->GetWidth (), bitmap->GetHeight () );

	WICPixelFormatGUID formatGUID = GUID_WICPixelFormat24bppBGR;
	piBitmapFrame->SetPixelFormat ( &formatGUID );

	piBitmapFrame->WritePixels ( bitmap->GetHeight (), bitmap->GetStride (), bitmap->GetByteArraySize (), bitmap->GetByteArray () );

	piBitmapFrame->Commit ();

	if ( piBitmapFrame )
		piBitmapFrame->Release ();

	piEncoder->Commit ();

	if ( piEncoder )
		piEncoder->Release ();

	if ( piStream )
		piStream->Release ();
}
