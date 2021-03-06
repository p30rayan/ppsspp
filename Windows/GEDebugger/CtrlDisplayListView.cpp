﻿#include "Windows/GEDebugger/CtrlDisplayListView.h"
#include "Core/Config.h"
#include "Windows/GEDebugger/GEDebugger.h"

const PTCHAR CtrlDisplayListView::windowClass = _T("CtrlDisplayListView");

void CtrlDisplayListView::registerClass()
{
	WNDCLASSEX wndClass;

	wndClass.cbSize         = sizeof(wndClass);
	wndClass.lpszClassName  = windowClass;
	wndClass.hInstance      = GetModuleHandle(0);
	wndClass.lpfnWndProc    = wndProc;
	wndClass.hCursor        = LoadCursor (NULL, IDC_ARROW);
	wndClass.hIcon          = 0;
	wndClass.lpszMenuName   = 0;
	wndClass.hbrBackground  = (HBRUSH)GetSysColorBrush(COLOR_WINDOW);
	wndClass.style          = 0;
	wndClass.cbClsExtra     = 0;
	wndClass.cbWndExtra     = sizeof(CtrlDisplayListView*);
	wndClass.hIconSm        = 0;

	RegisterClassEx(&wndClass);
}

CtrlDisplayListView::CtrlDisplayListView(HWND _wnd)
	: wnd(_wnd)
{
	SetWindowLongPtr(wnd, GWLP_USERDATA, (LONG) this);
	SetWindowLong(wnd, GWL_STYLE, GetWindowLong(wnd,GWL_STYLE) | WS_VSCROLL);
	SetScrollRange(wnd, SB_VERT, -1,1,TRUE);
	
	instructionSize = 4;
	rowHeight = g_Config.iFontHeight+2;
	charWidth = g_Config.iFontWidth;

	font = CreateFont(rowHeight-2,charWidth,0,0,FW_DONTCARE,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,
		L"Lucida Console");
	boldfont = CreateFont(rowHeight-2,charWidth,0,0,FW_DEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,
		L"Lucida Console");

	pixelPositions.addressStart = 16;
	pixelPositions.opcodeStart = pixelPositions.addressStart + 19*charWidth;

	hasFocus = false;
	validDisplayList = false;
}

CtrlDisplayListView::~CtrlDisplayListView()
{

}

CtrlDisplayListView *CtrlDisplayListView::getFrom(HWND hwnd)
{
	return (CtrlDisplayListView*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
}

LRESULT CALLBACK CtrlDisplayListView::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	CtrlDisplayListView *win = CtrlDisplayListView::getFrom(hwnd);

	switch(msg)
	{
	case WM_NCCREATE:
		// Allocate a new CustCtrl structure for this window.
		win = new CtrlDisplayListView(hwnd);
		
		// Continue with window creation.
		return win != NULL;
	case WM_SIZE:
		win->redraw();
		break;
	case WM_PAINT:
		win->onPaint(wParam,lParam);
		break;
	case WM_SETFOCUS:
		SetFocus(hwnd);
		win->hasFocus=true;
		win->redraw();
		break;
	case WM_KILLFOCUS:
		win->hasFocus=false;
		win->redraw();
		break;
	case WM_VSCROLL:
		win->onVScroll(wParam,lParam);
		break;
	case WM_MOUSEWHEEL:
		if (GET_WHEEL_DELTA_WPARAM(wParam) > 0)
		{
			win->scrollWindow(-3);
		} else if (GET_WHEEL_DELTA_WPARAM(wParam) < 0) {
			win->scrollWindow(3);
		}
		break;
	case WM_LBUTTONDOWN:
		win->onMouseDown(wParam,lParam,1);
		break;
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		win->onKeyDown(wParam,lParam);
		return 0;
	case WM_GETDLGCODE:
		if (lParam && ((MSG*)lParam)->message == WM_KEYDOWN)
		{
			switch (wParam)
			{
			case VK_TAB:
				return DLGC_WANTMESSAGE;
			default:
				return DLGC_WANTCHARS|DLGC_WANTARROWS;
			}
		}
		return DLGC_WANTCHARS|DLGC_WANTARROWS;
	}
	
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void CtrlDisplayListView::redraw()
{
	GetClientRect(wnd, &rect);
	visibleRows = rect.bottom/rowHeight;

	InvalidateRect(wnd, NULL, FALSE);
	UpdateWindow(wnd); 
}


void CtrlDisplayListView::onPaint(WPARAM wParam, LPARAM lParam)
{
	if (!validDisplayList) return;

	PAINTSTRUCT ps;
	HDC actualHdc = BeginPaint(wnd, &ps);
	HDC hdc = CreateCompatibleDC(actualHdc);
	HBITMAP hBM = CreateCompatibleBitmap(actualHdc, rect.right-rect.left, rect.bottom-rect.top);
	SelectObject(hdc, hBM);

	SetBkMode(hdc, TRANSPARENT);

	HPEN nullPen=CreatePen(0,0,0xffffff);
	HPEN condPen=CreatePen(0,0,0xFF3020);
	HBRUSH nullBrush=CreateSolidBrush(0xffffff);
	HBRUSH currentBrush=CreateSolidBrush(0xFFEfE8);

	HPEN oldPen=(HPEN)SelectObject(hdc,nullPen);
	HBRUSH oldBrush=(HBRUSH)SelectObject(hdc,nullBrush);
	HFONT oldFont = (HFONT)SelectObject(hdc,(HGDIOBJ)font);

	for (int i = 0; i < visibleRows+2; i++)
	{
		unsigned int address=windowStart + i*instructionSize;
		bool stall = address == list.stall;

		int rowY1 = rowHeight*i;
		int rowY2 = rowHeight*(i+1);

		// draw background
		COLORREF backgroundColor = stall ? 0xCCCCFF : 0xFFFFFF;
		COLORREF textColor = 0x000000;

		if (address == curAddress)
		{
			if (hasFocus)
			{
				backgroundColor = address == curAddress ? 0xFF8822 : 0xFF9933;
				textColor = 0xFFFFFF;
			} else {
				backgroundColor = 0xC0C0C0;
			}
		}

		HBRUSH backgroundBrush = CreateSolidBrush(backgroundColor);
		HPEN backgroundPen = CreatePen(0,0,backgroundColor);
		SelectObject(hdc,backgroundBrush);
		SelectObject(hdc,backgroundPen);
		Rectangle(hdc,0,rowY1,rect.right,rowY1+rowHeight);
		
		SelectObject(hdc,currentBrush);
		SelectObject(hdc,nullPen);

		DeleteObject(backgroundBrush);
		DeleteObject(backgroundPen);

		SetTextColor(hdc,textColor);

		GPUDebugOp op = gpuDebug->DissassembleOp(address);

		char addressText[64];
		sprintf(addressText,"%08X %08X",op.pc,op.op);
		TextOutA(hdc,pixelPositions.addressStart,rowY1+2,addressText,(int)strlen(addressText));

		if (address == list.pc)
		{
			TextOut(hdc,pixelPositions.opcodeStart-8,rowY1,L"■",1);
		}

		const char* opcode = op.desc.c_str();
		SelectObject(hdc,stall ? boldfont : font);
		TextOutA(hdc,pixelPositions.opcodeStart,rowY1+2,opcode,(int)strlen(opcode));
		SelectObject(hdc,font);
	}

	SelectObject(hdc,oldFont);
	SelectObject(hdc,oldPen);
	SelectObject(hdc,oldBrush);

	// copy bitmap to the actual hdc
	BitBlt(actualHdc, 0, 0, rect.right, rect.bottom, hdc, 0, 0, SRCCOPY);
	DeleteObject(hBM);
	DeleteDC(hdc);

	DeleteObject(nullPen);
	DeleteObject(condPen);

	DeleteObject(nullBrush);
	DeleteObject(currentBrush);
	
	EndPaint(wnd, &ps);
}

void CtrlDisplayListView::onMouseDown(WPARAM wParam, LPARAM lParam, int button)
{
	int x = LOWORD(lParam);
	int y = HIWORD(lParam);

	int line = y/rowHeight;
	u32 newAddress = windowStart + line*instructionSize;
	setCurAddress(newAddress);

	SetFocus(wnd);
	redraw();
}

void CtrlDisplayListView::onVScroll(WPARAM wParam, LPARAM lParam)
{
	switch (wParam & 0xFFFF)
	{
	case SB_LINEDOWN:
		windowStart += instructionSize;
		break;
	case SB_LINEUP:
		windowStart -= instructionSize;
		break;
	case SB_PAGEDOWN:
		windowStart += visibleRows*instructionSize;
		break;
	case SB_PAGEUP:
		windowStart -= visibleRows*instructionSize;
		break;
	default:
		return;
	}
	redraw();
}

void CtrlDisplayListView::onKeyDown(WPARAM wParam, LPARAM lParam)
{
	u32 windowEnd = windowStart+visibleRows*instructionSize;

	switch (wParam & 0xFFFF)
	{
	case VK_DOWN:
		setCurAddress(curAddress + instructionSize, GetAsyncKeyState(VK_SHIFT) != 0);
		scrollAddressIntoView();
		break;
	case VK_UP:
		setCurAddress(curAddress - instructionSize, GetAsyncKeyState(VK_SHIFT) != 0);
		scrollAddressIntoView();
		break;
	case VK_NEXT:
		if (curAddress != windowEnd - instructionSize && curAddressIsVisible()) {
			setCurAddress(windowEnd - instructionSize, GetAsyncKeyState(VK_SHIFT) != 0);
			scrollAddressIntoView();
		} else {
			setCurAddress(curAddress + visibleRows * instructionSize, GetAsyncKeyState(VK_SHIFT) != 0);
			scrollAddressIntoView();
		}
		break;
	case VK_PRIOR:
		if (curAddress != windowStart && curAddressIsVisible()) {
			setCurAddress(windowStart, GetAsyncKeyState(VK_SHIFT) != 0);
			scrollAddressIntoView();
		} else {
			setCurAddress(curAddress - visibleRows * instructionSize, GetAsyncKeyState(VK_SHIFT) != 0);
			scrollAddressIntoView();
		}
		break;
	case VK_LEFT:
		gotoAddr(list.pc);
		return;
	case VK_F10:
	case VK_F11:
		SendMessage(GetParent(wnd),WM_GEDBG_STEPDISPLAYLIST,0,0);
		break;
	}
	redraw();
}


void CtrlDisplayListView::scrollAddressIntoView()
{
	u32 windowEnd = windowStart + visibleRows * instructionSize;

	if (curAddress < windowStart)
		windowStart = curAddress;
	else if (curAddress >= windowEnd)
		windowStart = curAddress - visibleRows * instructionSize + instructionSize;
}

bool CtrlDisplayListView::curAddressIsVisible()
{
	u32 windowEnd = windowStart + visibleRows * instructionSize;
	return curAddress >= windowStart && curAddress < windowEnd;
}