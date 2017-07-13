#include "DaramCam.h"

DCScreenCapturer::~DCScreenCapturer ()
{

}

DCAudioCapturer::~DCAudioCapturer ()
{

}

unsigned DCAudioCapturer::GetByterate ()
{
	return GetChannels () * GetSamplerate () * ( GetBitsPerSample () / 8 );
}