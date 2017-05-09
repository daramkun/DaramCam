#define _CRT_SECURE_NO_WARNINGS
#include <DaramCam.h>
#pragma comment ( lib, "DaramCam.lib" )

#include <stdio.h>
#include <string.h>

#pragma comment ( lib, "Shlwapi.lib" )

int main ( void )
{
	DCStartup ();

	IStream * stream;

	/*DWORD processes [ 4096 ] = { 0, };
	unsigned processCount;
	DCGetProcesses ( processes, &processCount );

	for ( unsigned i = 0; i < processCount; ++i )
	{
		DWORD process = processes [ i ];
		HWND hWnd = DCGetActiveWindowFromProcess ( process );
		if ( process == 0 || hWnd == 0 ) continue;
		char nameBuffer [ 4096 ] = { 0, };
		DCGetProcessName ( process, nameBuffer, 4096 );
		if ( strlen ( nameBuffer ) == 0 )
			continue;

		printf ( "%48s (DWORD: %5d, HWND: %d)\n", nameBuffer, process, hWnd );
	}

	DWORD process;
	printf ( "Select process (0 is default process): " );
	scanf ( "%d", &process );

	HWND hWnd;
	if ( process == 0 )
		hWnd = NULL;
	else
		hWnd = DCGetActiveWindowFromProcess ( process );*/

	/**/SHCreateStreamOnFileEx ( TEXT ( "Z:\\Test.png" ), STGM_READWRITE | STGM_CREATE, 0, false, 0, &stream );

	//DCGDIScreenCapturer * screenCapturer = new DCGDIScreenCapturer ( hWnd );
	//DCGDIScreenCapturer * screenCapturer = new DCGDIScreenCapturer ( 0 );
	DCDXGIScreenCapturer * screenCapturer = new DCDXGIScreenCapturer ();
	RECT region = { 1920, 0, 1920 * 2, 1080 };
	//RECT region = { 0, 0, 1920, 1080 };
	//RECT region = { 1920, 0, 1920 + 960, 540 };
	//RECT region = { 0, 0, 1920, 1080 };
	//RECT region = { 0, 0, 960, 540 };
	screenCapturer->SetRegion ( &region );
	screenCapturer->Capture ();

	DCImageGenerator * imgGen = new DCWICImageGenerator ( DCWICImageType_PNG );
	imgGen->Generate ( stream, screenCapturer->GetCapturedBitmap () );

	stream->Release ();

	delete imgGen;

	SHCreateStreamOnFileEx ( TEXT ( "Z:\\Test.gif" ), STGM_READWRITE | STGM_CREATE, 0, false, 0, &stream );

	DCVideoGenerator * vidGen = new DCWICVideoGenerator ( 33 );
	vidGen->Begin ( stream, screenCapturer );
	Sleep ( 10000 );
	vidGen->End ();
	stream->Release ();

	delete vidGen;

	delete screenCapturer;/**/

	/*
	///// Audio Capture
	std::vector<IMMDevice*> devices;
	DCWASAPIAudioCapturer::GetMultimediaDevices ( devices );

	for ( int i = 0; i < devices.size (); ++i )
	{
		printf ( "Device %d: %S\n", i, DCGetDeviceName ( devices [ i ] ) );
	}
	printf ( "> " );

	int selected;
	scanf ( "%d", &selected );

	DCWASAPIAudioCapturer * audioCapturer = new DCWASAPIAudioCapturer ( devices [ selected ] );
	DCWASAPIAudioCapturer::ReleaseMultimediaDevices ( devices );

	SHCreateStreamOnFileEx ( TEXT ( "Z:\\Test.wav" ), STGM_READWRITE | STGM_CREATE, 0, false, 0, &stream );

	DCAudioGenerator * audGen = new DCWaveAudioGenerator ();

	printf ( "Master Volume: %f\n", audioCapturer->GetVolume () );

	audGen->Begin ( stream, audioCapturer );
	Sleep ( 10000 );
	audGen->End ();

	delete audioCapturer;/**/

	DCShutdown ();

	return 0;
}