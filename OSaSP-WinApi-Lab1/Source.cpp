#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>
#include "resource.h"

using namespace Gdiplus;
#pragma comment(lib, "Gdiplus.lib")

#define DEFAULT_WIDTH 200
#define DEFAULT_HEIGHT 100
#define DEFAULT_BKGND_COLOR RGB(65, 105, 225)
#define WM_WAITABLE_TIMER (WM_USER + 0x0001)
#define _SECOND 10000000

struct figureState
{
	int x;
	int y;
	int width;
	int height;
	int delta;
	int xDir = 1;
	int yDir = 1;
}figure;

struct delta
{
	int x;
	int y;
}delta;

RECT wndRect;
BOOL isMooving = false;
BOOL isBmpLoaded = false;
HINSTANCE hInst;
GdiplusStartupInput gdiplusStartupInput;
ULONG_PTR gdiplusToken;
const UINT ID_TIMER = 1;
HWND hWnd;
BOOL isTimerRunning = false;
HANDLE hThread;
DWORD dwThreadId;
BOOL isWaitableTimerRunning = false;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
ATOM MyRegisterClass(HINSTANCE hInstance);
HWND InitInstance(HINSTANCE hInstance, int nCmdShow);
VOID Paint(HWND hWnd, LPPAINTSTRUCT lpPS);
VOID SetFigureState(int x, int y, int w, int h, int d);
VOID KeyboardHandler(HWND hWnd, WPARAM wParam);
VOID MouseWheelHandler(WPARAM wParam);
RECT InitFigureRect();
VOID ShowBmp(HDC dc);
VOID ValidateXY();
VOID Move();
DWORD WINAPI SetWTimer(LPVOID lpParam);
VOID CALLBACK TimerAPCProc(LPVOID lpArg, DWORD dwTimerLowValue, DWORD dwTimerHighWalue);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	hInst = hInstance;
	MyRegisterClass(hInstance);
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	hWnd = InitInstance(hInstance, nCmdShow);
	if (!hWnd)
		return FALSE;

	

	MSG msg;

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;
	HBRUSH hBrush = CreateSolidBrush(RGB(65, 105, 225));

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW || CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = hBrush;
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = L"MainWindow";
	wcex.hIconSm = wcex.hIcon;

	return RegisterClassEx(&wcex);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	BITMAP bm;
	int width = DEFAULT_WIDTH;
	int height = DEFAULT_HEIGHT;
	
	
	
	switch (message)
	{
	case WM_CREATE:
		
		GetClientRect(hWnd, &wndRect);
		
		SetFigureState(wndRect.right / 2 - width / 2,
			wndRect.bottom / 2 - height / 2, width,
			height, 5);
		break;

	case WM_ERASEBKGND:
		return (LRESULT)1;

	case WM_KEYDOWN:
		KeyboardHandler(hWnd, wParam);
		InvalidateRect(hWnd, NULL, TRUE);
		break;
	
	case WM_LBUTTONDOWN:
		POINT ptMousePos;
		ptMousePos.x = GET_X_LPARAM(lParam);
		ptMousePos.y = GET_Y_LPARAM(lParam);
		RECT figureRect = InitFigureRect();
		if (PtInRect(&figureRect, ptMousePos))
		{
			isMooving = true;
			delta.x = ptMousePos.x - figure.x;
			delta.y = ptMousePos.y - figure.y;
		}
			
		InvalidateRect(hWnd, NULL, TRUE);
		break;
		
	case WM_MOUSEMOVE:
		if (isMooving)
		{
			figure.x = GET_X_LPARAM(lParam) - delta.x;
			figure.y = GET_Y_LPARAM(lParam) - delta.y;
			ValidateXY();
			InvalidateRect(hWnd, NULL, TRUE);
		}
		break;

	case WM_LBUTTONUP:
		isMooving = false;
		break;

	case WM_MOUSEWHEEL:
		MouseWheelHandler(wParam);
		InvalidateRect(hWnd, NULL, TRUE);
		break;

	case WM_PAINT:
		BeginPaint(hWnd, &ps);
		Paint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_SIZE:
		GetClientRect(hWnd, &wndRect);
		break;

	case WM_WAITABLE_TIMER:
		Move();
		break;

	case WM_DESTROY:
		if (hThread != NULL)
			CloseHandle(hThread);

		if (gdiplusToken)
			GdiplusShutdown(gdiplusToken);

		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

HWND InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	HWND hWnd = CreateWindow(L"MainWindow", L"Hey, wake up!",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0,
		CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

	if (!hWnd)
		return 0;

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return hWnd;
}

VOID Paint(HWND hWnd, LPPAINTSTRUCT lpPS)
{
	RECT rc;
	HDC hdcMem;
	HBITMAP hbmBkGnd, hbmOld, hbmLogo;
	HBRUSH hbrBkGnd, hbrFill, hbrOld;

	GetClientRect(hWnd, &rc);
	hdcMem = CreateCompatibleDC(lpPS->hdc);
	
	hbmBkGnd = CreateCompatibleBitmap(lpPS->hdc, rc.right - rc.left,
		rc.bottom - rc.top);
	hbmOld = (HBITMAP)SelectObject(hdcMem, hbmBkGnd);
	hbrBkGnd = CreateSolidBrush(DEFAULT_BKGND_COLOR);
	FillRect(hdcMem, &rc, hbrBkGnd);
	DeleteObject(hbmBkGnd);
	DeleteObject(hbrBkGnd);

	if (isBmpLoaded)
	{
		ShowBmp(hdcMem);
	}
	else
	{
		figure.width = DEFAULT_WIDTH;
		figure.height = DEFAULT_HEIGHT;
		ValidateXY();
		hbrFill = CreateSolidBrush(RGB(0, 0, 255));
		hbrOld = (HBRUSH)SelectObject(hdcMem, hbrFill);

		Rectangle(hdcMem, figure.x, figure.y,
			figure.x + figure.width, figure.y + figure.height);
		SelectObject(hdcMem, hbrOld);
		DeleteObject(hbrFill);
	}
	
	BitBlt(lpPS->hdc, rc.left, rc.top,
		rc.right - rc.left, rc.bottom - rc.top,
		hdcMem, 0, 0, SRCCOPY);

	SelectObject(hdcMem, hbmOld);
	
	DeleteDC(hdcMem);
}

VOID SetFigureState(int x, int y, int w, int h, int d)
{
	figure.x = x;
	figure.y = y;
	figure.width = w;
	figure.height = h;
	figure.delta = d;
}

VOID KeyboardHandler(HWND hWnd, WPARAM wParam)
{
	switch (wParam)
	{
	case VK_LEFT:
		figure.x -= figure.delta;
		break;

	case VK_RIGHT:
		figure.x += figure.delta;
		break;

	case VK_UP:
		figure.y -= figure.delta;
		break;

	case VK_DOWN:
		figure.y += figure.delta;
		break;

	case VK_RETURN:
		isBmpLoaded = !isBmpLoaded;
		break;

	case VK_SPACE:
		if (!isTimerRunning)
		{
			SetTimer(hWnd, ID_TIMER, 10, (TIMERPROC)Move);
			isTimerRunning = true;
		}
		else
		{
			KillTimer(hWnd, ID_TIMER);
			isTimerRunning = false;
		}
		break;
	case VK_ESCAPE:
		if (!isWaitableTimerRunning)
		{
			hThread = CreateThread(NULL, 0, SetWTimer, NULL, 0, &dwThreadId);
			isWaitableTimerRunning = true;
		}
		else
		{
			CloseHandle(hThread);
			isWaitableTimerRunning = false;
		}
		break;
	}	
	ValidateXY();
}

VOID MouseWheelHandler(WPARAM wParam)
{
	WORD fwKeys = GET_KEYSTATE_WPARAM(wParam);
	short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

	int direction = (zDelta > 0) ? 1 : -1;
	if (fwKeys != MK_SHIFT)
	{
		figure.y -= direction * figure.delta;
	}
	else
	{
		figure.x -= direction * figure.delta;
	}
	ValidateXY();
}

VOID ValidateXY()
{
	if (figure.y < 0)
	{
		figure.y = 0;
	}
	else if (figure.y > wndRect.bottom - figure.height)
	{
		figure.y = wndRect.bottom - figure.height;
	}

	if (figure.x < 0)
	{
		figure.x = 0;
	}
	else if (figure.x > wndRect.right - figure.width)
	{
		figure.x = wndRect.right - figure.width;
	}
}

RECT InitFigureRect()
{
	RECT figureRect;
	figureRect.left = figure.x;
	figureRect.top = figure.y;
	figureRect.right = figure.x + figure.width;
	figureRect.bottom = figure.y + figure.height;
	return figureRect;
}

VOID ShowBmp(HDC dc)
{
	HDC memDC = CreateCompatibleDC(dc);
	Graphics graphics(dc);
	Image image(L"bsuirlogo.bmp");

	ImageAttributes imAtt;
	imAtt.SetColorKey(
		Color(31, 58, 144),
		Color(255, 255, 255),
		ColorAdjustTypeBitmap);

	figure.width = image.GetWidth();
	figure.height = image.GetHeight();

	graphics.DrawImage(
		&image,
		Rect(figure.x, figure.y, figure.width, figure.height),
		0, 0, figure.width, figure.height,
		UnitPixel,
		&imAtt);
	DeleteDC(memDC);
}

VOID Move()
{
	figure.x += figure.xDir * figure.delta;
	figure.y += figure.yDir * figure.delta;

	if (figure.x <= 0 || figure.x + figure.width >= wndRect.right)
		figure.xDir = -figure.xDir;
	
	if (figure.y + figure.height >= wndRect.bottom || figure.y<=0)
		figure.yDir = -figure.yDir;

	ValidateXY();
	InvalidateRect(hWnd, NULL, TRUE);
}

DWORD WINAPI SetWTimer(LPVOID lpParam)
{
	BOOL bSuccess;
	__int64 qwDueTime;
	LARGE_INTEGER liDueTime;
	HANDLE hTimer = CreateWaitableTimer(NULL, FALSE, TEXT("MyTimer"));

	if (hTimer != NULL)
	{
		__try
		{
			qwDueTime = -5 * _SECOND;

			liDueTime.LowPart = (DWORD)(qwDueTime & 0xFFFFFFFF);
			liDueTime.QuadPart = (LONG)(qwDueTime >> 32);

			bSuccess = SetWaitableTimer(
				hTimer,
				&liDueTime,
				10,
				TimerAPCProc,
				NULL,
				FALSE);
			if (bSuccess)
			{
				while (isWaitableTimerRunning)
				{
					SleepEx(INFINITE, TRUE);
				}
			}
		}
		__finally
		{
			CloseHandle(hTimer);
		}
	}
	return 0;
}

VOID TimerAPCProc(LPVOID lpArg, DWORD dwTimerLowValue, DWORD dwTimerHighWalue)
{
	PostMessage(hWnd, WM_WAITABLE_TIMER, NULL, NULL);
}