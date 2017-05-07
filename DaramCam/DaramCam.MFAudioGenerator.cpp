#include "DaramCam.h"
#include <stdio.h>

DCMFAudioGenerator::DCMFAudioGenerator ( DCMFAudioType _audioType )
{
	audioType = _audioType;
}

DCMFAudioGenerator::~DCMFAudioGenerator ()
{

}

DWORD WINAPI MFAG_Progress ( LPVOID ag )
{
	DCMFAudioGenerator * audioGen = ( DCMFAudioGenerator* ) ag;

	HRESULT hr;

	IMFByteStream * byteStream = NULL;
	//HRESULT hr = MFCreateMFByteStreamOnStream ( audioGen->stream, &byteStream );
	hr = audioGen->stream->QueryInterface ( __uuidof( IMFByteStream ), ( void** ) &byteStream );

	IMFMediaType * mediaType;
	hr = MFCreateMediaType ( &mediaType );
	mediaType->SetGUID ( MF_MT_MAJOR_TYPE, MFMediaType_Audio );
	switch ( audioGen->audioType )
	{
	case DCMFAudioType_MP3:
		mediaType->SetGUID ( MF_MT_SUBTYPE, MFAudioFormat_MP3 );
		mediaType->SetUINT32 ( MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 320 );
		break;
	case DCMFAudioType_WAV:
		mediaType->SetGUID ( MF_MT_SUBTYPE, MFAudioFormat_PCM );
		break;
	case DCMFAudioType_AAC:
		mediaType->SetGUID ( MF_MT_SUBTYPE, MFAudioFormat_AAC );
		mediaType->SetUINT32 ( MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 24000 );
		mediaType->SetUINT32 ( MF_MT_AAC_PAYLOAD_TYPE, 0 );
		mediaType->SetUINT32 ( MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0x29 );
		break;
	}
	mediaType->SetUINT32 ( MF_MT_AUDIO_NUM_CHANNELS, audioGen->capturer->GetChannels () );
	mediaType->SetUINT32 ( MF_MT_AUDIO_BITS_PER_SAMPLE, audioGen->capturer->GetBitsPerSample () );
	mediaType->SetUINT32 ( MF_MT_AUDIO_SAMPLES_PER_SECOND, audioGen->capturer->GetSamplerate () );

	IMFMediaSink * mediaSink = nullptr;
	switch ( audioGen->audioType )
	{
	case DCMFAudioType_MP3: hr = MFCreateMP3MediaSink ( byteStream, &mediaSink ); break;
	//case DCMFAudioType_WAV: hr = MFCreateWAVEMediaSink ( byteStream, mediaType, &mediaSink ); break;
	case DCMFAudioType_AAC: hr = MFCreateADTSMediaSink ( byteStream, mediaType, &mediaSink ); break;
	}
	IMFSinkWriter * sinkWriter;
	hr = MFCreateSinkWriterFromMediaSink ( mediaSink, nullptr, &sinkWriter );

	mediaType->Release ();

	DCAudioCapturer * capturer = audioGen->capturer;
	capturer->Begin ();

	DWORD frameCount = 0;
	hr = sinkWriter->AddStream ( mediaType, &frameCount );

	hr = sinkWriter->BeginWriting ();

	while ( audioGen->threadRunning )
	{
		unsigned bufferLength;
		void * audioData = capturer->GetAudioData ( &bufferLength );

		IMFSample * sample;
		hr = MFCreateSample ( &sample );
		IMFMediaBuffer * buffer;
		hr = MFCreateMemoryBuffer ( bufferLength, &buffer );
		
		BYTE * pBuffer;
		hr = buffer->Lock ( &pBuffer, nullptr, nullptr );
		memcpy ( pBuffer, audioData, bufferLength );
		hr = buffer->Unlock ();

		hr = sample->AddBuffer ( buffer );
		hr = sinkWriter->WriteSample ( frameCount, sample );
		hr = sinkWriter->Flush ( frameCount );
		sample->Release ();
		buffer->Release ();
		printf ( "%d(%d), %d\n", frameCount, bufferLength, audioGen->threadRunning );
	}
	sinkWriter->Finalize ();
	capturer->End ();

	sinkWriter->Release ();
	mediaSink->Shutdown ();
	mediaSink->Release ();
	byteStream->Release ();

	return 0;
}

void DCMFAudioGenerator::Begin ( IStream * _stream, DCAudioCapturer * _capturer )
{
	capturer = _capturer;
	stream = _stream;
	
	threadRunning = true;
	threadHandle = CreateThread ( nullptr, 0, MFAG_Progress, this, 0, 0 );
}

void DCMFAudioGenerator::End ()
{
	threadRunning = false;
	WaitForSingleObject ( threadHandle, INFINITE );
}
