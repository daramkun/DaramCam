#include "DaramCam.h"

#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

const CLSID CLSID_MMDeviceEnumerator = __uuidof( MMDeviceEnumerator );
const IID IID_IMMDeviceEnumerator = __uuidof( IMMDeviceEnumerator );
const IID IID_IAudioClient = __uuidof( IAudioClient );
const IID IID_IAudioCaptureClient = __uuidof( IAudioCaptureClient );

DCWASAPIAudioCapturer::DCWASAPIAudioCapturer ()
	: byteArray ( nullptr )
{
	CoCreateInstance ( CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, ( void** ) &pEnumerator );

	pEnumerator->GetDefaultAudioEndpoint ( eCapture, eConsole, &pDevice );

	pDevice->Activate ( IID_IAudioClient, CLSCTX_ALL, NULL, ( void** ) &pAudioClient );

	pAudioClient->GetMixFormat ( &pwfx );


	REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
	pAudioClient->Initialize ( AUDCLNT_SHAREMODE_SHARED, 0, hnsRequestedDuration, 0, pwfx, NULL );

	UINT32 bufferFrameCount;
	pAudioClient->GetBufferSize ( &bufferFrameCount );

	pAudioClient->GetService ( IID_IAudioCaptureClient, ( void** ) &pCaptureClient );
}

DCWASAPIAudioCapturer::~DCWASAPIAudioCapturer ()
{
	if ( byteArray )
		delete [] byteArray;

	CoTaskMemFree ( pwfx );

	pCaptureClient->Release ();
	pAudioClient->Release ();
	pDevice->Release ();
	pEnumerator->Release ();
}

void DCWASAPIAudioCapturer::Begin ()
{
	pAudioClient->Start ();
}

void DCWASAPIAudioCapturer::End ()
{
	pAudioClient->Stop ();
}

unsigned DCWASAPIAudioCapturer::GetChannels () { return pwfx->nChannels; }
unsigned DCWASAPIAudioCapturer::GetBitsPerSample () { return pwfx->wBitsPerSample; }
unsigned DCWASAPIAudioCapturer::GetSamplerate () { return pwfx->nSamplesPerSec; }

void * DCWASAPIAudioCapturer::GetAudioData ( unsigned * bufferLength )
{
	UINT32 packetLength = 0;
	DWORD flags;
	UINT32 numFramesAvailable;
	BYTE *pData;

	pCaptureClient->GetNextPacketSize ( &packetLength );
	if ( packetLength != byteArrayLength )
	{
		if ( byteArray )
			delete [] byteArray;
		byteArray = new char [ byteArrayLength = packetLength ];
	}

	pCaptureClient->GetBuffer ( &pData, &numFramesAvailable, &flags, NULL, NULL );

	if ( flags & AUDCLNT_BUFFERFLAGS_SILENT )
	{
		return nullptr;
	}

	memcpy ( byteArray, pData, byteArrayLength );
	pCaptureClient->ReleaseBuffer ( numFramesAvailable );

	*bufferLength = byteArrayLength;
	return byteArray;
}
