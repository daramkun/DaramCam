#include "DaramCam.h"

#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

const CLSID CLSID_MMDeviceEnumerator = __uuidof( MMDeviceEnumerator );
const IID IID_IMMDeviceEnumerator = __uuidof( IMMDeviceEnumerator );
const IID IID_IAudioClient = __uuidof( IAudioClient );
const IID IID_IAudioCaptureClient = __uuidof( IAudioCaptureClient );
const IID IID_ISimpleAudioVolume = __uuidof( ISimpleAudioVolume );

DCWASAPIAudioCapturer::DCWASAPIAudioCapturer ( IMMDevice * pDevice )
	: byteArray ( nullptr )
{
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
	pAudioClient->Initialize ( AUDCLNT_SHAREMODE_SHARED, GetStreamFlags (), hnsRequestedDuration, 0, pwfx, NULL );

	REFERENCE_TIME hnsDefaultDevicePeriod;
	pAudioClient->GetDevicePeriod ( &hnsDefaultDevicePeriod, NULL );
	pAudioClient->GetBufferSize ( &bufferFrameCount );

	pAudioClient->GetService ( IID_IAudioCaptureClient, ( void** ) &pCaptureClient );
	pAudioClient->GetService ( IID_ISimpleAudioVolume, ( void** ) &pAudioVolume );
	originalVolume = GetVolume ();

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

	SetVolume ( originalVolume );
	pAudioVolume->Release ();

	pCaptureClient->Release ();
	pAudioClient->Release ();
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

	HRESULT hr;
	hr = pCaptureClient->GetNextPacketSize ( &packetLength );

	bool bFirstPacket = true;
	while ( SUCCEEDED ( hr ) && packetLength > 0 )
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

		hr = pCaptureClient->GetNextPacketSize ( &packetLength );
	}

	DWORD dwWaitResult = WaitForMultipleObjects ( 1, &hWakeUp, FALSE, INFINITE );

	*bufferLength = totalLength;
	return byteArray;
}

float DCWASAPIAudioCapturer::GetVolume ()
{
	float temp;
	pAudioVolume->GetMasterVolume ( &temp );
	return temp;
}

void DCWASAPIAudioCapturer::SetVolume ( float volume )
{
	pAudioVolume->SetMasterVolume ( volume, NULL );
}

DWORD DCWASAPIAudioCapturer::GetStreamFlags () { return 0; }

void DCWASAPIAudioCapturer::GetMultimediaDevices ( std::vector<IMMDevice*> & devices )
{
	IMMDeviceEnumerator *pEnumerator;
	IMMDeviceCollection * pCollection;

	CoCreateInstance ( CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, ( void** ) &pEnumerator );
	pEnumerator->EnumAudioEndpoints ( eCapture, DEVICE_STATE_ACTIVE, &pCollection );

	UINT collectionCount;
	pCollection->GetCount ( &collectionCount );
	for ( UINT i = 0; i < collectionCount; ++i )
	{
		IMMDevice * pDevice;
		pCollection->Item ( i, &pDevice );
		devices.push_back ( pDevice );
	}

	pCollection->Release ();
	pEnumerator->Release ();
}

void DCWASAPIAudioCapturer::ReleaseMultimediaDevices ( std::vector<IMMDevice*> & devices )
{
	for ( auto i = devices.begin (); i != devices.end (); ++i )
		( *i )->Release ();
	devices.clear ();
}

IMMDevice * DCWASAPIAudioCapturer::GetDefaultMultimediaDevice ()
{
	IMMDeviceEnumerator *pEnumerator;
	IMMDevice * pDevice;

	CoCreateInstance ( CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, ( void** ) &pEnumerator );
	pEnumerator->GetDefaultAudioEndpoint ( eRender, eConsole, &pDevice );
	pEnumerator->Release ();

	return pDevice;
}

DCWASAPILoopbackAudioCapturer::DCWASAPILoopbackAudioCapturer ()
	: DCWASAPIAudioCapturer ( DCWASAPIAudioCapturer::GetDefaultMultimediaDevice () )
{

}

DWORD DCWASAPILoopbackAudioCapturer::GetStreamFlags () { return AUDCLNT_STREAMFLAGS_LOOPBACK; }
