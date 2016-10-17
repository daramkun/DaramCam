#include "DaramCam.h"
#include <wincodecsdk.h>
#pragma comment ( lib, "windowscodecs.lib" )

DCWICVideoGenerator::DCWICVideoGenerator ( unsigned _frameTick )
{
	CoCreateInstance ( CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, ( LPVOID* ) &piFactory );

	frameTick = _frameTick;
}

DCWICVideoGenerator::~DCWICVideoGenerator ()
{
	if ( piFactory )
		piFactory->Release ();
}

DWORD WINAPI WICVG_Progress ( LPVOID vg )
{
	DCWICVideoGenerator * videoGen = ( DCWICVideoGenerator* ) vg;

	IWICStream * piStream;
	IWICBitmapEncoder * piEncoder;

	videoGen->piFactory->CreateStream ( &piStream );
	piStream->InitializeFromIStream ( videoGen->stream );
	
	videoGen->piFactory->CreateEncoder ( GUID_ContainerFormatGif, NULL, &piEncoder );
	HRESULT hr = piEncoder->Initialize ( piStream, WICBitmapEncoderNoCache );

	IWICMetadataQueryWriter * queryWriter;
	piEncoder->GetMetadataQueryWriter ( &queryWriter );
	PROPVARIANT propValue = { 0, };
	propValue.vt = VT_UI2;
	propValue.uiVal = videoGen->frameTick;
	queryWriter->SetMetadataByName ( TEXT ( "/grctlext/Delay" ), &propValue );
	queryWriter->Release ();

	IWICBitmapFrameEncode *piBitmapFrame = NULL;
	IPropertyBag2 *pPropertybag = NULL;

	while ( videoGen->threadRunning )
	{
		hr = piEncoder->CreateNewFrame ( &piBitmapFrame, &pPropertybag );
		if ( hr != S_OK )
		{
			videoGen->threadRunning = false;
			continue;
		}
		hr = piBitmapFrame->Initialize ( pPropertybag );

		videoGen->capturer->Capture ();
		DCBitmap * bitmap = &videoGen->capturer->GetCapturedBitmap ();
		piBitmapFrame->SetSize ( bitmap->GetWidth (), bitmap->GetHeight () );
		auto bitmapSource = bitmap->ToWICBitmap ( videoGen->piFactory );

		piBitmapFrame->WriteSource ( bitmapSource, nullptr );
		piBitmapFrame->Commit ();
		piBitmapFrame->Release ();
		bitmapSource->Release ();

		Sleep ( videoGen->frameTick );
	}

	piEncoder->Commit ();

	if ( piEncoder )
		piEncoder->Release ();

	if ( piStream )
		piStream->Release ();

	return 0;
}

void DCWICVideoGenerator::Begin ( IStream * _stream, DCCapturer * _capturer )
{
	stream = _stream;
	capturer = _capturer;

	threadRunning = true;
	threadHandle = CreateThread ( nullptr, 0, WICVG_Progress, this, 0, 0 );
}

void DCWICVideoGenerator::End ()
{
	threadRunning = false;
	WaitForSingleObject ( threadHandle, INFINITE );
}
