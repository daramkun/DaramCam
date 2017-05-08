#include "DaramCam.h"

#include <dxgi.h>

#include "External Libraries\DXGIManager\DXGIManager.h"

DCDXGIScreenCapturer::DCDXGIScreenCapturer ( DCDXGIScreenCapturerRange range )
	: capturedBitmap ( 0, 0 ), captureRegion ( nullptr )
{
	DXGIManager * dxgiManager = new DXGIManager;
	this->dxgiManager = dxgiManager;

	dxgiManager->SetCaptureSource ( range == DCDXGIScreenCapturerRange_Desktop ? CSDesktop : CSMonitor1 );
	tempBits = nullptr;

	RECT outputRect;
	dxgiManager->GetOutputRect ( outputRect );

	if ( tempBits != nullptr )
		delete [] tempBits;
	tempBits = new BYTE [ ( outputRect.right - outputRect.left )* ( outputRect.bottom - outputRect.top ) * 4 ];

	SetRegion ( nullptr );
}

DCDXGIScreenCapturer::~DCDXGIScreenCapturer ()
{
	if ( tempBits )
		delete [] tempBits;
	delete dxgiManager;
}

void DCDXGIScreenCapturer::Capture ()
{
	DXGIManager * dxgiManager = static_cast< DXGIManager* > ( this->dxgiManager );
	RECT outputRect;
	dxgiManager->GetOutputRect ( outputRect );
	UINT fullWidth = outputRect.right - outputRect.left;
	dxgiManager->GetOutputBits ( tempBits, outputRect );

	if ( captureRegion != nullptr )
		outputRect = *captureRegion;

	int width4times = capturedBitmap.GetWidth () * 4;
	BYTE* ba = capturedBitmap.GetByteArray ();
	for ( int y = outputRect.top; y < outputRect.bottom; ++y )
		memcpy ( ba + ( ( y - outputRect.top ) * width4times ), tempBits + ( y * fullWidth + outputRect.left ) * 4, width4times );
}

DCBitmap * DCDXGIScreenCapturer::GetCapturedBitmap () { return &capturedBitmap; }

void DCDXGIScreenCapturer::SetRegion ( RECT * region )
{
	if ( region == nullptr )
	{
		RECT outputRect;
		DXGIManager * dxgiManager = static_cast< DXGIManager* > ( this->dxgiManager );
		dxgiManager->GetOutputRect ( outputRect );
		
		capturedBitmap.Resize ( outputRect.right - outputRect.left, outputRect.bottom - outputRect.top, 4 );
	}
	else
	{
		capturedBitmap.Resize ( region->right - region->left, region->bottom - region->top, 4 );
	}
	captureRegion = region;
}
