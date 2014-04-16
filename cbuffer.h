#pragma once

#include <util.h>
#include "comptr.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <d3d11.h>

#include <cassert>

using namespace util;

namespace Framework
{
	// Wrapper for constant buffers
	template <typename T>
	class CB
	{
	public:
		void	Init(ID3D11Device * pDevice);
		void	Update(ID3D11DeviceContext * pCtx, const T * pData);
		void	Bind(ID3D11DeviceContext * pCtx, uint slot);

		comptr<ID3D11Buffer>	m_pBuf;
	};

	// Inline template implementation

	template <typename T>
	inline void CB<T>::Init(ID3D11Device * pDevice)
	{
		D3D11_BUFFER_DESC bufDesc =
		{
			((sizeof(T) + 15) / 16) * 16,	// Round up to next 16 bytes
			D3D11_USAGE_DYNAMIC,
			D3D11_BIND_CONSTANT_BUFFER,
			D3D11_CPU_ACCESS_WRITE,
		};

		if (FAILED(pDevice->CreateBuffer(&bufDesc, nullptr, &m_pBuf)))
		{
			assert(false);
		}
	}

	template <typename T>
	inline void CB<T>::Update(ID3D11DeviceContext * pCtx, const T * pData)
	{
		D3D11_MAPPED_SUBRESOURCE mapped = {};
		if (FAILED(pCtx->Map(m_pBuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			assert(false);
			return;
		}
		memcpy(mapped.pData, pData, sizeof(T));
		pCtx->Unmap(m_pBuf, 0);
	}

	template <typename T>
	inline void CB<T>::Bind(ID3D11DeviceContext * pCtx, uint slot)
	{
		pCtx->VSSetConstantBuffers(slot, 1, &m_pBuf);
		pCtx->PSSetConstantBuffers(slot, 1, &m_pBuf);
	}
}