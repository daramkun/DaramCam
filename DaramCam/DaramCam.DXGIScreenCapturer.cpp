#include "DaramCam.h"

#include <dxgi.h>

#include "External Libraries\DXGIManager\DXGIManager.h"

DCDXGIScreenCapturer::DCDXGIScreenCapturer ()
	: capturedBitmap ( 0, 0 ), captureRegion ( nullptr )
{
	DXGIManager * dxgiManager = new DXGIManager;
	this->dxgiManager = dxgiManager;

	dxgiManager->SetCaptureSource ( CSDesktop );
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

	if ( captureRegion == nullptr )
		dxgiManager->GetOutputRect ( outputRect );
	else
		outputRect = *captureRegion;

//#pragma omp parallel for
	for ( int y = outputRect.top; y < outputRect.bottom; ++y )
	{
		for ( unsigned x = outputRect.left; x < outputRect.right; ++x )
		{
			capturedBitmap.SetColorRef ( ( ( COLORREF* ) tempBits ) [ y * fullWidth + x ], x - outputRect.left, y - outputRect.top );
		}
	}
}

DCBitmap * DCDXGIScreenCapturer::GetCapturedBitmap () { return &capturedBitmap; }

void DCDXGIScreenCapturer::SetRegion ( RECT * region )
{
	if ( region == nullptr )
	{
		RECT outputRect;
		DXGIManager * dxgiManager = static_cast< DXGIManager* > ( this->dxgiManager );
		dxgiManager->GetOutputRect ( outputRect );
		
		capturedBitmap.Resize ( outputRect.right - outputRect.left, outputRect.bottom - outputRect.top );
	}
	else
	{
		capturedBitmap.Resize ( region->right - region->left, region->bottom - region->top );
	}
	captureRegion = region;
}
