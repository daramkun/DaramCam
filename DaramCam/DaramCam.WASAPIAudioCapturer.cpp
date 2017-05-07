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
	HRESULT hr = CoCreateInstance ( CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, ( void** ) &pEnumerator );

	pEnumerator->GetDefaultAudioEndpoint ( eRender, eConsole, &pDevice );

	pDevice->Activate ( IID_IAudioClient, CLSCTX_ALL, NULL, ( void** ) &pAudioClient );

	pAudioClient->GetMixFormat ( &pwfx );
	switch ( pwfx->wFormatTag )
	{
	case WAVE_FORMAT_IEEE_FLOAT:
		pwfx->wFormatTag = WAVE_FORMAT_PCM;
		pwfx->wBitsPerSample = 16;
		pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
		pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
		break;

	case WAVE_FORMAT_EXTENSIBLE:
		{
			PWAVEFORMATEXTENSIBLE pEx = reinterpret_cast<PWAVEFORMATEXTENSIBLE>( pwfx );
			if ( IsEqualGUID ( KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, pEx->SubFormat ) )
			{
				pEx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
				pEx->Samples.wValidBitsPerSample = 16;
				pwfx->wBitsPerSample = 16;
				pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
				pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
			}
		}
		break;
	}

	REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
	pAudioClient->Initialize ( AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, hnsRequestedDuration, 0, pwfx, NULL );

	REFERENCE_TIME hnsDefaultDevicePeriod;
	pAudioClient->GetDevicePeriod ( &hnsDefaultDevicePeriod, NULL );
	pAudioClient->GetBufferSize ( &bufferFrameCount );

	hr = pAudioClient->GetService ( IID_IAudioCaptureClient, ( void** ) &pCaptureClient );

	byteArray = new char [ byteArrayLength = 46000 * 128 ];

	hWakeUp = CreateWaitableTimer ( NULL, FALSE, NULL );

	LARGE_INTEGER liFirstFire;
	liFirstFire.QuadPart = -hnsDefaultDevicePeriod / 2;
	LONG lTimeBetweenFires = ( LONG ) hnsDefaultDevicePeriod / 2 / ( 10 * 1000 );
	BOOL bOK = SetWaitableTimer (
		hWakeUp,
		&liFirstFire,
		lTimeBetweenFires,
		NULL, NULL, FALSE
	);
}

DCWASAPIAudioCapturer::~DCWASAPIAudioCapturer ()
{
	CloseHandle ( hWakeUp );

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
	DWORD totalLength = 0;

	pCaptureClient->GetNextPacketSize ( &packetLength );

	bool bFirstPacket = true;
	HRESULT hr;
	for ( hr = pCaptureClient->GetNextPacketSize ( &packetLength ); SUCCEEDED ( hr ) && packetLength > 0; hr = pCaptureClient->GetNextPacketSize ( &packetLength ) )
	{
		BYTE *pData;
		UINT32 nNumFramesToRead;
		DWORD dwFlags;

		if ( FAILED ( hr = pCaptureClient->GetBuffer ( &pData, &nNumFramesToRead, &dwFlags, NULL, NULL ) ) )
			return nullptr;

		if ( !( bFirstPacket && AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY == dwFlags ) && ( 0 != dwFlags ) )
			return nullptr;

		if ( 0 == nNumFramesToRead )
			return nullptr;

		LONG lBytesToWrite = nNumFramesToRead * pwfx->nBlockAlign;
		memcpy ( byteArray + totalLength, pData, lBytesToWrite );
		totalLength += lBytesToWrite;

		if ( FAILED ( hr = pCaptureClient->ReleaseBuffer ( nNumFramesToRead ) ) )
			return nullptr;

		bFirstPacket = false;
	}

	DWORD dwWaitResult = WaitForMultipleObjects ( 1, &hWakeUp, FALSE, INFINITE );

	*bufferLength = totalLength;
	return byteArray;
}
