#include "DaramCam.h"

DCBitmap::DCBitmap ( unsigned _width, unsigned _height )
	: byteArray ( nullptr )
{
	Resize ( _width, _height );
}

DCBitmap::~DCBitmap ()
{
	delete [] byteArray;
}

unsigned char * DCBitmap::GetByteArray () { return byteArray; }
unsigned DCBitmap::GetWidth () { return width; }
unsigned DCBitmap::GetHeight () { return height; }
unsigned DCBitmap::GetStride () { return stride; }

unsigned DCBitmap::GetByteArraySize ()
{
	return width * height * 3;
}

void DCBitmap::Resize ( unsigned _width, unsigned _height )
{
	if ( byteArray ) delete [] byteArray;

	width = _width; height = _height;
	stride = ( _width * 24 + 7 ) / 8;
	byteArray = new unsigned char [ stride * height ];
}
