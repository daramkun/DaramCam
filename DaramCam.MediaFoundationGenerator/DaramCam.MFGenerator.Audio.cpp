#include "DaramCam.MediaFoundationGenerator.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfplay.h>
#include <mftransform.h>

class DCMFAudioGenerator : public DCAudioGenerator
{
	friend DWORD WINAPI MFAG_Progress ( LPVOID vg );
public:
	DCMFAudioGenerator ( DWORD containerFormat, DWORD audioFormat );
	~DCMFAudioGenerator ();

public:
	virtual void Begin ( IStream * stream, DCAudioCapturer * capturer );
	virtual void End ();

private:
	IStream * stream;
	IMFByteStream * byteStream;
	DCAudioCapturer * capturer;

	DWORD containerFormat;
	DWORD audioFormat;

	bool threadRunning;
	HANDLE threadHandle;

	IMFMediaSink * mediaSink;
	IMFSinkWriter * sinkWriter;

	DWORD streamIndex;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

DARAMCAMMEDIAFOUNDATIONGENERATOR_EXPORTS DCAudioGenerator * DCCreateMFAudioGenerator ( DWORD containerFormat, DWORD audioFormat )
{
	return new DCMFAudioGenerator ( containerFormat, audioFormat );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

DCMFAudioGenerator::DCMFAudioGenerator ( DWORD containerFormat, DWORD audioFormat )
	: stream ( nullptr ), capturer ( nullptr ),
	  containerFormat ( containerFormat ), audioFormat ( audioFormat ),
	  streamIndex ( 0 )
{


}

DCMFAudioGenerator::~DCMFAudioGenerator ()
{


}

DWORD WINAPI MFAG_Progress ( LPVOID vg )
{
	DCMFAudioGenerator * audioGen = ( DCMFAudioGenerator* ) vg;

	audioGen->capturer->Begin ();
	audioGen->sinkWriter->BeginWriting ();

	HRESULT hr;

	unsigned total = 0;
	MFTIME totalDuration = 0;
	while ( audioGen->threadRunning )
	{
		unsigned bufferLength;
		void * data = audioGen->capturer->GetAudioData ( &bufferLength );
		if ( data == nullptr || bufferLength == 0 )
			continue;

		IMFSample * sample;
		if ( FAILED ( hr = MFCreateSample ( &sample ) ) )
			continue;

		sample->SetSampleFlags ( 0 );
		sample->SetSampleTime ( totalDuration );
		MFTIME duration = ( bufferLength / ( audioGen->capturer->GetByterate () ) ) * 10000ULL;
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

		if ( FAILED ( hr = audioGen->sinkWriter->WriteSample ( audioGen->streamIndex, sample ) ) )
			continue;

		sample->Release ();

		total += bufferLength;
	}

	//audioGen->sinkWriter->Flush ( audioGen->streamIndex );
	audioGen->sinkWriter->Finalize ();
	audioGen->capturer->End ();

	audioGen->mediaSink->Shutdown ();

	return 0;
}

void DCMFAudioGenerator::Begin ( IStream * _stream, DCAudioCapturer * _capturer )
{
	HRESULT hr;

	stream = _stream;
	if ( FAILED ( hr = MFCreateMFByteStreamOnStream ( stream, &byteStream ) ) )
		return;
	capturer = _capturer;

	IMFMediaType * audioMediaType;
	if ( FAILED ( hr = MFCreateMediaType ( &audioMediaType ) ) )
		return;
	audioMediaType->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Audio );
	audioMediaType->SetGUID ( MF_MT_SUBTYPE, MFAudioFormat_AAC );
	audioMediaType->SetUINT32 ( MF_MT_AUDIO_BITS_PER_SAMPLE, 16 );
	audioMediaType->SetUINT32 ( MF_MT_AUDIO_SAMPLES_PER_SECOND, _capturer->GetSamplerate () );
	audioMediaType->SetUINT32 ( MF_MT_AUDIO_NUM_CHANNELS, _capturer->GetChannels () );
	audioMediaType->SetUINT32 ( MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 24000 );

	if ( FAILED ( hr = MFCreateMPEG4MediaSink ( byteStream, nullptr, audioMediaType, &mediaSink ) ) )
		return;

	audioMediaType->Release ();

	if ( FAILED ( hr = MFCreateSinkWriterFromMediaSink ( mediaSink, nullptr, &sinkWriter ) ) )
		return;

	IMFMediaType * inputMediaType;
	if ( FAILED ( hr = MFCreateMediaType ( &inputMediaType ) ) )
		return;
	inputMediaType->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Audio );
	inputMediaType->SetGUID ( MF_MT_SUBTYPE, MFAudioFormat_PCM );
	inputMediaType->SetUINT32 ( MF_MT_AUDIO_BITS_PER_SAMPLE, 16 );
	inputMediaType->SetUINT32 ( MF_MT_AUDIO_SAMPLES_PER_SECOND, _capturer->GetSamplerate () );
	inputMediaType->SetUINT32 ( MF_MT_AUDIO_NUM_CHANNELS, _capturer->GetChannels () );

	if ( FAILED ( hr = sinkWriter->SetInputMediaType ( streamIndex, inputMediaType, nullptr ) ) )
		return;

	inputMediaType->Release ();

	threadRunning = true;
	threadHandle = CreateThread ( nullptr, 0, MFAG_Progress, this, 0, 0 );
}

void DCMFAudioGenerator::End ()
{
	threadRunning = false;
	WaitForSingleObject ( threadHandle, INFINITE );

	sinkWriter->Release ();
	mediaSink->Release ();
	byteStream->Release ();
}
