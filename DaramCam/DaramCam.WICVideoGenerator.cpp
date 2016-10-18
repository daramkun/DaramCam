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

	memset ( &propValue, 0, sizeof ( propValue ) );
	propValue.vt = VT_UI1 | VT_VECTOR;
	propValue.caub.cElems = 11;
	propValue.caub.pElems = new UCHAR [ 11 ];
	memcpy ( propValue.caub.pElems, "NETSCAPE2.0", 11 );
	queryWriter->SetMetadataByName ( L"/appext/Application", &propValue );
	delete [] propValue.caub.pElems;

	// From: http://www.pixcl.com/WIC-and-Animated-GIF-Files.htm
	memset ( &propValue, 0, sizeof ( propValue ) );
	propValue.vt = VT_UI1 | VT_VECTOR;
	propValue.caub.cElems = 5;
	propValue.caub.pElems = new UCHAR [ 5 ];
	*( propValue.caub.pElems ) = 3; // must be > 1,
	*( propValue.caub.pElems + 1 ) = 1; // defines animated GIF
	*( propValue.caub.pElems + 2 ) = 0; // LSB 0 = infinite loop.
	*( propValue.caub.pElems + 3 ) = 0; // MSB of iteration count value
	*( propValue.caub.pElems + 4 ) = 0; // NULL == end of data
	queryWriter->SetMetadataByName ( TEXT ( "/appext/Data" ), &propValue );
	delete [] propValue.caub.pElems;

	//PropVariantClear ( &propValue );
	memset ( &propValue, 0, sizeof ( propValue ) );
	propValue.vt = VT_UI2;
	propValue.uiVal = ( USHORT ) videoGen->capturer->GetCapturedBitmap ().GetWidth ();
	queryWriter->SetMetadataByName ( TEXT ( "/logscrdesc/Width" ), &propValue );
	memset ( &propValue, 0, sizeof ( propValue ) );
	propValue.vt = VT_UI2;
	propValue.uiVal = ( USHORT ) videoGen->capturer->GetCapturedBitmap ().GetHeight ();
	queryWriter->SetMetadataByName ( TEXT ( "/logscrdesc/Height" ), &propValue );
	PropVariantClear ( &propValue );

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

		piBitmapFrame->GetMetadataQueryWriter ( &queryWriter );
		piBitmapFrame->Commit ();

		memset ( &propValue, 0, sizeof ( propValue ) );
		propValue.vt = VT_UI2;
		propValue.uiVal = videoGen->frameTick / 10;
		queryWriter->SetMetadataByName ( TEXT ( "/grctlext/Delay" ), &propValue );
		queryWriter->Release ();

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

void DCWICVideoGenerator::Begin ( IStream * _stream, DCScreenCapturer * _capturer )
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
