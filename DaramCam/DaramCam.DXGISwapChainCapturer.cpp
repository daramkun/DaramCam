#include "DaramCam.h"

#include <dxgi.h>

class DCDXGISwapChainCapturer : public DCScreenCapturer
{
public:
	DCDXGISwapChainCapturer ( IUnknown * dxgiSwapChain );
	virtual ~DCDXGISwapChainCapturer ();

public:
	virtual void Capture ();
	virtual DCBitmap * GetCapturedBitmap () noexcept;

private:
	IDXGISwapChain * dxgiSwapChain;
	DCBitmap capturedBitmap;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

DARAMCAM_EXPORTS DCScreenCapturer * DCCreateDXGISwapChainCapturer ( IUnknown * dxgiSwapChain )
{
	return new DCDXGISwapChainCapturer ( dxgiSwapChain );
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

DCDXGISwapChainCapturer::DCDXGISwapChainCapturer ( IUnknown * dxgiSwapChain )
	: capturedBitmap ( 0, 0 )
{
	dxgiSwapChain->QueryInterface<IDXGISwapChain> ( &this->dxgiSwapChain );

	DXGI_SWAP_CHAIN_DESC desc;
	this->dxgiSwapChain->GetDesc ( &desc );
	capturedBitmap.Resize ( desc.BufferDesc.Width, desc.BufferDesc.Height, 4 );
}

DCDXGISwapChainCapturer::~DCDXGISwapChainCapturer ()
{
	dxgiSwapChain->Release ();
}

void DCDXGISwapChainCapturer::Capture ()
{
	IDXGISurface * surface;
	dxgiSwapChain->GetBuffer ( 0, __uuidof ( IDXGISurface ), ( void ** ) &surface );
	DXGI_MAPPED_RECT locked;
	surface->Map ( &locked, 0 );
	memcpy ( capturedBitmap.GetByteArray (), locked.pBits, capturedBitmap.GetByteArraySize () );
	surface->Unmap ();
}

DCBitmap * DCDXGISwapChainCapturer::GetCapturedBitmap () noexcept { return &capturedBitmap; }
