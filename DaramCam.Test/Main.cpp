#define _CRT_SECURE_NO_WARNINGS
#include <DaramCam.h>
#pragma comment ( lib, "DaramCam.lib" )

#include <stdio.h>
#include <string.h>

class FileStream : public IStream, IMFByteStream
{
	FileStream ( HANDLE hFile )
	{
		_refcount = 1;
		_hFile = hFile;
	}

	~FileStream ()
	{
		if ( _hFile != INVALID_HANDLE_VALUE )
		{
			::CloseHandle ( _hFile );
			_hFile = NULL;
		}
	}

public:
	HRESULT static OpenFile ( LPCWSTR pName, IStream ** ppStream, bool fWrite )
	{
		HANDLE hFile = ::CreateFileW ( pName, fWrite ? GENERIC_WRITE : GENERIC_READ, FILE_SHARE_READ,
			NULL, fWrite ? CREATE_ALWAYS : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );

		if ( hFile == INVALID_HANDLE_VALUE )
			return HRESULT_FROM_WIN32 ( GetLastError () );

		*ppStream = new FileStream ( hFile );

		if ( *ppStream == NULL )
			CloseHandle ( hFile );

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface ( REFIID iid, void ** ppvObject )
	{
		if ( iid == __uuidof( IUnknown )
			|| iid == __uuidof( IStream )
			|| iid == __uuidof( ISequentialStream ) )
		{
			*ppvObject = static_cast<IStream*>( this );
			AddRef ();
			return S_OK;
		}
		else if ( iid == __uuidof ( IMFByteStream ) )
		{
			*ppvObject = static_cast< IMFByteStream* >( this );
			AddRef ();
			return S_OK;
		}
		else
			return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef ( void )
	{
		return ( ULONG ) InterlockedIncrement ( &_refcount );
	}

	virtual ULONG STDMETHODCALLTYPE Release ( void )
	{
		ULONG res = ( ULONG ) InterlockedDecrement ( &_refcount );
		if ( res == 0 )
			delete this;
		return res;
	}

	// ISequentialStream Interface
public:
	virtual HRESULT STDMETHODCALLTYPE Read ( void* pv, ULONG cb, ULONG* pcbRead )
	{
		BOOL rc = ReadFile ( _hFile, pv, cb, pcbRead, NULL );
		return ( rc ) ? S_OK : HRESULT_FROM_WIN32 ( GetLastError () );
	}

	virtual HRESULT STDMETHODCALLTYPE Write ( void const* pv, ULONG cb, ULONG* pcbWritten )
	{
		BOOL rc = WriteFile ( _hFile, pv, cb, pcbWritten, NULL );
		return rc ? S_OK : HRESULT_FROM_WIN32 ( GetLastError () );
	}

	// IStream Interface
public:
	virtual HRESULT STDMETHODCALLTYPE SetSize ( ULARGE_INTEGER ) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE CopyTo ( IStream*, ULARGE_INTEGER, ULARGE_INTEGER*, ULARGE_INTEGER* ) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE Commit ( DWORD ) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE Revert ( void ) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE LockRegion ( ULARGE_INTEGER, ULARGE_INTEGER, DWORD ) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE UnlockRegion ( ULARGE_INTEGER, ULARGE_INTEGER, DWORD ) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE Clone ( IStream ** ) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE Seek ( LARGE_INTEGER liDistanceToMove, DWORD dwOrigin,
		ULARGE_INTEGER* lpNewFilePointer )
	{
		DWORD dwMoveMethod;

		switch ( dwOrigin )
		{
		case STREAM_SEEK_SET:
			dwMoveMethod = FILE_BEGIN;
			break;
		case STREAM_SEEK_CUR:
			dwMoveMethod = FILE_CURRENT;
			break;
		case STREAM_SEEK_END:
			dwMoveMethod = FILE_END;
			break;
		default:
			return STG_E_INVALIDFUNCTION;
			break;
		}

		if ( SetFilePointerEx ( _hFile, liDistanceToMove, ( PLARGE_INTEGER ) lpNewFilePointer,
			dwMoveMethod ) == 0 )
			return HRESULT_FROM_WIN32 ( GetLastError () );
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE Stat ( STATSTG* pStatstg, DWORD grfStatFlag )
	{
		if ( GetFileSizeEx ( _hFile, ( PLARGE_INTEGER ) &pStatstg->cbSize ) == 0 )
			return HRESULT_FROM_WIN32 ( GetLastError () );
		return S_OK;
	}

private:
	virtual HRESULT STDMETHODCALLTYPE GetCapabilities ( __RPC__out DWORD *pdwCapabilities ) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE GetLength ( __RPC__out QWORD *pqwLength )
	{
		DWORD c;
		GetFileSize ( _hFile, &c );
		*pqwLength = c;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE SetLength ( QWORD qwLength ) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE GetCurrentPosition ( __RPC__out QWORD *pqwPosition )
	{
		LARGE_INTEGER liOfs = { 0 };
		LARGE_INTEGER liNew = { 0 };
		SetFilePointerEx ( _hFile, liOfs, &liNew, FILE_CURRENT );
		*pqwPosition = liNew.QuadPart;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE SetCurrentPosition ( QWORD qwPosition )
	{
		LARGE_INTEGER li;
		li.QuadPart = qwPosition;
		return Seek ( li, SEEK_SET, nullptr );
	}

	virtual HRESULT STDMETHODCALLTYPE IsEndOfStream ( __RPC__out BOOL *pfEndOfStream )
	{
		LARGE_INTEGER pos, size;
		LARGE_INTEGER li = { 0, };
		if ( !SetFilePointerEx ( _hFile, li, &pos, FILE_CURRENT ) )
			return HRESULT_FROM_WIN32 ( GetLastError () );
		if ( !GetFileSizeEx ( _hFile, &size ) )
			return HRESULT_FROM_WIN32 ( GetLastError () );
		*pfEndOfStream = ( pos.QuadPart == size.QuadPart );
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE Read ( __RPC__out_ecount_full ( cb ) BYTE *pb, ULONG cb, __RPC__out ULONG *pcbRead )
	{
		return Read ( ( void* ) pb, cb, pcbRead );
	}

	virtual /* [local] */ HRESULT STDMETHODCALLTYPE BeginRead ( _Out_writes_bytes_ ( cb )  BYTE *pb, ULONG cb, IMFAsyncCallback *pCallback, IUnknown *punkState ) { return E_NOTIMPL; }

	virtual /* [local] */ HRESULT STDMETHODCALLTYPE EndRead ( IMFAsyncResult *pResult, _Out_  ULONG *pcbRead ) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE Write ( __RPC__in_ecount_full ( cb ) const BYTE *pb, ULONG cb, __RPC__out ULONG *pcbWritten )
	{
		return Write ( ( void* ) pb, cb, pcbWritten );
	}

	virtual /* [local] */ HRESULT STDMETHODCALLTYPE BeginWrite ( _In_reads_bytes_ ( cb ) const BYTE *pb, ULONG cb, IMFAsyncCallback *pCallback, IUnknown *punkState ) { return E_NOTIMPL; }

	virtual /* [local] */ HRESULT STDMETHODCALLTYPE EndWrite ( IMFAsyncResult *pResult, _Out_  ULONG *pcbWritten ) { return E_NOTIMPL; }

	virtual HRESULT STDMETHODCALLTYPE Seek ( MFBYTESTREAM_SEEK_ORIGIN SeekOrigin, LONGLONG llSeekOffset, DWORD dwSeekFlags, __RPC__out QWORD *pqwCurrentPosition )
	{
		LARGE_INTEGER li;
		li.QuadPart = llSeekOffset;
		ULARGE_INTEGER uli;
		HRESULT hr = Seek ( li, 0, &uli );
		if ( hr != S_OK ) return hr;

		if ( pqwCurrentPosition )
			*pqwCurrentPosition = uli.QuadPart;

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE Flush ( void ) { FlushFileBuffers ( _hFile ); return S_OK; }

	virtual HRESULT STDMETHODCALLTYPE Close ( void ) { return E_NOTIMPL; }

private:
	HANDLE _hFile;
	LONG _refcount;
};

int main ( void )
{
	DCStartup ();

	/*DWORD processes [ 4096 ] = { 0, };
	unsigned processCount;
	DCGetProcesses ( processes, &processCount );

	for ( unsigned i = 0; i < processCount; ++i )
	{
		DWORD process = processes [ i ];
		HWND hWnd = DCGetActiveWindowFromProcess ( process );
		if ( process == 0 || hWnd == 0 ) continue;
		char nameBuffer [ 4096 ] = { 0, };
		DCGetProcessName ( process, nameBuffer, 4096 );
		if ( strlen ( nameBuffer ) == 0 )
			continue;

		printf ( "%48s (DWORD: %5d, HWND: %d)\n", nameBuffer, process, hWnd );
	}

	DWORD process;
	printf ( "Select process (0 is default process): " );
	scanf ( "%d", &process );

	HWND hWnd;
	if ( process == 0 )
		hWnd = NULL;
	else
		hWnd = DCGetActiveWindowFromProcess ( process );*/

	IStream * stream;
	/*FileStream::OpenFile ( TEXT ( "Z:\\Test.png" ), &stream, true );

	//DCGDIScreenCapturer * screenCapturer = new DCGDIScreenCapturer ( hWnd );
	DCDXGIScreenCapturer * screenCapturer = new DCDXGIScreenCapturer ();
	RECT region = { 1920, 0, 1920 * 2, 1080 };
	screenCapturer->SetRegion ( &region );
	screenCapturer->Capture ();

	DCImageGenerator * imgGen = new DCWICImageGenerator ( DCWICImageType_PNG );
	imgGen->Generate ( stream, screenCapturer->GetCapturedBitmap () );

	stream->Release ();

	delete imgGen;

	FileStream::OpenFile ( TEXT ( "Z:\\Test.gif" ), &stream, true );

	DCVideoGenerator * vidGen = new DCWICVideoGenerator ( 16 );
	vidGen->Begin ( stream, screenCapturer );
	Sleep ( 10000 );
	vidGen->End ();
	stream->Release ();

	delete vidGen;

	delete screenCapturer;*/

	DCWASAPIAudioCapturer * audioCapturer = new DCWASAPIAudioCapturer ();

	FileStream::OpenFile ( TEXT ( "Z:\\Test.mp3" ), &stream, true );

	DCAudioGenerator * audGen = new DCMFAudioGenerator ( DCMFAudioType_MP3 );

	audGen->Begin ( stream, audioCapturer );
	Sleep ( 10000 );
	audGen->End ();

	delete audioCapturer;

	DCShutdown ();

	return 0;
}