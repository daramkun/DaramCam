#include "DaramCam.h"

DCBitmap::DCBitmap ( unsigned _width, unsigned _height )
	: byteArray ( nullptr )
{
	Resize ( _width, _height );
}

DCBitmap::~DCBitmap ()
{
	if( byteArray )
		delete [] byteArray;
	byteArray = nullptr;
}

unsigned char * DCBitmap::GetByteArray () { return byteArray; }
unsigned DCBitmap::GetWidth () { return width; }
unsigned DCBitmap::GetHeight () { return height; }
unsigned DCBitmap::GetStride () { return stride; }

unsigned DCBitmap::GetByteArraySize ()
{
	return ( ( ( stride + 3 ) / 4 ) * 4 ) * height;
}

IWICBitmap * DCBitmap::ToWICBitmap ( IWICImagingFactory * factory )
{
	IWICBitmap * bitmap;
	factory->CreateBitmap ( width, height, GUID_WICPixelFormat24bppBGR, WICBitmapCacheOnDemand, &bitmap );
	
	WICRect wicRect = { 0, 0, ( int ) width, ( int ) height };
	IWICBitmapLock * bitmapLock;
	bitmap->Lock ( &wicRect, WICBitmapLockWrite, &bitmapLock );

	unsigned arrLen = GetByteArraySize ();
	WICInProcPointer ptr;
	unsigned wicStride = 0;
	bitmapLock->GetDataPointer ( &arrLen, &ptr );
	bitmapLock->GetStride ( &wicStride );
	//memcpy ( ptr, byteArray, arrLen );
	for ( unsigned i = 0; i < height; ++i )
	{
		int pos1 = i * wicStride;
		int pos2 = i * stride;
		memcpy ( ptr + ( pos1 ), byteArray + ( pos2 ), min ( stride, wicStride ) );
	}
	bitmapLock->Release ();

	return bitmap;
}

void DCBitmap::Resize ( unsigned _width, unsigned _height )
{
	if ( _width == width && _height == _height ) return;
	if ( byteArray ) delete [] byteArray;
	if ( _width == 0 && _height == 0 ) { byteArray = nullptr; return; }

	width = _width; height = _height;
	stride = ( _width * 24 + 7 ) / 8;
	byteArray = new unsigned char [ GetByteArraySize () ];
}

COLORREF DCBitmap::GetColorRef ( unsigned x, unsigned y )
{
	unsigned basePos = ( x * 3 ) + ( y * stride );
	return RGB ( byteArray [ basePos + 0 ], byteArray [ basePos + 1 ], byteArray [ basePos + 2 ] );
}

void DCBitmap::SetColorRef ( COLORREF colorRef, unsigned x, unsigned y )
{
	unsigned basePos = ( x * 3 ) + ( y * stride );
	byteArray [ basePos + 0 ] = GetRValue ( colorRef );
	byteArray [ basePos + 1 ] = GetGValue ( colorRef );
	byteArray [ basePos + 2 ] = GetBValue ( colorRef );
}

void DCBitmap::CopyFrom ( HDC hDC, HBITMAP hBitmap )
{
	BITMAPINFO bmpInfo = { 0, };
	bmpInfo.bmiHeader.biSize = sizeof ( BITMAPINFOHEADER );
	GetDIBits ( hDC, hBitmap, 0, 0, NULL, &bmpInfo, DIB_RGB_COLORS );
	if ( width != bmpInfo.bmiHeader.biWidth || height != bmpInfo.bmiHeader.biHeight )
		Resize ( bmpInfo.bmiHeader.biWidth, bmpInfo.bmiHeader.biHeight );
	bmpInfo.bmiHeader.biBitCount = 24;
	bmpInfo.bmiHeader.biCompression = BI_RGB;
	for ( unsigned i = 0; i < height; ++i )
		GetDIBits ( hDC, hBitmap, i, 1, ( byteArray + ( ( height - 1 - i ) * stride ) ), &bmpInfo, DIB_RGB_COLORS );
}

void DCBitmap::CopyFrom ( IDXGISurface * dxgiSurface )
{
	DXGI_SURFACE_DESC surfaceDesc;
	dxgiSurface->GetDesc ( &surfaceDesc );
	if ( width != surfaceDesc.Width || height != surfaceDesc.Height )
		Resize ( surfaceDesc.Width, surfaceDesc.Height );
	DXGI_MAPPED_RECT dxgiMappedRect;
	dxgiSurface->Map ( &dxgiMappedRect, DXGI_MAP_READ );
	for ( unsigned y = 0; y < surfaceDesc.Height; ++y )
	{
		for ( unsigned x = 0; x < surfaceDesc.Width; ++x )
		{
			unsigned basePos;
			switch ( surfaceDesc.Format )
			{
			case DXGI_FORMAT_R8G8B8A8_UNORM:
				basePos = y * dxgiMappedRect.Pitch + ( x * 4 );
				COLORREF colorRef = RGB ( dxgiMappedRect.pBits [ basePos + 0 ], dxgiMappedRect.pBits [ basePos + 1 ], dxgiMappedRect.pBits [ basePos + 2 ] );
				SetColorRef ( colorRef, x, y );
			break;
			}
		}
	}
	dxgiSurface->Unmap ();
}

void DCBitmap::CopyFrom ( IDirect3DSurface9 * d3dSurface )
{
	D3DSURFACE_DESC surfaceDesc;
	d3dSurface->GetDesc ( &surfaceDesc );
	if ( width != surfaceDesc.Width || height != surfaceDesc.Height )
		Resize ( surfaceDesc.Width, surfaceDesc.Height );

	D3DLOCKED_RECT lockedRect;
	d3dSurface->LockRect ( &lockedRect, nullptr, 0 );
	for ( unsigned y = 0; y < surfaceDesc.Height; ++y )
	{
		for ( unsigned x = 0; x < surfaceDesc.Width; ++x )
		{
			unsigned basePos;
			switch ( surfaceDesc.Format )
			{
			case D3DFMT_A8R8G8B8:
			case D3DFMT_X8R8G8B8:
				basePos = y * lockedRect.Pitch + ( x * 4 );
				COLORREF colorRef = ( ( D3DCOLOR* ) lockedRect.pBits ) [ basePos ];
				SetColorRef ( colorRef, x, y );
				break;
			}
		}
	}
	d3dSurface->UnlockRect ();
}