#include "DaramCam.h"

#pragma comment ( lib, "winmm.lib" )

#include <ppltasks.h>

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

	memset ( &propValue, 0, sizeof ( propValue ) );
	propValue.vt = VT_UI2;
	propValue.uiVal = ( USHORT ) videoGen->capturer->GetCapturedBitmap ()->GetWidth ();
	queryWriter->SetMetadataByName ( TEXT ( "/logscrdesc/Width" ), &propValue );
	memset ( &propValue, 0, sizeof ( propValue ) );
	propValue.vt = VT_UI2;
	propValue.uiVal = ( USHORT ) videoGen->capturer->GetCapturedBitmap ()->GetHeight ();
	queryWriter->SetMetadataByName ( TEXT ( "/logscrdesc/Height" ), &propValue );
	PropVariantClear ( &propValue );

	queryWriter->Release ();

	IWICBitmapFrameEncode *piBitmapFrame = NULL;
	IPropertyBag2 *pPropertybag = NULL;

	memset ( &propValue, 0, sizeof ( propValue ) );
	propValue.vt = VT_UI2;

	while ( videoGen->threadRunning || !videoGen->capturedQueue.empty () )
	{
		if ( videoGen->capturedQueue.empty () )
		{
			Sleep ( 0 );
			continue;
		}

		piEncoder->CreateNewFrame ( &piBitmapFrame, &pPropertybag );
		if ( hr != S_OK )
		{
			videoGen->threadRunning = false;
			return -1;
		}
		piBitmapFrame->Initialize ( pPropertybag );

		WICVG_CONTAINER container;
		if ( !videoGen->capturedQueue.try_pop ( container ) )
		{
			Sleep ( 1 );
			continue;
		}
		UINT imgWidth, imgHeight;
		container.bitmapSource->GetSize ( &imgWidth, &imgHeight );
		piBitmapFrame->SetSize ( imgWidth, imgHeight );
		piBitmapFrame->WriteSource ( container.bitmapSource, nullptr );

		piBitmapFrame->GetMetadataQueryWriter ( &queryWriter );
		piBitmapFrame->Commit ();

		propValue.uiVal = ( WORD ) container.deltaTime;
		queryWriter->SetMetadataByName ( TEXT ( "/grctlext/Delay" ), &propValue );
		queryWriter->Release ();

		container.bitmapSource->Release ();

		piBitmapFrame->Release ();

		Sleep ( 1 );
	}

	piEncoder->Commit ();

	if ( piEncoder )
		piEncoder->Release ();

	if ( piStream )
		piStream->Release ();

	return 0;
}

DWORD WINAPI WICVG_Capturer ( LPVOID vg )
{
	DCWICVideoGenerator * videoGen = ( DCWICVideoGenerator* ) vg;

	DWORD lastTick, currentTick;
	lastTick = currentTick = timeGetTime ();
	while ( videoGen->threadRunning )
	{
		if ( ( currentTick = timeGetTime () ) - lastTick >= videoGen->frameTick )
		{
			videoGen->capturer->Capture ();
			DCBitmap * bitmap = videoGen->capturer->GetCapturedBitmap ();

			WICVG_CONTAINER container;
			container.deltaTime = ( currentTick - lastTick ) / 10;
			container.bitmapSource = bitmap->ToWICBitmap ( videoGen->piFactory, false );

			videoGen->capturedQueue.push ( container );

			lastTick = currentTick;
		}

		Sleep ( 1 );
	}

	return 0;
}

void DCWICVideoGenerator::Begin ( IStream * _stream, DCScreenCapturer * _capturer )
{
	stream = _stream;
	capturer = _capturer;

	threadRunning = true;
	threadHandles [ 0 ] = CreateThread ( nullptr, 0, WICVG_Progress, this, 0, nullptr );
	threadHandles [ 1 ] = CreateThread ( nullptr, 0, WICVG_Capturer, this, 0, nullptr );
}

void DCWICVideoGenerator::End ()
{
	threadRunning = false;
	//WaitForSingleObject ( threadHandle, INFINITE );
	WaitForMultipleObjects ( 2, threadHandles, true, INFINITE );
}
