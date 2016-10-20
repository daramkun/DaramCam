#include "DaramCam.h"
#include <Windows.h>

DCGDIScreenCapturer::DCGDIScreenCapturer ( HWND hWnd )
	: capturedBitmap ( 0, 0 ), hWnd ( hWnd )
{
	desktopDC = /*hWnd == NULL ? */GetDC ( hWnd );// : GetWindowDC ( hWnd );
	captureDC = CreateCompatibleDC ( desktopDC );
	SetRegion ( nullptr );
}

DCGDIScreenCapturer::~DCGDIScreenCapturer ()
{
	DeleteObject ( captureBitmap );
	DeleteDC ( captureDC );
	ReleaseDC ( hWnd, desktopDC );
}

void DCGDIScreenCapturer::Capture ()
{
	HBITMAP oldBitmap = ( HBITMAP ) SelectObject ( captureDC, captureBitmap );
	if ( captureRegion == nullptr )
	{
		BitBlt ( captureDC, 0, 0, capturedBitmap.GetWidth (), capturedBitmap.GetHeight (), desktopDC, 0, 0, SRCCOPY );
	}
	else
		BitBlt ( captureDC,
			0, 0,
			captureRegion->right - captureRegion->left, captureRegion->bottom - captureRegion->top,
			desktopDC, captureRegion->left, captureRegion->top, SRCCOPY );
	SelectObject ( captureDC, oldBitmap );
	
	capturedBitmap.CopyFrom ( captureDC, captureBitmap );
}

DCBitmap * DCGDIScreenCapturer::GetCapturedBitmap () { return &capturedBitmap; }

void DCGDIScreenCapturer::SetRegion ( RECT * _region )
{
	if ( _region == nullptr && captureRegion == nullptr )
		return;

	captureRegion = _region;
	DeleteObject ( captureBitmap );

	if ( _region != nullptr )
		capturedBitmap.Resize ( _region->right - _region->left, _region->bottom - _region->top );
	else
	{
		unsigned w, h;
		if ( hWnd == NULL )
		{
			w = GetSystemMetrics ( SM_CXVIRTUALSCREEN );
			h = GetSystemMetrics ( SM_CYVIRTUALSCREEN );
		}
		else
		{
			RECT r;
			GetClientRect ( hWnd, &r );
			w = r.right - r.left;
			h = r.bottom - r.top;
		}

		capturedBitmap.Resize ( w, h );
	}
	captureBitmap = CreateCompatibleBitmap ( desktopDC, capturedBitmap.GetWidth (), capturedBitmap.GetHeight () );
}
