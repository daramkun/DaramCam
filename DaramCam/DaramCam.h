#ifndef __DARAMKUN_DARAMCAM_H__
#define __DARAMKUN_DARAMCAM_H__

#include <Windows.h>
#include <wincodec.h>
#include <string>
#include <fstream>

#if ! ( defined ( _WIN32 ) || defined ( _WIN64 ) || defined ( WINDOWS ) || defined ( _WINDOWS ) )
#error "This library is for Windows only."
#endif

#ifndef	DARAMCAM_EXPORTS
#define	DARAMCAM_EXPORTS					__declspec(dllimport)
#else
#undef	DARAMCAM_EXPORTS
#define	DARAMCAM_EXPORTS					__declspec(dllexport)
#endif

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

public:
	void Resize ( unsigned width, unsigned height );

private:
	unsigned char * byteArray;
	unsigned width, height, stride;
};

// Abstract Screen Capturer
class DARAMCAM_EXPORTS DCCapturer
{
public:
	virtual ~DCCapturer ();

public:
	virtual void Capture () = 0;
	virtual DCBitmap & GetCapturedBitmap () = 0;
};

// GDI Screen Capturer
class DARAMCAM_EXPORTS DCGDICapturer : public DCCapturer
{
public:
	DCGDICapturer ();
	virtual ~DCGDICapturer ();

public:
	virtual void Capture ();
	virtual DCBitmap & GetCapturedBitmap ();

public:
	void SetRegion ( RECT * region = nullptr );

private:
	HDC desktopDC;
	HDC captureDC;
	HBITMAP captureBitmap;

	RECT * captureRegion;

	DCBitmap capturedBitmap;
};

// Abstract Image File Generator
class DARAMCAM_EXPORTS DCImageGenerator
{
public:
	virtual ~DCImageGenerator ();

public:
	virtual void Generate ( IStream * stream, DCBitmap * bitmap ) = 0;
};

enum DCWICImageType
{
	DCWICImageType_BMP,
	DCWICImageType_JPEG,
	DCWICImageType_PNG,
	DCWICImageType_TIFF,
};

// Image File Generator via Windows Imaging Codec
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
	virtual void Begin ( IStream * stream, DCCapturer * capturer ) = 0;
	virtual void End () = 0;
};

// Video File Generator via Windows Imaging Codec
// Can generate GIF
class DARAMCAM_EXPORTS DCWICVideoGenerator
{
public:
	DCWICVideoGenerator ();
	virtual ~DCWICVideoGenerator ();

public:
	virtual void Begin ( IStream * stream, DCCapturer * capturer );
	virtual void End ();

private:
	IWICImagingFactory * piFactory;
	GUID gifContainerGUID;

	IStream * stream;
	DCCapturer * capturer;
	IWICStream * piStream;
	IWICBitmapEncoder * piEncoder;
};

#endif