#ifndef __DARAMKUN_DARAMCAM_H__
#define __DARAMKUN_DARAMCAM_H__

#include <Windows.h>
#include <wincodec.h>
#include <Mmdeviceapi.h>
#include <shlwapi.h>

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
DARAMCAM_EXPORTS BSTR DCGetDeviceName ( IMMDevice * pDevice );
DARAMCAM_EXPORTS double DCGetCurrentTime ();
DARAMCAM_EXPORTS void DCGetMultimediaDevices ( std::vector<IMMDevice*> & devices );
DARAMCAM_EXPORTS void DCReleaseMultimediaDevices ( std::vector<IMMDevice*> & devices );

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

	IWICBitmap * ToWICBitmap ();

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
DARAMCAM_EXPORTS DCScreenCapturer* DCCreateGDIScreenCapturer ( HWND hWnd = NULL );
DARAMCAM_EXPORTS void DCSetRegionToGDIScreenCapturer ( DCScreenCapturer * capturer, const RECT * region = NULL );

enum DCDXGIScreenCapturerRange
{
	DCDXGIScreenCapturerRange_Desktop,
	DCDXGIScreenCapturerRange_MainMonitor,
	DCDXGIScreenCapturerRange_SubMonitors,
};

// DXGI Screen Capturer
DARAMCAM_EXPORTS DCScreenCapturer* DCCreateDXGIScreenCapturer ( DCDXGIScreenCapturerRange range = DCDXGIScreenCapturerRange_Desktop );

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

// WASAPI Audio Capturer
DARAMCAM_EXPORTS DCAudioCapturer* DCCreateWASAPIAudioCapturer ( IMMDevice * pDevice );
DARAMCAM_EXPORTS DCAudioCapturer* DCCreateWASAPILoopbackAudioCapturer ();

DARAMCAM_EXPORTS float DCGetVolumeFromWASAPIAudioCapturer ( DCAudioCapturer * capturer );
DARAMCAM_EXPORTS void DCSetVolumeToWASAPIAudioCapturer ( DCAudioCapturer * capturer, float volume );

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
DARAMCAM_EXPORTS DCImageGenerator* DCCreateWICImageGenerator ( DCWICImageType imageType );
DARAMCAM_EXPORTS void DCSetSizeToWICImageGenerator ( DCImageGenerator * generator, const SIZE * size = nullptr );

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

enum {
	WICVG_FRAMETICK_30FPS = 33,
	WICVG_FRAMETICK_24FPS = 41
};

// Video File Generator via Windows Imaging Component
// Can generate GIF only
DARAMCAM_EXPORTS DCVideoGenerator* DCCreateWICVideoGenerator ( unsigned frameTick = WICVG_FRAMETICK_30FPS );
DARAMCAM_EXPORTS void DCSetSizeToWICVideoGenerator ( DCVideoGenerator * generator, const SIZE * size );

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

// Audio File Generator via Original Code
// Can generate WAV only
DARAMCAM_EXPORTS DCAudioGenerator* DCCreateWaveAudioGenerator ();

#endif