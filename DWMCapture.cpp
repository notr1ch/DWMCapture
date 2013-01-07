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


extern "C" __declspec(dllexport) bool LoadPlugin();
extern "C" __declspec(dllexport) void UnloadPlugin();
extern "C" __declspec(dllexport) CTSTR GetPluginName();
extern "C" __declspec(dllexport) CTSTR GetPluginDescription();

HINSTANCE hinstMain = NULL;

#define DWMCAPTURE_CLASSNAME TEXT("DWMCapture")

struct WindowInfo
{
    String strClass;
    String strProcess;
};

struct ConfigDialogData
{
    XElement *data;
    List<WindowInfo> windowData;

    UINT cx, cy;

    inline void ClearData()
    {
        for(UINT i=0; i<windowData.Num(); i++)
            windowData[i].strClass.Clear();
        windowData.Clear();
    }

    inline ~ConfigDialogData()
    {
        for(UINT i=0; i<windowData.Num(); i++)
            windowData[i].strClass.Clear();
    }
};

void RefreshWindowList(HWND hwndCombobox, ConfigDialogData &configData)
{
    SendMessage(hwndCombobox, CB_RESETCONTENT, 0, 0);
    configData.ClearData();

    HWND hwndCurrent = GetWindow(GetDesktopWindow(), GW_CHILD);
    do
    {
        if(IsWindowVisible(hwndCurrent))
        {
            RECT clientRect;
            GetClientRect(hwndCurrent, &clientRect);

            HWND hwndParent = GetParent(hwndCurrent);

            DWORD exStyles = (DWORD)GetWindowLongPtr(hwndCurrent, GWL_EXSTYLE);
            DWORD styles = (DWORD)GetWindowLongPtr(hwndCurrent, GWL_STYLE);

            if( (exStyles & WS_EX_TOOLWINDOW) == 0 && (styles & WS_CHILD) == 0 /*&& hwndParent == NULL*/)
            {
                String strWindowName;
                strWindowName.SetLength(GetWindowTextLength(hwndCurrent));
                GetWindowText(hwndCurrent, strWindowName, strWindowName.Length()+1);

                bool b64bit = false;

                //-------

                DWORD processID;
                GetWindowThreadProcessId(hwndCurrent, &processID);
                if(processID == GetCurrentProcessId())
                    continue;

                TCHAR fileName[MAX_PATH+1];
                scpy(fileName, TEXT("unknown"));

                HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, processID);
                if(hProcess)
                {
                    DWORD dwSize = MAX_PATH;
                    QueryFullProcessImageName(hProcess, 0, fileName, &dwSize);
                }
                else
                {
                    hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processID);
                    if(hProcess)
                    {
                        CloseHandle(hProcess);
                    }

                    continue;
                }

                //-------

                String strFileName = fileName;
                strFileName.FindReplace(TEXT("\\"), TEXT("/"));

                String strText;
                strText << TEXT("[") << GetPathFileName(strFileName);
                strText << (b64bit ? TEXT("*64") : TEXT("*32"));
                strText << TEXT("]: ") << strWindowName;

                int id = (int)SendMessage(hwndCombobox, CB_ADDSTRING, 0, (LPARAM)strText.Array());
                SendMessage(hwndCombobox, CB_SETITEMDATA, id, (LPARAM)hwndCurrent);

                String strClassName;
                strClassName.SetLength(256);
                GetClassName(hwndCurrent, strClassName.Array(), 255);
                strClassName.SetLength(slen(strClassName));

                WindowInfo &info    = *configData.windowData.CreateNew();
                info.strClass       = strClassName;
                info.strProcess     = fileName;
            }
        }
    } while (hwndCurrent = GetNextWindow(hwndCurrent, GW_HWNDNEXT));
}

INT_PTR CALLBACK ConfigureDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
                ConfigDialogData *info = (ConfigDialogData*)lParam;
                XElement *data = info->data;

                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)lParam);
                LocalizeWindow(hwnd);

                //--------------------------------------------

                SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDC_REFRESH, BN_CLICKED), (LPARAM)GetDlgItem(hwnd, IDC_APPLIST));

                //--------------------------------------------

                BOOL bCaptureMouse = data->GetInt(TEXT("captureMouse"), 1);
                SendMessage(GetDlgItem(hwnd, IDC_STRETCHTOSCREEN),    BM_SETCHECK, data->GetInt(TEXT("stretchImage")) ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hwnd, IDC_CAPTUREMOUSE),       BM_SETCHECK, bCaptureMouse                      ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hwnd, IDC_INVERTMOUSEONCLICK), BM_SETCHECK, data->GetInt(TEXT("invertMouse"))  ? BST_CHECKED : BST_UNCHECKED, 0);
                EnableWindow(GetDlgItem(hwnd, IDC_INVERTMOUSEONCLICK), bCaptureMouse);

                return TRUE;
            }

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_CAPTUREMOUSE:
                    {
                        BOOL bCaptureMouse = SendMessage(GetDlgItem(hwnd, IDC_CAPTUREMOUSE), BM_GETCHECK, 0, 0) == BST_CHECKED;
                        EnableWindow(GetDlgItem(hwnd, IDC_INVERTMOUSEONCLICK), bCaptureMouse);
                    }
                    break;

                case IDC_REFRESH:
                    {
                        ConfigDialogData *info = (ConfigDialogData*)GetWindowLongPtr(hwnd, DWLP_USER);
                        XElement *data = info->data;

                        CTSTR lpWindowName = data->GetString(TEXT("window"));

                        HWND hwndWindowList = GetDlgItem(hwnd, IDC_APPLIST);
                        RefreshWindowList(hwndWindowList, *info);

                        UINT windowID = 0;
                        if(lpWindowName)
                            windowID = (UINT)SendMessage(hwndWindowList, CB_FINDSTRINGEXACT, -1, (LPARAM)lpWindowName);

                        if(windowID != CB_ERR)
                            SendMessage(hwndWindowList, CB_SETCURSEL, windowID, 0);
                        else
                            SendMessage(hwndWindowList, CB_SETCURSEL, 0, 0);

                        String strInfoText;

                        //todo: remove later whem more stable
                        strInfoText << TEXT("Note: This plugin is currently experimental and may not be fully stable yet.\r\n");

                        SetWindowText(GetDlgItem(hwnd, IDC_INFO), strInfoText);
                    }
                    break;

                case IDOK:
                    {
                        UINT windowID = (UINT)SendMessage(GetDlgItem(hwnd, IDC_APPLIST), CB_GETCURSEL, 0, 0);
                        if(windowID == CB_ERR) windowID = 0;

                        ConfigDialogData *info = (ConfigDialogData*)GetWindowLongPtr(hwnd, DWLP_USER);
                        XElement *data = info->data;

                        if(!info->windowData.Num())
                            return 0;

                        String strWindow = GetCBText(GetDlgItem(hwnd, IDC_APPLIST), windowID);
                        data->SetString(TEXT("window"),      strWindow);
                        data->SetString(TEXT("windowClass"), info->windowData[windowID].strClass);

                        data->SetInt(TEXT("stretchImage"), SendMessage(GetDlgItem(hwnd, IDC_STRETCHTOSCREEN), BM_GETCHECK, 0, 0) == BST_CHECKED);
                        data->SetInt(TEXT("captureMouse"), SendMessage(GetDlgItem(hwnd, IDC_CAPTUREMOUSE), BM_GETCHECK, 0, 0) == BST_CHECKED);
                        data->SetInt(TEXT("invertMouse"),  SendMessage(GetDlgItem(hwnd, IDC_INVERTMOUSEONCLICK), BM_GETCHECK, 0, 0) == BST_CHECKED);
                    }

                case IDCANCEL:
                    EndDialog(hwnd, LOWORD(wParam));
            }
            break;

        case WM_CLOSE:
            EndDialog(hwnd, IDCANCEL);
    }
    return 0;
}

bool STDCALL ConfigureDWMCaptureSource(XElement *element, bool bCreating)
{
    if(!element)
    {
        AppWarning(TEXT("ConfigureDWMCaptureSource: NULL element"));
        return false;
    }

    XElement *data = element->GetElement(TEXT("data"));
    if(!data)
        data = element->CreateElement(TEXT("data"));

    ConfigDialogData *configData = new ConfigDialogData;
    configData->data = data;

    if(DialogBoxParam(hinstMain, MAKEINTRESOURCE(IDD_CONFIG), API->GetMainWindow(), ConfigureDialogProc, (LPARAM)configData) == IDOK)
    {
        //UINT width, height;
        //API->GetBaseSize(width, height);

        //element->SetInt(TEXT("cx"), width);
        //element->SetInt(TEXT("cy"), height);

        DWMCaptureSource *source = new DWMCaptureSource;
        if (source->Init(data))
        {
            element->SetInt(TEXT("cx"), source->GetSize().x);
            element->SetInt(TEXT("cy"), source->GetSize().y);
            delete source;
        }

        delete configData;
        return true;
    }

    delete configData;
    return false;
}

ImageSource* STDCALL CreateDWMCaptureSource(XElement *data)
{
    DWMCaptureSource *source = new DWMCaptureSource;
    if(!source->Init(data))
    {
        delete source;
        return NULL;
    }

    return source;
}

bool LoadPlugin()
{
    API->RegisterImageSourceClass(DWMCAPTURE_CLASSNAME, TEXT("DWM Shared Surface Capture"), (OBSCREATEPROC)CreateDWMCaptureSource, (OBSCONFIGPROC)ConfigureDWMCaptureSource);
    return true;
}

void UnloadPlugin()
{
}

CTSTR GetPluginName()
{
    return TEXT("DWM Shared Surface Capture");
}

CTSTR GetPluginDescription()
{
    return TEXT("Capture windows directly from DWM shared surface textures using undocumented Windows APIs. Only works well with Direct3D apps. Highly experimental!");
}

BOOL CALLBACK DllMain(HINSTANCE hInst, DWORD dwReason, LPVOID lpBla)
{
    if(dwReason == DLL_PROCESS_ATTACH)
        hinstMain = hInst;

    return TRUE;
}
