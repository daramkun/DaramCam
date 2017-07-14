#include "DaramCam.MediaFoundationGenerator.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfplay.h>
#include <mftransform.h>

#include <atomic>

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

	std::atomic_bool threadRunning;
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
	if ( containerFormat == DCMFCONTAINER_ADTS || containerFormat == DCMFCONTAINER_MP4 )
	{
		if ( audioFormat != DCMFAF_AAC )
			return nullptr;
	}
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

	IMFSample * sample;
	MFCreateSample ( &sample );
	IMFMediaBuffer * buffer;
	MFCreateMemoryBuffer ( audioGen->capturer->GetSamplerate (), &buffer );
	sample->AddBuffer ( buffer );

	audioGen->capturer->Begin ();
	audioGen->sinkWriter->BeginWriting ();

	MFTIME totalDuration = 0;
	while ( audioGen->threadRunning )
	{
		unsigned bufferLength;
		void * data = audioGen->capturer->GetAudioData ( &bufferLength );
		if ( data == nullptr || bufferLength == 0 )
			continue;

		sample->SetSampleFlags ( 0 );
		sample->SetSampleTime ( totalDuration );
		MFTIME duration = ( bufferLength * 10000000ULL ) / audioGen->capturer->GetByterate ();
		sample->SetSampleDuration ( duration );
		totalDuration += duration;

		buffer->SetCurrentLength ( bufferLength );
		BYTE * pbBuffer;
		buffer->Lock ( &pbBuffer, nullptr, nullptr );
		RtlCopyMemory ( pbBuffer, data, bufferLength );
		buffer->Unlock ();

		audioGen->sinkWriter->WriteSample ( audioGen->streamIndex, sample );

		Sleep ( 0 );
	}

	buffer->Release ();
	sample->Release ();

	audioGen->sinkWriter->Flush ( audioGen->streamIndex );
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
	if ( audioFormat == DCMFAF_AAC )
	{
		audioMediaType->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Audio );
		audioMediaType->SetGUID ( MF_MT_SUBTYPE, MFAudioFormat_AAC );
		audioMediaType->SetUINT32 ( MF_MT_AUDIO_BITS_PER_SAMPLE, 16 );
		audioMediaType->SetUINT32 ( MF_MT_AUDIO_SAMPLES_PER_SECOND, _capturer->GetSamplerate () );
		audioMediaType->SetUINT32 ( MF_MT_AUDIO_NUM_CHANNELS, _capturer->GetChannels () );
		audioMediaType->SetUINT32 ( MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 24000 );
		audioMediaType->SetUINT32 ( MF_MT_AAC_PAYLOAD_TYPE, 0 );
	}
	else return;

	if ( containerFormat == DCMFCONTAINER_MP4 )
	{
		if ( FAILED ( hr = MFCreateMPEG4MediaSink ( byteStream, nullptr, audioMediaType, &mediaSink ) ) )
			return;
	}
	else if ( containerFormat == DCMFCONTAINER_ADTS )
	{
		if ( FAILED ( hr = MFCreateADTSMediaSink ( byteStream, audioMediaType, &mediaSink ) ) )
			return;
	}
	else return;

	audioMediaType->Release ();

	IMFAttributes * attr;
	MFCreateAttributes ( &attr, 1 );
	attr->SetUINT32 ( MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, 1 );
	attr->SetUINT32 ( MF_SINK_WRITER_DISABLE_THROTTLING, 1 );

	if ( FAILED ( hr = MFCreateSinkWriterFromMediaSink ( mediaSink, attr, &sinkWriter ) ) )
		return;

	attr->Release ();

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
