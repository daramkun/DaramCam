#include "DaramCam.MediaFoundationGenerator.h"
#include "DaramCam.MediaFoundationGenerator.Internal.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfplay.h>
#include <mftransform.h>
#include <codecapi.h>

#pragma comment ( lib, "winmm.lib" )

#include <atomic>

#pragma intrinsic(memcpy)

struct MFVG_CONTAINER { DCBitmap * bitmapSource; UINT deltaTime; };

class DCMFVideoGenerator : public DCVideoGenerator
{
	friend DWORD WINAPI MFVG_Progress ( LPVOID vg );
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

	HANDLE threadHandle;
	std::atomic_bool threadRunning;

	IMFMediaSink * mediaSink;
	IMFSinkWriter * sinkWriter;

	DWORD streamIndex;

	unsigned frameTick;
	unsigned fps;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

DARAMCAMMEDIAFOUNDATIONGENERATOR_EXPORTS DCVideoGenerator * DCCreateMFVideoGenerator ( DWORD containerFormat, DWORD videoFormat, unsigned frameTick )
{
	if ( containerFormat != DCMFCONTAINER_MP4 ) return nullptr;

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

	DCBitmap * bitmap = videoGen->capturer->GetCapturedBitmap ();
	bitmap->SetIsDirectMode ( true );
	unsigned stride = bitmap->GetStride (), height = bitmap->GetHeight ();

	IMFSample * sample;
	MFCreateSample ( &sample );
	IMFMediaBuffer * buffer;
	MFCreateMemoryBuffer ( bitmap->GetByteArraySize (), &buffer );
	sample->AddBuffer ( buffer );

	videoGen->sinkWriter->BeginWriting ();

	MFTIME totalTime = 0;
	DWORD lastTick, currentTick;
	lastTick = currentTick = timeGetTime ();
	while ( videoGen->threadRunning )
	{
		if ( ( currentTick = timeGetTime () ) - lastTick >= videoGen->frameTick )
		{
			sample->SetSampleFlags ( 0 );
			sample->SetSampleTime ( totalTime );
			MFTIME duration = ( currentTick - lastTick ) * 10000ULL;
			sample->SetSampleDuration ( duration );
			totalTime += duration;

			buffer->SetCurrentLength ( bitmap->GetByteArraySize () );
			BYTE * pbBuffer;
			buffer->Lock ( &pbBuffer, nullptr, nullptr );
			videoGen->capturer->Capture ( true );
			buffer->Unlock ();

			videoGen->sinkWriter->WriteSample ( videoGen->streamIndex, sample );

			lastTick = currentTick;
		}
		//Sleep ( 0 );
	}

	buffer->Release ();
	sample->Release ();

	videoGen->sinkWriter->Flush ( videoGen->streamIndex );
	videoGen->sinkWriter->Finalize ();
	videoGen->mediaSink->Shutdown ();

	bitmap->SetIsDirectMode ( false );

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
	videoMediaType->SetUINT32 ( MF_MT_AVG_BITRATE, _capturer->GetCapturedBitmap ()->GetWidth () * _capturer->GetCapturedBitmap ()->GetHeight () * 5 );
	videoMediaType->SetUINT32 ( MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Main );
	videoMediaType->SetUINT32 ( MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive );
	MFSetAttributeSize ( videoMediaType, MF_MT_FRAME_SIZE, _capturer->GetCapturedBitmap ()->GetWidth (), _capturer->GetCapturedBitmap ()->GetHeight () );
	MFSetAttributeRatio ( videoMediaType, MF_MT_FRAME_RATE, fps, 1 );
	MFSetAttributeRatio ( videoMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1 );

	if ( FAILED ( MFCreateMPEG4MediaSink ( byteStream, videoMediaType, nullptr, &mediaSink ) ) )
		return;

	videoMediaType->Release ();

	IMFAttributes * attr;
	MFCreateAttributes ( &attr, 1 );
	attr->SetUINT32 ( MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, 1 );
	attr->SetUINT32 ( MF_SINK_WRITER_DISABLE_THROTTLING, 1 );

	if ( FAILED ( MFCreateSinkWriterFromMediaSink ( mediaSink, attr, &sinkWriter ) ) )
		return;

	attr->Release ();

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
	threadHandle = CreateThread ( nullptr, 0, MFVG_Progress, this, 0, nullptr );
}

void DCMFVideoGenerator::End ()
{
	threadRunning = false;
	WaitForSingleObject ( threadHandle, INFINITE );

	sinkWriter->Release ();
	mediaSink->Release ();
	byteStream->Release ();
	stream->Release ();
}