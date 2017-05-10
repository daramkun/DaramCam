#include "DaramCam.h"

#include <dxgi.h>

#include "External Libraries\DXGIManager\DXGIManager.h"

#pragma intrinsic(memcpy) 

class DCDXGIScreenCapturer : public DCScreenCapturer
{
public:
	DCDXGIScreenCapturer ( DCDXGIScreenCapturerRange range );
	virtual ~DCDXGIScreenCapturer ();

public:
	virtual void Capture ();
	virtual DCBitmap * GetCapturedBitmap ();

private:
	void* dxgiManager;
	DCBitmap capturedBitmap;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

DARAMCAM_EXPORTS DCScreenCapturer * DCCreateDXGIScreenCapturer ( DCDXGIScreenCapturerRange range )
{
	return new DCDXGIScreenCapturer ( range );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

DCDXGIScreenCapturer::DCDXGIScreenCapturer ( DCDXGIScreenCapturerRange range )
	: capturedBitmap ( 0, 0 )
{
	DXGIManager * dxgiManager = new DXGIManager;
	this->dxgiManager = dxgiManager;

	CaptureSource captureSource = CSDesktop;
	switch ( range )
	{
	case DCDXGIScreenCapturerRange_MainMonitor: captureSource = CSMonitor1; break;
	case DCDXGIScreenCapturerRange_SubMonitors: captureSource = CSMonitor2; break;
	}
	dxgiManager->SetCaptureSource ( captureSource );

	RECT outputRect;
	dxgiManager->GetOutputRect ( outputRect );
	capturedBitmap.Resize ( outputRect.right - outputRect.left, outputRect.bottom - outputRect.top, 4 );

	Capture ();
}

DCDXGIScreenCapturer::~DCDXGIScreenCapturer ()
{
	delete dxgiManager;
}

void DCDXGIScreenCapturer::Capture ()
{
	DXGIManager * dxgiManager = static_cast< DXGIManager* > ( this->dxgiManager );
	RECT outputRect;
	dxgiManager->GetOutputRect ( outputRect );
	while ( FAILED ( dxgiManager->GetOutputBits ( capturedBitmap.GetByteArray () ) ) )
		;
}

DCBitmap * DCDXGIScreenCapturer::GetCapturedBitmap () { return &capturedBitmap; }