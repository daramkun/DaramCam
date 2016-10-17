#include "DaramCam.h"
#include <Windows.h>

DCGDICapturer::DCGDICapturer ()
	: capturedBitmap ( GetSystemMetrics ( SM_CXVIRTUALSCREEN ), GetSystemMetrics ( SM_CYVIRTUALSCREEN ) ),
	captureRegion ( nullptr )
{
	desktopDC = GetDC ( NULL );
	captureDC = CreateCompatibleDC ( desktopDC );
	captureBitmap = CreateCompatibleBitmap ( desktopDC, capturedBitmap.GetWidth (), capturedBitmap.GetHeight () );
}

DCGDICapturer::~DCGDICapturer ()
{
	DeleteObject ( captureBitmap );
	DeleteDC ( captureDC );
	ReleaseDC ( NULL, desktopDC );
}

void GetImageDataFromHBITMAP ( HDC hdc, HBITMAP bitmap, DCBitmap * cb )
{
	BITMAPINFO bmpInfo;
	memset ( &bmpInfo, 0, sizeof ( BITMAPINFOHEADER ) );
	bmpInfo.bmiHeader.biSize = sizeof ( BITMAPINFOHEADER );
	GetDIBits ( hdc, bitmap, 0, 0, NULL, &bmpInfo, DIB_RGB_COLORS );
	bmpInfo.bmiHeader.biBitCount = 24;
	bmpInfo.bmiHeader.biCompression = BI_RGB;
	for ( unsigned i = 0; i < cb->GetHeight (); ++i )
		GetDIBits ( hdc, bitmap, i, 1, ( cb->GetByteArray () + ( ( cb->GetHeight () - 1 - i ) * cb->GetStride () ) ), &bmpInfo, DIB_RGB_COLORS );
}

void DCGDICapturer::Capture ()
{
	HBITMAP oldBitmap = ( HBITMAP ) SelectObject ( captureDC, captureBitmap );
	if ( !captureRegion )
		BitBlt ( captureDC, 0, 0, capturedBitmap.GetWidth (), capturedBitmap.GetHeight (), desktopDC, 0, 0, SRCCOPY );
	else
		BitBlt ( captureDC,
			0, 0,
			captureRegion->right - captureRegion->left, captureRegion->bottom - captureRegion->top,
			desktopDC, captureRegion->left, captureRegion->top, SRCCOPY );
	SelectObject ( captureDC, oldBitmap );

	GetImageDataFromHBITMAP ( captureDC, captureBitmap, &capturedBitmap );
}

DCBitmap & DCGDICapturer::GetCapturedBitmap () { return capturedBitmap; }

void DCGDICapturer::SetRegion ( RECT * _region )
{
	captureRegion = _region;

	DeleteObject ( captureBitmap );
	if ( _region != nullptr )
	{
		capturedBitmap.Resize ( _region->right - _region->left, _region->bottom - _region->top );
		captureBitmap = CreateCompatibleBitmap ( desktopDC, _region->right - _region->left, _region->bottom - _region->top );
	}
	else
	{
		capturedBitmap.Resize ( GetSystemMetrics ( SM_CXVIRTUALSCREEN ), GetSystemMetrics ( SM_CYVIRTUALSCREEN ) );
		captureBitmap = CreateCompatibleBitmap ( desktopDC, capturedBitmap.GetWidth (), capturedBitmap.GetHeight () );
	}
}
