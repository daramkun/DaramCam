#include "DaramCam.h"
#include "DaramCam.Internal.h"

class DARAMCAM_EXPORTS DCWICImageGenerator : public DCImageGenerator
{
public:
	DCWICImageGenerator ( DCWICImageType imageType );
	virtual ~DCWICImageGenerator ();

public:
	virtual void Generate ( IStream * stream, DCBitmap * bitmap );

private:
	GUID containerGUID;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

DARAMCAM_EXPORTS DCImageGenerator * DCCreateWICImageGenerator ( DCWICImageType imageType )
{
	return new DCWICImageGenerator ( imageType );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

DCWICImageGenerator::DCWICImageGenerator ( DCWICImageType imageType )
{
	switch ( imageType )
	{
	case DCWICImageType_BMP: containerGUID = GUID_ContainerFormatBmp; break;
	case DCWICImageType_JPEG: containerGUID = GUID_ContainerFormatJpeg; break;
	case DCWICImageType_PNG: containerGUID = GUID_ContainerFormatPng; break;
	case DCWICImageType_TIFF: containerGUID = GUID_ContainerFormatTiff; break;
	}
}

DCWICImageGenerator::~DCWICImageGenerator () { }

void DCWICImageGenerator::Generate ( IStream * stream, DCBitmap * bitmap )
{
	IWICStream * piStream = nullptr;
	g_piFactory->CreateStream ( &piStream );

	piStream->InitializeFromIStream ( stream );

	IWICBitmapEncoder * piEncoder;
	g_piFactory->CreateEncoder ( containerGUID, NULL, &piEncoder );
	piEncoder->Initialize ( piStream, WICBitmapEncoderNoCache );

	IWICBitmapFrameEncode *piBitmapFrame = NULL;
	IPropertyBag2 *pPropertybag = NULL;
	piEncoder->CreateNewFrame ( &piBitmapFrame, &pPropertybag );
	piBitmapFrame->Initialize ( pPropertybag );

	piBitmapFrame->SetSize ( bitmap->GetWidth (), bitmap->GetHeight () );

	WICPixelFormatGUID formatGUID = bitmap->GetColorDepth () == 3 ? GUID_WICPixelFormat24bppBGR : GUID_WICPixelFormat32bppBGRA;
	piBitmapFrame->SetPixelFormat ( &formatGUID );

	WICRect wicRect = { 0, 0, ( int ) bitmap->GetWidth (), ( int ) bitmap->GetHeight () };
	auto wicBitmap = bitmap->ToWICBitmap ();
	piBitmapFrame->WriteSource ( wicBitmap, &wicRect );
	wicBitmap->Release ();

	piBitmapFrame->Commit ();

	if ( piBitmapFrame )
		piBitmapFrame->Release ();

	piEncoder->Commit ();

	if ( piEncoder )
		piEncoder->Release ();

	if ( piStream )
		piStream->Release ();
}
