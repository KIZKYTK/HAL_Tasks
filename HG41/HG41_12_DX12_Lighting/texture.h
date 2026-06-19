#pragma once

#include "main.h"


using namespace Microsoft::WRL;

class CTexture
{
private:

	ComPtr<ID3D12Resource>			m_Resource;
	ComPtr<ID3D12DescriptorHeap>	m_DescriptorHeap;


public:

	void Load( const char *FileName );

	ComPtr<ID3D12DescriptorHeap> GetSDescriptorHeap() { return m_DescriptorHeap; }


};