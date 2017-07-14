#include "DaramCam.MediaFoundationGenerator.h"
#include "DaramCam.MediaFoundationGenerator.Internal.h"

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

class DCMFAudioVideoGenerator : public DCAudioVideoGenerator
{
	friend DWORD WINAPI MFVG_Progress2 ( LPVOID vg );
	friend DWORD WINAPI MFAG_Progress2 ( LPVOID vg );
public:
	DCMFAudioVideoGenerator ( DWORD containerFormat, DWORD videoFormat, DWORD audioFormat, unsigned frameTick );
	virtual ~DCMFAudioVideoGenerator ();

public:
	virtual void Begin ( IStream * stream, DCScreenCapturer * capturer, DCAudioCapturer * audioCapturer );
	virtual void End ();

private:
	IStream * stream;
	IMFByteStream * byteStream;
	DCScreenCapturer * videoCapturer;
	DCAudioCapturer * audioCapturer;

	DWORD containerFormat;
	DWORD videoFormat;
	DWORD audioFormat;

	HANDLE threadHandles [ 2 ];
	bool threadRunning, writingStarted;

	IMFMediaSink * mediaSink;
	IMFSinkWriter * sinkWriter;

	DWORD videoStreamIndex, audioStreamIndex;

	unsigned frameTick;
	unsigned fps;

	Concurrency::concurrent_queue<MFVG_CONTAINER> capturedQueue;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

DARAMCAMMEDIAFOUNDATIONGENERATOR_EXPORTS DCAudioVideoGenerator * DCCreateMFAudioVideoGenerator ( DWORD containerFormat, DWORD videoFormat, DWORD audioFormat, unsigned frameTick )
{
	return new DCMFAudioVideoGenerator ( containerFormat, videoFormat, audioFormat, frameTick );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

DCMFAudioVideoGenerator::DCMFAudioVideoGenerator ( DWORD containerFormat, DWORD videoFormat, DWORD audioFormat, unsigned _frameTick )
	: containerFormat ( containerFormat ), videoFormat ( videoFormat ), audioFormat ( audioFormat ),
	  videoStreamIndex ( 0 ), audioStreamIndex ( 1 )
{
	frameTick = _frameTick;
	fps = __convertFrameTick ( _frameTick );
}

DCMFAudioVideoGenerator::~DCMFAudioVideoGenerator () { }

DWORD WINAPI MFVG_Progress2 ( LPVOID vg )
{
	DCMFAudioVideoGenerator * videoGen = ( DCMFAudioVideoGenerator* ) vg;

	videoGen->sinkWriter->BeginWriting ();
	videoGen->writingStarted = true;

	MFTIME totalTime = 0;
	DWORD lastTick, currentTick;
	lastTick = currentTick = timeGetTime ();
	while ( videoGen->threadRunning || !videoGen->capturedQueue.empty () )
	{
		if ( ( currentTick = timeGetTime () ) - lastTick >= videoGen->frameTick )
		{
			videoGen->videoCapturer->Capture ();
			DCBitmap * bitmap = videoGen->videoCapturer->GetCapturedBitmap ();

			IMFSample * sample;
			MFCreateSample ( &sample );

			sample->SetSampleFlags ( 0 );
			sample->SetSampleTime ( totalTime );
			MFTIME duration = ( currentTick - lastTick ) * 10000ULL;
			sample->SetSampleDuration ( duration );
			totalTime += duration;

			IMFMediaBuffer * buffer;
			MFCreateMemoryBuffer ( bitmap->GetByteArraySize (), &buffer );
			buffer->SetCurrentLength ( bitmap->GetByteArraySize () );
			BYTE * pbBuffer;
			buffer->Lock ( &pbBuffer, nullptr, nullptr );
			unsigned stride = bitmap->GetStride ();
			for ( unsigned y = 0; y < bitmap->GetHeight (); ++y )
			{
				UINT offset1 = ( bitmap->GetHeight () - y - 1 ) * stride,
					offset2 = y * stride;
				RtlCopyMemory ( pbBuffer + offset1, bitmap->GetByteArray () + offset2, bitmap->GetStride () );
			}
			buffer->Unlock ();

			sample->AddBuffer ( buffer );

			buffer->Release ();

			videoGen->sinkWriter->WriteSample ( videoGen->videoStreamIndex, sample );

			sample->Release ();

			lastTick = currentTick;
		}
		Sleep ( 1 );
	}

	videoGen->writingStarted = false;
	videoGen->sinkWriter->Flush ( videoGen->videoStreamIndex );
	videoGen->sinkWriter->Finalize ();
	videoGen->mediaSink->Shutdown ();

	return 0;
}

DWORD WINAPI MFAG_Progress2 ( LPVOID vg )
{
	DCMFAudioVideoGenerator * audioGen = ( DCMFAudioVideoGenerator* ) vg;

	while ( !audioGen->writingStarted );

	audioGen->audioCapturer->Begin ();
	
	HRESULT hr;

	MFTIME totalDuration = 0;
	while ( audioGen->threadRunning && audioGen->writingStarted )
	{
		unsigned bufferLength;
		void * data = audioGen->audioCapturer->GetAudioData ( &bufferLength );
		if ( data == nullptr || bufferLength == 0 )
			continue;

		IMFSample * sample;
		if ( FAILED ( hr = MFCreateSample ( &sample ) ) )
			continue;

		sample->SetSampleFlags ( 0 );
		sample->SetSampleTime ( totalDuration );
		MFTIME duration = ( bufferLength * 10000000ULL ) / audioGen->audioCapturer->GetByterate ();
		sample->SetSampleDuration ( duration );
		totalDuration += duration;

		IMFMediaBuffer * buffer;
		if ( FAILED ( hr = MFCreateMemoryBuffer ( bufferLength, &buffer ) ) )
			continue;
		buffer->SetCurrentLength ( bufferLength );
		BYTE * pbBuffer;
		buffer->Lock ( &pbBuffer, nullptr, nullptr );
		RtlCopyMemory ( pbBuffer, data, bufferLength );
		buffer->Unlock ();

		if ( FAILED ( hr = sample->AddBuffer ( buffer ) ) )
			continue;

		buffer->Release ();

		if ( FAILED ( hr = audioGen->sinkWriter->WriteSample ( audioGen->audioStreamIndex, sample ) ) )
			continue;

		sample->Release ();
	}
	audioGen->audioCapturer->End ();

	return 0;
}

void DCMFAudioVideoGenerator::Begin ( IStream * _stream, DCScreenCapturer * _videoCapturer, DCAudioCapturer * _audioCapturer )
{
	stream = _stream;
	if ( FAILED ( MFCreateMFByteStreamOnStream ( stream, &byteStream ) ) )
		return;
	videoCapturer = _videoCapturer;
	audioCapturer = _audioCapturer;

	IMFMediaType * videoMediaType;
	if ( FAILED ( MFCreateMediaType ( &videoMediaType ) ) )
		return;
	videoMediaType->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Video );
	videoMediaType->SetGUID ( MF_MT_SUBTYPE, MFVideoFormat_H264 );
	videoMediaType->SetUINT32 ( MF_MT_AVG_BITRATE, _videoCapturer->GetCapturedBitmap ()->GetWidth () * _videoCapturer->GetCapturedBitmap ()->GetHeight () * 5 );
	videoMediaType->SetUINT32 ( MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_High );
	videoMediaType->SetUINT32 ( MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive );
	MFSetAttributeSize ( videoMediaType, MF_MT_FRAME_SIZE, _videoCapturer->GetCapturedBitmap ()->GetWidth (), _videoCapturer->GetCapturedBitmap ()->GetHeight () );
	MFSetAttributeRatio ( videoMediaType, MF_MT_FRAME_RATE, fps, 1 );
	MFSetAttributeRatio ( videoMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1 );

	IMFMediaType * audioMediaType;
	if ( FAILED ( MFCreateMediaType ( &audioMediaType ) ) )
		return;
	audioMediaType->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Audio );
	audioMediaType->SetGUID ( MF_MT_SUBTYPE, MFAudioFormat_AAC );
	audioMediaType->SetUINT32 ( MF_MT_AUDIO_BITS_PER_SAMPLE, 16 );
	audioMediaType->SetUINT32 ( MF_MT_AUDIO_SAMPLES_PER_SECOND, audioCapturer->GetSamplerate () );
	audioMediaType->SetUINT32 ( MF_MT_AUDIO_NUM_CHANNELS, audioCapturer->GetChannels () );
	audioMediaType->SetUINT32 ( MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 24000 );
	audioMediaType->SetUINT32 ( MF_MT_AAC_PAYLOAD_TYPE, 0 );

	if ( FAILED ( MFCreateFMPEG4MediaSink ( byteStream, videoMediaType, audioMediaType, &mediaSink ) ) )
		return;

	audioMediaType->Release ();
	videoMediaType->Release ();

	IMFAttributes * attr;
	MFCreateAttributes ( &attr, 1 );
	attr->SetUINT32 ( MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, 1 );
	attr->SetUINT32 ( MF_SINK_WRITER_DISABLE_THROTTLING, 1 );

	if ( FAILED ( MFCreateSinkWriterFromMediaSink ( mediaSink, attr, &sinkWriter ) ) )
		return;

	attr->Release ();

	IMFMediaType * inputVideoMediaType;
	if ( FAILED ( MFCreateMediaType ( &inputVideoMediaType ) ) )
		return;
	inputVideoMediaType->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Video );
	inputVideoMediaType->SetGUID ( MF_MT_SUBTYPE, _videoCapturer->GetCapturedBitmap ()->GetColorDepth () == 4 ? MFVideoFormat_RGB32 : MFVideoFormat_RGB24 );
	inputVideoMediaType->SetUINT32 ( MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive );
	MFSetAttributeSize ( inputVideoMediaType, MF_MT_FRAME_SIZE, _videoCapturer->GetCapturedBitmap ()->GetWidth (), _videoCapturer->GetCapturedBitmap ()->GetHeight () );
	MFSetAttributeRatio ( inputVideoMediaType, MF_MT_FRAME_RATE, fps, 1 );
	MFSetAttributeRatio ( inputVideoMediaType, MF_MT_PIXEL_ASPECT_RATIO, 1, 1 );

	if ( FAILED ( sinkWriter->SetInputMediaType ( videoStreamIndex, inputVideoMediaType, nullptr ) ) )
		return;

	inputVideoMediaType->Release ();

	IMFMediaType * inputAudioMediaType;
	if ( FAILED ( MFCreateMediaType ( &inputAudioMediaType ) ) )
		return;
	inputAudioMediaType->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Audio );
	inputAudioMediaType->SetGUID ( MF_MT_SUBTYPE, MFAudioFormat_PCM );
	inputAudioMediaType->SetUINT32 ( MF_MT_AUDIO_BITS_PER_SAMPLE, 16 );
	inputAudioMediaType->SetUINT32 ( MF_MT_AUDIO_SAMPLES_PER_SECOND, audioCapturer->GetSamplerate () );
	inputAudioMediaType->SetUINT32 ( MF_MT_AUDIO_NUM_CHANNELS, audioCapturer->GetChannels () );

	if ( FAILED ( sinkWriter->SetInputMediaType ( audioStreamIndex, inputAudioMediaType, nullptr ) ) )
		return;

	inputAudioMediaType->Release ();

	threadRunning = true;
	writingStarted = false;
	threadHandles [ 0 ] = CreateThread ( nullptr, 0, MFVG_Progress2, this, 0, nullptr );
	threadHandles [ 1 ] = CreateThread ( nullptr, 0, MFAG_Progress2, this, 0, nullptr );
}

void DCMFAudioVideoGenerator::End ()
{
	threadRunning = false;
	WaitForMultipleObjects ( 2, threadHandles, true, INFINITE );

	sinkWriter->Release ();
	mediaSink->Release ();
	byteStream->Release ();
	stream->Release ();
}