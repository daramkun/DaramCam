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

#include <vector>

#if ! ( defined ( _WIN32 ) || defined ( _WIN64 ) || defined ( WINDOWS ) || defined ( _WINDOWS ) )
#error "This library is for Windows only."
#endif

#ifndef	DARAMCAM_EXPORTS
#define	DARAMCAM_EXPORTS					__declspec(dllimport)
#else
#undef	DARAMCAM_EXPORTS
#define	DARAMCAM_EXPORTS					__declspec(dllexport)
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

DARAMCAM_EXPORTS void DCStartup ();
DARAMCAM_EXPORTS void DCShutdown ();
DARAMCAM_EXPORTS void DCGetProcesses ( DWORD * buffer, unsigned * bufferSize );
DARAMCAM_EXPORTS void DCGetProcessName ( DWORD pId, char * nameBuffer, unsigned nameBufferSize );
DARAMCAM_EXPORTS HWND DCGetActiveWindowFromProcess ( DWORD pId );

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

// 24-bit Fixed RGB Color Bitmap Data Structure
class DARAMCAM_EXPORTS DCBitmap
{
public:
	DCBitmap ( unsigned width, unsigned height, unsigned colorDepth = 3 );
	~DCBitmap ();

public:
	unsigned char * GetByteArray ();
	unsigned GetWidth ();
	unsigned GetHeight ();
	unsigned GetColorDepth ();
	unsigned GetStride ();

	unsigned GetByteArraySize ();

	IWICBitmap * ToWICBitmap ( IWICImagingFactory * factory, bool useCached = true );

public:
	void Resize ( unsigned width, unsigned height, unsigned colorDepth = 3 );

public:
	COLORREF GetColorRef ( unsigned x, unsigned y );
	void SetColorRef ( COLORREF colorRef, unsigned x, unsigned y );

public:
	void CopyFrom ( HDC hDC, HBITMAP hBitmap );

private:
	unsigned char * byteArray;
	unsigned width, height, colorDepth, stride;

	IWICBitmap * wicCached;
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

// Abstract Image File Generator
class DARAMCAM_EXPORTS DCImageGenerator
{
public:
	virtual ~DCImageGenerator ();

public:
	virtual void Generate ( IStream * stream, DCBitmap * bitmap ) = 0;
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

// Abstract Audio File Generator
class DARAMCAM_EXPORTS DCAudioGenerator
{
public:
	virtual ~DCAudioGenerator ();

public:
	virtual void Begin ( IStream * stream, DCAudioCapturer * capturer ) = 0;
	virtual void End () = 0;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

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

enum DCDXGIScreenCapturerRange
{
	DCDXGIScreenCapturerRange_Desktop,
	DCDXGIScreenCapturerRange_MainMonitor,
};

// DXGI Screen Capturer
class DARAMCAM_EXPORTS DCDXGIScreenCapturer : public DCScreenCapturer
{
public:
	DCDXGIScreenCapturer ( DCDXGIScreenCapturerRange range = DCDXGIScreenCapturerRange_Desktop );
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

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

// WASAPI Audio Capturer
class DARAMCAM_EXPORTS DCWASAPIAudioCapturer : public DCAudioCapturer
{
public:
	DCWASAPIAudioCapturer ( IMMDevice * pDevice );
	virtual ~DCWASAPIAudioCapturer ();

public:
	virtual void Begin ();
	virtual void End ();

	virtual unsigned GetChannels ();
	virtual unsigned GetBitsPerSample ();
	virtual unsigned GetSamplerate ();

	virtual void* GetAudioData ( unsigned * bufferLength );

public:
	float GetVolume ();
	void SetVolume ( float volume );

protected:
	virtual DWORD GetStreamFlags ();

public:
	static void GetMultimediaDevices ( std::vector<IMMDevice*> & devices );
	static void ReleaseMultimediaDevices ( std::vector<IMMDevice*> & devices );
	static IMMDevice* GetDefaultMultimediaDevice ();

private:
	IAudioClient *pAudioClient;
	IAudioCaptureClient * pCaptureClient;

	ISimpleAudioVolume * pAudioVolume;
	float originalVolume;

	WAVEFORMATEX *pwfx;

	char * byteArray;
	unsigned byteArrayLength;

	UINT32 bufferFrameCount;

	HANDLE hWakeUp;
};

class DARAMCAM_EXPORTS DCWASAPILoopbackAudioCapturer : public DCWASAPIAudioCapturer
{
public:
	DCWASAPILoopbackAudioCapturer ();

protected:
	virtual DWORD GetStreamFlags ();
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

class DARAMCAM_EXPORTS DCWaveAudioGenerator : public DCAudioGenerator
{
	friend DWORD WINAPI WAVAG_Progress ( LPVOID vg );
public:
	DCWaveAudioGenerator ();

public:
	virtual void Begin ( IStream * stream, DCAudioCapturer * capturer );
	virtual void End ();

private:
	IStream * stream;
	DCAudioCapturer * capturer;

	bool threadRunning;
	HANDLE threadHandle;
};

#endif