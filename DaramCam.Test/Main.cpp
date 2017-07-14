#define _CRT_SECURE_NO_WARNINGS
#include <DaramCam.h>
#pragma comment ( lib, "DaramCam.lib" )
#include <DaramCam.MediaFoundationGenerator.h>
#pragma comment ( lib, "DaramCam.MediaFoundationGenerator.lib" )

#include <stdio.h>
#include <string.h>

#pragma comment ( lib, "Shlwapi.lib" )

int main ( void )
{
	DCStartup ();
	DCMFStartup ();

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

	///// Image Capture
	/**/
	SHCreateStreamOnFileEx ( TEXT ( "Z:\\Test.png" ), STGM_READWRITE | STGM_CREATE, 0, false, 0, &stream );

	//DCScreenCapturer * screenCapturer = new DCCreateGDIScreenCapturer ( hWnd );
	//DCScreenCapturer * screenCapturer = DCCreateGDIScreenCapturer ( 0 );
	DCScreenCapturer * screenCapturer = DCCreateDXGIScreenCapturer ( DCDXGIScreenCapturerRange_SubMonitors );
	screenCapturer->Capture ();

	DCImageGenerator * imgGen = DCCreateWICImageGenerator ( DCWICImageType_PNG );
	SIZE size = { 640, 360 };
	DCSetSizeToWICImageGenerator ( imgGen, &size );
	imgGen->Generate ( stream, screenCapturer->GetCapturedBitmap () );

	stream->Release ();

	delete imgGen;

	SHCreateStreamOnFileEx ( TEXT ( "Z:\\Test.mp4" ), STGM_READWRITE | STGM_CREATE, 0, false, 0, &stream );

	//DCVideoGenerator * vidGen = DCCreateWICVideoGenerator ( WICVG_FRAMETICK_24FPS );
	DCVideoGenerator * vidGen = DCCreateMFVideoGenerator ( DCMFCONTAINER_MP4, DCMFVF_H264, DCMFVF_FRAMETICK_60FPS );
	//DCSetSizeToWICVideoGenerator ( vidGen, &size );
	vidGen->Begin ( stream, screenCapturer );
	Sleep ( 10000 );
	vidGen->End ();
	stream->Release ();

	delete vidGen;

	delete screenCapturer;
	/**/

	///// Audio Capture
	/*
	std::vector<CComPtr<IMMDevice>> devices;
	DCGetMultimediaDevices ( devices );

	for ( int i = 0; i < devices.size (); ++i )
	{
		printf ( "Device %d: %S\n", i, DCGetDeviceName ( devices [ i ] ) );
	}
	printf ( "> " );

	int selected;
	scanf ( "%d", &selected );

	DCAudioCapturer * audioCapturer = DCCreateWASAPIAudioCapturer ( devices [ selected ] );/**/
	/*
	DCAudioCapturer * audioCapturer = DCCreateWASAPILoopbackAudioCapturer ();
	
	//SHCreateStreamOnFileEx ( TEXT ( "Z:\\Test.adts" ), STGM_READWRITE | STGM_CREATE, 0, false, 0, &stream );
	SHCreateStreamOnFileEx ( TEXT ( "Z:\\Test.m4a" ), STGM_READWRITE | STGM_CREATE, 0, false, 0, &stream );

	//DCAudioGenerator * audGen = DCCreateWaveAudioGenerator ();
	DCAudioGenerator * audGen = DCCreateMFAudioGenerator ( DCMFCONTAINER_MP4, DCMFAF_AAC );

	audGen->Begin ( stream, audioCapturer );
	Sleep ( 10000 );
	audGen->End ();

	delete audGen;

	delete audioCapturer;
	/**/

	///// Audio and Video Capture
	/*
	SHCreateStreamOnFileEx ( TEXT ( "Z:\\Test.mp4" ), STGM_READWRITE | STGM_CREATE, 0, false, 0, &stream );
	DCScreenCapturer * screenCapturer = DCCreateDXGIScreenCapturer ( DCDXGIScreenCapturerRange_SubMonitors );
	DCAudioCapturer * audioCapturer = DCCreateWASAPILoopbackAudioCapturer ();

	DCAudioVideoGenerator * audvidGen = DCCreateMFAudioVideoGenerator ( DCMFCONTAINER_MP4, DCMFVF_H264, DCMFAF_AAC, DCMFVF_FRAMETICK_60FPS );

	audvidGen->Begin ( stream, screenCapturer, audioCapturer );
	Sleep ( 10000 );
	audvidGen->End ();

	delete audioCapturer;
	delete screenCapturer;/**/

	DCMFShutdown ();
	DCShutdown ();

	return 0;
}