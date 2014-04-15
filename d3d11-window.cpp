#include "d3d11-window.h"

#include <util.h>
#include <cassert>

namespace Framework
{
	static LRESULT CALLBACK StaticMsgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	D3D11Window::D3D11Window()
	:	m_hInstance(nullptr),
		m_hWnd(nullptr),
		m_pSwapChain(),
		m_pDevice(),
		m_pCtx(),
		m_pRtvSRGB(),
		m_pRtvRaw(),
		m_width(0),
		m_height(0)
	{
	}

	bool D3D11Window::Init(
		const char * windowClassName,
		const char * windowTitle,
		HINSTANCE hInstance,
		int nShowCmd)
	{
		m_hInstance = hInstance;

		// Register window class
		WNDCLASS wc =
		{
			0,
			&StaticMsgProc,
			0,
			0,
			hInstance,
			LoadIcon(nullptr, IDI_APPLICATION),
			LoadCursor(nullptr, IDC_ARROW),
			nullptr,
			nullptr,
			windowClassName,
		};
		if (!RegisterClass(&wc))
		{
			assert(false);
			return false;
		}

		// Create the window
		m_hWnd = CreateWindow(
					windowClassName,
					windowTitle,
					WS_OVERLAPPEDWINDOW,
					CW_USEDEFAULT,
					CW_USEDEFAULT,
					CW_USEDEFAULT,
					CW_USEDEFAULT,
					nullptr,
					nullptr,
					hInstance,
					this);
		if (!m_hWnd)
		{
			assert(false);
			return false;
		}

		// Initialize D3D11
		UINT flags = 0;
#ifdef _DEBUG
		flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
		DXGI_SWAP_CHAIN_DESC swapChainDesc =
		{
			{ 0, 0, {}, DXGI_FORMAT_R8G8B8A8_UNORM, },
			{ 1, 0, },
			DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_BACK_BUFFER,
			2,
			m_hWnd,
			true,
			DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
		};
		D3D_FEATURE_LEVEL featureLevel;
		if (FAILED(D3D11CreateDeviceAndSwapChain(
						nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
						flags, nullptr, 0, D3D11_SDK_VERSION,
						&swapChainDesc,
						&m_pSwapChain, &m_pDevice,
						&featureLevel, &m_pCtx)))
		{
			assert(false);
			return false;
		}

		// Show the window
		ShowWindow(m_hWnd, nShowCmd);

		return true;
	}

	void D3D11Window::Shutdown()
	{
		m_hInstance = nullptr;
		m_width = 0;
		m_height = 0;

		m_pRtvSRGB.release();
		m_pRtvRaw.release();
		m_pCtx.release();
		m_pDevice.release();
		m_pSwapChain.release();

		if (m_hWnd)
		{
			DestroyWindow(m_hWnd);
			m_hWnd = nullptr;
		}
	}

	int D3D11Window::MainLoop()
	{
		MSG msg;

		for (;;)
		{
			// Handle any messages
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				DispatchMessage(&msg);
			}

			// Quit if necessary
			if (msg.message == WM_QUIT)
				break;

			// Render the frame
			OnRender();
		}

		// Return code specified in PostQuitMessage() ends up in wParam of WM_QUIT
		return int(msg.wParam);
	}

	static LRESULT CALLBACK StaticMsgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		D3D11Window * pWindow;

		if (message == WM_CREATE)
		{
			// On creation, stash pointer to the D3D11Window object in the window data
			CREATESTRUCT * pCreate = (CREATESTRUCT *)lParam;
			pWindow = (D3D11Window *)pCreate->lpCreateParams;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, LONG_PTR(pWindow));
		}
		else
		{
			// Retrieve the D3D11Window object from the window data
			pWindow = (D3D11Window *)GetWindowLongPtr(hWnd, GWLP_USERDATA);

			// If it's not there yet (due to messages prior to WM_CREATE),
			// just fall back to DefWindowProc
			if (!pWindow)
				return DefWindowProc(hWnd, message, wParam, lParam);
		}

		// Pass the message to the D3D11Window object
		return pWindow->MsgProc(hWnd, message, wParam, lParam);
	}

	LRESULT D3D11Window::MsgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_CLOSE:
			Shutdown();
			return 0;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		case WM_SIZE:
			{
				int width = int(LOWORD(lParam));
				int height = int(HIWORD(lParam));
				if (width > 0 && height > 0 && (width != m_width || height != m_height))
					OnResize(width, height);
				return 0;
			}

		case WM_SIZING:
			{
				RECT clientRect;
				GetClientRect(hWnd, &clientRect);
				int width = clientRect.right - clientRect.left;
				int height = clientRect.bottom - clientRect.top;
				if (width > 0 && height > 0 && (width != m_width || height != m_height))
					OnResize(width, height);
				return 0;
			}

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}

	void D3D11Window::OnResize(int width, int height)
	{
		m_width = width;
		m_height = height;

		// Have to release old render target views before swap chain can be resized
		m_pRtvSRGB.release();
		m_pRtvRaw.release();

		// Resize the swap chain to fit the window again
		if (!m_pSwapChain)
		{
			assert(false);
			return;
		}
		if (FAILED(m_pSwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0)))
		{
			assert(false);
			return;
		}

		// Retrieve the back buffer
		comptr<ID3D11Texture2D> pTex;
		if (FAILED(m_pSwapChain->GetBuffer(0, IID_ID3D11Texture2D, (void **)&pTex)))
		{
			assert(false);
			return;
		}

		// Create render target views in sRGB and raw formats
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = 
		{
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			D3D11_RTV_DIMENSION_TEXTURE2D,
		};
		if (FAILED(m_pDevice->CreateRenderTargetView(pTex, &rtvDesc, &m_pRtvSRGB)))
		{
			assert(false);
		}
		rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		if (FAILED(m_pDevice->CreateRenderTargetView(pTex, &rtvDesc, &m_pRtvRaw)))
		{
			assert(false);
		}
	}
}
