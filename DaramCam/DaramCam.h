#ifndef __DARAMKUN_DARAMCAM_H__
#define __DARAMKUN_DARAMCAM_H__

#include <Windows.h>
#include <wincodec.h>
#include <wincodecsdk.h>
#include <Audioclient.h>
#include <Mmdeviceapi.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <shlwapi.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <d3d9.h>
#include <gl/GL.h>

#if ! ( defined ( _WIN32 ) || defined ( _WIN64 ) || defined ( WINDOWS ) || defined ( _WINDOWS ) )
#error "This library is for Windows only."
#endif

#ifndef	DARAMCAM_EXPORTS
#define	DARAMCAM_EXPORTS					__declspec(dllimport)
#else
#undef	DARAMCAM_EXPORTS
#define	DARAMCAM_EXPORTS					__declspec(dllexport)
#endif

DARAMCAM_EXPORTS void DCStartup ();
DARAMCAM_EXPORTS void DCShutdown ();
DARAMCAM_EXPORTS void DCGetProcesses ( DWORD * buffer, unsigned * bufferSize );
DARAMCAM_EXPORTS void DCGetProcessName ( DWORD pId, char * nameBuffer, unsigned nameBufferSize );
DARAMCAM_EXPORTS HWND DCGetActiveWindowFromProcess ( DWORD pId );

// 24-bit Fixed RGB Color Bitmap Data Structure
class DARAMCAM_EXPORTS DCBitmap
{
public:
	DCBitmap ( unsigned width, unsigned height );
	~DCBitmap ();

public:
	unsigned char * GetByteArray ();
	unsigned GetWidth ();
	unsigned GetHeight ();
	unsigned GetStride ();

	unsigned GetByteArraySize ();

	IWICBitmap * ToWICBitmap ( IWICImagingFactory * factory );

public:
	void Resize ( unsigned width, unsigned height );

public:
	COLORREF GetColorRef ( unsigned x, unsigned y );
	void SetColorRef ( COLORREF colorRef, unsigned x, unsigned y );

public:
	void CopyFrom ( HDC hDC, HBITMAP hBitmap );
	void CopyFrom ( IDXGISurface * dxgiSurface );
	void CopyFrom ( IDirect3DSurface9 * d3dSurface );

private:
	unsigned char * byteArray;
	unsigned width, height, stride;
};

// Abstract Screen Capturer
class DARAMCAM_EXPORTS DCScreenCapturer
{
public:
	virtual ~DCScreenCapturer ();

public:
	virtual void Capture () = 0;
	virtual DCBitmap * GetCapturedBitmap () = 0;
};

// GDI Screen Capturer
class DARAMCAM_EXPORTS DCGDIScreenCapturer : public DCScreenCapturer
{
public:
	DCGDIScreenCapturer ( HWND hWnd = NULL );
	virtual ~DCGDIScreenCapturer ();

public:
	virtual void Capture ();
	virtual DCBitmap* GetCapturedBitmap ();

public:
	void SetRegion ( RECT * region = nullptr );

private:
	HWND hWnd;
	HDC desktopDC;
	HDC captureDC;
	HBITMAP captureBitmap;

	RECT * captureRegion;

	DCBitmap capturedBitmap;
};

// DXGI Screen Capturer
class DARAMCAM_EXPORTS DCDXGIScreenCapturer : public DCScreenCapturer
{
public:
	DCDXGIScreenCapturer ();
	virtual ~DCDXGIScreenCapturer ();

public:
	virtual void Capture ();
	virtual DCBitmap * GetCapturedBitmap ();

public:
	void SetRegion ( RECT * region = nullptr );

private:
	void* dxgiManager;
	BYTE* tempBits;

	RECT * captureRegion;
	DCBitmap capturedBitmap;
};

// Abstract Audio Capturer
class DARAMCAM_EXPORTS DCAudioCapturer
{
public:
	virtual ~DCAudioCapturer ();

public:
	virtual void Begin () = 0;
	virtual void End () = 0;

	virtual unsigned GetChannels () = 0;
	virtual unsigned GetBitsPerSample () = 0;
	virtual unsigned GetSamplerate () = 0;

	virtual void* GetAudioData ( unsigned * bufferLength ) = 0;
};

// WASAPI Audio Capturer
class DARAMCAM_EXPORTS DCWASAPIAudioCapturer : public DCAudioCapturer
{
public:
	DCWASAPIAudioCapturer ();
	virtual ~DCWASAPIAudioCapturer ();

public:
	virtual void Begin ();
	virtual void End ();

	virtual unsigned GetChannels ();
	virtual unsigned GetBitsPerSample ();
	virtual unsigned GetSamplerate ();

	virtual void* GetAudioData ( unsigned * bufferLength );

private:
	IMMDeviceEnumerator *pEnumerator;
	IMMDevice *pDevice;
	IAudioClient *pAudioClient;
	IAudioCaptureClient * pCaptureClient;

	WAVEFORMATEX *pwfx;

	char * byteArray;
	unsigned byteArrayLength;

	UINT32 bufferFrameCount;

	HANDLE hWakeUp;
};

// Abstract Image File Generator
class DARAMCAM_EXPORTS DCImageGenerator
{
public:
	virtual ~DCImageGenerator ();

public:
	virtual void Generate ( IStream * stream, DCBitmap * bitmap ) = 0;
};

// Image Types for WIC Image Generator
enum DCWICImageType
{
	DCWICImageType_BMP,
	DCWICImageType_JPEG,
	DCWICImageType_PNG,
	DCWICImageType_TIFF,
};

// Image File Generator via Windows Imaging Component
// Can generate BMP, JPEG, PNG, TIFF
class DARAMCAM_EXPORTS DCWICImageGenerator : public DCImageGenerator
{
public:
	DCWICImageGenerator ( DCWICImageType imageType );
	virtual ~DCWICImageGenerator ();

public:
	virtual void Generate ( IStream * stream, DCBitmap * bitmap );

private:
	IWICImagingFactory * piFactory;
	GUID containerGUID;
};

// Abstract Video File Generator
class DARAMCAM_EXPORTS DCVideoGenerator
{
public:
	virtual ~DCVideoGenerator ();

public:
	virtual void Begin ( IStream * stream, DCScreenCapturer * capturer ) = 0;
	virtual void End () = 0;
};

// Video File Generator via Windows Imaging Component
// Can generate GIF
class DARAMCAM_EXPORTS DCWICVideoGenerator : public DCVideoGenerator
{
	friend DWORD WINAPI WICVG_Progress ( LPVOID vg );
public:
	DCWICVideoGenerator ( unsigned frameTick = 42 );
	virtual ~DCWICVideoGenerator ();

public:
	virtual void Begin ( IStream * stream, DCScreenCapturer * capturer );
	virtual void End ();

private:
	IWICImagingFactory * piFactory;

	IStream * stream;
	DCScreenCapturer * capturer;

	HANDLE threadHandle;
	bool threadRunning;

	unsigned frameTick;
};

enum DCMFContainerType
{
	DCMFContainerType_MP4,
};

struct DCMFVideoConfig
{
public:
	unsigned bitrate;
};

// Video File Generator via Windows Media Foundation
// Can generate MP4
// Cannot use in Windows N/KN
class DARAMCAM_EXPORTS DCMFVideoGenerator : public DCVideoGenerator
{
public:
	DCMFVideoGenerator ( DCMFContainerType containerType = DCMFContainerType_MP4, unsigned fps = 30 );
	virtual ~DCMFVideoGenerator ();

public:
	virtual void Begin ( IStream * stream, DCScreenCapturer * capturer );
	virtual void End ();

private:
	IMFTranscodeProfile * pProfile;
	IMFMediaSource * pSource;
	IMFTopology * pTopology;

	DCMFVideoConfig videoConfig;
};

// Abstract Audio File Generator
class DARAMCAM_EXPORTS DCAudioGenerator
{
public:
	virtual ~DCAudioGenerator ();

public:
	virtual void Begin ( IStream * stream, DCAudioCapturer * capturer ) = 0;
	virtual void End () = 0;
};

enum DCMFAudioType
{
	DCMFAudioType_MP3,
	DCMFAudioType_AAC,
	DCMFAudioType_WMA,
	DCMFAudioType_WAV,
};

// Audio File Generator via Windows Media Foundation
// Can generate MP3, AAC, WAV
// Cannot use in Windows N/KN
class DARAMCAM_EXPORTS DCMFAudioGenerator : public DCAudioGenerator
{
	friend DWORD WINAPI MFAG_Progress ( LPVOID vg );
public:
	DCMFAudioGenerator ( DCMFAudioType audioType );
	virtual ~DCMFAudioGenerator ();

public:
	virtual void Begin ( IStream * stream, DCAudioCapturer * capturer );
	virtual void End ();

private:
	DCMFAudioType audioType;

	IStream * stream;
	DCAudioCapturer * capturer;

	HANDLE threadHandle;
	bool threadRunning;
};

#endif