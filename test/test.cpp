#include <framework.h>
#include <AntTweakBar.h>
#include "shader-common.h"

// Shader bytecode generated by build process
#include "world_vs.h"
#include "simple_ps.h"

using namespace util;
using namespace Framework;



// Globals

float3 g_vecDirectionalLight = normalize(makefloat3(1, 1, 1));
rgb g_rgbDirectionalLight = makergb(1.0f, 1.0f, 0.77f);
rgb g_rgbSky = makergb(0.44f, 0.56f, 1.0f);

float g_debugSlider0 = 0.0f;
float g_debugSlider1 = 0.0f;
float g_debugSlider2 = 0.0f;
float g_debugSlider3 = 0.0f;



// Constant buffers

struct CBFrame								// matches cbuffer CBFrame in shader-common.h
{
	float4x4	m_matWorldToClip;
	float4x4	m_matWorldToUvzwShadow;
	point3		m_posCamera;
	float		m_dummy0;

	float3		m_vecDirectionalLight;
	float		m_dummy1;

	rgb			m_rgbDirectionalLight;
	float		m_exposure;					// Exposure multiplier
};

struct CBDebug								// matches cbuffer CBDebug in shader-common.h
{
	float		m_debugKey;					// Mapped to spacebar - 0 if up, 1 if down
	float		m_debugSlider0;				// Mapped to debug sliders in UI
	float		m_debugSlider1;				// ...
	float		m_debugSlider2;				// ...
	float		m_debugSlider3;				// ...
};



// Window class

class TestWindow : public D3D11Window
{
public:
	typedef D3D11Window super;

						TestWindow();

	bool				Init(HINSTANCE hInstance);
	virtual void		Shutdown();
	virtual LRESULT		MsgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	virtual void		OnResize(int2_arg dimsNew);
	virtual void		OnRender();

	Mesh								m_meshSponza;
	MaterialLib							m_mtlLibSponza;
	TextureLib							m_texLibSponza;
	Texture2D							m_texStone;
	comptr<ID3D11VertexShader>			m_pVsWorld;
	comptr<ID3D11PixelShader>			m_pPsSimple;
	comptr<ID3D11InputLayout>			m_pInputLayout;
	CB<CBFrame>							m_cbFrame;
	CB<CBDebug>							m_cbDebug;
	FPSCamera							m_camera;
	Timer								m_timer;
};

// TestWindow implementation

TestWindow::TestWindow()
{
}

bool TestWindow::Init(HINSTANCE hInstance)
{
	super::Init("TestWindow", "Test", hInstance);

	// Ensure the asset pack is up to date
	static const AssetCompileInfo s_assets[] =
	{
		{ "sponza/sponza_cracksfilled.obj", ACK_OBJMesh, },
		{ "sponza/sponza.mtl", ACK_OBJMtlLib, },
		{ "sponza/sp_luk.jpg", ACK_TextureWithMips, },
		{ "sponza/sp_luk-bump.jpg", ACK_TextureWithMips, },
		{ "sponza/00_skap.jpg", ACK_TextureWithMips, },
		{ "sponza/01_stub.jpg", ACK_TextureWithMips, },
		{ "sponza/01_stub-bump.jpg", ACK_TextureWithMips, },
		{ "sponza/01_s_ba.jpg", ACK_TextureWithMips, },
		{ "sponza/01_st_kp.jpg", ACK_TextureWithMips, },
		{ "sponza/01_st_kp-bump.jpg", ACK_TextureWithMips, },
		{ "sponza/x01_st.jpg", ACK_TextureWithMips, },
		{ "sponza/kamen-stup.jpg", ACK_TextureWithMips, },
		{ "sponza/reljef.jpg", ACK_TextureWithMips, },
		{ "sponza/reljef-bump.jpg", ACK_TextureWithMips, },
		{ "sponza/kamen.jpg", ACK_TextureWithMips, },
		{ "sponza/kamen-bump.jpg", ACK_TextureWithMips, },
		{ "sponza/prozor1.jpg", ACK_TextureWithMips, },
		{ "sponza/vrata_kr.jpg", ACK_TextureWithMips, },
		{ "sponza/vrata_ko.jpg", ACK_TextureWithMips, },
	};
	comptr<AssetPack> pPack = new AssetPack;
	if (!LoadAssetPackOrCompileIfOutOfDate("sponza-assets.zip", s_assets, dim(s_assets), pPack))
	{
		ERR("Couldn't load or compile Sponza asset pack");
		return false;
	}

	// Load assets
	if (!LoadTextureLibFromAssetPack(pPack, s_assets, dim(s_assets), &m_texLibSponza))
	{
		ERR("Couldn't load Sponza texture library");
		return false;
	}
	if (!LoadMaterialLibFromAssetPack(pPack, "sponza/sponza.mtl", &m_texLibSponza, &m_mtlLibSponza))
	{
		ERR("Couldn't load Sponza material library");
		return false;
	}
	if (!LoadMeshFromAssetPack(pPack, "sponza/sponza_cracksfilled.obj", &m_mtlLibSponza, &m_meshSponza))
	{
		ERR("Couldn't load Sponza mesh");
		return false;
	}
	if (!LoadTexture2DFromAssetPack(pPack, "sponza/kamen.jpg", &m_texStone))
	{
		ERR("Couldn't load Sponza stone texture");
		return false;
	}

	m_meshSponza.UploadToGPU(m_pDevice);
	m_texStone.UploadToGPU(m_pDevice);

	// Load shaders
	CHECK_D3D(m_pDevice->CreateVertexShader(world_vs_bytecode, dim(world_vs_bytecode), nullptr, &m_pVsWorld));
	CHECK_D3D(m_pDevice->CreatePixelShader(simple_ps_bytecode, dim(simple_ps_bytecode), nullptr, &m_pPsSimple));

	// Initialize the input layout, and validate it against all the vertex shaders

	D3D11_INPUT_ELEMENT_DESC aInputDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, UINT(offsetof(Vertex, m_pos)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, UINT(offsetof(Vertex, m_normal)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, UINT(offsetof(Vertex, m_uv)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	CHECK_D3D(m_pDevice->CreateInputLayout(
							aInputDescs, dim(aInputDescs),
							world_vs_bytecode, dim(world_vs_bytecode),
							&m_pInputLayout));

	// Init constant buffers
	m_cbFrame.Init(m_pDevice);
	m_cbDebug.Init(m_pDevice);

	// Init the camera
	m_camera.m_moveSpeed = 3.0f;
	m_camera.m_mbuttonActivate = MBUTTON_Left;
	m_camera.LookAt(
				makepoint3(-8.7f, 6.8f, 0.0f),
				makepoint3(0.0f, 5.0f, 0.0f));

	// Init AntTweakBar
	CHECK_ERR(TwInit(TW_DIRECT3D11, m_pDevice));

	// Automatically use the biggest font size
	TwDefine("GLOBAL fontsize=3 fontresizable=false");

	// Create bar for FPS display
	TwBar * pTwBarFPS = TwNewBar("FPS");
	TwDefine("FPS position='15 15' size='225 80' valueswidth=75 refresh=0.5");
	TwAddVarCB(
			pTwBarFPS, "Frame time (ms)", TW_TYPE_FLOAT,
			nullptr,
			[](void * value, void * timestep) { 
				*(float *)value = 1000.0f * *(float *)timestep;
			},
			&m_timer.m_timestep,
			"precision=2");
	TwAddVarCB(
			pTwBarFPS, "FPS", TW_TYPE_FLOAT,
			nullptr,
			[](void * value, void * timestep) { 
				*(float *)value = 1.0f / *(float *)timestep;
			},
			&m_timer.m_timestep,
			"precision=1");

	// Create bar for debug sliders
	TwBar * pTwBarDebug = TwNewBar("Debug");
	TwDefine("Debug position='15 110' size='225 115' valueswidth=75");
	TwAddVarRW(pTwBarDebug, "g_debugSlider0", TW_TYPE_FLOAT, &g_debugSlider0, "min=0.0 step=0.01 precision=2");
	TwAddVarRW(pTwBarDebug, "g_debugSlider1", TW_TYPE_FLOAT, &g_debugSlider1, "min=0.0 step=0.01 precision=2");
	TwAddVarRW(pTwBarDebug, "g_debugSlider2", TW_TYPE_FLOAT, &g_debugSlider2, "min=0.0 step=0.01 precision=2");
	TwAddVarRW(pTwBarDebug, "g_debugSlider3", TW_TYPE_FLOAT, &g_debugSlider3, "min=0.0 step=0.01 precision=2");

	// Create bar for lighting
	TwBar * pTwBarLight = TwNewBar("Lighting");
	TwDefine("Lighting position='15 240' size='275 355' valueswidth=130");
	TwAddVarRW(pTwBarLight, "Light direction", TW_TYPE_DIR3F, &g_vecDirectionalLight, nullptr);
	TwAddVarRW(pTwBarLight, "Light color", TW_TYPE_COLOR3F, &g_rgbDirectionalLight, nullptr);
	TwAddVarRW(pTwBarLight, "Sky color", TW_TYPE_COLOR3F, &g_rgbSky, nullptr);

	// Create bar for camera position and orientation
	TwBar * pTwBarCamera = TwNewBar("Camera");
	TwDefine("Camera position='255 15' size='195 180' valueswidth=75 refresh=0.5");
	TwAddVarRO(pTwBarCamera, "Camera X", TW_TYPE_FLOAT, &m_camera.m_pos.x, "precision=3");
	TwAddVarRO(pTwBarCamera, "Camera Y", TW_TYPE_FLOAT, &m_camera.m_pos.y, "precision=3");
	TwAddVarRO(pTwBarCamera, "Camera Z", TW_TYPE_FLOAT, &m_camera.m_pos.z, "precision=3");
	TwAddVarRO(pTwBarCamera, "Yaw", TW_TYPE_FLOAT, &m_camera.m_yaw, "precision=3");
	TwAddVarRO(pTwBarCamera, "Pitch", TW_TYPE_FLOAT, &m_camera.m_pitch, "precision=3");
	auto lambdaNegate = [](void * outValue, void * inValue) { *(float *)outValue = -*(float *)inValue; };
	TwAddVarCB(pTwBarCamera, "Look X", TW_TYPE_FLOAT, nullptr, lambdaNegate, &m_camera.m_viewToWorld.m_linear[2].x, "precision=3");
	TwAddVarCB(pTwBarCamera, "Look Y", TW_TYPE_FLOAT, nullptr, lambdaNegate, &m_camera.m_viewToWorld.m_linear[2].y, "precision=3");
	TwAddVarCB(pTwBarCamera, "Look Z", TW_TYPE_FLOAT, nullptr, lambdaNegate, &m_camera.m_viewToWorld.m_linear[2].z, "precision=3");

	return true;
}

void TestWindow::Shutdown()
{
	TwTerminate();
	super::Shutdown();
}

LRESULT TestWindow::MsgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// Give AntTweakBar and the camera a crack at the message
	if (TwEventWin(hWnd, message, wParam, lParam))
		return 0;
	if (m_camera.HandleWindowsMessage(message, wParam, lParam))
		return 0;

	switch (message)
	{
	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_ESCAPE:
			Shutdown();
			break;
		}
		return 0;

	default:
		return super::MsgProc(hWnd, message, wParam, lParam);
	}
}

void TestWindow::OnResize(int2_arg dimsNew)
{
	super::OnResize(dimsNew);

	// Update projection matrix for new aspect ratio
	m_camera.SetProjection(1.0f, float(dimsNew.x) / float(dimsNew.y), 0.1f, 1000.0f);
}

void TestWindow::OnRender()
{
	m_timer.OnFrameStart();
	m_camera.Update(m_timer.m_timestep);

	m_pCtx->ClearState();
	m_pCtx->IASetInputLayout(m_pInputLayout);
	m_pCtx->RSSetState(m_pRsDefault);
	m_pCtx->OMSetDepthStencilState(m_pDssDepthTest, 0);

	// Set up whole-frame constant buffers

	CBFrame cbFrame =
	{
		m_camera.m_worldToClip,
		float4x4::identity(),
		m_camera.m_pos,
		0,
		g_vecDirectionalLight,
		0,
		g_rgbDirectionalLight,
		1.0f,
	};
	m_cbFrame.Update(m_pCtx, &cbFrame);
	m_cbFrame.Bind(m_pCtx, CB_FRAME);

	CBDebug cbDebug =
	{
		// !!!UNDONE: move keyboard tracking into an input system that respects focus, etc.
		GetAsyncKeyState(' ') ? 1.0f : 0.0f,
		g_debugSlider0,
		g_debugSlider1,
		g_debugSlider2,
		g_debugSlider3,
	};
	m_cbDebug.Update(m_pCtx, &cbDebug);
	m_cbDebug.Bind(m_pCtx, CB_DEBUG);

	m_pCtx->ClearRenderTargetView(m_pRtvRaw, makergba(g_rgbSky, 1.0f));
	m_pCtx->ClearDepthStencilView(m_pDsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
	BindSRGBBackBuffer(m_pCtx);

	m_pCtx->VSSetShader(m_pVsWorld, nullptr, 0);
	m_pCtx->PSSetShader(m_pPsSimple, nullptr, 0);
	m_pCtx->PSSetShaderResources(0, 1, &m_texStone.m_pSrv);
	m_pCtx->PSSetSamplers(0, 1, &m_pSsTrilinearRepeatAniso);
	m_meshSponza.Draw(m_pCtx);

	CHECK_WARN(TwDraw());
	CHECK_D3D(m_pSwapChain->Present(1, 0));
}



// Get the whole shebang going

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	(void)hPrevInstance;
	(void)lpCmdLine;
	(void)nCmdShow;

	TestWindow w;
	if (!w.Init(hInstance))
	{
		w.Shutdown();
		return 1;
	}

	w.MainLoop(SW_SHOWMAXIMIZED);
	return 0;
}
