/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
********************************************************************************/


#include "DWMCapture.h"

typedef BOOL (WINAPI *DwmGetDxSharedSurface_td) (
	__in HWND hwnd, 
	__out_opt HANDLE* p1, 
	__out_opt LUID* p2, 
	__out_opt ULONG* p3, 
	__out_opt ULONG* p4, 
	__out_opt ULONGLONG* p5 );

typedef int (WINAPI *GetSharedSurface_td) (
    HWND hwnd,
    LUID adapterLuid,
    UINT32 one,
    UINT32 two,
    UINT32 *d3dFormat,
    HANDLE sharedHandle,
    INT64 unknown);

GetSharedSurface_td GetSharedSurface = NULL;
DwmGetDxSharedSurface_td DwmGetSharedSurface = NULL;

typedef struct _D3DKMT_OPENADAPTERFROMDEVICENAME
{
	wchar_t	*pDeviceName;
	HANDLE	hAdapter;
	LUID		AdapterLuid;
} D3DKMT_OPENADAPTERFROMDEVICENAME;

typedef struct _D3DKMT_CLOSEADAPTER
{
	HANDLE	hAdapter;
} D3DKMT_CLOSEADAPTER;

typedef struct _D3DKMT_QUERYADAPTERINFO
{
	HANDLE			hAdapter;
	unsigned int	Type;
	void			*pPrivateDriverData;
	unsigned int	PrivateDriverDataSize;
} D3DKMT_QUERYADAPTERINFO;

typedef struct _D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME
{
	WCHAR	DeviceName[32];
	HANDLE	hAdapter;
	LUID	AdapterLuid;
	unsigned int	VidPnSourceId;
} D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME;

typedef struct _D3DKMT_SEGMENTSIZEINFO
{
	ULONGLONG	DedicatedVideoMemorySize;
	ULONGLONG	DedicatedSystemMemorySize;
	ULONGLONG	SharedSystemMemorySize;
} D3DKMT_SEGMENTSIZEINFO;

typedef struct _D3DKMT_ADAPTERREGISTRYINFO
{
	WCHAR	AdapterString[MAX_PATH];
	WCHAR	BiosString[MAX_PATH];
	WCHAR	DacType[MAX_PATH];
	WCHAR	ChipType[MAX_PATH];
} D3DKMT_ADAPTERREGISTRYINFO;

typedef int	(APIENTRY *PFND3DKMT_CLOSEADAPTER)(IN CONST D3DKMT_CLOSEADAPTER *pData);
typedef int	(APIENTRY *PFND3DKMT_OPENADAPTERFROMGDIDISPLAYNAME)(IN OUT D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME *pData);
typedef int	(__stdcall *PFNDWM_DXGETWINDOWSHAREDSURFACE)(HWND hWnd, LUID adapterLuid, LUID someLuid, DWORD *pD3DFormat,
	HANDLE *pSharedHandle, unsigned __int64 *arg7);

void		GetWindowSharedSurfaceHandle(HWND hWnd, HANDLE *sharedHandle, DWORD *d3dFormat)
{
	HMONITOR										hMonitor = NULL;
	LUID											adapterLuid = { 0, 0 };
	LUID											nullLuid = { 0, 0 };
	MONITORINFOEX									monitorInfo;
	unsigned __int64								unknown = 0;
	D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME			openDev;
	D3DKMT_CLOSEADAPTER								closeDev;
	static PFNDWM_DXGETWINDOWSHAREDSURFACE			pDwmpDxGetWindowSharedSurface = NULL;
	static PFND3DKMT_OPENADAPTERFROMGDIDISPLAYNAME	pOpenAdapterFromGdiDisplayName = NULL;
	static PFND3DKMT_CLOSEADAPTER					pCloseAdapter = NULL;
	static FARPROC									pDwmpFlush = NULL;

	hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
	monitorInfo.cbSize = sizeof(monitorInfo);
	GetMonitorInfo(hMonitor, &monitorInfo);

	if (pOpenAdapterFromGdiDisplayName == NULL || pCloseAdapter == NULL || pDwmpDxGetWindowSharedSurface == NULL)
	{
		HMODULE	hGdi = LoadLibrary(TEXT("gdi32.dll"));
		pOpenAdapterFromGdiDisplayName = 
		(PFND3DKMT_OPENADAPTERFROMGDIDISPLAYNAME)GetProcAddress(hGdi, "D3DKMTOpenAdapterFromGdiDisplayName");
		pCloseAdapter = 
		(PFND3DKMT_CLOSEADAPTER)GetProcAddress(hGdi, "D3DKMTCloseAdapter");

		HMODULE	hDwmApi = LoadLibrary(TEXT("dwmapi.dll"));
		pDwmpDxGetWindowSharedSurface = (PFNDWM_DXGETWINDOWSHAREDSURFACE)GetProcAddress(hDwmApi, (LPCSTR)100);
	}

	//mbstowcs(openDev.DeviceName, monitorInfo.szDevice, sizeof(monitorInfo.szDevice));

    scpy(openDev.DeviceName, monitorInfo.szDevice);

	pOpenAdapterFromGdiDisplayName(&openDev);
	adapterLuid = openDev.AdapterLuid;

	closeDev.hAdapter = openDev.hAdapter;
	pCloseAdapter(&closeDev);

	pDwmpDxGetWindowSharedSurface(hWnd, adapterLuid, nullLuid, d3dFormat, sharedHandle, &unknown);
}

HANDLE GetDWMSharedHandle(HWND hwnd)
{
    //HANDLE out;
    //DWORD format;

    //GetWindowSharedSurfaceHandle (hwnd, &out, &format);

    //return out;

    if (!DwmGetSharedSurface)
        DwmGetSharedSurface = ((DwmGetDxSharedSurface_td)GetProcAddress(GetModuleHandle(TEXT("USER32")), "DwmGetDxSharedSurface"));

    //if (!GetSharedSurface)
      //  GetSharedSurface = (GetSharedSurface_td)GetProcAddress(LoadLibrary(TEXT("DWMAPI.DLL")), (LPCSTR)MAKEDWORD(100,0));


    //GetSharedSurface(hwnd, 

    HANDLE surface;
    LUID adapter;
    ULONG pFmtWindow;
    ULONG pPresentFlags;
    ULONGLONG pWin32kUpdateId;

    DwmGetSharedSurface(hwnd, &surface, &adapter, &pFmtWindow, &pPresentFlags, &pWin32kUpdateId);

    return surface;
}

bool DWMCaptureSource::Init(XElement *data)
{
    BOOL enabled;
    
    DwmIsCompositionEnabled(&enabled);
    if (!enabled)
        return false;

    this->data = data;

    GetTargetSize ();

    return true;
}

DWMCaptureSource::~DWMCaptureSource()
{
    if(warningID)
    {
        API->RemoveStreamInfo(warningID);
        warningID = 0;
    }

    EndScene(); //should never actually need to be called, but doing it anyway just to be safe
}

void DWMCaptureSource::EndCapture()
{
    if(warningID)
    {
        API->RemoveStreamInfo(warningID);
        warningID = 0;
    }
}

void DWMCaptureSource::Preprocess()
{
}

void DWMCaptureSource::BeginScene()
{
    strWindowClass = data->GetString(TEXT("windowClass"));
    if(strWindowClass.IsEmpty())
        return;

    bStretch = data->GetInt(TEXT("stretchImage")) != 0;
    bCaptureMouse = data->GetInt(TEXT("captureMouse"), 1) != 0;

    if(bCaptureMouse && data->GetInt(TEXT("invertMouse")))
        invertShader = CreatePixelShaderFromFile(TEXT("shaders\\InvertTexture.pShader"));

    AttemptCapture();
}

void DWMCaptureSource::AttemptCapture()
{
    hwndTarget = FindWindow(strWindowClass, NULL);
    if(!hwndTarget)
    {
        if(!warningID)
            warningID = API->AddStreamInfo(Str("Sources.SoftwareCaptureSource.WindowNotFound"), StreamInfoPriority_High);

        return;
    }

    if(warningID)
    {
        API->RemoveStreamInfo(warningID);
        warningID = 0;
    }

    RECT clientRect, windowRect;
    GetClientRect(hwndTarget, &clientRect);
    GetWindowRect(hwndTarget, &windowRect);

    hwndCapture = hwndTarget;

    //textureStartX = clientRect.left 

    HANDLE textureHandle = GetDWMSharedHandle(hwndTarget);

    sharedTexture = GS->CreateTextureFromSharedHandle(0, 0, textureHandle);
}

Vect2 DWMCaptureSource::GetTargetSize()
{
    HWND hwndTarget = FindWindow(data->GetString(TEXT("windowClass")), NULL);
    if(!hwndTarget)
        return Vect2(64,64);

    HANDLE textureHandle = GetDWMSharedHandle(hwndTarget);

    HRESULT         err;
    ID3D10Resource  *tempResource;
    ID3D10Texture2D *pTexture;
    ID3D10Device    *d3d;

    if (!GS)
        return Vect2(64,64);

    d3d = static_cast<ID3D10Device*>(GS->GetDevice());

    if (SUCCEEDED(err = d3d->OpenSharedResource(textureHandle, __uuidof(ID3D10Resource), (void**)&tempResource)))
    {
        if (SUCCEEDED(err = tempResource->QueryInterface(__uuidof(ID3D10Texture2D), (void**)&pTexture)))
        {
            D3D10_TEXTURE2D_DESC    textureDesc;

            pTexture->GetDesc(&textureDesc);

            texWidth = textureDesc.Width;
            texHeight = textureDesc.Height;

            tempResource->Release();

            return Vect2((float)texWidth, (float)texHeight);
        }
    }

    return Vect2(64,64);
}

void DWMCaptureSource::EndScene()
{
    if(invertShader)
    {
        delete invertShader;
        invertShader = NULL;
    }

    if(cursorTexture)
    {
        delete cursorTexture;
        cursorTexture = NULL;
    }

    if (sharedTexture)
    {
        delete sharedTexture;
        sharedTexture = NULL;
    }

    EndCapture();
}

void DWMCaptureSource::Tick(float fSeconds)
{
}

inline double round(double val)
{
    if(!_isnan(val) || !_finite(val))
        return val;

    if(val > 0.0f)
        return floor(val+0.5);
    else
        return floor(val-0.5);
}

LPBYTE GetCursorData(HICON hIcon, ICONINFO &ii, UINT &size)
{
    BITMAP bmp;
    HBITMAP hBmp = ii.hbmColor ? ii.hbmColor : ii.hbmMask;

    if(GetObject(hBmp, sizeof(bmp), &bmp) != 0)
    {
        BITMAPINFO bi;
        zero(&bi, sizeof(bi));

        size = bmp.bmWidth;

        void* lpBits;

        BITMAPINFOHEADER &bih = bi.bmiHeader;
        bih.biSize = sizeof(bih);
        bih.biBitCount = 32;
        bih.biWidth  = bmp.bmWidth;
        bih.biHeight = bmp.bmHeight;
        bih.biPlanes = 1;

        HDC hTempDC = CreateCompatibleDC(NULL);
        HBITMAP hBitmap = CreateDIBSection(hTempDC, &bi, DIB_RGB_COLORS, &lpBits, NULL, 0);
        HBITMAP hbmpOld = (HBITMAP)SelectObject(hTempDC, hBitmap);

        zero(lpBits, bmp.bmHeight*bmp.bmWidth*4);
        DrawIcon(hTempDC, 0, 0, hIcon);

        LPBYTE lpData = (LPBYTE)Allocate(bmp.bmHeight*bmp.bmWidth*4);
        mcpy(lpData, lpBits, bmp.bmHeight*bmp.bmWidth*4);

        SelectObject(hTempDC, hbmpOld);
        DeleteObject(hBitmap);
        DeleteDC(hTempDC);

        return lpData;
    }

    return NULL;
}

void DWMCaptureSource::Render(const Vect2 &pos, const Vect2 &size)
{
    if(sharedTexture)
    {
        //----------------------------------------------------------
        // capture mouse

        bMouseCaptured = false;
        if(bCaptureMouse)
        {
            CURSORINFO ci;
            zero(&ci, sizeof(ci));
            ci.cbSize = sizeof(ci);

            if(GetCursorInfo(&ci) && hwndCapture)
            {
                mcpy(&cursorPos, &ci.ptScreenPos, sizeof(cursorPos));

                ScreenToClient(hwndCapture, &cursorPos);

                if(ci.flags & CURSOR_SHOWING)
                {
                    if(ci.hCursor == hCurrentCursor)
                        bMouseCaptured = true;
                    else
                    {
                        HICON hIcon = CopyIcon(ci.hCursor);
                        hCurrentCursor = ci.hCursor;

                        delete cursorTexture;
                        cursorTexture = NULL;

                        if(hIcon)
                        {
                            ICONINFO ii;
                            if(GetIconInfo(hIcon, &ii))
                            {
                                xHotspot = int(ii.xHotspot);
                                yHotspot = int(ii.yHotspot);

                                UINT size;
                                LPBYTE lpData = GetCursorData(hIcon, ii, size);
                                if(lpData)
                                {
                                    cursorTexture = CreateTexture(size, size, GS_BGRA, lpData, FALSE);
                                    if(cursorTexture)
                                        bMouseCaptured = true;

                                    Free(lpData);
                                }

                                DeleteObject(ii.hbmColor);
                                DeleteObject(ii.hbmMask);
                            }

                            DestroyIcon(hIcon);
                        }
                    }
                }
            }
        }

        //----------------------------------------------------------
        // game texture
        EnableBlending (FALSE);
        DrawSpriteEx (sharedTexture, (255<<24) | 0xFFFFFF, pos.x, pos.y, pos.x + size.x, pos.y+size.y, 0.0, 0.0, 1.0, 1.0);
        EnableBlending (TRUE);
        //----------------------------------------------------------
        // draw mouse
        
        if(bMouseCaptured && cursorTexture)
        {
            Vect2 texPos = Vect2(0.0f, 0.0f);
            Vect2 texStretch = Vect2(1.0f, 1.0f);

            texPos = pos;

            float fCursorX = (pos.x + texPos.x + texStretch.x * float(cursorPos.x-xHotspot));
            float fCursorY = (pos.y + texPos.y + texStretch.y * float(cursorPos.y-xHotspot));
            float fCursorCX = texStretch.x * float(cursorTexture->Width());
            float fCursorCY = texStretch.y * float(cursorTexture->Height());

            Shader *lastShader;
            bool bInvertCursor = false;
            if(invertShader)
            {
                lastShader = GetCurrentPixelShader();
                if(bInvertCursor = ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 || (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0))
                    LoadPixelShader(invertShader);
            }

            DrawSprite(cursorTexture, 0xFFFFFFFF, fCursorX, fCursorY+fCursorCY, fCursorX+fCursorCX, fCursorY);

            if(bInvertCursor)
                LoadPixelShader(lastShader);
        }
    }
}

Vect2 DWMCaptureSource::GetSize() const
{
    if (texWidth)
        return Vect2(float(texWidth), float(texHeight));
    else
        return Vect2(64, 64);
}

void DWMCaptureSource::UpdateSettings()
{
    EndScene();
    BeginScene();
}
