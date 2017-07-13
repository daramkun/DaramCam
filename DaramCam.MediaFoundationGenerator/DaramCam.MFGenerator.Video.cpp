#include "DaramCam.MediaFoundationGenerator.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfplay.h>
#include <mftransform.h>
#include <codecapi.h>

#pragma comment ( lib, "winmm.lib" )

#include <concurrent_queue.h>
#include <ppltasks.h>

struct MFVG_CONTAINER { DCBitmap * bitmapSource; UINT deltaTime; };

class DCMFVideoGenerator : public DCVideoGenerator
{
	friend DWORD WINAPI MFVG_Progress ( LPVOID vg );
	friend DWORD WINAPI MFVG_Capturer ( LPVOID vg );
public:
	DCMFVideoGenerator ( DWORD containerFormat, DWORD videoFormat, unsigned frameTick );
	virtual ~DCMFVideoGenerator ();

public:
	virtual void Begin ( IStream * stream, DCScreenCapturer * capturer );
	virtual void End ();

private:
	IStream * stream;
	IMFByteStream * byteStream;
	DCScreenCapturer * capturer;

	DWORD containerFormat;
	DWORD videoFormat;

	HANDLE threadHandles [ 2 ];
	bool threadRunning;

	IMFMediaSink * mediaSink;
	IMFSinkWriter * sinkWriter;

	DWORD streamIndex;

	unsigned frameTick;
	unsigned fps;

	Concurrency::concurrent_queue<MFVG_CONTAINER> capturedQueue;
};

unsigned __convertFrameTick ( unsigned frameTick )
{
	switch ( frameTick )
	{
	case DCMFVF_FRAMETICK_60FPS: return 60;
	case DCMFVF_FRAMETICK_30FPS: return 30;
	case DCMFVF_FRAMETICK_24FPS: return 24;
	default: 0;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

DARAMCAMMEDIAFOUNDATIONGENERATOR_EXPORTS DCVideoGenerator * DCCreateMFVideoGenerator ( DWORD containerFormat, DWORD videoFormat, unsigned frameTick )
{
	return new DCMFVideoGenerator ( containerFormat, videoFormat, frameTick );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

DCMFVideoGenerator::DCMFVideoGenerator ( DWORD containerFormat, DWORD videoFormat, unsigned _frameTick )
	: containerFormat ( containerFormat ), videoFormat ( videoFormat ), streamIndex ( 0 )
{
	frameTick = _frameTick;
	fps = __convertFrameTick ( _frameTick );
}

DCMFVideoGenerator::~DCMFVideoGenerator () { }

DWORD WINAPI MFVG_Progress ( LPVOID vg )
{
	DCMFVideoGenerator * videoGen = ( DCMFVideoGenerator* ) vg;

	HRESULT hr;

	videoGen->sinkWriter->BeginWriting ();

	MFTIME totalTime = 0;
	while ( videoGen->threadRunning || !videoGen->capturedQueue.empty () )
	{
		if ( videoGen->capturedQueue.empty () )
		{
			Sleep ( 0 );
			continue;
		}

		MFVG_CONTAINER container;
		if ( !videoGen->capturedQueue.try_pop ( container ) )
		{
			Sleep ( 1 );
			continue;
		}

		IMFSample * sample;
		if ( FAILED ( MFCreateSample ( &sample ) ) )
			continue;

		sample->SetSampleFlags ( 0 );
		sample->SetSampleTime ( totalTime );
		MFTIME duration = container.deltaTime * 10000ULL;
		sample->SetSampleDuration ( duration );
		totalTime += duration;

		IMFMediaBuffer * buffer;
		if ( FAILED ( MFCreateMemoryBuffer ( container.bitmapSource->GetByteArraySize (), &buffer ) ) )
			continue;
		buffer->SetCurrentLength ( container.bitmapSource->GetByteArraySize () );
		BYTE * pbBuffer;
		buffer->Lock ( &pbBuffer, nullptr, nullptr );
		unsigned stride = container.bitmapSource->GetStride ();
		for ( unsigned y = 0; y < container.bitmapSource->GetHeight (); ++y )
		{
			UINT offset1 = ( container.bitmapSource->GetHeight () - y - 1 ) * stride,
				 offset2 = y * stride;
			RtlCopyMemory ( pbBuffer + offset1, container.bitmapSource->GetByteArray () + offset2, container.bitmapSource->GetStride () );
		}
		buffer->Unlock ();

		if ( FAILED ( sample->AddBuffer ( buffer ) ) )
			continue;

		buffer->Release ();

		if ( FAILED ( hr = videoGen->sinkWriter->WriteSample ( videoGen->streamIndex, sample ) ) )
			continue;

		sample->Release ();

		delete container.bitmapSource;

		Sleep ( 1 );
	}

	videoGen->sinkWriter->Finalize ();
	videoGen->mediaSink->Shutdown ();

	return 0;
}

DWORD WINAPI MFVG_Capturer ( LPVOID vg )
{
	DCMFVideoGenerator * videoGen = ( DCMFVideoGenerator* ) vg;

	DWORD lastTick, currentTick;
	lastTick = currentTick = timeGetTime ();
	while ( videoGen->threadRunning )
	{
		if ( ( currentTick = timeGetTime () ) - lastTick >= videoGen->frameTick )
		{
			videoGen->capturer->Capture ();
			DCBitmap * bitmap = videoGen->capturer->GetCapturedBitmap ();

			MFVG_CONTAINER container;
			container.deltaTime = currentTick - lastTick;
			container.bitmapSource = bitmap->Clone ();

			videoGen->capturedQueue.push ( container );

			lastTick = currentTick;
		}

		Sleep ( 1 );
	}

	return 0;
}

void DCMFVideoGenerator::Begin ( IStream * _stream, DCScreenCapturer * _capturer )
{
	stream = _stream;
	if ( FAILED ( MFCreateMFByteStreamOnStream ( stream, &byteStream ) ) )
		return;
	capturer = _capturer;

	IMFMediaType * videoMediaType;
	if ( FAILED ( MFCreateMediaType ( &videoMediaType ) ) )
		return;
	videoMediaType->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Video );
	videoMediaType->SetGUID ( MF_MT_SUBTYPE, MFVideoFormat_H264 );
	videoMediaType->SetUINT32 ( MF_MT_AVG_BITRATE, 10000000 );
	videoMediaType->SetUINT32 ( MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_High );
	videoMediaType->SetUINT32 ( MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive );
	MFSetAttributeSize ( videoMediaType, MF_MT_FRAME_SIZE, _capturer->GetCapturedBitmap ()->GetWidth (), _capturer->GetCapturedBitmap ()->GetHeight () );
	MFSetAttributeRatio ( videoMediaType, MF_MT_FRAME_RATE, fps, 1 );
	MFSetAttributeRatio ( videoMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1 );

	if ( FAILED ( MFCreateMPEG4MediaSink ( byteStream, videoMediaType, nullptr, &mediaSink ) ) )
		return;

	videoMediaType->Release ();

	if ( FAILED ( MFCreateSinkWriterFromMediaSink ( mediaSink, nullptr, &sinkWriter ) ) )
		return;

	IMFMediaType * inputMediaType;
	if ( FAILED ( MFCreateMediaType ( &inputMediaType ) ) )
		return;
	inputMediaType->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Video );
	inputMediaType->SetGUID ( MF_MT_SUBTYPE, _capturer->GetCapturedBitmap ()->GetColorDepth () == 4 ? MFVideoFormat_RGB32 : MFVideoFormat_RGB24 );
	inputMediaType->SetUINT32 ( MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive );
	MFSetAttributeSize ( inputMediaType, MF_MT_FRAME_SIZE, _capturer->GetCapturedBitmap ()->GetWidth (), _capturer->GetCapturedBitmap ()->GetHeight () );
	MFSetAttributeRatio ( inputMediaType, MF_MT_FRAME_RATE, fps, 1 );
	MFSetAttributeRatio ( inputMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1 );

	if ( FAILED ( sinkWriter->SetInputMediaType ( streamIndex, inputMediaType, nullptr ) ) )
		return;

	inputMediaType->Release ();

	threadRunning = true;
	threadHandles [ 0 ] = CreateThread ( nullptr, 0, MFVG_Progress, this, 0, nullptr );
	threadHandles [ 1 ] = CreateThread ( nullptr, 0, MFVG_Capturer, this, 0, nullptr );
}

void DCMFVideoGenerator::End ()
{
	threadRunning = false;
	WaitForMultipleObjects ( 2, threadHandles, true, INFINITE );

	sinkWriter->Release ();
	mediaSink->Release ();
	byteStream->Release ();
	stream->Release ();
}