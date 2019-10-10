/******************************************************************************************
*	Chili DirectX Framework Version 16.07.20											  *
*	Graphics.cpp																		  *
*	Copyright 2016 PlanetChili.net <http://www.planetchili.net>							  *
*																						  *
*	This file is part of The Chili DirectX Framework.									  *
*																						  *
*	The Chili DirectX Framework is free software: you can redistribute it and/or modify	  *
*	it under the terms of the GNU General Public License as published by				  *
*	the Free Software Foundation, either version 3 of the License, or					  *
*	(at your option) any later version.													  *
*																						  *
*	The Chili DirectX Framework is distributed in the hope that it will be useful,		  *
*	but WITHOUT ANY WARRANTY; without even the implied warranty of						  *
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the						  *
*	GNU General Public License for more details.										  *
*																						  *
*	You should have received a copy of the GNU General Public License					  *
*	along with The Chili DirectX Framework.  If not, see <http://www.gnu.org/licenses/>.  *
******************************************************************************************/
#include "MainWindow.h"
#include "Graphics.h"
#include "DXErr.h"
#include "ChiliException.h"
#include <assert.h>
#include <string>
#include <array>

// Ignore the intellisense error "cannot open source file" for .shh files.
// They will be created during the build sequence before the preprocessor runs.
namespace FramebufferShaders
{
#include "FramebufferPS.shh"
#include "FramebufferVS.shh"
}

#pragma comment( lib,"d3d11.lib" )

#define CHILI_GFX_EXCEPTION( hr,note ) Graphics::Exception( hr,note,_CRT_WIDE(__FILE__),__LINE__ )

using Microsoft::WRL::ComPtr;

Graphics::Graphics( HWNDKey& key )
{
	assert( key.hWnd != nullptr );

	//////////////////////////////////////////////////////
	// create device and swap chain/get render target view
	DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferCount = 1;
	sd.BufferDesc.Width = Graphics::ScreenWidth;
	sd.BufferDesc.Height = Graphics::ScreenHeight;
	sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 1;
	sd.BufferDesc.RefreshRate.Denominator = 60;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = key.hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	HRESULT				hr;
	UINT				createFlags = 0u;
#ifdef CHILI_USE_D3D_DEBUG_LAYER
#ifdef _DEBUG
	createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
#endif
	
	// create device and front/back buffers
	if( FAILED( hr = D3D11CreateDeviceAndSwapChain( 
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		createFlags,
		nullptr,
		0,
		D3D11_SDK_VERSION,
		&sd,
		&pSwapChain,
		&pDevice,
		nullptr,
		&pImmediateContext ) ) )
	{
		throw CHILI_GFX_EXCEPTION( hr,L"Creating device and swap chain" );
	}

	// get handle to backbuffer
	ComPtr<ID3D11Resource> pBackBuffer;
	if( FAILED( hr = pSwapChain->GetBuffer(
		0,
		__uuidof( ID3D11Texture2D ),
		(LPVOID*)&pBackBuffer ) ) )
	{
		throw CHILI_GFX_EXCEPTION( hr,L"Getting back buffer" );
	}

	// create a view on backbuffer that we can render to
	if( FAILED( hr = pDevice->CreateRenderTargetView( 
		pBackBuffer.Get(),
		nullptr,
		&pRenderTargetView ) ) )
	{
		throw CHILI_GFX_EXCEPTION( hr,L"Creating render target view on backbuffer" );
	}


	// set backbuffer as the render target using created view
	pImmediateContext->OMSetRenderTargets( 1,pRenderTargetView.GetAddressOf(),nullptr );


	// set viewport dimensions
	D3D11_VIEWPORT vp;
	vp.Width = float( Graphics::ScreenWidth );
	vp.Height = float( Graphics::ScreenHeight );
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0.0f;
	vp.TopLeftY = 0.0f;
	pImmediateContext->RSSetViewports( 1,&vp );


	///////////////////////////////////////
	// create texture for cpu render target
	D3D11_TEXTURE2D_DESC sysTexDesc;
	sysTexDesc.Width = Graphics::ScreenWidth;
	sysTexDesc.Height = Graphics::ScreenHeight;
	sysTexDesc.MipLevels = 1;
	sysTexDesc.ArraySize = 1;
	sysTexDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	sysTexDesc.SampleDesc.Count = 1;
	sysTexDesc.SampleDesc.Quality = 0;
	sysTexDesc.Usage = D3D11_USAGE_DYNAMIC;
	sysTexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	sysTexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	sysTexDesc.MiscFlags = 0;
	// create the texture
	if( FAILED( hr = pDevice->CreateTexture2D( &sysTexDesc,nullptr,&pSysBufferTexture ) ) )
	{
		throw CHILI_GFX_EXCEPTION( hr,L"Creating sysbuffer texture" );
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = sysTexDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	// create the resource view on the texture
	if( FAILED( hr = pDevice->CreateShaderResourceView( pSysBufferTexture.Get(),
		&srvDesc,&pSysBufferTextureView ) ) )
	{
		throw CHILI_GFX_EXCEPTION( hr,L"Creating view on sysBuffer texture" );
	}


	////////////////////////////////////////////////
	// create pixel shader for framebuffer
	// Ignore the intellisense error "namespace has no member"
	if( FAILED( hr = pDevice->CreatePixelShader(
		FramebufferShaders::FramebufferPSBytecode,
		sizeof( FramebufferShaders::FramebufferPSBytecode ),
		nullptr,
		&pPixelShader ) ) )
	{
		throw CHILI_GFX_EXCEPTION( hr,L"Creating pixel shader" );
	}
	

	/////////////////////////////////////////////////
	// create vertex shader for framebuffer
	// Ignore the intellisense error "namespace has no member"
	if( FAILED( hr = pDevice->CreateVertexShader(
		FramebufferShaders::FramebufferVSBytecode,
		sizeof( FramebufferShaders::FramebufferVSBytecode ),
		nullptr,
		&pVertexShader ) ) )
	{
		throw CHILI_GFX_EXCEPTION( hr,L"Creating vertex shader" );
	}
	

	//////////////////////////////////////////////////////////////
	// create and fill vertex buffer with quad for rendering frame
	const FSQVertex vertices[] =
	{
		{ -1.0f,1.0f,0.5f,0.0f,0.0f },
		{ 1.0f,1.0f,0.5f,1.0f,0.0f },
		{ 1.0f,-1.0f,0.5f,1.0f,1.0f },
		{ -1.0f,1.0f,0.5f,0.0f,0.0f },
		{ 1.0f,-1.0f,0.5f,1.0f,1.0f },
		{ -1.0f,-1.0f,0.5f,0.0f,1.0f },
	};
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof( FSQVertex ) * 6;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0u;
	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = vertices;
	if( FAILED( hr = pDevice->CreateBuffer( &bd,&initData,&pVertexBuffer ) ) )
	{
		throw CHILI_GFX_EXCEPTION( hr,L"Creating vertex buffer" );
	}

	
	//////////////////////////////////////////
	// create input layout for fullscreen quad
	const D3D11_INPUT_ELEMENT_DESC ied[] =
	{
		{ "POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0 },
		{ "TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0 }
	};

	// Ignore the intellisense error "namespace has no member"
	if( FAILED( hr = pDevice->CreateInputLayout( ied,2,
		FramebufferShaders::FramebufferVSBytecode,
		sizeof( FramebufferShaders::FramebufferVSBytecode ),
		&pInputLayout ) ) )
	{
		throw CHILI_GFX_EXCEPTION( hr,L"Creating input layout" );
	}


	////////////////////////////////////////////////////
	// Create sampler state for fullscreen textured quad
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	if( FAILED( hr = pDevice->CreateSamplerState( &sampDesc,&pSamplerState ) ) )
	{
		throw CHILI_GFX_EXCEPTION( hr,L"Creating sampler state" );
	}

	// allocate memory for sysbuffer (16-byte aligned for faster access)
	pSysBuffer = reinterpret_cast<Color*>( 
		_aligned_malloc( sizeof( Color ) * Graphics::ScreenWidth * Graphics::ScreenHeight,16u ) );
}

Graphics::~Graphics()
{
	// free sysbuffer memory (aligned free)
	if( pSysBuffer )
	{
		_aligned_free( pSysBuffer );
		pSysBuffer = nullptr;
	}
	// clear the state of the device context before destruction
	if( pImmediateContext ) pImmediateContext->ClearState();
}

void Graphics::EndFrame()
{
	HRESULT hr;

	// lock and map the adapter memory for copying over the sysbuffer
	if( FAILED( hr = pImmediateContext->Map( pSysBufferTexture.Get(),0u,
		D3D11_MAP_WRITE_DISCARD,0u,&mappedSysBufferTexture ) ) )
	{
		throw CHILI_GFX_EXCEPTION( hr,L"Mapping sysbuffer" );
	}
	// setup parameters for copy operation
	Color* pDst = reinterpret_cast<Color*>(mappedSysBufferTexture.pData );
	const size_t dstPitch = mappedSysBufferTexture.RowPitch / sizeof( Color );
	const size_t srcPitch = Graphics::ScreenWidth;
	const size_t rowBytes = srcPitch * sizeof( Color );
	// perform the copy line-by-line
	for( size_t y = 0u; y < Graphics::ScreenHeight; y++ )
	{
		memcpy( &pDst[ y * dstPitch ],&pSysBuffer[y * srcPitch],rowBytes );
	}
	// release the adapter memory
	pImmediateContext->Unmap( pSysBufferTexture.Get(),0u );

	// render offscreen scene texture to back buffer
	pImmediateContext->IASetInputLayout( pInputLayout.Get() );
	pImmediateContext->VSSetShader( pVertexShader.Get(),nullptr,0u );
	pImmediateContext->PSSetShader( pPixelShader.Get(),nullptr,0u );
	pImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
	const UINT stride = sizeof( FSQVertex );
	const UINT offset = 0u;
	pImmediateContext->IASetVertexBuffers( 0u,1u,pVertexBuffer.GetAddressOf(),&stride,&offset );
	pImmediateContext->PSSetShaderResources( 0u,1u,pSysBufferTextureView.GetAddressOf() );
	pImmediateContext->PSSetSamplers( 0u,1u,pSamplerState.GetAddressOf() );
	pImmediateContext->Draw( 6u,0u );

	// flip back/front buffers
	if( FAILED( hr = pSwapChain->Present( 1u,0u ) ) )
	{
		if( hr == DXGI_ERROR_DEVICE_REMOVED )
		{
			throw CHILI_GFX_EXCEPTION( pDevice->GetDeviceRemovedReason(),L"Presenting back buffer [device removed]" );
		}
		else
		{
			throw CHILI_GFX_EXCEPTION( hr,L"Presenting back buffer" );
		}
	}
}

void Graphics::BeginFrame()
{
	// clear the sysbuffer
	memset( pSysBuffer,0u,sizeof( Color ) * Graphics::ScreenHeight * Graphics::ScreenWidth );
}

void Graphics::PutPixel( int x,int y,Color c )
{
	assert( x >= 0 );
	assert( x < int( Graphics::ScreenWidth ) );
	assert( y >= 0 );
	assert( y < int( Graphics::ScreenHeight ) );
	pSysBuffer[Graphics::ScreenWidth * y + x] = c;
}

void Graphics::DrawRect(int x0, int y0, int x1, int y1, Color c)
{
	for (int x = x0; x < x1; ++x)
	{
		for (int y = y0; y < y1; ++y)
		{
			PutPixel(x, y, c);
		}
	}
}

void Graphics::DrawRectDim(int x0, int y0, int width, int height, Color c)
{
	DrawRect(x0, y0, x0 + width, y0 + height, c);
}

void Graphics::DrawRectPadded(int x0, int y0, int width, int height, Color c)
{
	DrawRectDim(x0 + 2, y0 + 2, width - 2, height - 2, c);
}

void Graphics::DrawHollowRect(int x0, int y0, int width, int height, Color c)
{
	for (int x = x0; x <= x0+width; ++x)
	{
		PutPixel(x, y0, c);
		PutPixel(x, y0 + height, c);
	}

	for (int y = y0 + 1; y <= y0 + height - 1; ++y)
	{
		PutPixel(x0, y, c);
		PutPixel(x0 + width, y, c);
	}
}

void Graphics::DrawGameOver(int x, int y)
{
	PutPixel(49 + x, 0 + y, 0, 146, 14);
	PutPixel(50 + x, 0 + y, 0, 146, 14);
	PutPixel(51 + x, 0 + y, 0, 146, 14);
	PutPixel(49 + x, 1 + y, 0, 146, 14);
	PutPixel(50 + x, 1 + y, 0, 146, 14);
	PutPixel(51 + x, 1 + y, 0, 146, 14);
	PutPixel(52 + x, 1 + y, 0, 146, 14);
	PutPixel(38 + x, 2 + y, 0, 146, 14);
	PutPixel(39 + x, 2 + y, 0, 146, 14);
	PutPixel(40 + x, 2 + y, 0, 146, 14);
	PutPixel(41 + x, 2 + y, 0, 146, 14);
	PutPixel(50 + x, 2 + y, 0, 146, 14);
	PutPixel(51 + x, 2 + y, 0, 146, 14);
	PutPixel(52 + x, 2 + y, 0, 146, 14);
	PutPixel(36 + x, 3 + y, 0, 146, 14);
	PutPixel(37 + x, 3 + y, 0, 146, 14);
	PutPixel(38 + x, 3 + y, 0, 146, 14);
	PutPixel(39 + x, 3 + y, 0, 146, 14);
	PutPixel(40 + x, 3 + y, 0, 146, 14);
	PutPixel(41 + x, 3 + y, 0, 146, 14);
	PutPixel(42 + x, 3 + y, 0, 146, 14);
	PutPixel(43 + x, 3 + y, 0, 146, 14);
	PutPixel(50 + x, 3 + y, 0, 146, 14);
	PutPixel(51 + x, 3 + y, 0, 146, 14);
	PutPixel(52 + x, 3 + y, 0, 146, 14);
	PutPixel(53 + x, 3 + y, 0, 146, 14);
	PutPixel(35 + x, 4 + y, 0, 146, 14);
	PutPixel(36 + x, 4 + y, 0, 146, 14);
	PutPixel(37 + x, 4 + y, 0, 146, 14);
	PutPixel(38 + x, 4 + y, 0, 146, 14);
	PutPixel(39 + x, 4 + y, 0, 146, 14);
	PutPixel(40 + x, 4 + y, 0, 146, 14);
	PutPixel(41 + x, 4 + y, 0, 146, 14);
	PutPixel(42 + x, 4 + y, 0, 146, 14);
	PutPixel(43 + x, 4 + y, 0, 146, 14);
	PutPixel(44 + x, 4 + y, 0, 146, 14);
	PutPixel(51 + x, 4 + y, 0, 146, 14);
	PutPixel(52 + x, 4 + y, 0, 146, 14);
	PutPixel(53 + x, 4 + y, 0, 146, 14);
	PutPixel(68 + x, 4 + y, 0, 146, 14);
	PutPixel(69 + x, 4 + y, 0, 146, 14);
	PutPixel(70 + x, 4 + y, 0, 146, 14);
	PutPixel(71 + x, 4 + y, 0, 146, 14);
	PutPixel(72 + x, 4 + y, 0, 146, 14);
	PutPixel(73 + x, 4 + y, 0, 146, 14);
	PutPixel(74 + x, 4 + y, 0, 146, 14);
	PutPixel(75 + x, 4 + y, 0, 146, 14);
	PutPixel(76 + x, 4 + y, 0, 146, 14);
	PutPixel(77 + x, 4 + y, 0, 146, 14);
	PutPixel(78 + x, 4 + y, 0, 146, 14);
	PutPixel(79 + x, 4 + y, 0, 146, 14);
	PutPixel(34 + x, 5 + y, 0, 118, 11);
	PutPixel(35 + x, 5 + y, 0, 146, 14);
	PutPixel(36 + x, 5 + y, 0, 146, 14);
	PutPixel(37 + x, 5 + y, 0, 146, 14);
	PutPixel(38 + x, 5 + y, 0, 146, 14);
	PutPixel(39 + x, 5 + y, 0, 146, 14);
	PutPixel(40 + x, 5 + y, 0, 146, 14);
	PutPixel(41 + x, 5 + y, 0, 146, 14);
	PutPixel(42 + x, 5 + y, 0, 146, 14);
	PutPixel(43 + x, 5 + y, 0, 146, 14);
	PutPixel(44 + x, 5 + y, 0, 146, 14);
	PutPixel(51 + x, 5 + y, 0, 142, 13);
	PutPixel(52 + x, 5 + y, 0, 146, 14);
	PutPixel(53 + x, 5 + y, 0, 146, 14);
	PutPixel(54 + x, 5 + y, 0, 146, 14);
	PutPixel(66 + x, 5 + y, 0, 146, 14);
	PutPixel(67 + x, 5 + y, 0, 146, 14);
	PutPixel(68 + x, 5 + y, 0, 146, 14);
	PutPixel(69 + x, 5 + y, 0, 146, 14);
	PutPixel(70 + x, 5 + y, 0, 146, 14);
	PutPixel(71 + x, 5 + y, 0, 146, 14);
	PutPixel(72 + x, 5 + y, 0, 146, 14);
	PutPixel(73 + x, 5 + y, 0, 146, 14);
	PutPixel(74 + x, 5 + y, 0, 146, 14);
	PutPixel(75 + x, 5 + y, 0, 146, 14);
	PutPixel(76 + x, 5 + y, 0, 146, 14);
	PutPixel(77 + x, 5 + y, 0, 146, 14);
	PutPixel(78 + x, 5 + y, 0, 146, 14);
	PutPixel(79 + x, 5 + y, 0, 146, 14);
	PutPixel(80 + x, 5 + y, 0, 146, 14);
	PutPixel(34 + x, 6 + y, 0, 146, 14);
	PutPixel(35 + x, 6 + y, 0, 146, 14);
	PutPixel(36 + x, 6 + y, 0, 146, 14);
	PutPixel(37 + x, 6 + y, 0, 146, 14);
	PutPixel(38 + x, 6 + y, 0, 146, 14);
	PutPixel(39 + x, 6 + y, 0, 146, 14);
	PutPixel(40 + x, 6 + y, 0, 146, 14);
	PutPixel(41 + x, 6 + y, 0, 146, 14);
	PutPixel(42 + x, 6 + y, 0, 146, 14);
	PutPixel(43 + x, 6 + y, 0, 146, 14);
	PutPixel(44 + x, 6 + y, 0, 146, 14);
	PutPixel(45 + x, 6 + y, 0, 146, 14);
	PutPixel(52 + x, 6 + y, 0, 146, 14);
	PutPixel(53 + x, 6 + y, 0, 146, 14);
	PutPixel(54 + x, 6 + y, 0, 146, 14);
	PutPixel(55 + x, 6 + y, 0, 146, 14);
	PutPixel(65 + x, 6 + y, 0, 146, 14);
	PutPixel(66 + x, 6 + y, 0, 146, 14);
	PutPixel(67 + x, 6 + y, 0, 146, 14);
	PutPixel(68 + x, 6 + y, 0, 146, 14);
	PutPixel(69 + x, 6 + y, 0, 146, 14);
	PutPixel(70 + x, 6 + y, 0, 146, 14);
	PutPixel(71 + x, 6 + y, 0, 146, 14);
	PutPixel(72 + x, 6 + y, 0, 146, 14);
	PutPixel(73 + x, 6 + y, 0, 146, 14);
	PutPixel(74 + x, 6 + y, 0, 146, 14);
	PutPixel(75 + x, 6 + y, 0, 146, 14);
	PutPixel(76 + x, 6 + y, 0, 146, 14);
	PutPixel(77 + x, 6 + y, 0, 146, 14);
	PutPixel(78 + x, 6 + y, 0, 146, 14);
	PutPixel(79 + x, 6 + y, 0, 146, 14);
	PutPixel(80 + x, 6 + y, 0, 146, 14);
	PutPixel(81 + x, 6 + y, 0, 146, 14);
	PutPixel(34 + x, 7 + y, 0, 146, 14);
	PutPixel(35 + x, 7 + y, 0, 146, 14);
	PutPixel(36 + x, 7 + y, 0, 146, 14);
	PutPixel(37 + x, 7 + y, 0, 146, 14);
	PutPixel(38 + x, 7 + y, 0, 146, 14);
	PutPixel(39 + x, 7 + y, 0, 146, 14);
	PutPixel(40 + x, 7 + y, 0, 146, 14);
	PutPixel(41 + x, 7 + y, 0, 146, 14);
	PutPixel(42 + x, 7 + y, 0, 146, 14);
	PutPixel(43 + x, 7 + y, 0, 146, 14);
	PutPixel(44 + x, 7 + y, 0, 146, 14);
	PutPixel(45 + x, 7 + y, 0, 146, 14);
	PutPixel(53 + x, 7 + y, 0, 146, 14);
	PutPixel(54 + x, 7 + y, 0, 146, 14);
	PutPixel(55 + x, 7 + y, 0, 146, 14);
	PutPixel(65 + x, 7 + y, 0, 146, 14);
	PutPixel(66 + x, 7 + y, 0, 146, 14);
	PutPixel(67 + x, 7 + y, 0, 146, 14);
	PutPixel(68 + x, 7 + y, 0, 142, 13);
	PutPixel(79 + x, 7 + y, 0, 146, 14);
	PutPixel(80 + x, 7 + y, 0, 146, 14);
	PutPixel(81 + x, 7 + y, 0, 146, 14);
	PutPixel(82 + x, 7 + y, 0, 146, 14);
	PutPixel(34 + x, 8 + y, 0, 146, 14);
	PutPixel(35 + x, 8 + y, 0, 146, 14);
	PutPixel(36 + x, 8 + y, 0, 146, 14);
	PutPixel(37 + x, 8 + y, 0, 146, 14);
	PutPixel(38 + x, 8 + y, 0, 146, 14);
	PutPixel(39 + x, 8 + y, 0, 146, 14);
	PutPixel(40 + x, 8 + y, 0, 146, 14);
	PutPixel(41 + x, 8 + y, 0, 146, 14);
	PutPixel(42 + x, 8 + y, 0, 146, 14);
	PutPixel(43 + x, 8 + y, 0, 146, 14);
	PutPixel(44 + x, 8 + y, 0, 146, 14);
	PutPixel(45 + x, 8 + y, 0, 146, 14);
	PutPixel(53 + x, 8 + y, 0, 146, 14);
	PutPixel(54 + x, 8 + y, 0, 146, 14);
	PutPixel(55 + x, 8 + y, 0, 146, 14);
	PutPixel(56 + x, 8 + y, 0, 146, 14);
	PutPixel(64 + x, 8 + y, 0, 146, 14);
	PutPixel(65 + x, 8 + y, 0, 146, 14);
	PutPixel(66 + x, 8 + y, 0, 146, 14);
	PutPixel(67 + x, 8 + y, 0, 146, 14);
	PutPixel(80 + x, 8 + y, 0, 146, 14);
	PutPixel(81 + x, 8 + y, 0, 146, 14);
	PutPixel(82 + x, 8 + y, 0, 146, 14);
	PutPixel(34 + x, 9 + y, 0, 146, 14);
	PutPixel(35 + x, 9 + y, 0, 146, 14);
	PutPixel(36 + x, 9 + y, 0, 146, 14);
	PutPixel(37 + x, 9 + y, 0, 146, 14);
	PutPixel(38 + x, 9 + y, 0, 146, 14);
	PutPixel(39 + x, 9 + y, 0, 146, 14);
	PutPixel(40 + x, 9 + y, 0, 146, 14);
	PutPixel(41 + x, 9 + y, 0, 146, 14);
	PutPixel(42 + x, 9 + y, 0, 146, 14);
	PutPixel(43 + x, 9 + y, 0, 146, 14);
	PutPixel(44 + x, 9 + y, 0, 146, 14);
	PutPixel(45 + x, 9 + y, 0, 146, 14);
	PutPixel(54 + x, 9 + y, 0, 146, 14);
	PutPixel(55 + x, 9 + y, 0, 146, 14);
	PutPixel(56 + x, 9 + y, 0, 146, 14);
	PutPixel(57 + x, 9 + y, 0, 146, 14);
	PutPixel(64 + x, 9 + y, 0, 146, 14);
	PutPixel(65 + x, 9 + y, 0, 146, 14);
	PutPixel(66 + x, 9 + y, 0, 146, 14);
	PutPixel(80 + x, 9 + y, 0, 146, 14);
	PutPixel(81 + x, 9 + y, 0, 146, 14);
	PutPixel(82 + x, 9 + y, 0, 146, 14);
	PutPixel(83 + x, 9 + y, 0, 118, 11);
	PutPixel(34 + x, 10 + y, 0, 146, 14);
	PutPixel(35 + x, 10 + y, 0, 146, 14);
	PutPixel(36 + x, 10 + y, 0, 146, 14);
	PutPixel(37 + x, 10 + y, 0, 146, 14);
	PutPixel(38 + x, 10 + y, 0, 146, 14);
	PutPixel(39 + x, 10 + y, 0, 146, 14);
	PutPixel(40 + x, 10 + y, 0, 146, 14);
	PutPixel(41 + x, 10 + y, 0, 146, 14);
	PutPixel(42 + x, 10 + y, 0, 146, 14);
	PutPixel(43 + x, 10 + y, 0, 146, 14);
	PutPixel(44 + x, 10 + y, 0, 146, 14);
	PutPixel(45 + x, 10 + y, 0, 146, 14);
	PutPixel(55 + x, 10 + y, 0, 146, 14);
	PutPixel(56 + x, 10 + y, 0, 146, 14);
	PutPixel(57 + x, 10 + y, 0, 146, 14);
	PutPixel(64 + x, 10 + y, 0, 146, 14);
	PutPixel(65 + x, 10 + y, 0, 146, 14);
	PutPixel(66 + x, 10 + y, 0, 146, 14);
	PutPixel(81 + x, 10 + y, 0, 146, 14);
	PutPixel(82 + x, 10 + y, 0, 146, 14);
	PutPixel(83 + x, 10 + y, 0, 146, 14);
	PutPixel(28 + x, 11 + y, 0, 146, 14);
	PutPixel(29 + x, 11 + y, 0, 146, 14);
	PutPixel(30 + x, 11 + y, 0, 146, 14);
	PutPixel(31 + x, 11 + y, 0, 146, 14);
	PutPixel(32 + x, 11 + y, 0, 146, 14);
	PutPixel(35 + x, 11 + y, 0, 146, 14);
	PutPixel(36 + x, 11 + y, 0, 146, 14);
	PutPixel(37 + x, 11 + y, 0, 146, 14);
	PutPixel(38 + x, 11 + y, 0, 146, 14);
	PutPixel(39 + x, 11 + y, 0, 146, 14);
	PutPixel(40 + x, 11 + y, 0, 146, 14);
	PutPixel(41 + x, 11 + y, 0, 146, 14);
	PutPixel(42 + x, 11 + y, 0, 146, 14);
	PutPixel(43 + x, 11 + y, 0, 146, 14);
	PutPixel(44 + x, 11 + y, 0, 146, 14);
	PutPixel(55 + x, 11 + y, 0, 146, 14);
	PutPixel(56 + x, 11 + y, 0, 146, 14);
	PutPixel(57 + x, 11 + y, 0, 146, 14);
	PutPixel(58 + x, 11 + y, 0, 146, 14);
	PutPixel(64 + x, 11 + y, 0, 146, 14);
	PutPixel(65 + x, 11 + y, 0, 146, 14);
	PutPixel(66 + x, 11 + y, 0, 146, 14);
	PutPixel(81 + x, 11 + y, 0, 146, 14);
	PutPixel(82 + x, 11 + y, 0, 146, 14);
	PutPixel(83 + x, 11 + y, 0, 146, 14);
	PutPixel(27 + x, 12 + y, 0, 146, 14);
	PutPixel(28 + x, 12 + y, 0, 146, 14);
	PutPixel(29 + x, 12 + y, 0, 146, 14);
	PutPixel(30 + x, 12 + y, 0, 146, 14);
	PutPixel(31 + x, 12 + y, 0, 146, 14);
	PutPixel(32 + x, 12 + y, 0, 146, 14);
	PutPixel(33 + x, 12 + y, 0, 146, 14);
	PutPixel(35 + x, 12 + y, 0, 142, 13);
	PutPixel(36 + x, 12 + y, 0, 146, 14);
	PutPixel(37 + x, 12 + y, 0, 146, 14);
	PutPixel(38 + x, 12 + y, 0, 146, 14);
	PutPixel(39 + x, 12 + y, 0, 146, 14);
	PutPixel(40 + x, 12 + y, 0, 146, 14);
	PutPixel(41 + x, 12 + y, 0, 146, 14);
	PutPixel(42 + x, 12 + y, 0, 146, 14);
	PutPixel(43 + x, 12 + y, 0, 146, 14);
	PutPixel(56 + x, 12 + y, 0, 146, 14);
	PutPixel(57 + x, 12 + y, 0, 146, 14);
	PutPixel(58 + x, 12 + y, 0, 146, 14);
	PutPixel(64 + x, 12 + y, 0, 146, 14);
	PutPixel(65 + x, 12 + y, 0, 146, 14);
	PutPixel(66 + x, 12 + y, 0, 146, 14);
	PutPixel(67 + x, 12 + y, 0, 146, 14);
	PutPixel(68 + x, 12 + y, 0, 146, 14);
	PutPixel(69 + x, 12 + y, 0, 146, 14);
	PutPixel(70 + x, 12 + y, 0, 146, 14);
	PutPixel(71 + x, 12 + y, 0, 146, 14);
	PutPixel(72 + x, 12 + y, 0, 146, 14);
	PutPixel(73 + x, 12 + y, 0, 146, 14);
	PutPixel(74 + x, 12 + y, 0, 146, 14);
	PutPixel(75 + x, 12 + y, 0, 146, 14);
	PutPixel(76 + x, 12 + y, 0, 146, 14);
	PutPixel(77 + x, 12 + y, 0, 146, 14);
	PutPixel(78 + x, 12 + y, 0, 146, 14);
	PutPixel(79 + x, 12 + y, 0, 146, 14);
	PutPixel(80 + x, 12 + y, 0, 146, 14);
	PutPixel(81 + x, 12 + y, 0, 146, 14);
	PutPixel(82 + x, 12 + y, 0, 146, 14);
	PutPixel(83 + x, 12 + y, 0, 146, 14);
	PutPixel(26 + x, 13 + y, 0, 146, 14);
	PutPixel(27 + x, 13 + y, 0, 146, 14);
	PutPixel(28 + x, 13 + y, 0, 146, 14);
	PutPixel(29 + x, 13 + y, 0, 146, 14);
	PutPixel(30 + x, 13 + y, 0, 146, 14);
	PutPixel(31 + x, 13 + y, 0, 146, 14);
	PutPixel(32 + x, 13 + y, 0, 146, 14);
	PutPixel(33 + x, 13 + y, 0, 146, 14);
	PutPixel(34 + x, 13 + y, 0, 146, 14);
	PutPixel(37 + x, 13 + y, 0, 146, 14);
	PutPixel(38 + x, 13 + y, 0, 146, 14);
	PutPixel(39 + x, 13 + y, 0, 146, 14);
	PutPixel(40 + x, 13 + y, 0, 146, 14);
	PutPixel(41 + x, 13 + y, 0, 146, 14);
	PutPixel(42 + x, 13 + y, 0, 146, 14);
	PutPixel(56 + x, 13 + y, 0, 142, 13);
	PutPixel(57 + x, 13 + y, 0, 146, 14);
	PutPixel(58 + x, 13 + y, 0, 146, 14);
	PutPixel(59 + x, 13 + y, 0, 146, 14);
	PutPixel(64 + x, 13 + y, 0, 146, 14);
	PutPixel(65 + x, 13 + y, 0, 146, 14);
	PutPixel(66 + x, 13 + y, 0, 146, 14);
	PutPixel(67 + x, 13 + y, 0, 146, 14);
	PutPixel(68 + x, 13 + y, 0, 146, 14);
	PutPixel(69 + x, 13 + y, 0, 146, 14);
	PutPixel(70 + x, 13 + y, 0, 146, 14);
	PutPixel(71 + x, 13 + y, 0, 146, 14);
	PutPixel(72 + x, 13 + y, 0, 146, 14);
	PutPixel(73 + x, 13 + y, 0, 146, 14);
	PutPixel(74 + x, 13 + y, 0, 146, 14);
	PutPixel(75 + x, 13 + y, 0, 146, 14);
	PutPixel(76 + x, 13 + y, 0, 146, 14);
	PutPixel(77 + x, 13 + y, 0, 146, 14);
	PutPixel(78 + x, 13 + y, 0, 146, 14);
	PutPixel(79 + x, 13 + y, 0, 146, 14);
	PutPixel(80 + x, 13 + y, 0, 146, 14);
	PutPixel(81 + x, 13 + y, 0, 146, 14);
	PutPixel(82 + x, 13 + y, 0, 146, 14);
	PutPixel(83 + x, 13 + y, 0, 146, 14);
	PutPixel(25 + x, 14 + y, 0, 146, 14);
	PutPixel(26 + x, 14 + y, 0, 146, 14);
	PutPixel(27 + x, 14 + y, 0, 146, 14);
	PutPixel(28 + x, 14 + y, 0, 146, 14);
	PutPixel(29 + x, 14 + y, 0, 146, 14);
	PutPixel(30 + x, 14 + y, 0, 146, 14);
	PutPixel(31 + x, 14 + y, 0, 146, 14);
	PutPixel(32 + x, 14 + y, 0, 146, 14);
	PutPixel(33 + x, 14 + y, 0, 146, 14);
	PutPixel(34 + x, 14 + y, 0, 146, 14);
	PutPixel(57 + x, 14 + y, 0, 146, 14);
	PutPixel(58 + x, 14 + y, 0, 146, 14);
	PutPixel(59 + x, 14 + y, 0, 146, 14);
	PutPixel(60 + x, 14 + y, 0, 146, 14);
	PutPixel(64 + x, 14 + y, 0, 146, 14);
	PutPixel(65 + x, 14 + y, 0, 146, 14);
	PutPixel(66 + x, 14 + y, 0, 146, 14);
	PutPixel(67 + x, 14 + y, 0, 146, 14);
	PutPixel(68 + x, 14 + y, 0, 146, 14);
	PutPixel(69 + x, 14 + y, 0, 146, 14);
	PutPixel(70 + x, 14 + y, 0, 146, 14);
	PutPixel(71 + x, 14 + y, 0, 146, 14);
	PutPixel(72 + x, 14 + y, 0, 146, 14);
	PutPixel(73 + x, 14 + y, 0, 146, 14);
	PutPixel(74 + x, 14 + y, 0, 146, 14);
	PutPixel(75 + x, 14 + y, 0, 146, 14);
	PutPixel(76 + x, 14 + y, 0, 146, 14);
	PutPixel(77 + x, 14 + y, 0, 146, 14);
	PutPixel(78 + x, 14 + y, 0, 146, 14);
	PutPixel(79 + x, 14 + y, 0, 146, 14);
	PutPixel(80 + x, 14 + y, 0, 146, 14);
	PutPixel(81 + x, 14 + y, 0, 146, 14);
	PutPixel(82 + x, 14 + y, 0, 146, 14);
	PutPixel(83 + x, 14 + y, 0, 146, 14);
	PutPixel(24 + x, 15 + y, 0, 25, 2);
	PutPixel(25 + x, 15 + y, 0, 146, 14);
	PutPixel(26 + x, 15 + y, 0, 146, 14);
	PutPixel(27 + x, 15 + y, 0, 146, 14);
	PutPixel(28 + x, 15 + y, 0, 146, 14);
	PutPixel(29 + x, 15 + y, 0, 146, 14);
	PutPixel(30 + x, 15 + y, 0, 146, 14);
	PutPixel(31 + x, 15 + y, 0, 146, 14);
	PutPixel(32 + x, 15 + y, 0, 146, 14);
	PutPixel(33 + x, 15 + y, 0, 146, 14);
	PutPixel(34 + x, 15 + y, 0, 146, 14);
	PutPixel(58 + x, 15 + y, 0, 146, 14);
	PutPixel(59 + x, 15 + y, 0, 146, 14);
	PutPixel(60 + x, 15 + y, 0, 146, 14);
	PutPixel(64 + x, 15 + y, 0, 146, 14);
	PutPixel(65 + x, 15 + y, 0, 146, 14);
	PutPixel(66 + x, 15 + y, 0, 146, 14);
	PutPixel(81 + x, 15 + y, 0, 146, 14);
	PutPixel(82 + x, 15 + y, 0, 146, 14);
	PutPixel(83 + x, 15 + y, 0, 146, 14);
	PutPixel(24 + x, 16 + y, 0, 146, 14);
	PutPixel(25 + x, 16 + y, 0, 146, 14);
	PutPixel(26 + x, 16 + y, 0, 146, 14);
	PutPixel(27 + x, 16 + y, 0, 146, 14);
	PutPixel(28 + x, 16 + y, 0, 146, 14);
	PutPixel(29 + x, 16 + y, 0, 146, 14);
	PutPixel(30 + x, 16 + y, 0, 146, 14);
	PutPixel(31 + x, 16 + y, 0, 146, 14);
	PutPixel(32 + x, 16 + y, 0, 146, 14);
	PutPixel(33 + x, 16 + y, 0, 146, 14);
	PutPixel(34 + x, 16 + y, 0, 146, 14);
	PutPixel(58 + x, 16 + y, 0, 146, 14);
	PutPixel(59 + x, 16 + y, 0, 146, 14);
	PutPixel(60 + x, 16 + y, 0, 146, 14);
	PutPixel(61 + x, 16 + y, 0, 146, 14);
	PutPixel(64 + x, 16 + y, 0, 146, 14);
	PutPixel(65 + x, 16 + y, 0, 146, 14);
	PutPixel(66 + x, 16 + y, 0, 146, 14);
	PutPixel(81 + x, 16 + y, 0, 146, 14);
	PutPixel(82 + x, 16 + y, 0, 146, 14);
	PutPixel(83 + x, 16 + y, 0, 146, 14);
	PutPixel(23 + x, 17 + y, 0, 146, 14);
	PutPixel(24 + x, 17 + y, 0, 146, 14);
	PutPixel(25 + x, 17 + y, 0, 146, 14);
	PutPixel(26 + x, 17 + y, 0, 146, 14);
	PutPixel(27 + x, 17 + y, 0, 146, 14);
	PutPixel(28 + x, 17 + y, 0, 146, 14);
	PutPixel(29 + x, 17 + y, 0, 146, 14);
	PutPixel(30 + x, 17 + y, 0, 146, 14);
	PutPixel(31 + x, 17 + y, 0, 146, 14);
	PutPixel(32 + x, 17 + y, 0, 146, 14);
	PutPixel(33 + x, 17 + y, 0, 146, 14);
	PutPixel(34 + x, 17 + y, 0, 146, 14);
	PutPixel(59 + x, 17 + y, 0, 146, 14);
	PutPixel(60 + x, 17 + y, 0, 146, 14);
	PutPixel(61 + x, 17 + y, 0, 146, 14);
	PutPixel(62 + x, 17 + y, 0, 146, 14);
	PutPixel(64 + x, 17 + y, 0, 146, 14);
	PutPixel(65 + x, 17 + y, 0, 146, 14);
	PutPixel(66 + x, 17 + y, 0, 146, 14);
	PutPixel(81 + x, 17 + y, 0, 146, 14);
	PutPixel(82 + x, 17 + y, 0, 146, 14);
	PutPixel(83 + x, 17 + y, 0, 146, 14);
	PutPixel(22 + x, 18 + y, 0, 146, 14);
	PutPixel(23 + x, 18 + y, 0, 146, 14);
	PutPixel(24 + x, 18 + y, 0, 146, 14);
	PutPixel(25 + x, 18 + y, 0, 146, 14);
	PutPixel(26 + x, 18 + y, 0, 146, 14);
	PutPixel(27 + x, 18 + y, 0, 146, 14);
	PutPixel(28 + x, 18 + y, 0, 146, 14);
	PutPixel(29 + x, 18 + y, 0, 146, 14);
	PutPixel(30 + x, 18 + y, 0, 146, 14);
	PutPixel(31 + x, 18 + y, 0, 146, 14);
	PutPixel(32 + x, 18 + y, 0, 146, 14);
	PutPixel(33 + x, 18 + y, 0, 146, 14);
	PutPixel(34 + x, 18 + y, 0, 146, 14);
	PutPixel(60 + x, 18 + y, 0, 146, 14);
	PutPixel(61 + x, 18 + y, 0, 146, 14);
	PutPixel(62 + x, 18 + y, 0, 146, 14);
	PutPixel(64 + x, 18 + y, 0, 146, 14);
	PutPixel(65 + x, 18 + y, 0, 146, 14);
	PutPixel(66 + x, 18 + y, 0, 146, 14);
	PutPixel(81 + x, 18 + y, 0, 146, 14);
	PutPixel(82 + x, 18 + y, 0, 146, 14);
	PutPixel(83 + x, 18 + y, 0, 146, 14);
	PutPixel(21 + x, 19 + y, 0, 146, 14);
	PutPixel(22 + x, 19 + y, 0, 146, 14);
	PutPixel(23 + x, 19 + y, 0, 146, 14);
	PutPixel(24 + x, 19 + y, 0, 146, 14);
	PutPixel(25 + x, 19 + y, 0, 146, 14);
	PutPixel(26 + x, 19 + y, 0, 146, 14);
	PutPixel(27 + x, 19 + y, 0, 146, 14);
	PutPixel(28 + x, 19 + y, 0, 146, 14);
	PutPixel(29 + x, 19 + y, 0, 146, 14);
	PutPixel(30 + x, 19 + y, 0, 146, 14);
	PutPixel(31 + x, 19 + y, 0, 146, 14);
	PutPixel(32 + x, 19 + y, 0, 146, 14);
	PutPixel(33 + x, 19 + y, 0, 146, 14);
	PutPixel(34 + x, 19 + y, 0, 146, 14);
	PutPixel(60 + x, 19 + y, 0, 146, 14);
	PutPixel(61 + x, 19 + y, 0, 146, 14);
	PutPixel(62 + x, 19 + y, 0, 146, 14);
	PutPixel(63 + x, 19 + y, 0, 146, 14);
	PutPixel(64 + x, 19 + y, 0, 146, 14);
	PutPixel(65 + x, 19 + y, 0, 146, 14);
	PutPixel(66 + x, 19 + y, 0, 146, 14);
	PutPixel(81 + x, 19 + y, 0, 146, 14);
	PutPixel(82 + x, 19 + y, 0, 146, 14);
	PutPixel(83 + x, 19 + y, 0, 146, 14);
	PutPixel(20 + x, 20 + y, 0, 146, 14);
	PutPixel(21 + x, 20 + y, 0, 146, 14);
	PutPixel(22 + x, 20 + y, 0, 146, 14);
	PutPixel(23 + x, 20 + y, 0, 146, 14);
	PutPixel(24 + x, 20 + y, 0, 146, 14);
	PutPixel(25 + x, 20 + y, 0, 146, 14);
	PutPixel(26 + x, 20 + y, 0, 146, 14);
	PutPixel(27 + x, 20 + y, 0, 146, 14);
	PutPixel(28 + x, 20 + y, 0, 146, 14);
	PutPixel(29 + x, 20 + y, 0, 146, 14);
	PutPixel(30 + x, 20 + y, 0, 146, 14);
	PutPixel(31 + x, 20 + y, 0, 146, 14);
	PutPixel(32 + x, 20 + y, 0, 146, 14);
	PutPixel(33 + x, 20 + y, 0, 146, 14);
	PutPixel(34 + x, 20 + y, 0, 146, 14);
	PutPixel(61 + x, 20 + y, 0, 146, 14);
	PutPixel(62 + x, 20 + y, 0, 146, 14);
	PutPixel(63 + x, 20 + y, 0, 146, 14);
	PutPixel(64 + x, 20 + y, 0, 146, 14);
	PutPixel(65 + x, 20 + y, 0, 146, 14);
	PutPixel(66 + x, 20 + y, 0, 146, 14);
	PutPixel(81 + x, 20 + y, 0, 146, 14);
	PutPixel(82 + x, 20 + y, 0, 146, 14);
	PutPixel(83 + x, 20 + y, 0, 146, 14);
	PutPixel(19 + x, 21 + y, 0, 146, 14);
	PutPixel(20 + x, 21 + y, 0, 146, 14);
	PutPixel(21 + x, 21 + y, 0, 146, 14);
	PutPixel(22 + x, 21 + y, 0, 146, 14);
	PutPixel(23 + x, 21 + y, 0, 146, 14);
	PutPixel(24 + x, 21 + y, 0, 146, 14);
	PutPixel(25 + x, 21 + y, 0, 146, 14);
	PutPixel(26 + x, 21 + y, 0, 146, 14);
	PutPixel(27 + x, 21 + y, 0, 146, 14);
	PutPixel(28 + x, 21 + y, 0, 146, 14);
	PutPixel(29 + x, 21 + y, 0, 146, 14);
	PutPixel(30 + x, 21 + y, 0, 146, 14);
	PutPixel(31 + x, 21 + y, 0, 146, 14);
	PutPixel(32 + x, 21 + y, 0, 146, 14);
	PutPixel(33 + x, 21 + y, 0, 146, 14);
	PutPixel(34 + x, 21 + y, 0, 146, 14);
	PutPixel(61 + x, 21 + y, 0, 146, 14);
	PutPixel(62 + x, 21 + y, 0, 146, 14);
	PutPixel(63 + x, 21 + y, 0, 146, 14);
	PutPixel(64 + x, 21 + y, 0, 146, 14);
	PutPixel(65 + x, 21 + y, 0, 146, 14);
	PutPixel(66 + x, 21 + y, 0, 146, 14);
	PutPixel(81 + x, 21 + y, 0, 146, 14);
	PutPixel(82 + x, 21 + y, 0, 146, 14);
	PutPixel(83 + x, 21 + y, 0, 146, 14);
	PutPixel(18 + x, 22 + y, 0, 146, 14);
	PutPixel(19 + x, 22 + y, 0, 146, 14);
	PutPixel(20 + x, 22 + y, 0, 146, 14);
	PutPixel(21 + x, 22 + y, 0, 146, 14);
	PutPixel(22 + x, 22 + y, 0, 146, 14);
	PutPixel(23 + x, 22 + y, 0, 146, 14);
	PutPixel(24 + x, 22 + y, 0, 146, 14);
	PutPixel(25 + x, 22 + y, 0, 146, 14);
	PutPixel(26 + x, 22 + y, 0, 146, 14);
	PutPixel(27 + x, 22 + y, 0, 146, 14);
	PutPixel(28 + x, 22 + y, 0, 146, 14);
	PutPixel(29 + x, 22 + y, 0, 146, 14);
	PutPixel(30 + x, 22 + y, 0, 146, 14);
	PutPixel(31 + x, 22 + y, 0, 146, 14);
	PutPixel(32 + x, 22 + y, 0, 146, 14);
	PutPixel(33 + x, 22 + y, 0, 146, 14);
	PutPixel(34 + x, 22 + y, 0, 146, 14);
	PutPixel(35 + x, 22 + y, 0, 25, 2);
	PutPixel(62 + x, 22 + y, 0, 146, 14);
	PutPixel(63 + x, 22 + y, 0, 146, 14);
	PutPixel(64 + x, 22 + y, 0, 146, 14);
	PutPixel(65 + x, 22 + y, 0, 146, 14);
	PutPixel(66 + x, 22 + y, 0, 146, 14);
	PutPixel(81 + x, 22 + y, 0, 146, 14);
	PutPixel(82 + x, 22 + y, 0, 146, 14);
	PutPixel(83 + x, 22 + y, 0, 146, 14);
	PutPixel(17 + x, 23 + y, 0, 3, 0);
	PutPixel(18 + x, 23 + y, 0, 146, 14);
	PutPixel(19 + x, 23 + y, 0, 146, 14);
	PutPixel(20 + x, 23 + y, 0, 146, 14);
	PutPixel(21 + x, 23 + y, 0, 146, 14);
	PutPixel(22 + x, 23 + y, 0, 146, 14);
	PutPixel(23 + x, 23 + y, 0, 146, 14);
	PutPixel(24 + x, 23 + y, 0, 146, 14);
	PutPixel(25 + x, 23 + y, 0, 146, 14);
	PutPixel(26 + x, 23 + y, 0, 146, 14);
	PutPixel(27 + x, 23 + y, 0, 146, 14);
	PutPixel(28 + x, 23 + y, 0, 146, 14);
	PutPixel(30 + x, 23 + y, 0, 146, 14);
	PutPixel(31 + x, 23 + y, 0, 146, 14);
	PutPixel(32 + x, 23 + y, 0, 146, 14);
	PutPixel(33 + x, 23 + y, 0, 146, 14);
	PutPixel(34 + x, 23 + y, 0, 146, 14);
	PutPixel(35 + x, 23 + y, 0, 25, 2);
	PutPixel(63 + x, 23 + y, 0, 146, 14);
	PutPixel(64 + x, 23 + y, 0, 146, 14);
	PutPixel(65 + x, 23 + y, 0, 146, 14);
	PutPixel(66 + x, 23 + y, 0, 146, 14);
	PutPixel(81 + x, 23 + y, 0, 146, 14);
	PutPixel(82 + x, 23 + y, 0, 146, 14);
	PutPixel(83 + x, 23 + y, 0, 146, 14);
	PutPixel(17 + x, 24 + y, 0, 146, 14);
	PutPixel(18 + x, 24 + y, 0, 146, 14);
	PutPixel(19 + x, 24 + y, 0, 146, 14);
	PutPixel(20 + x, 24 + y, 0, 146, 14);
	PutPixel(21 + x, 24 + y, 0, 146, 14);
	PutPixel(22 + x, 24 + y, 0, 146, 14);
	PutPixel(23 + x, 24 + y, 0, 146, 14);
	PutPixel(24 + x, 24 + y, 0, 146, 14);
	PutPixel(25 + x, 24 + y, 0, 146, 14);
	PutPixel(26 + x, 24 + y, 0, 146, 14);
	PutPixel(27 + x, 24 + y, 0, 146, 14);
	PutPixel(30 + x, 24 + y, 0, 146, 14);
	PutPixel(31 + x, 24 + y, 0, 146, 14);
	PutPixel(32 + x, 24 + y, 0, 146, 14);
	PutPixel(33 + x, 24 + y, 0, 146, 14);
	PutPixel(34 + x, 24 + y, 0, 146, 14);
	PutPixel(35 + x, 24 + y, 0, 146, 14);
	PutPixel(36 + x, 24 + y, 0, 146, 14);
	PutPixel(37 + x, 24 + y, 0, 146, 14);
	PutPixel(38 + x, 24 + y, 0, 146, 14);
	PutPixel(39 + x, 24 + y, 0, 146, 14);
	PutPixel(40 + x, 24 + y, 0, 146, 14);
	PutPixel(41 + x, 24 + y, 0, 146, 14);
	PutPixel(42 + x, 24 + y, 0, 146, 14);
	PutPixel(43 + x, 24 + y, 0, 146, 14);
	PutPixel(44 + x, 24 + y, 0, 146, 14);
	PutPixel(45 + x, 24 + y, 0, 146, 14);
	PutPixel(46 + x, 24 + y, 0, 146, 14);
	PutPixel(47 + x, 24 + y, 0, 146, 14);
	PutPixel(48 + x, 24 + y, 0, 146, 14);
	PutPixel(49 + x, 24 + y, 0, 146, 14);
	PutPixel(50 + x, 24 + y, 0, 146, 14);
	PutPixel(51 + x, 24 + y, 0, 146, 14);
	PutPixel(52 + x, 24 + y, 0, 146, 14);
	PutPixel(53 + x, 24 + y, 0, 146, 14);
	PutPixel(54 + x, 24 + y, 0, 146, 14);
	PutPixel(55 + x, 24 + y, 0, 146, 14);
	PutPixel(56 + x, 24 + y, 0, 146, 14);
	PutPixel(57 + x, 24 + y, 0, 146, 14);
	PutPixel(58 + x, 24 + y, 0, 146, 14);
	PutPixel(59 + x, 24 + y, 0, 146, 14);
	PutPixel(60 + x, 24 + y, 0, 146, 14);
	PutPixel(61 + x, 24 + y, 0, 146, 14);
	PutPixel(62 + x, 24 + y, 0, 146, 14);
	PutPixel(63 + x, 24 + y, 0, 146, 14);
	PutPixel(64 + x, 24 + y, 0, 146, 14);
	PutPixel(65 + x, 24 + y, 0, 146, 14);
	PutPixel(66 + x, 24 + y, 0, 146, 14);
	PutPixel(81 + x, 24 + y, 0, 146, 14);
	PutPixel(82 + x, 24 + y, 0, 146, 14);
	PutPixel(83 + x, 24 + y, 0, 146, 14);
	PutPixel(16 + x, 25 + y, 0, 146, 14);
	PutPixel(17 + x, 25 + y, 0, 146, 14);
	PutPixel(18 + x, 25 + y, 0, 146, 14);
	PutPixel(19 + x, 25 + y, 0, 146, 14);
	PutPixel(20 + x, 25 + y, 0, 146, 14);
	PutPixel(21 + x, 25 + y, 0, 146, 14);
	PutPixel(22 + x, 25 + y, 0, 146, 14);
	PutPixel(23 + x, 25 + y, 0, 146, 14);
	PutPixel(24 + x, 25 + y, 0, 146, 14);
	PutPixel(25 + x, 25 + y, 0, 146, 14);
	PutPixel(26 + x, 25 + y, 0, 146, 14);
	PutPixel(30 + x, 25 + y, 0, 146, 14);
	PutPixel(31 + x, 25 + y, 0, 146, 14);
	PutPixel(32 + x, 25 + y, 0, 146, 14);
	PutPixel(33 + x, 25 + y, 0, 146, 14);
	PutPixel(34 + x, 25 + y, 0, 146, 14);
	PutPixel(35 + x, 25 + y, 0, 146, 14);
	PutPixel(36 + x, 25 + y, 0, 146, 14);
	PutPixel(37 + x, 25 + y, 0, 146, 14);
	PutPixel(38 + x, 25 + y, 0, 146, 14);
	PutPixel(39 + x, 25 + y, 0, 146, 14);
	PutPixel(40 + x, 25 + y, 0, 146, 14);
	PutPixel(41 + x, 25 + y, 0, 146, 14);
	PutPixel(42 + x, 25 + y, 0, 146, 14);
	PutPixel(43 + x, 25 + y, 0, 146, 14);
	PutPixel(44 + x, 25 + y, 0, 146, 14);
	PutPixel(45 + x, 25 + y, 0, 146, 14);
	PutPixel(46 + x, 25 + y, 0, 146, 14);
	PutPixel(47 + x, 25 + y, 0, 146, 14);
	PutPixel(48 + x, 25 + y, 0, 146, 14);
	PutPixel(49 + x, 25 + y, 0, 146, 14);
	PutPixel(50 + x, 25 + y, 0, 146, 14);
	PutPixel(51 + x, 25 + y, 0, 146, 14);
	PutPixel(52 + x, 25 + y, 0, 146, 14);
	PutPixel(53 + x, 25 + y, 0, 146, 14);
	PutPixel(54 + x, 25 + y, 0, 146, 14);
	PutPixel(55 + x, 25 + y, 0, 146, 14);
	PutPixel(56 + x, 25 + y, 0, 146, 14);
	PutPixel(57 + x, 25 + y, 0, 146, 14);
	PutPixel(58 + x, 25 + y, 0, 146, 14);
	PutPixel(59 + x, 25 + y, 0, 146, 14);
	PutPixel(60 + x, 25 + y, 0, 146, 14);
	PutPixel(61 + x, 25 + y, 0, 146, 14);
	PutPixel(62 + x, 25 + y, 0, 146, 14);
	PutPixel(63 + x, 25 + y, 0, 146, 14);
	PutPixel(64 + x, 25 + y, 0, 146, 14);
	PutPixel(65 + x, 25 + y, 0, 146, 14);
	PutPixel(66 + x, 25 + y, 0, 146, 14);
	PutPixel(81 + x, 25 + y, 0, 146, 14);
	PutPixel(82 + x, 25 + y, 0, 146, 14);
	PutPixel(83 + x, 25 + y, 0, 146, 14);
	PutPixel(15 + x, 26 + y, 0, 146, 14);
	PutPixel(16 + x, 26 + y, 0, 146, 14);
	PutPixel(17 + x, 26 + y, 0, 146, 14);
	PutPixel(18 + x, 26 + y, 0, 146, 14);
	PutPixel(19 + x, 26 + y, 0, 146, 14);
	PutPixel(20 + x, 26 + y, 0, 146, 14);
	PutPixel(21 + x, 26 + y, 0, 146, 14);
	PutPixel(22 + x, 26 + y, 0, 146, 14);
	PutPixel(23 + x, 26 + y, 0, 146, 14);
	PutPixel(24 + x, 26 + y, 0, 146, 14);
	PutPixel(25 + x, 26 + y, 0, 146, 14);
	PutPixel(30 + x, 26 + y, 0, 146, 14);
	PutPixel(31 + x, 26 + y, 0, 146, 14);
	PutPixel(32 + x, 26 + y, 0, 146, 14);
	PutPixel(33 + x, 26 + y, 0, 146, 14);
	PutPixel(34 + x, 26 + y, 0, 146, 14);
	PutPixel(35 + x, 26 + y, 0, 146, 14);
	PutPixel(36 + x, 26 + y, 0, 146, 14);
	PutPixel(37 + x, 26 + y, 0, 146, 14);
	PutPixel(38 + x, 26 + y, 0, 146, 14);
	PutPixel(39 + x, 26 + y, 0, 146, 14);
	PutPixel(40 + x, 26 + y, 0, 146, 14);
	PutPixel(41 + x, 26 + y, 0, 146, 14);
	PutPixel(42 + x, 26 + y, 0, 146, 14);
	PutPixel(43 + x, 26 + y, 0, 146, 14);
	PutPixel(44 + x, 26 + y, 0, 146, 14);
	PutPixel(45 + x, 26 + y, 0, 146, 14);
	PutPixel(46 + x, 26 + y, 0, 146, 14);
	PutPixel(47 + x, 26 + y, 0, 146, 14);
	PutPixel(48 + x, 26 + y, 0, 146, 14);
	PutPixel(49 + x, 26 + y, 0, 146, 14);
	PutPixel(50 + x, 26 + y, 0, 146, 14);
	PutPixel(51 + x, 26 + y, 0, 146, 14);
	PutPixel(52 + x, 26 + y, 0, 146, 14);
	PutPixel(53 + x, 26 + y, 0, 146, 14);
	PutPixel(54 + x, 26 + y, 0, 146, 14);
	PutPixel(55 + x, 26 + y, 0, 146, 14);
	PutPixel(56 + x, 26 + y, 0, 146, 14);
	PutPixel(57 + x, 26 + y, 0, 146, 14);
	PutPixel(58 + x, 26 + y, 0, 146, 14);
	PutPixel(59 + x, 26 + y, 0, 146, 14);
	PutPixel(60 + x, 26 + y, 0, 146, 14);
	PutPixel(61 + x, 26 + y, 0, 146, 14);
	PutPixel(62 + x, 26 + y, 0, 146, 14);
	PutPixel(63 + x, 26 + y, 0, 146, 14);
	PutPixel(64 + x, 26 + y, 0, 146, 14);
	PutPixel(65 + x, 26 + y, 0, 146, 14);
	PutPixel(66 + x, 26 + y, 0, 146, 14);
	PutPixel(81 + x, 26 + y, 0, 146, 14);
	PutPixel(82 + x, 26 + y, 0, 146, 14);
	PutPixel(83 + x, 26 + y, 0, 146, 14);
	PutPixel(14 + x, 27 + y, 0, 146, 14);
	PutPixel(15 + x, 27 + y, 0, 146, 14);
	PutPixel(16 + x, 27 + y, 0, 146, 14);
	PutPixel(17 + x, 27 + y, 0, 146, 14);
	PutPixel(18 + x, 27 + y, 0, 146, 14);
	PutPixel(19 + x, 27 + y, 0, 146, 14);
	PutPixel(20 + x, 27 + y, 0, 146, 14);
	PutPixel(21 + x, 27 + y, 0, 146, 14);
	PutPixel(22 + x, 27 + y, 0, 146, 14);
	PutPixel(23 + x, 27 + y, 0, 146, 14);
	PutPixel(24 + x, 27 + y, 0, 146, 14);
	PutPixel(30 + x, 27 + y, 0, 146, 14);
	PutPixel(31 + x, 27 + y, 0, 146, 14);
	PutPixel(32 + x, 27 + y, 0, 146, 14);
	PutPixel(33 + x, 27 + y, 0, 146, 14);
	PutPixel(34 + x, 27 + y, 0, 146, 14);
	PutPixel(35 + x, 27 + y, 0, 146, 14);
	PutPixel(36 + x, 27 + y, 0, 146, 14);
	PutPixel(37 + x, 27 + y, 0, 146, 14);
	PutPixel(38 + x, 27 + y, 0, 146, 14);
	PutPixel(39 + x, 27 + y, 0, 146, 14);
	PutPixel(40 + x, 27 + y, 0, 146, 14);
	PutPixel(41 + x, 27 + y, 0, 146, 14);
	PutPixel(42 + x, 27 + y, 0, 146, 14);
	PutPixel(43 + x, 27 + y, 0, 146, 14);
	PutPixel(44 + x, 27 + y, 0, 146, 14);
	PutPixel(45 + x, 27 + y, 0, 146, 14);
	PutPixel(46 + x, 27 + y, 0, 146, 14);
	PutPixel(64 + x, 27 + y, 0, 146, 14);
	PutPixel(65 + x, 27 + y, 0, 146, 14);
	PutPixel(66 + x, 27 + y, 0, 146, 14);
	PutPixel(81 + x, 27 + y, 0, 146, 14);
	PutPixel(82 + x, 27 + y, 0, 146, 14);
	PutPixel(83 + x, 27 + y, 0, 146, 14);
	PutPixel(13 + x, 28 + y, 0, 146, 14);
	PutPixel(14 + x, 28 + y, 0, 146, 14);
	PutPixel(15 + x, 28 + y, 0, 146, 14);
	PutPixel(16 + x, 28 + y, 0, 146, 14);
	PutPixel(17 + x, 28 + y, 0, 146, 14);
	PutPixel(18 + x, 28 + y, 0, 146, 14);
	PutPixel(19 + x, 28 + y, 0, 146, 14);
	PutPixel(20 + x, 28 + y, 0, 146, 14);
	PutPixel(21 + x, 28 + y, 0, 146, 14);
	PutPixel(22 + x, 28 + y, 0, 146, 14);
	PutPixel(23 + x, 28 + y, 0, 146, 14);
	PutPixel(30 + x, 28 + y, 0, 146, 14);
	PutPixel(31 + x, 28 + y, 0, 146, 14);
	PutPixel(32 + x, 28 + y, 0, 146, 14);
	PutPixel(33 + x, 28 + y, 0, 146, 14);
	PutPixel(34 + x, 28 + y, 0, 146, 14);
	PutPixel(35 + x, 28 + y, 0, 146, 14);
	PutPixel(36 + x, 28 + y, 0, 146, 14);
	PutPixel(37 + x, 28 + y, 0, 146, 14);
	PutPixel(38 + x, 28 + y, 0, 146, 14);
	PutPixel(39 + x, 28 + y, 0, 146, 14);
	PutPixel(40 + x, 28 + y, 0, 146, 14);
	PutPixel(41 + x, 28 + y, 0, 146, 14);
	PutPixel(42 + x, 28 + y, 0, 146, 14);
	PutPixel(43 + x, 28 + y, 0, 146, 14);
	PutPixel(44 + x, 28 + y, 0, 146, 14);
	PutPixel(45 + x, 28 + y, 0, 146, 14);
	PutPixel(46 + x, 28 + y, 0, 146, 14);
	PutPixel(47 + x, 28 + y, 0, 146, 14);
	PutPixel(64 + x, 28 + y, 0, 146, 14);
	PutPixel(65 + x, 28 + y, 0, 146, 14);
	PutPixel(66 + x, 28 + y, 0, 146, 14);
	PutPixel(81 + x, 28 + y, 0, 146, 14);
	PutPixel(82 + x, 28 + y, 0, 146, 14);
	PutPixel(83 + x, 28 + y, 0, 146, 14);
	PutPixel(12 + x, 29 + y, 0, 146, 14);
	PutPixel(13 + x, 29 + y, 0, 146, 14);
	PutPixel(14 + x, 29 + y, 0, 146, 14);
	PutPixel(15 + x, 29 + y, 0, 146, 14);
	PutPixel(16 + x, 29 + y, 0, 146, 14);
	PutPixel(17 + x, 29 + y, 0, 146, 14);
	PutPixel(18 + x, 29 + y, 0, 146, 14);
	PutPixel(19 + x, 29 + y, 0, 146, 14);
	PutPixel(20 + x, 29 + y, 0, 146, 14);
	PutPixel(21 + x, 29 + y, 0, 146, 14);
	PutPixel(22 + x, 29 + y, 0, 146, 14);
	PutPixel(23 + x, 29 + y, 0, 146, 14);
	PutPixel(30 + x, 29 + y, 0, 146, 14);
	PutPixel(31 + x, 29 + y, 0, 146, 14);
	PutPixel(32 + x, 29 + y, 0, 146, 14);
	PutPixel(33 + x, 29 + y, 0, 146, 14);
	PutPixel(34 + x, 29 + y, 0, 146, 14);
	PutPixel(35 + x, 29 + y, 0, 146, 14);
	PutPixel(36 + x, 29 + y, 0, 146, 14);
	PutPixel(37 + x, 29 + y, 0, 146, 14);
	PutPixel(38 + x, 29 + y, 0, 146, 14);
	PutPixel(39 + x, 29 + y, 0, 146, 14);
	PutPixel(40 + x, 29 + y, 0, 146, 14);
	PutPixel(41 + x, 29 + y, 0, 146, 14);
	PutPixel(42 + x, 29 + y, 0, 146, 14);
	PutPixel(43 + x, 29 + y, 0, 146, 14);
	PutPixel(44 + x, 29 + y, 0, 146, 14);
	PutPixel(45 + x, 29 + y, 0, 146, 14);
	PutPixel(46 + x, 29 + y, 0, 146, 14);
	PutPixel(47 + x, 29 + y, 0, 146, 14);
	PutPixel(63 + x, 29 + y, 0, 25, 2);
	PutPixel(64 + x, 29 + y, 0, 146, 14);
	PutPixel(65 + x, 29 + y, 0, 146, 14);
	PutPixel(66 + x, 29 + y, 0, 146, 14);
	PutPixel(81 + x, 29 + y, 0, 146, 14);
	PutPixel(82 + x, 29 + y, 0, 146, 14);
	PutPixel(83 + x, 29 + y, 0, 146, 14);
	PutPixel(11 + x, 30 + y, 0, 146, 14);
	PutPixel(12 + x, 30 + y, 0, 146, 14);
	PutPixel(13 + x, 30 + y, 0, 146, 14);
	PutPixel(14 + x, 30 + y, 0, 146, 14);
	PutPixel(15 + x, 30 + y, 0, 146, 14);
	PutPixel(16 + x, 30 + y, 0, 146, 14);
	PutPixel(17 + x, 30 + y, 0, 146, 14);
	PutPixel(18 + x, 30 + y, 0, 146, 14);
	PutPixel(19 + x, 30 + y, 0, 146, 14);
	PutPixel(20 + x, 30 + y, 0, 146, 14);
	PutPixel(21 + x, 30 + y, 0, 146, 14);
	PutPixel(22 + x, 30 + y, 0, 146, 14);
	PutPixel(30 + x, 30 + y, 0, 146, 14);
	PutPixel(31 + x, 30 + y, 0, 146, 14);
	PutPixel(32 + x, 30 + y, 0, 146, 14);
	PutPixel(33 + x, 30 + y, 0, 146, 14);
	PutPixel(34 + x, 30 + y, 0, 146, 14);
	PutPixel(35 + x, 30 + y, 0, 146, 14);
	PutPixel(36 + x, 30 + y, 0, 146, 14);
	PutPixel(37 + x, 30 + y, 0, 146, 14);
	PutPixel(38 + x, 30 + y, 0, 146, 14);
	PutPixel(39 + x, 30 + y, 0, 146, 14);
	PutPixel(40 + x, 30 + y, 0, 146, 14);
	PutPixel(41 + x, 30 + y, 0, 146, 14);
	PutPixel(42 + x, 30 + y, 0, 146, 14);
	PutPixel(43 + x, 30 + y, 0, 146, 14);
	PutPixel(44 + x, 30 + y, 0, 146, 14);
	PutPixel(45 + x, 30 + y, 0, 146, 14);
	PutPixel(46 + x, 30 + y, 0, 146, 14);
	PutPixel(47 + x, 30 + y, 0, 146, 14);
	PutPixel(63 + x, 30 + y, 0, 146, 14);
	PutPixel(64 + x, 30 + y, 0, 146, 14);
	PutPixel(65 + x, 30 + y, 0, 146, 14);
	PutPixel(66 + x, 30 + y, 0, 146, 14);
	PutPixel(80 + x, 30 + y, 0, 25, 2);
	PutPixel(81 + x, 30 + y, 0, 146, 14);
	PutPixel(82 + x, 30 + y, 0, 146, 14);
	PutPixel(83 + x, 30 + y, 0, 146, 14);
	PutPixel(10 + x, 31 + y, 0, 70, 6);
	PutPixel(11 + x, 31 + y, 0, 146, 14);
	PutPixel(12 + x, 31 + y, 0, 146, 14);
	PutPixel(13 + x, 31 + y, 0, 146, 14);
	PutPixel(14 + x, 31 + y, 0, 146, 14);
	PutPixel(15 + x, 31 + y, 0, 146, 14);
	PutPixel(16 + x, 31 + y, 0, 146, 14);
	PutPixel(17 + x, 31 + y, 0, 146, 14);
	PutPixel(18 + x, 31 + y, 0, 146, 14);
	PutPixel(19 + x, 31 + y, 0, 146, 14);
	PutPixel(20 + x, 31 + y, 0, 146, 14);
	PutPixel(21 + x, 31 + y, 0, 146, 14);
	PutPixel(30 + x, 31 + y, 0, 146, 14);
	PutPixel(31 + x, 31 + y, 0, 146, 14);
	PutPixel(32 + x, 31 + y, 0, 146, 14);
	PutPixel(33 + x, 31 + y, 0, 146, 14);
	PutPixel(34 + x, 31 + y, 0, 146, 14);
	PutPixel(35 + x, 31 + y, 0, 146, 14);
	PutPixel(36 + x, 31 + y, 0, 146, 14);
	PutPixel(37 + x, 31 + y, 0, 146, 14);
	PutPixel(38 + x, 31 + y, 0, 146, 14);
	PutPixel(39 + x, 31 + y, 0, 146, 14);
	PutPixel(40 + x, 31 + y, 0, 146, 14);
	PutPixel(41 + x, 31 + y, 0, 146, 14);
	PutPixel(42 + x, 31 + y, 0, 146, 14);
	PutPixel(43 + x, 31 + y, 0, 146, 14);
	PutPixel(44 + x, 31 + y, 0, 146, 14);
	PutPixel(45 + x, 31 + y, 0, 146, 14);
	PutPixel(46 + x, 31 + y, 0, 146, 14);
	PutPixel(47 + x, 31 + y, 0, 146, 14);
	PutPixel(63 + x, 31 + y, 0, 146, 14);
	PutPixel(64 + x, 31 + y, 0, 146, 14);
	PutPixel(65 + x, 31 + y, 0, 146, 14);
	PutPixel(66 + x, 31 + y, 0, 146, 14);
	PutPixel(67 + x, 31 + y, 0, 146, 14);
	PutPixel(80 + x, 31 + y, 0, 146, 14);
	PutPixel(81 + x, 31 + y, 0, 146, 14);
	PutPixel(82 + x, 31 + y, 0, 146, 14);
	PutPixel(10 + x, 32 + y, 0, 146, 14);
	PutPixel(11 + x, 32 + y, 0, 146, 14);
	PutPixel(12 + x, 32 + y, 0, 146, 14);
	PutPixel(13 + x, 32 + y, 0, 146, 14);
	PutPixel(14 + x, 32 + y, 0, 146, 14);
	PutPixel(15 + x, 32 + y, 0, 146, 14);
	PutPixel(16 + x, 32 + y, 0, 146, 14);
	PutPixel(17 + x, 32 + y, 0, 146, 14);
	PutPixel(18 + x, 32 + y, 0, 146, 14);
	PutPixel(19 + x, 32 + y, 0, 146, 14);
	PutPixel(20 + x, 32 + y, 0, 146, 14);
	PutPixel(32 + x, 32 + y, 0, 146, 14);
	PutPixel(33 + x, 32 + y, 0, 146, 14);
	PutPixel(34 + x, 32 + y, 0, 146, 14);
	PutPixel(35 + x, 32 + y, 0, 146, 14);
	PutPixel(36 + x, 32 + y, 0, 146, 14);
	PutPixel(37 + x, 32 + y, 0, 146, 14);
	PutPixel(38 + x, 32 + y, 0, 146, 14);
	PutPixel(39 + x, 32 + y, 0, 146, 14);
	PutPixel(40 + x, 32 + y, 0, 146, 14);
	PutPixel(41 + x, 32 + y, 0, 146, 14);
	PutPixel(42 + x, 32 + y, 0, 146, 14);
	PutPixel(43 + x, 32 + y, 0, 146, 14);
	PutPixel(44 + x, 32 + y, 0, 146, 14);
	PutPixel(45 + x, 32 + y, 0, 146, 14);
	PutPixel(46 + x, 32 + y, 0, 118, 11);
	PutPixel(62 + x, 32 + y, 0, 146, 14);
	PutPixel(63 + x, 32 + y, 0, 146, 14);
	PutPixel(64 + x, 32 + y, 0, 146, 14);
	PutPixel(65 + x, 32 + y, 0, 146, 14);
	PutPixel(66 + x, 32 + y, 0, 146, 14);
	PutPixel(67 + x, 32 + y, 0, 146, 14);
	PutPixel(68 + x, 32 + y, 0, 25, 2);
	PutPixel(79 + x, 32 + y, 0, 146, 14);
	PutPixel(80 + x, 32 + y, 0, 146, 14);
	PutPixel(81 + x, 32 + y, 0, 146, 14);
	PutPixel(82 + x, 32 + y, 0, 146, 14);
	PutPixel(9 + x, 33 + y, 0, 146, 14);
	PutPixel(10 + x, 33 + y, 0, 146, 14);
	PutPixel(11 + x, 33 + y, 0, 146, 14);
	PutPixel(12 + x, 33 + y, 0, 146, 14);
	PutPixel(13 + x, 33 + y, 0, 146, 14);
	PutPixel(14 + x, 33 + y, 0, 146, 14);
	PutPixel(15 + x, 33 + y, 0, 146, 14);
	PutPixel(16 + x, 33 + y, 0, 146, 14);
	PutPixel(17 + x, 33 + y, 0, 146, 14);
	PutPixel(18 + x, 33 + y, 0, 146, 14);
	PutPixel(19 + x, 33 + y, 0, 146, 14);
	PutPixel(35 + x, 33 + y, 0, 146, 14);
	PutPixel(36 + x, 33 + y, 0, 146, 14);
	PutPixel(37 + x, 33 + y, 0, 146, 14);
	PutPixel(62 + x, 33 + y, 0, 146, 14);
	PutPixel(63 + x, 33 + y, 0, 146, 14);
	PutPixel(64 + x, 33 + y, 0, 146, 14);
	PutPixel(65 + x, 33 + y, 0, 146, 14);
	PutPixel(66 + x, 33 + y, 0, 146, 14);
	PutPixel(67 + x, 33 + y, 0, 146, 14);
	PutPixel(68 + x, 33 + y, 0, 146, 14);
	PutPixel(69 + x, 33 + y, 0, 146, 14);
	PutPixel(70 + x, 33 + y, 0, 146, 14);
	PutPixel(71 + x, 33 + y, 0, 146, 14);
	PutPixel(72 + x, 33 + y, 0, 146, 14);
	PutPixel(73 + x, 33 + y, 0, 146, 14);
	PutPixel(74 + x, 33 + y, 0, 146, 14);
	PutPixel(75 + x, 33 + y, 0, 146, 14);
	PutPixel(76 + x, 33 + y, 0, 146, 14);
	PutPixel(77 + x, 33 + y, 0, 146, 14);
	PutPixel(78 + x, 33 + y, 0, 146, 14);
	PutPixel(79 + x, 33 + y, 0, 146, 14);
	PutPixel(80 + x, 33 + y, 0, 146, 14);
	PutPixel(81 + x, 33 + y, 0, 146, 14);
	PutPixel(82 + x, 33 + y, 0, 25, 2);
	PutPixel(8 + x, 34 + y, 0, 146, 14);
	PutPixel(9 + x, 34 + y, 0, 146, 14);
	PutPixel(10 + x, 34 + y, 0, 146, 14);
	PutPixel(11 + x, 34 + y, 0, 146, 14);
	PutPixel(12 + x, 34 + y, 0, 146, 14);
	PutPixel(13 + x, 34 + y, 0, 146, 14);
	PutPixel(14 + x, 34 + y, 0, 146, 14);
	PutPixel(15 + x, 34 + y, 0, 146, 14);
	PutPixel(16 + x, 34 + y, 0, 146, 14);
	PutPixel(17 + x, 34 + y, 0, 146, 14);
	PutPixel(18 + x, 34 + y, 0, 146, 14);
	PutPixel(35 + x, 34 + y, 0, 146, 14);
	PutPixel(36 + x, 34 + y, 0, 146, 14);
	PutPixel(37 + x, 34 + y, 0, 146, 14);
	PutPixel(38 + x, 34 + y, 0, 146, 14);
	PutPixel(61 + x, 34 + y, 0, 146, 14);
	PutPixel(62 + x, 34 + y, 0, 146, 14);
	PutPixel(63 + x, 34 + y, 0, 146, 14);
	PutPixel(64 + x, 34 + y, 0, 146, 14);
	PutPixel(65 + x, 34 + y, 0, 146, 14);
	PutPixel(66 + x, 34 + y, 0, 146, 14);
	PutPixel(67 + x, 34 + y, 0, 146, 14);
	PutPixel(68 + x, 34 + y, 0, 146, 14);
	PutPixel(69 + x, 34 + y, 0, 146, 14);
	PutPixel(70 + x, 34 + y, 0, 146, 14);
	PutPixel(71 + x, 34 + y, 0, 146, 14);
	PutPixel(72 + x, 34 + y, 0, 146, 14);
	PutPixel(73 + x, 34 + y, 0, 146, 14);
	PutPixel(74 + x, 34 + y, 0, 146, 14);
	PutPixel(75 + x, 34 + y, 0, 146, 14);
	PutPixel(76 + x, 34 + y, 0, 146, 14);
	PutPixel(77 + x, 34 + y, 0, 146, 14);
	PutPixel(78 + x, 34 + y, 0, 146, 14);
	PutPixel(79 + x, 34 + y, 0, 146, 14);
	PutPixel(80 + x, 34 + y, 0, 146, 14);
	PutPixel(81 + x, 34 + y, 0, 146, 14);
	PutPixel(8 + x, 35 + y, 0, 146, 14);
	PutPixel(9 + x, 35 + y, 0, 146, 14);
	PutPixel(10 + x, 35 + y, 0, 146, 14);
	PutPixel(11 + x, 35 + y, 0, 146, 14);
	PutPixel(12 + x, 35 + y, 0, 146, 14);
	PutPixel(13 + x, 35 + y, 0, 146, 14);
	PutPixel(14 + x, 35 + y, 0, 146, 14);
	PutPixel(15 + x, 35 + y, 0, 146, 14);
	PutPixel(16 + x, 35 + y, 0, 146, 14);
	PutPixel(17 + x, 35 + y, 0, 146, 14);
	PutPixel(36 + x, 35 + y, 0, 146, 14);
	PutPixel(37 + x, 35 + y, 0, 146, 14);
	PutPixel(38 + x, 35 + y, 0, 146, 14);
	PutPixel(39 + x, 35 + y, 0, 146, 14);
	PutPixel(60 + x, 35 + y, 0, 146, 14);
	PutPixel(61 + x, 35 + y, 0, 146, 14);
	PutPixel(62 + x, 35 + y, 0, 146, 14);
	PutPixel(63 + x, 35 + y, 0, 146, 14);
	PutPixel(64 + x, 35 + y, 0, 146, 14);
	PutPixel(65 + x, 35 + y, 0, 146, 14);
	PutPixel(66 + x, 35 + y, 0, 146, 14);
	PutPixel(67 + x, 35 + y, 0, 146, 14);
	PutPixel(68 + x, 35 + y, 0, 146, 14);
	PutPixel(69 + x, 35 + y, 0, 146, 14);
	PutPixel(70 + x, 35 + y, 0, 146, 14);
	PutPixel(71 + x, 35 + y, 0, 146, 14);
	PutPixel(72 + x, 35 + y, 0, 146, 14);
	PutPixel(73 + x, 35 + y, 0, 146, 14);
	PutPixel(74 + x, 35 + y, 0, 146, 14);
	PutPixel(75 + x, 35 + y, 0, 146, 14);
	PutPixel(76 + x, 35 + y, 0, 146, 14);
	PutPixel(77 + x, 35 + y, 0, 146, 14);
	PutPixel(78 + x, 35 + y, 0, 146, 14);
	PutPixel(79 + x, 35 + y, 0, 146, 14);
	PutPixel(8 + x, 36 + y, 0, 146, 14);
	PutPixel(9 + x, 36 + y, 0, 146, 14);
	PutPixel(10 + x, 36 + y, 0, 146, 14);
	PutPixel(11 + x, 36 + y, 0, 146, 14);
	PutPixel(12 + x, 36 + y, 0, 146, 14);
	PutPixel(13 + x, 36 + y, 0, 146, 14);
	PutPixel(14 + x, 36 + y, 0, 146, 14);
	PutPixel(15 + x, 36 + y, 0, 146, 14);
	PutPixel(16 + x, 36 + y, 0, 146, 14);
	PutPixel(17 + x, 36 + y, 0, 146, 14);
	PutPixel(18 + x, 36 + y, 0, 146, 14);
	PutPixel(37 + x, 36 + y, 0, 146, 14);
	PutPixel(38 + x, 36 + y, 0, 146, 14);
	PutPixel(39 + x, 36 + y, 0, 146, 14);
	PutPixel(40 + x, 36 + y, 0, 146, 14);
	PutPixel(59 + x, 36 + y, 0, 146, 14);
	PutPixel(60 + x, 36 + y, 0, 146, 14);
	PutPixel(61 + x, 36 + y, 0, 146, 14);
	PutPixel(62 + x, 36 + y, 0, 146, 14);
	PutPixel(63 + x, 36 + y, 0, 146, 14);
	PutPixel(64 + x, 36 + y, 0, 146, 14);
	PutPixel(65 + x, 36 + y, 0, 146, 14);
	PutPixel(66 + x, 36 + y, 0, 146, 14);
	PutPixel(67 + x, 36 + y, 0, 146, 14);
	PutPixel(68 + x, 36 + y, 0, 146, 14);
	PutPixel(69 + x, 36 + y, 0, 146, 14);
	PutPixel(70 + x, 36 + y, 0, 146, 14);
	PutPixel(71 + x, 36 + y, 0, 146, 14);
	PutPixel(72 + x, 36 + y, 0, 146, 14);
	PutPixel(73 + x, 36 + y, 0, 146, 14);
	PutPixel(74 + x, 36 + y, 0, 146, 14);
	PutPixel(9 + x, 37 + y, 0, 146, 14);
	PutPixel(10 + x, 37 + y, 0, 146, 14);
	PutPixel(11 + x, 37 + y, 0, 146, 14);
	PutPixel(12 + x, 37 + y, 0, 146, 14);
	PutPixel(13 + x, 37 + y, 0, 146, 14);
	PutPixel(14 + x, 37 + y, 0, 146, 14);
	PutPixel(15 + x, 37 + y, 0, 146, 14);
	PutPixel(16 + x, 37 + y, 0, 146, 14);
	PutPixel(17 + x, 37 + y, 0, 146, 14);
	PutPixel(18 + x, 37 + y, 0, 146, 14);
	PutPixel(19 + x, 37 + y, 0, 146, 14);
	PutPixel(37 + x, 37 + y, 0, 70, 6);
	PutPixel(38 + x, 37 + y, 0, 146, 14);
	PutPixel(39 + x, 37 + y, 0, 146, 14);
	PutPixel(40 + x, 37 + y, 0, 146, 14);
	PutPixel(41 + x, 37 + y, 0, 146, 14);
	PutPixel(58 + x, 37 + y, 0, 146, 14);
	PutPixel(59 + x, 37 + y, 0, 146, 14);
	PutPixel(60 + x, 37 + y, 0, 146, 14);
	PutPixel(61 + x, 37 + y, 0, 146, 14);
	PutPixel(62 + x, 37 + y, 0, 146, 14);
	PutPixel(63 + x, 37 + y, 0, 146, 14);
	PutPixel(64 + x, 37 + y, 0, 146, 14);
	PutPixel(65 + x, 37 + y, 0, 146, 14);
	PutPixel(66 + x, 37 + y, 0, 146, 14);
	PutPixel(67 + x, 37 + y, 0, 146, 14);
	PutPixel(68 + x, 37 + y, 0, 146, 14);
	PutPixel(69 + x, 37 + y, 0, 146, 14);
	PutPixel(70 + x, 37 + y, 0, 146, 14);
	PutPixel(71 + x, 37 + y, 0, 146, 14);
	PutPixel(72 + x, 37 + y, 0, 146, 14);
	PutPixel(73 + x, 37 + y, 0, 146, 14);
	PutPixel(74 + x, 37 + y, 0, 146, 14);
	PutPixel(9 + x, 38 + y, 0, 146, 14);
	PutPixel(10 + x, 38 + y, 0, 146, 14);
	PutPixel(11 + x, 38 + y, 0, 146, 14);
	PutPixel(12 + x, 38 + y, 0, 146, 14);
	PutPixel(13 + x, 38 + y, 0, 146, 14);
	PutPixel(14 + x, 38 + y, 0, 146, 14);
	PutPixel(15 + x, 38 + y, 0, 146, 14);
	PutPixel(16 + x, 38 + y, 0, 146, 14);
	PutPixel(17 + x, 38 + y, 0, 146, 14);
	PutPixel(18 + x, 38 + y, 0, 146, 14);
	PutPixel(19 + x, 38 + y, 0, 146, 14);
	PutPixel(20 + x, 38 + y, 0, 146, 14);
	PutPixel(38 + x, 38 + y, 0, 70, 6);
	PutPixel(39 + x, 38 + y, 0, 146, 14);
	PutPixel(40 + x, 38 + y, 0, 146, 14);
	PutPixel(41 + x, 38 + y, 0, 146, 14);
	PutPixel(42 + x, 38 + y, 0, 146, 14);
	PutPixel(43 + x, 38 + y, 0, 146, 14);
	PutPixel(56 + x, 38 + y, 0, 146, 14);
	PutPixel(57 + x, 38 + y, 0, 146, 14);
	PutPixel(58 + x, 38 + y, 0, 146, 14);
	PutPixel(59 + x, 38 + y, 0, 146, 14);
	PutPixel(60 + x, 38 + y, 0, 146, 14);
	PutPixel(61 + x, 38 + y, 0, 146, 14);
	PutPixel(62 + x, 38 + y, 0, 146, 14);
	PutPixel(63 + x, 38 + y, 0, 146, 14);
	PutPixel(64 + x, 38 + y, 0, 146, 14);
	PutPixel(65 + x, 38 + y, 0, 146, 14);
	PutPixel(66 + x, 38 + y, 0, 146, 14);
	PutPixel(67 + x, 38 + y, 0, 146, 14);
	PutPixel(68 + x, 38 + y, 0, 146, 14);
	PutPixel(69 + x, 38 + y, 0, 146, 14);
	PutPixel(70 + x, 38 + y, 0, 146, 14);
	PutPixel(71 + x, 38 + y, 0, 146, 14);
	PutPixel(72 + x, 38 + y, 0, 146, 14);
	PutPixel(73 + x, 38 + y, 0, 146, 14);
	PutPixel(74 + x, 38 + y, 0, 146, 14);
	PutPixel(10 + x, 39 + y, 0, 146, 14);
	PutPixel(11 + x, 39 + y, 0, 146, 14);
	PutPixel(12 + x, 39 + y, 0, 146, 14);
	PutPixel(13 + x, 39 + y, 0, 146, 14);
	PutPixel(14 + x, 39 + y, 0, 146, 14);
	PutPixel(15 + x, 39 + y, 0, 146, 14);
	PutPixel(16 + x, 39 + y, 0, 146, 14);
	PutPixel(17 + x, 39 + y, 0, 146, 14);
	PutPixel(18 + x, 39 + y, 0, 146, 14);
	PutPixel(19 + x, 39 + y, 0, 146, 14);
	PutPixel(20 + x, 39 + y, 0, 146, 14);
	PutPixel(21 + x, 39 + y, 0, 146, 14);
	PutPixel(40 + x, 39 + y, 0, 146, 14);
	PutPixel(41 + x, 39 + y, 0, 146, 14);
	PutPixel(42 + x, 39 + y, 0, 146, 14);
	PutPixel(43 + x, 39 + y, 0, 146, 14);
	PutPixel(44 + x, 39 + y, 0, 146, 14);
	PutPixel(45 + x, 39 + y, 0, 146, 14);
	PutPixel(46 + x, 39 + y, 0, 146, 14);
	PutPixel(53 + x, 39 + y, 0, 146, 14);
	PutPixel(54 + x, 39 + y, 0, 146, 14);
	PutPixel(55 + x, 39 + y, 0, 146, 14);
	PutPixel(56 + x, 39 + y, 0, 146, 14);
	PutPixel(57 + x, 39 + y, 0, 146, 14);
	PutPixel(58 + x, 39 + y, 0, 146, 14);
	PutPixel(59 + x, 39 + y, 0, 146, 14);
	PutPixel(60 + x, 39 + y, 0, 146, 14);
	PutPixel(61 + x, 39 + y, 0, 146, 14);
	PutPixel(62 + x, 39 + y, 0, 146, 14);
	PutPixel(63 + x, 39 + y, 0, 146, 14);
	PutPixel(64 + x, 39 + y, 0, 146, 14);
	PutPixel(65 + x, 39 + y, 0, 146, 14);
	PutPixel(66 + x, 39 + y, 0, 146, 14);
	PutPixel(67 + x, 39 + y, 0, 146, 14);
	PutPixel(68 + x, 39 + y, 0, 146, 14);
	PutPixel(69 + x, 39 + y, 0, 146, 14);
	PutPixel(70 + x, 39 + y, 0, 146, 14);
	PutPixel(71 + x, 39 + y, 0, 146, 14);
	PutPixel(72 + x, 39 + y, 0, 146, 14);
	PutPixel(73 + x, 39 + y, 0, 146, 14);
	PutPixel(74 + x, 39 + y, 0, 146, 14);
	PutPixel(11 + x, 40 + y, 0, 146, 14);
	PutPixel(12 + x, 40 + y, 0, 146, 14);
	PutPixel(13 + x, 40 + y, 0, 146, 14);
	PutPixel(14 + x, 40 + y, 0, 146, 14);
	PutPixel(15 + x, 40 + y, 0, 146, 14);
	PutPixel(16 + x, 40 + y, 0, 146, 14);
	PutPixel(17 + x, 40 + y, 0, 146, 14);
	PutPixel(18 + x, 40 + y, 0, 146, 14);
	PutPixel(19 + x, 40 + y, 0, 146, 14);
	PutPixel(20 + x, 40 + y, 0, 146, 14);
	PutPixel(21 + x, 40 + y, 0, 146, 14);
	PutPixel(22 + x, 40 + y, 0, 146, 14);
	PutPixel(41 + x, 40 + y, 0, 146, 14);
	PutPixel(42 + x, 40 + y, 0, 146, 14);
	PutPixel(43 + x, 40 + y, 0, 146, 14);
	PutPixel(44 + x, 40 + y, 0, 146, 14);
	PutPixel(45 + x, 40 + y, 0, 146, 14);
	PutPixel(46 + x, 40 + y, 0, 146, 14);
	PutPixel(47 + x, 40 + y, 0, 146, 14);
	PutPixel(48 + x, 40 + y, 0, 146, 14);
	PutPixel(49 + x, 40 + y, 0, 146, 14);
	PutPixel(50 + x, 40 + y, 0, 146, 14);
	PutPixel(51 + x, 40 + y, 0, 146, 14);
	PutPixel(52 + x, 40 + y, 0, 146, 14);
	PutPixel(53 + x, 40 + y, 0, 146, 14);
	PutPixel(54 + x, 40 + y, 0, 146, 14);
	PutPixel(55 + x, 40 + y, 0, 146, 14);
	PutPixel(56 + x, 40 + y, 0, 146, 14);
	PutPixel(57 + x, 40 + y, 0, 146, 14);
	PutPixel(58 + x, 40 + y, 0, 146, 14);
	PutPixel(59 + x, 40 + y, 0, 146, 14);
	PutPixel(60 + x, 40 + y, 0, 146, 14);
	PutPixel(61 + x, 40 + y, 0, 146, 14);
	PutPixel(62 + x, 40 + y, 0, 146, 14);
	PutPixel(63 + x, 40 + y, 0, 146, 14);
	PutPixel(64 + x, 40 + y, 0, 146, 14);
	PutPixel(65 + x, 40 + y, 0, 146, 14);
	PutPixel(66 + x, 40 + y, 0, 146, 14);
	PutPixel(67 + x, 40 + y, 0, 146, 14);
	PutPixel(68 + x, 40 + y, 0, 146, 14);
	PutPixel(69 + x, 40 + y, 0, 146, 14);
	PutPixel(70 + x, 40 + y, 0, 146, 14);
	PutPixel(71 + x, 40 + y, 0, 146, 14);
	PutPixel(72 + x, 40 + y, 0, 146, 14);
	PutPixel(73 + x, 40 + y, 0, 146, 14);
	PutPixel(74 + x, 40 + y, 0, 146, 14);
	PutPixel(75 + x, 40 + y, 0, 146, 14);
	PutPixel(76 + x, 40 + y, 0, 146, 14);
	PutPixel(77 + x, 40 + y, 0, 70, 6);
	PutPixel(12 + x, 41 + y, 0, 146, 14);
	PutPixel(13 + x, 41 + y, 0, 146, 14);
	PutPixel(14 + x, 41 + y, 0, 146, 14);
	PutPixel(15 + x, 41 + y, 0, 146, 14);
	PutPixel(16 + x, 41 + y, 0, 146, 14);
	PutPixel(17 + x, 41 + y, 0, 146, 14);
	PutPixel(18 + x, 41 + y, 0, 146, 14);
	PutPixel(19 + x, 41 + y, 0, 146, 14);
	PutPixel(20 + x, 41 + y, 0, 146, 14);
	PutPixel(21 + x, 41 + y, 0, 146, 14);
	PutPixel(22 + x, 41 + y, 0, 146, 14);
	PutPixel(23 + x, 41 + y, 0, 146, 14);
	PutPixel(40 + x, 41 + y, 0, 146, 14);
	PutPixel(41 + x, 41 + y, 0, 146, 14);
	PutPixel(42 + x, 41 + y, 0, 146, 14);
	PutPixel(43 + x, 41 + y, 0, 146, 14);
	PutPixel(44 + x, 41 + y, 0, 146, 14);
	PutPixel(45 + x, 41 + y, 0, 146, 14);
	PutPixel(46 + x, 41 + y, 0, 146, 14);
	PutPixel(47 + x, 41 + y, 0, 146, 14);
	PutPixel(48 + x, 41 + y, 0, 146, 14);
	PutPixel(49 + x, 41 + y, 0, 146, 14);
	PutPixel(50 + x, 41 + y, 0, 146, 14);
	PutPixel(51 + x, 41 + y, 0, 146, 14);
	PutPixel(52 + x, 41 + y, 0, 146, 14);
	PutPixel(53 + x, 41 + y, 0, 146, 14);
	PutPixel(54 + x, 41 + y, 0, 146, 14);
	PutPixel(55 + x, 41 + y, 0, 146, 14);
	PutPixel(56 + x, 41 + y, 0, 146, 14);
	PutPixel(57 + x, 41 + y, 0, 146, 14);
	PutPixel(58 + x, 41 + y, 0, 146, 14);
	PutPixel(59 + x, 41 + y, 0, 146, 14);
	PutPixel(60 + x, 41 + y, 0, 146, 14);
	PutPixel(61 + x, 41 + y, 0, 146, 14);
	PutPixel(62 + x, 41 + y, 0, 146, 14);
	PutPixel(63 + x, 41 + y, 0, 146, 14);
	PutPixel(64 + x, 41 + y, 0, 146, 14);
	PutPixel(65 + x, 41 + y, 0, 146, 14);
	PutPixel(66 + x, 41 + y, 0, 146, 14);
	PutPixel(67 + x, 41 + y, 0, 146, 14);
	PutPixel(68 + x, 41 + y, 0, 146, 14);
	PutPixel(69 + x, 41 + y, 0, 146, 14);
	PutPixel(70 + x, 41 + y, 0, 146, 14);
	PutPixel(71 + x, 41 + y, 0, 146, 14);
	PutPixel(72 + x, 41 + y, 0, 146, 14);
	PutPixel(73 + x, 41 + y, 0, 146, 14);
	PutPixel(74 + x, 41 + y, 0, 146, 14);
	PutPixel(75 + x, 41 + y, 0, 146, 14);
	PutPixel(76 + x, 41 + y, 0, 146, 14);
	PutPixel(77 + x, 41 + y, 0, 146, 14);
	PutPixel(78 + x, 41 + y, 0, 146, 14);
	PutPixel(3 + x, 42 + y, 0, 25, 2);
	PutPixel(4 + x, 42 + y, 0, 146, 14);
	PutPixel(5 + x, 42 + y, 0, 146, 14);
	PutPixel(6 + x, 42 + y, 0, 25, 2);
	PutPixel(13 + x, 42 + y, 0, 146, 14);
	PutPixel(14 + x, 42 + y, 0, 146, 14);
	PutPixel(15 + x, 42 + y, 0, 146, 14);
	PutPixel(16 + x, 42 + y, 0, 146, 14);
	PutPixel(17 + x, 42 + y, 0, 146, 14);
	PutPixel(18 + x, 42 + y, 0, 146, 14);
	PutPixel(19 + x, 42 + y, 0, 146, 14);
	PutPixel(20 + x, 42 + y, 0, 146, 14);
	PutPixel(21 + x, 42 + y, 0, 146, 14);
	PutPixel(22 + x, 42 + y, 0, 146, 14);
	PutPixel(23 + x, 42 + y, 0, 146, 14);
	PutPixel(24 + x, 42 + y, 0, 146, 14);
	PutPixel(39 + x, 42 + y, 0, 146, 14);
	PutPixel(40 + x, 42 + y, 0, 146, 14);
	PutPixel(41 + x, 42 + y, 0, 146, 14);
	PutPixel(42 + x, 42 + y, 0, 146, 14);
	PutPixel(43 + x, 42 + y, 0, 146, 14);
	PutPixel(44 + x, 42 + y, 0, 146, 14);
	PutPixel(45 + x, 42 + y, 0, 146, 14);
	PutPixel(46 + x, 42 + y, 0, 146, 14);
	PutPixel(47 + x, 42 + y, 0, 146, 14);
	PutPixel(48 + x, 42 + y, 0, 146, 14);
	PutPixel(49 + x, 42 + y, 0, 146, 14);
	PutPixel(50 + x, 42 + y, 0, 146, 14);
	PutPixel(51 + x, 42 + y, 0, 146, 14);
	PutPixel(52 + x, 42 + y, 0, 146, 14);
	PutPixel(53 + x, 42 + y, 0, 146, 14);
	PutPixel(54 + x, 42 + y, 0, 146, 14);
	PutPixel(55 + x, 42 + y, 0, 146, 14);
	PutPixel(56 + x, 42 + y, 0, 146, 14);
	PutPixel(57 + x, 42 + y, 0, 146, 14);
	PutPixel(58 + x, 42 + y, 0, 146, 14);
	PutPixel(59 + x, 42 + y, 0, 146, 14);
	PutPixel(60 + x, 42 + y, 0, 146, 14);
	PutPixel(61 + x, 42 + y, 0, 146, 14);
	PutPixel(62 + x, 42 + y, 0, 146, 14);
	PutPixel(63 + x, 42 + y, 0, 146, 14);
	PutPixel(64 + x, 42 + y, 0, 146, 14);
	PutPixel(65 + x, 42 + y, 0, 146, 14);
	PutPixel(66 + x, 42 + y, 0, 146, 14);
	PutPixel(67 + x, 42 + y, 0, 146, 14);
	PutPixel(68 + x, 42 + y, 0, 146, 14);
	PutPixel(69 + x, 42 + y, 0, 146, 14);
	PutPixel(70 + x, 42 + y, 0, 146, 14);
	PutPixel(71 + x, 42 + y, 0, 146, 14);
	PutPixel(72 + x, 42 + y, 0, 146, 14);
	PutPixel(73 + x, 42 + y, 0, 146, 14);
	PutPixel(74 + x, 42 + y, 0, 146, 14);
	PutPixel(75 + x, 42 + y, 0, 146, 14);
	PutPixel(76 + x, 42 + y, 0, 146, 14);
	PutPixel(77 + x, 42 + y, 0, 146, 14);
	PutPixel(78 + x, 42 + y, 0, 146, 14);
	PutPixel(79 + x, 42 + y, 0, 146, 14);
	PutPixel(1 + x, 43 + y, 0, 146, 14);
	PutPixel(2 + x, 43 + y, 0, 146, 14);
	PutPixel(3 + x, 43 + y, 0, 146, 14);
	PutPixel(4 + x, 43 + y, 0, 146, 14);
	PutPixel(5 + x, 43 + y, 0, 146, 14);
	PutPixel(6 + x, 43 + y, 0, 146, 14);
	PutPixel(7 + x, 43 + y, 0, 146, 14);
	PutPixel(8 + x, 43 + y, 0, 146, 14);
	PutPixel(9 + x, 43 + y, 0, 146, 14);
	PutPixel(10 + x, 43 + y, 0, 146, 14);
	PutPixel(11 + x, 43 + y, 0, 146, 14);
	PutPixel(12 + x, 43 + y, 0, 146, 14);
	PutPixel(13 + x, 43 + y, 0, 146, 14);
	PutPixel(14 + x, 43 + y, 0, 146, 14);
	PutPixel(15 + x, 43 + y, 0, 146, 14);
	PutPixel(16 + x, 43 + y, 0, 146, 14);
	PutPixel(17 + x, 43 + y, 0, 146, 14);
	PutPixel(18 + x, 43 + y, 0, 146, 14);
	PutPixel(19 + x, 43 + y, 0, 146, 14);
	PutPixel(20 + x, 43 + y, 0, 146, 14);
	PutPixel(21 + x, 43 + y, 0, 146, 14);
	PutPixel(22 + x, 43 + y, 0, 146, 14);
	PutPixel(23 + x, 43 + y, 0, 146, 14);
	PutPixel(24 + x, 43 + y, 0, 146, 14);
	PutPixel(25 + x, 43 + y, 0, 146, 14);
	PutPixel(38 + x, 43 + y, 0, 146, 14);
	PutPixel(39 + x, 43 + y, 0, 146, 14);
	PutPixel(40 + x, 43 + y, 0, 146, 14);
	PutPixel(41 + x, 43 + y, 0, 146, 14);
	PutPixel(77 + x, 43 + y, 0, 146, 14);
	PutPixel(78 + x, 43 + y, 0, 146, 14);
	PutPixel(79 + x, 43 + y, 0, 146, 14);
	PutPixel(0 + x, 44 + y, 0, 146, 14);
	PutPixel(1 + x, 44 + y, 0, 146, 14);
	PutPixel(2 + x, 44 + y, 0, 146, 14);
	PutPixel(3 + x, 44 + y, 0, 146, 14);
	PutPixel(4 + x, 44 + y, 0, 146, 14);
	PutPixel(5 + x, 44 + y, 0, 146, 14);
	PutPixel(6 + x, 44 + y, 0, 146, 14);
	PutPixel(7 + x, 44 + y, 0, 146, 14);
	PutPixel(8 + x, 44 + y, 0, 146, 14);
	PutPixel(9 + x, 44 + y, 0, 146, 14);
	PutPixel(10 + x, 44 + y, 0, 146, 14);
	PutPixel(11 + x, 44 + y, 0, 146, 14);
	PutPixel(12 + x, 44 + y, 0, 146, 14);
	PutPixel(13 + x, 44 + y, 0, 146, 14);
	PutPixel(14 + x, 44 + y, 0, 146, 14);
	PutPixel(15 + x, 44 + y, 0, 146, 14);
	PutPixel(16 + x, 44 + y, 0, 146, 14);
	PutPixel(17 + x, 44 + y, 0, 146, 14);
	PutPixel(18 + x, 44 + y, 0, 146, 14);
	PutPixel(19 + x, 44 + y, 0, 146, 14);
	PutPixel(20 + x, 44 + y, 0, 146, 14);
	PutPixel(21 + x, 44 + y, 0, 146, 14);
	PutPixel(22 + x, 44 + y, 0, 146, 14);
	PutPixel(23 + x, 44 + y, 0, 146, 14);
	PutPixel(24 + x, 44 + y, 0, 146, 14);
	PutPixel(25 + x, 44 + y, 0, 146, 14);
	PutPixel(26 + x, 44 + y, 0, 146, 14);
	PutPixel(38 + x, 44 + y, 0, 146, 14);
	PutPixel(39 + x, 44 + y, 0, 146, 14);
	PutPixel(40 + x, 44 + y, 0, 146, 14);
	PutPixel(77 + x, 44 + y, 0, 3, 0);
	PutPixel(78 + x, 44 + y, 0, 146, 14);
	PutPixel(79 + x, 44 + y, 0, 146, 14);
	PutPixel(80 + x, 44 + y, 0, 146, 14);
	PutPixel(0 + x, 45 + y, 0, 146, 14);
	PutPixel(1 + x, 45 + y, 0, 146, 14);
	PutPixel(2 + x, 45 + y, 0, 146, 14);
	PutPixel(3 + x, 45 + y, 0, 146, 14);
	PutPixel(4 + x, 45 + y, 0, 146, 14);
	PutPixel(5 + x, 45 + y, 0, 146, 14);
	PutPixel(6 + x, 45 + y, 0, 146, 14);
	PutPixel(7 + x, 45 + y, 0, 146, 14);
	PutPixel(8 + x, 45 + y, 0, 146, 14);
	PutPixel(9 + x, 45 + y, 0, 146, 14);
	PutPixel(10 + x, 45 + y, 0, 146, 14);
	PutPixel(11 + x, 45 + y, 0, 146, 14);
	PutPixel(12 + x, 45 + y, 0, 146, 14);
	PutPixel(13 + x, 45 + y, 0, 146, 14);
	PutPixel(14 + x, 45 + y, 0, 146, 14);
	PutPixel(15 + x, 45 + y, 0, 146, 14);
	PutPixel(16 + x, 45 + y, 0, 146, 14);
	PutPixel(17 + x, 45 + y, 0, 146, 14);
	PutPixel(18 + x, 45 + y, 0, 146, 14);
	PutPixel(19 + x, 45 + y, 0, 146, 14);
	PutPixel(20 + x, 45 + y, 0, 146, 14);
	PutPixel(21 + x, 45 + y, 0, 146, 14);
	PutPixel(22 + x, 45 + y, 0, 146, 14);
	PutPixel(23 + x, 45 + y, 0, 146, 14);
	PutPixel(24 + x, 45 + y, 0, 146, 14);
	PutPixel(25 + x, 45 + y, 0, 146, 14);
	PutPixel(26 + x, 45 + y, 0, 146, 14);
	PutPixel(27 + x, 45 + y, 0, 118, 11);
	PutPixel(38 + x, 45 + y, 0, 146, 14);
	PutPixel(39 + x, 45 + y, 0, 146, 14);
	PutPixel(40 + x, 45 + y, 0, 146, 14);
	PutPixel(78 + x, 45 + y, 0, 146, 14);
	PutPixel(79 + x, 45 + y, 0, 146, 14);
	PutPixel(80 + x, 45 + y, 0, 146, 14);
	PutPixel(0 + x, 46 + y, 0, 146, 14);
	PutPixel(1 + x, 46 + y, 0, 146, 14);
	PutPixel(2 + x, 46 + y, 0, 146, 14);
	PutPixel(3 + x, 46 + y, 0, 146, 14);
	PutPixel(4 + x, 46 + y, 0, 146, 14);
	PutPixel(5 + x, 46 + y, 0, 146, 14);
	PutPixel(6 + x, 46 + y, 0, 146, 14);
	PutPixel(7 + x, 46 + y, 0, 146, 14);
	PutPixel(8 + x, 46 + y, 0, 146, 14);
	PutPixel(9 + x, 46 + y, 0, 146, 14);
	PutPixel(10 + x, 46 + y, 0, 146, 14);
	PutPixel(11 + x, 46 + y, 0, 146, 14);
	PutPixel(12 + x, 46 + y, 0, 146, 14);
	PutPixel(13 + x, 46 + y, 0, 146, 14);
	PutPixel(14 + x, 46 + y, 0, 146, 14);
	PutPixel(15 + x, 46 + y, 0, 146, 14);
	PutPixel(16 + x, 46 + y, 0, 146, 14);
	PutPixel(17 + x, 46 + y, 0, 146, 14);
	PutPixel(18 + x, 46 + y, 0, 146, 14);
	PutPixel(19 + x, 46 + y, 0, 146, 14);
	PutPixel(20 + x, 46 + y, 0, 146, 14);
	PutPixel(21 + x, 46 + y, 0, 146, 14);
	PutPixel(22 + x, 46 + y, 0, 146, 14);
	PutPixel(23 + x, 46 + y, 0, 146, 14);
	PutPixel(24 + x, 46 + y, 0, 146, 14);
	PutPixel(25 + x, 46 + y, 0, 146, 14);
	PutPixel(26 + x, 46 + y, 0, 146, 14);
	PutPixel(27 + x, 46 + y, 0, 146, 14);
	PutPixel(38 + x, 46 + y, 0, 146, 14);
	PutPixel(39 + x, 46 + y, 0, 146, 14);
	PutPixel(40 + x, 46 + y, 0, 146, 14);
	PutPixel(77 + x, 46 + y, 0, 118, 11);
	PutPixel(78 + x, 46 + y, 0, 146, 14);
	PutPixel(79 + x, 46 + y, 0, 146, 14);
	PutPixel(80 + x, 46 + y, 0, 146, 14);
	PutPixel(0 + x, 47 + y, 0, 146, 14);
	PutPixel(1 + x, 47 + y, 0, 146, 14);
	PutPixel(2 + x, 47 + y, 0, 146, 14);
	PutPixel(3 + x, 47 + y, 0, 146, 14);
	PutPixel(4 + x, 47 + y, 0, 146, 14);
	PutPixel(5 + x, 47 + y, 0, 146, 14);
	PutPixel(6 + x, 47 + y, 0, 146, 14);
	PutPixel(7 + x, 47 + y, 0, 146, 14);
	PutPixel(8 + x, 47 + y, 0, 146, 14);
	PutPixel(9 + x, 47 + y, 0, 146, 14);
	PutPixel(10 + x, 47 + y, 0, 146, 14);
	PutPixel(11 + x, 47 + y, 0, 146, 14);
	PutPixel(12 + x, 47 + y, 0, 146, 14);
	PutPixel(13 + x, 47 + y, 0, 146, 14);
	PutPixel(14 + x, 47 + y, 0, 146, 14);
	PutPixel(15 + x, 47 + y, 0, 146, 14);
	PutPixel(16 + x, 47 + y, 0, 146, 14);
	PutPixel(17 + x, 47 + y, 0, 146, 14);
	PutPixel(18 + x, 47 + y, 0, 146, 14);
	PutPixel(19 + x, 47 + y, 0, 146, 14);
	PutPixel(20 + x, 47 + y, 0, 146, 14);
	PutPixel(21 + x, 47 + y, 0, 146, 14);
	PutPixel(22 + x, 47 + y, 0, 146, 14);
	PutPixel(23 + x, 47 + y, 0, 146, 14);
	PutPixel(24 + x, 47 + y, 0, 146, 14);
	PutPixel(25 + x, 47 + y, 0, 146, 14);
	PutPixel(26 + x, 47 + y, 0, 146, 14);
	PutPixel(27 + x, 47 + y, 0, 146, 14);
	PutPixel(38 + x, 47 + y, 0, 146, 14);
	PutPixel(39 + x, 47 + y, 0, 146, 14);
	PutPixel(40 + x, 47 + y, 0, 146, 14);
	PutPixel(41 + x, 47 + y, 0, 146, 14);
	PutPixel(77 + x, 47 + y, 0, 146, 14);
	PutPixel(78 + x, 47 + y, 0, 146, 14);
	PutPixel(79 + x, 47 + y, 0, 146, 14);
	PutPixel(0 + x, 48 + y, 0, 146, 14);
	PutPixel(1 + x, 48 + y, 0, 146, 14);
	PutPixel(2 + x, 48 + y, 0, 146, 14);
	PutPixel(3 + x, 48 + y, 0, 146, 14);
	PutPixel(4 + x, 48 + y, 0, 146, 14);
	PutPixel(5 + x, 48 + y, 0, 146, 14);
	PutPixel(6 + x, 48 + y, 0, 146, 14);
	PutPixel(7 + x, 48 + y, 0, 146, 14);
	PutPixel(8 + x, 48 + y, 0, 146, 14);
	PutPixel(9 + x, 48 + y, 0, 146, 14);
	PutPixel(10 + x, 48 + y, 0, 146, 14);
	PutPixel(11 + x, 48 + y, 0, 146, 14);
	PutPixel(12 + x, 48 + y, 0, 146, 14);
	PutPixel(13 + x, 48 + y, 0, 146, 14);
	PutPixel(14 + x, 48 + y, 0, 146, 14);
	PutPixel(15 + x, 48 + y, 0, 146, 14);
	PutPixel(16 + x, 48 + y, 0, 146, 14);
	PutPixel(17 + x, 48 + y, 0, 146, 14);
	PutPixel(18 + x, 48 + y, 0, 146, 14);
	PutPixel(19 + x, 48 + y, 0, 146, 14);
	PutPixel(20 + x, 48 + y, 0, 146, 14);
	PutPixel(21 + x, 48 + y, 0, 146, 14);
	PutPixel(22 + x, 48 + y, 0, 146, 14);
	PutPixel(23 + x, 48 + y, 0, 146, 14);
	PutPixel(24 + x, 48 + y, 0, 146, 14);
	PutPixel(25 + x, 48 + y, 0, 146, 14);
	PutPixel(26 + x, 48 + y, 0, 146, 14);
	PutPixel(39 + x, 48 + y, 0, 146, 14);
	PutPixel(40 + x, 48 + y, 0, 146, 14);
	PutPixel(41 + x, 48 + y, 0, 146, 14);
	PutPixel(42 + x, 48 + y, 0, 146, 14);
	PutPixel(43 + x, 48 + y, 0, 146, 14);
	PutPixel(44 + x, 48 + y, 0, 146, 14);
	PutPixel(45 + x, 48 + y, 0, 146, 14);
	PutPixel(46 + x, 48 + y, 0, 146, 14);
	PutPixel(47 + x, 48 + y, 0, 146, 14);
	PutPixel(48 + x, 48 + y, 0, 146, 14);
	PutPixel(49 + x, 48 + y, 0, 146, 14);
	PutPixel(50 + x, 48 + y, 0, 146, 14);
	PutPixel(51 + x, 48 + y, 0, 146, 14);
	PutPixel(52 + x, 48 + y, 0, 146, 14);
	PutPixel(53 + x, 48 + y, 0, 146, 14);
	PutPixel(54 + x, 48 + y, 0, 146, 14);
	PutPixel(55 + x, 48 + y, 0, 146, 14);
	PutPixel(56 + x, 48 + y, 0, 146, 14);
	PutPixel(57 + x, 48 + y, 0, 146, 14);
	PutPixel(58 + x, 48 + y, 0, 146, 14);
	PutPixel(59 + x, 48 + y, 0, 146, 14);
	PutPixel(60 + x, 48 + y, 0, 146, 14);
	PutPixel(61 + x, 48 + y, 0, 146, 14);
	PutPixel(62 + x, 48 + y, 0, 146, 14);
	PutPixel(63 + x, 48 + y, 0, 146, 14);
	PutPixel(64 + x, 48 + y, 0, 146, 14);
	PutPixel(65 + x, 48 + y, 0, 146, 14);
	PutPixel(66 + x, 48 + y, 0, 146, 14);
	PutPixel(67 + x, 48 + y, 0, 146, 14);
	PutPixel(68 + x, 48 + y, 0, 146, 14);
	PutPixel(69 + x, 48 + y, 0, 146, 14);
	PutPixel(70 + x, 48 + y, 0, 146, 14);
	PutPixel(71 + x, 48 + y, 0, 146, 14);
	PutPixel(72 + x, 48 + y, 0, 146, 14);
	PutPixel(73 + x, 48 + y, 0, 146, 14);
	PutPixel(74 + x, 48 + y, 0, 146, 14);
	PutPixel(75 + x, 48 + y, 0, 146, 14);
	PutPixel(76 + x, 48 + y, 0, 146, 14);
	PutPixel(77 + x, 48 + y, 0, 146, 14);
	PutPixel(78 + x, 48 + y, 0, 146, 14);
	PutPixel(79 + x, 48 + y, 0, 146, 14);
	PutPixel(0 + x, 49 + y, 0, 142, 13);
	PutPixel(1 + x, 49 + y, 0, 146, 14);
	PutPixel(2 + x, 49 + y, 0, 146, 14);
	PutPixel(3 + x, 49 + y, 0, 146, 14);
	PutPixel(4 + x, 49 + y, 0, 146, 14);
	PutPixel(5 + x, 49 + y, 0, 146, 14);
	PutPixel(6 + x, 49 + y, 0, 146, 14);
	PutPixel(7 + x, 49 + y, 0, 146, 14);
	PutPixel(8 + x, 49 + y, 0, 146, 14);
	PutPixel(9 + x, 49 + y, 0, 146, 14);
	PutPixel(10 + x, 49 + y, 0, 146, 14);
	PutPixel(11 + x, 49 + y, 0, 146, 14);
	PutPixel(12 + x, 49 + y, 0, 146, 14);
	PutPixel(13 + x, 49 + y, 0, 146, 14);
	PutPixel(14 + x, 49 + y, 0, 146, 14);
	PutPixel(15 + x, 49 + y, 0, 146, 14);
	PutPixel(16 + x, 49 + y, 0, 146, 14);
	PutPixel(17 + x, 49 + y, 0, 146, 14);
	PutPixel(18 + x, 49 + y, 0, 146, 14);
	PutPixel(19 + x, 49 + y, 0, 146, 14);
	PutPixel(20 + x, 49 + y, 0, 146, 14);
	PutPixel(21 + x, 49 + y, 0, 146, 14);
	PutPixel(22 + x, 49 + y, 0, 146, 14);
	PutPixel(23 + x, 49 + y, 0, 146, 14);
	PutPixel(24 + x, 49 + y, 0, 146, 14);
	PutPixel(25 + x, 49 + y, 0, 146, 14);
	PutPixel(26 + x, 49 + y, 0, 146, 14);
	PutPixel(40 + x, 49 + y, 0, 146, 14);
	PutPixel(41 + x, 49 + y, 0, 146, 14);
	PutPixel(42 + x, 49 + y, 0, 146, 14);
	PutPixel(43 + x, 49 + y, 0, 146, 14);
	PutPixel(44 + x, 49 + y, 0, 146, 14);
	PutPixel(45 + x, 49 + y, 0, 146, 14);
	PutPixel(46 + x, 49 + y, 0, 146, 14);
	PutPixel(47 + x, 49 + y, 0, 146, 14);
	PutPixel(48 + x, 49 + y, 0, 146, 14);
	PutPixel(49 + x, 49 + y, 0, 146, 14);
	PutPixel(50 + x, 49 + y, 0, 146, 14);
	PutPixel(51 + x, 49 + y, 0, 146, 14);
	PutPixel(52 + x, 49 + y, 0, 146, 14);
	PutPixel(53 + x, 49 + y, 0, 146, 14);
	PutPixel(54 + x, 49 + y, 0, 146, 14);
	PutPixel(55 + x, 49 + y, 0, 146, 14);
	PutPixel(56 + x, 49 + y, 0, 146, 14);
	PutPixel(57 + x, 49 + y, 0, 146, 14);
	PutPixel(58 + x, 49 + y, 0, 146, 14);
	PutPixel(59 + x, 49 + y, 0, 146, 14);
	PutPixel(60 + x, 49 + y, 0, 146, 14);
	PutPixel(61 + x, 49 + y, 0, 146, 14);
	PutPixel(62 + x, 49 + y, 0, 146, 14);
	PutPixel(63 + x, 49 + y, 0, 146, 14);
	PutPixel(64 + x, 49 + y, 0, 146, 14);
	PutPixel(65 + x, 49 + y, 0, 146, 14);
	PutPixel(66 + x, 49 + y, 0, 146, 14);
	PutPixel(67 + x, 49 + y, 0, 146, 14);
	PutPixel(68 + x, 49 + y, 0, 146, 14);
	PutPixel(69 + x, 49 + y, 0, 146, 14);
	PutPixel(70 + x, 49 + y, 0, 146, 14);
	PutPixel(71 + x, 49 + y, 0, 146, 14);
	PutPixel(72 + x, 49 + y, 0, 146, 14);
	PutPixel(73 + x, 49 + y, 0, 146, 14);
	PutPixel(74 + x, 49 + y, 0, 146, 14);
	PutPixel(75 + x, 49 + y, 0, 146, 14);
	PutPixel(76 + x, 49 + y, 0, 146, 14);
	PutPixel(77 + x, 49 + y, 0, 146, 14);
	PutPixel(78 + x, 49 + y, 0, 146, 14);
	PutPixel(2 + x, 50 + y, 0, 146, 14);
	PutPixel(3 + x, 50 + y, 0, 146, 14);
	PutPixel(4 + x, 50 + y, 0, 146, 14);
	PutPixel(5 + x, 50 + y, 0, 146, 14);
	PutPixel(6 + x, 50 + y, 0, 146, 14);
	PutPixel(7 + x, 50 + y, 0, 146, 14);
	PutPixel(8 + x, 50 + y, 0, 146, 14);
	PutPixel(9 + x, 50 + y, 0, 146, 14);
	PutPixel(10 + x, 50 + y, 0, 146, 14);
	PutPixel(11 + x, 50 + y, 0, 146, 14);
	PutPixel(12 + x, 50 + y, 0, 146, 14);
	PutPixel(13 + x, 50 + y, 0, 146, 14);
	PutPixel(14 + x, 50 + y, 0, 146, 14);
	PutPixel(15 + x, 50 + y, 0, 146, 14);
	PutPixel(16 + x, 50 + y, 0, 146, 14);
	PutPixel(17 + x, 50 + y, 0, 146, 14);
	PutPixel(18 + x, 50 + y, 0, 146, 14);
	PutPixel(19 + x, 50 + y, 0, 146, 14);
	PutPixel(20 + x, 50 + y, 0, 146, 14);
	PutPixel(21 + x, 50 + y, 0, 146, 14);
	PutPixel(22 + x, 50 + y, 0, 146, 14);
	PutPixel(23 + x, 50 + y, 0, 146, 14);
	PutPixel(24 + x, 50 + y, 0, 146, 14);
	PutPixel(25 + x, 50 + y, 0, 142, 13);
	PutPixel(41 + x, 50 + y, 0, 146, 14);
	PutPixel(42 + x, 50 + y, 0, 146, 14);
	PutPixel(43 + x, 50 + y, 0, 146, 14);
	PutPixel(44 + x, 50 + y, 0, 146, 14);
	PutPixel(45 + x, 50 + y, 0, 146, 14);
	PutPixel(46 + x, 50 + y, 0, 146, 14);
	PutPixel(47 + x, 50 + y, 0, 146, 14);
	PutPixel(48 + x, 50 + y, 0, 146, 14);
	PutPixel(49 + x, 50 + y, 0, 146, 14);
	PutPixel(50 + x, 50 + y, 0, 146, 14);
	PutPixel(51 + x, 50 + y, 0, 146, 14);
	PutPixel(52 + x, 50 + y, 0, 146, 14);
	PutPixel(53 + x, 50 + y, 0, 146, 14);
	PutPixel(54 + x, 50 + y, 0, 146, 14);
	PutPixel(55 + x, 50 + y, 0, 146, 14);
	PutPixel(56 + x, 50 + y, 0, 146, 14);
	PutPixel(57 + x, 50 + y, 0, 146, 14);
	PutPixel(58 + x, 50 + y, 0, 146, 14);
	PutPixel(59 + x, 50 + y, 0, 146, 14);
	PutPixel(60 + x, 50 + y, 0, 146, 14);
	PutPixel(61 + x, 50 + y, 0, 146, 14);
	PutPixel(62 + x, 50 + y, 0, 146, 14);
	PutPixel(63 + x, 50 + y, 0, 146, 14);
	PutPixel(64 + x, 50 + y, 0, 146, 14);
	PutPixel(65 + x, 50 + y, 0, 146, 14);
	PutPixel(66 + x, 50 + y, 0, 146, 14);
	PutPixel(67 + x, 50 + y, 0, 146, 14);
	PutPixel(68 + x, 50 + y, 0, 146, 14);
	PutPixel(69 + x, 50 + y, 0, 146, 14);
	PutPixel(70 + x, 50 + y, 0, 146, 14);
	PutPixel(71 + x, 50 + y, 0, 146, 14);
	PutPixel(72 + x, 50 + y, 0, 146, 14);
	PutPixel(73 + x, 50 + y, 0, 146, 14);
	PutPixel(74 + x, 50 + y, 0, 146, 14);
	PutPixel(75 + x, 50 + y, 0, 146, 14);
	PutPixel(76 + x, 50 + y, 0, 146, 14);
	PutPixel(2 + x, 55 + y, 0, 146, 14);
	PutPixel(3 + x, 55 + y, 0, 146, 14);
	PutPixel(4 + x, 55 + y, 0, 146, 14);
	PutPixel(5 + x, 55 + y, 0, 146, 14);
	PutPixel(6 + x, 55 + y, 0, 146, 14);
	PutPixel(7 + x, 55 + y, 0, 146, 14);
	PutPixel(12 + x, 55 + y, 0, 146, 14);
	PutPixel(13 + x, 55 + y, 0, 146, 14);
	PutPixel(14 + x, 55 + y, 0, 146, 14);
	PutPixel(19 + x, 55 + y, 0, 146, 14);
	PutPixel(20 + x, 55 + y, 0, 146, 14);
	PutPixel(25 + x, 55 + y, 0, 146, 14);
	PutPixel(26 + x, 55 + y, 0, 146, 14);
	PutPixel(28 + x, 55 + y, 0, 146, 14);
	PutPixel(29 + x, 55 + y, 0, 146, 14);
	PutPixel(30 + x, 55 + y, 0, 146, 14);
	PutPixel(31 + x, 55 + y, 0, 146, 14);
	PutPixel(32 + x, 55 + y, 0, 146, 14);
	PutPixel(33 + x, 55 + y, 0, 146, 14);
	PutPixel(34 + x, 55 + y, 0, 146, 14);
	PutPixel(35 + x, 55 + y, 0, 146, 14);
	PutPixel(36 + x, 55 + y, 0, 146, 14);
	PutPixel(48 + x, 55 + y, 0, 118, 11);
	PutPixel(49 + x, 55 + y, 0, 146, 14);
	PutPixel(50 + x, 55 + y, 0, 146, 14);
	PutPixel(51 + x, 55 + y, 0, 146, 14);
	PutPixel(52 + x, 55 + y, 0, 146, 14);
	PutPixel(53 + x, 55 + y, 0, 146, 14);
	PutPixel(54 + x, 55 + y, 0, 146, 14);
	PutPixel(57 + x, 55 + y, 0, 146, 14);
	PutPixel(58 + x, 55 + y, 0, 146, 14);
	PutPixel(63 + x, 55 + y, 0, 146, 14);
	PutPixel(64 + x, 55 + y, 0, 146, 14);
	PutPixel(66 + x, 55 + y, 0, 3, 0);
	PutPixel(67 + x, 55 + y, 0, 146, 14);
	PutPixel(68 + x, 55 + y, 0, 146, 14);
	PutPixel(69 + x, 55 + y, 0, 146, 14);
	PutPixel(70 + x, 55 + y, 0, 146, 14);
	PutPixel(71 + x, 55 + y, 0, 146, 14);
	PutPixel(72 + x, 55 + y, 0, 146, 14);
	PutPixel(73 + x, 55 + y, 0, 146, 14);
	PutPixel(74 + x, 55 + y, 0, 146, 14);
	PutPixel(76 + x, 55 + y, 0, 146, 14);
	PutPixel(77 + x, 55 + y, 0, 146, 14);
	PutPixel(78 + x, 55 + y, 0, 146, 14);
	PutPixel(79 + x, 55 + y, 0, 146, 14);
	PutPixel(80 + x, 55 + y, 0, 146, 14);
	PutPixel(81 + x, 55 + y, 0, 146, 14);
	PutPixel(82 + x, 55 + y, 0, 146, 14);
	PutPixel(1 + x, 56 + y, 0, 146, 14);
	PutPixel(2 + x, 56 + y, 0, 146, 14);
	PutPixel(3 + x, 56 + y, 0, 146, 14);
	PutPixel(4 + x, 56 + y, 0, 146, 14);
	PutPixel(5 + x, 56 + y, 0, 146, 14);
	PutPixel(6 + x, 56 + y, 0, 146, 14);
	PutPixel(7 + x, 56 + y, 0, 146, 14);
	PutPixel(11 + x, 56 + y, 0, 146, 14);
	PutPixel(12 + x, 56 + y, 0, 146, 14);
	PutPixel(13 + x, 56 + y, 0, 146, 14);
	PutPixel(14 + x, 56 + y, 0, 146, 14);
	PutPixel(15 + x, 56 + y, 0, 146, 14);
	PutPixel(16 + x, 56 + y, 0, 142, 13);
	PutPixel(19 + x, 56 + y, 0, 146, 14);
	PutPixel(20 + x, 56 + y, 0, 146, 14);
	PutPixel(21 + x, 56 + y, 0, 146, 14);
	PutPixel(24 + x, 56 + y, 0, 146, 14);
	PutPixel(25 + x, 56 + y, 0, 146, 14);
	PutPixel(26 + x, 56 + y, 0, 146, 14);
	PutPixel(28 + x, 56 + y, 0, 146, 14);
	PutPixel(29 + x, 56 + y, 0, 146, 14);
	PutPixel(30 + x, 56 + y, 0, 146, 14);
	PutPixel(31 + x, 56 + y, 0, 146, 14);
	PutPixel(32 + x, 56 + y, 0, 146, 14);
	PutPixel(33 + x, 56 + y, 0, 146, 14);
	PutPixel(34 + x, 56 + y, 0, 146, 14);
	PutPixel(35 + x, 56 + y, 0, 146, 14);
	PutPixel(36 + x, 56 + y, 0, 142, 13);
	PutPixel(47 + x, 56 + y, 0, 146, 14);
	PutPixel(48 + x, 56 + y, 0, 146, 14);
	PutPixel(49 + x, 56 + y, 0, 146, 14);
	PutPixel(50 + x, 56 + y, 0, 25, 2);
	PutPixel(51 + x, 56 + y, 0, 25, 2);
	PutPixel(52 + x, 56 + y, 0, 146, 14);
	PutPixel(53 + x, 56 + y, 0, 146, 14);
	PutPixel(54 + x, 56 + y, 0, 146, 14);
	PutPixel(55 + x, 56 + y, 0, 146, 14);
	PutPixel(57 + x, 56 + y, 0, 146, 14);
	PutPixel(58 + x, 56 + y, 0, 146, 14);
	PutPixel(63 + x, 56 + y, 0, 146, 14);
	PutPixel(64 + x, 56 + y, 0, 146, 14);
	PutPixel(65 + x, 56 + y, 0, 146, 14);
	PutPixel(66 + x, 56 + y, 0, 146, 14);
	PutPixel(67 + x, 56 + y, 0, 146, 14);
	PutPixel(68 + x, 56 + y, 0, 146, 14);
	PutPixel(69 + x, 56 + y, 0, 146, 14);
	PutPixel(70 + x, 56 + y, 0, 146, 14);
	PutPixel(71 + x, 56 + y, 0, 146, 14);
	PutPixel(72 + x, 56 + y, 0, 146, 14);
	PutPixel(73 + x, 56 + y, 0, 146, 14);
	PutPixel(74 + x, 56 + y, 0, 146, 14);
	PutPixel(76 + x, 56 + y, 0, 146, 14);
	PutPixel(77 + x, 56 + y, 0, 146, 14);
	PutPixel(78 + x, 56 + y, 0, 146, 14);
	PutPixel(79 + x, 56 + y, 0, 25, 2);
	PutPixel(80 + x, 56 + y, 0, 146, 14);
	PutPixel(81 + x, 56 + y, 0, 146, 14);
	PutPixel(82 + x, 56 + y, 0, 146, 14);
	PutPixel(83 + x, 56 + y, 0, 146, 14);
	PutPixel(0 + x, 57 + y, 0, 146, 14);
	PutPixel(1 + x, 57 + y, 0, 146, 14);
	PutPixel(9 + x, 57 + y, 0, 25, 2);
	PutPixel(10 + x, 57 + y, 0, 146, 14);
	PutPixel(11 + x, 57 + y, 0, 146, 14);
	PutPixel(15 + x, 57 + y, 0, 146, 14);
	PutPixel(16 + x, 57 + y, 0, 146, 14);
	PutPixel(17 + x, 57 + y, 0, 118, 11);
	PutPixel(19 + x, 57 + y, 0, 146, 14);
	PutPixel(20 + x, 57 + y, 0, 146, 14);
	PutPixel(21 + x, 57 + y, 0, 146, 14);
	PutPixel(22 + x, 57 + y, 0, 146, 14);
	PutPixel(23 + x, 57 + y, 0, 146, 14);
	PutPixel(24 + x, 57 + y, 0, 146, 14);
	PutPixel(25 + x, 57 + y, 0, 146, 14);
	PutPixel(26 + x, 57 + y, 0, 146, 14);
	PutPixel(28 + x, 57 + y, 0, 146, 14);
	PutPixel(29 + x, 57 + y, 0, 146, 14);
	PutPixel(30 + x, 57 + y, 0, 146, 14);
	PutPixel(47 + x, 57 + y, 0, 146, 14);
	PutPixel(48 + x, 57 + y, 0, 146, 14);
	PutPixel(49 + x, 57 + y, 0, 146, 14);
	PutPixel(54 + x, 57 + y, 0, 146, 14);
	PutPixel(55 + x, 57 + y, 0, 146, 14);
	PutPixel(57 + x, 57 + y, 0, 146, 14);
	PutPixel(58 + x, 57 + y, 0, 146, 14);
	PutPixel(63 + x, 57 + y, 0, 146, 14);
	PutPixel(64 + x, 57 + y, 0, 146, 14);
	PutPixel(65 + x, 57 + y, 0, 25, 2);
	PutPixel(66 + x, 57 + y, 0, 146, 14);
	PutPixel(67 + x, 57 + y, 0, 146, 14);
	PutPixel(68 + x, 57 + y, 0, 146, 14);
	PutPixel(76 + x, 57 + y, 0, 146, 14);
	PutPixel(77 + x, 57 + y, 0, 146, 14);
	PutPixel(82 + x, 57 + y, 0, 146, 14);
	PutPixel(83 + x, 57 + y, 0, 146, 14);
	PutPixel(0 + x, 58 + y, 0, 146, 14);
	PutPixel(1 + x, 58 + y, 0, 146, 14);
	PutPixel(9 + x, 58 + y, 0, 146, 14);
	PutPixel(10 + x, 58 + y, 0, 146, 14);
	PutPixel(11 + x, 58 + y, 0, 118, 11);
	PutPixel(15 + x, 58 + y, 0, 25, 2);
	PutPixel(16 + x, 58 + y, 0, 146, 14);
	PutPixel(17 + x, 58 + y, 0, 146, 14);
	PutPixel(19 + x, 58 + y, 0, 146, 14);
	PutPixel(20 + x, 58 + y, 0, 146, 14);
	PutPixel(21 + x, 58 + y, 0, 146, 14);
	PutPixel(22 + x, 58 + y, 0, 146, 14);
	PutPixel(23 + x, 58 + y, 0, 146, 14);
	PutPixel(24 + x, 58 + y, 0, 146, 14);
	PutPixel(25 + x, 58 + y, 0, 146, 14);
	PutPixel(26 + x, 58 + y, 0, 146, 14);
	PutPixel(28 + x, 58 + y, 0, 146, 14);
	PutPixel(29 + x, 58 + y, 0, 146, 14);
	PutPixel(30 + x, 58 + y, 0, 146, 14);
	PutPixel(47 + x, 58 + y, 0, 146, 14);
	PutPixel(48 + x, 58 + y, 0, 146, 14);
	PutPixel(49 + x, 58 + y, 0, 146, 14);
	PutPixel(54 + x, 58 + y, 0, 146, 14);
	PutPixel(55 + x, 58 + y, 0, 146, 14);
	PutPixel(57 + x, 58 + y, 0, 146, 14);
	PutPixel(58 + x, 58 + y, 0, 146, 14);
	PutPixel(63 + x, 58 + y, 0, 146, 14);
	PutPixel(64 + x, 58 + y, 0, 146, 14);
	PutPixel(65 + x, 58 + y, 0, 25, 2);
	PutPixel(66 + x, 58 + y, 0, 146, 14);
	PutPixel(67 + x, 58 + y, 0, 146, 14);
	PutPixel(68 + x, 58 + y, 0, 146, 14);
	PutPixel(76 + x, 58 + y, 0, 146, 14);
	PutPixel(77 + x, 58 + y, 0, 146, 14);
	PutPixel(82 + x, 58 + y, 0, 146, 14);
	PutPixel(83 + x, 58 + y, 0, 146, 14);
	PutPixel(0 + x, 59 + y, 0, 146, 14);
	PutPixel(1 + x, 59 + y, 0, 146, 14);
	PutPixel(5 + x, 59 + y, 0, 146, 14);
	PutPixel(6 + x, 59 + y, 0, 146, 14);
	PutPixel(7 + x, 59 + y, 0, 146, 14);
	PutPixel(9 + x, 59 + y, 0, 146, 14);
	PutPixel(10 + x, 59 + y, 0, 146, 14);
	PutPixel(11 + x, 59 + y, 0, 25, 2);
	PutPixel(15 + x, 59 + y, 0, 70, 6);
	PutPixel(16 + x, 59 + y, 0, 146, 14);
	PutPixel(17 + x, 59 + y, 0, 146, 14);
	PutPixel(19 + x, 59 + y, 0, 146, 14);
	PutPixel(20 + x, 59 + y, 0, 146, 14);
	PutPixel(22 + x, 59 + y, 0, 146, 14);
	PutPixel(23 + x, 59 + y, 0, 146, 14);
	PutPixel(24 + x, 59 + y, 0, 25, 2);
	PutPixel(25 + x, 59 + y, 0, 146, 14);
	PutPixel(26 + x, 59 + y, 0, 146, 14);
	PutPixel(28 + x, 59 + y, 0, 146, 14);
	PutPixel(29 + x, 59 + y, 0, 146, 14);
	PutPixel(30 + x, 59 + y, 0, 146, 14);
	PutPixel(31 + x, 59 + y, 0, 146, 14);
	PutPixel(32 + x, 59 + y, 0, 146, 14);
	PutPixel(33 + x, 59 + y, 0, 146, 14);
	PutPixel(34 + x, 59 + y, 0, 146, 14);
	PutPixel(35 + x, 59 + y, 0, 146, 14);
	PutPixel(47 + x, 59 + y, 0, 146, 14);
	PutPixel(48 + x, 59 + y, 0, 146, 14);
	PutPixel(49 + x, 59 + y, 0, 146, 14);
	PutPixel(54 + x, 59 + y, 0, 146, 14);
	PutPixel(55 + x, 59 + y, 0, 146, 14);
	PutPixel(57 + x, 59 + y, 0, 146, 14);
	PutPixel(58 + x, 59 + y, 0, 146, 14);
	PutPixel(59 + x, 59 + y, 0, 146, 14);
	PutPixel(63 + x, 59 + y, 0, 146, 14);
	PutPixel(64 + x, 59 + y, 0, 146, 14);
	PutPixel(65 + x, 59 + y, 0, 25, 2);
	PutPixel(66 + x, 59 + y, 0, 146, 14);
	PutPixel(67 + x, 59 + y, 0, 146, 14);
	PutPixel(68 + x, 59 + y, 0, 146, 14);
	PutPixel(69 + x, 59 + y, 0, 146, 14);
	PutPixel(70 + x, 59 + y, 0, 146, 14);
	PutPixel(71 + x, 59 + y, 0, 146, 14);
	PutPixel(72 + x, 59 + y, 0, 146, 14);
	PutPixel(73 + x, 59 + y, 0, 146, 14);
	PutPixel(76 + x, 59 + y, 0, 146, 14);
	PutPixel(77 + x, 59 + y, 0, 146, 14);
	PutPixel(82 + x, 59 + y, 0, 146, 14);
	PutPixel(83 + x, 59 + y, 0, 146, 14);
	PutPixel(0 + x, 60 + y, 0, 146, 14);
	PutPixel(1 + x, 60 + y, 0, 146, 14);
	PutPixel(6 + x, 60 + y, 0, 146, 14);
	PutPixel(7 + x, 60 + y, 0, 146, 14);
	PutPixel(9 + x, 60 + y, 0, 146, 14);
	PutPixel(10 + x, 60 + y, 0, 146, 14);
	PutPixel(11 + x, 60 + y, 0, 146, 14);
	PutPixel(12 + x, 60 + y, 0, 146, 14);
	PutPixel(13 + x, 60 + y, 0, 146, 14);
	PutPixel(14 + x, 60 + y, 0, 146, 14);
	PutPixel(15 + x, 60 + y, 0, 146, 14);
	PutPixel(16 + x, 60 + y, 0, 146, 14);
	PutPixel(17 + x, 60 + y, 0, 146, 14);
	PutPixel(19 + x, 60 + y, 0, 146, 14);
	PutPixel(20 + x, 60 + y, 0, 146, 14);
	PutPixel(25 + x, 60 + y, 0, 146, 14);
	PutPixel(26 + x, 60 + y, 0, 146, 14);
	PutPixel(28 + x, 60 + y, 0, 146, 14);
	PutPixel(29 + x, 60 + y, 0, 146, 14);
	PutPixel(30 + x, 60 + y, 0, 146, 14);
	PutPixel(47 + x, 60 + y, 0, 146, 14);
	PutPixel(48 + x, 60 + y, 0, 146, 14);
	PutPixel(49 + x, 60 + y, 0, 146, 14);
	PutPixel(54 + x, 60 + y, 0, 146, 14);
	PutPixel(55 + x, 60 + y, 0, 146, 14);
	PutPixel(57 + x, 60 + y, 0, 146, 14);
	PutPixel(58 + x, 60 + y, 0, 146, 14);
	PutPixel(59 + x, 60 + y, 0, 146, 14);
	PutPixel(60 + x, 60 + y, 0, 146, 14);
	PutPixel(61 + x, 60 + y, 0, 146, 14);
	PutPixel(62 + x, 60 + y, 0, 146, 14);
	PutPixel(63 + x, 60 + y, 0, 146, 14);
	PutPixel(64 + x, 60 + y, 0, 146, 14);
	PutPixel(66 + x, 60 + y, 0, 146, 14);
	PutPixel(67 + x, 60 + y, 0, 146, 14);
	PutPixel(68 + x, 60 + y, 0, 146, 14);
	PutPixel(76 + x, 60 + y, 0, 146, 14);
	PutPixel(77 + x, 60 + y, 0, 146, 14);
	PutPixel(78 + x, 60 + y, 0, 146, 14);
	PutPixel(80 + x, 60 + y, 0, 146, 14);
	PutPixel(81 + x, 60 + y, 0, 146, 14);
	PutPixel(82 + x, 60 + y, 0, 146, 14);
	PutPixel(83 + x, 60 + y, 0, 146, 14);
	PutPixel(0 + x, 61 + y, 0, 146, 14);
	PutPixel(1 + x, 61 + y, 0, 146, 14);
	PutPixel(6 + x, 61 + y, 0, 146, 14);
	PutPixel(7 + x, 61 + y, 0, 146, 14);
	PutPixel(9 + x, 61 + y, 0, 146, 14);
	PutPixel(10 + x, 61 + y, 0, 146, 14);
	PutPixel(11 + x, 61 + y, 0, 146, 14);
	PutPixel(12 + x, 61 + y, 0, 146, 14);
	PutPixel(13 + x, 61 + y, 0, 146, 14);
	PutPixel(14 + x, 61 + y, 0, 146, 14);
	PutPixel(15 + x, 61 + y, 0, 146, 14);
	PutPixel(16 + x, 61 + y, 0, 146, 14);
	PutPixel(17 + x, 61 + y, 0, 146, 14);
	PutPixel(19 + x, 61 + y, 0, 146, 14);
	PutPixel(20 + x, 61 + y, 0, 146, 14);
	PutPixel(25 + x, 61 + y, 0, 146, 14);
	PutPixel(26 + x, 61 + y, 0, 146, 14);
	PutPixel(28 + x, 61 + y, 0, 146, 14);
	PutPixel(29 + x, 61 + y, 0, 146, 14);
	PutPixel(30 + x, 61 + y, 0, 146, 14);
	PutPixel(47 + x, 61 + y, 0, 146, 14);
	PutPixel(48 + x, 61 + y, 0, 146, 14);
	PutPixel(49 + x, 61 + y, 0, 146, 14);
	PutPixel(54 + x, 61 + y, 0, 146, 14);
	PutPixel(55 + x, 61 + y, 0, 146, 14);
	PutPixel(58 + x, 61 + y, 0, 146, 14);
	PutPixel(59 + x, 61 + y, 0, 146, 14);
	PutPixel(60 + x, 61 + y, 0, 146, 14);
	PutPixel(61 + x, 61 + y, 0, 146, 14);
	PutPixel(62 + x, 61 + y, 0, 146, 14);
	PutPixel(63 + x, 61 + y, 0, 146, 14);
	PutPixel(66 + x, 61 + y, 0, 146, 14);
	PutPixel(67 + x, 61 + y, 0, 146, 14);
	PutPixel(68 + x, 61 + y, 0, 146, 14);
	PutPixel(76 + x, 61 + y, 0, 146, 14);
	PutPixel(77 + x, 61 + y, 0, 146, 14);
	PutPixel(78 + x, 61 + y, 0, 146, 14);
	PutPixel(79 + x, 61 + y, 0, 146, 14);
	PutPixel(80 + x, 61 + y, 0, 146, 14);
	PutPixel(81 + x, 61 + y, 0, 146, 14);
	PutPixel(82 + x, 61 + y, 0, 146, 14);
	PutPixel(0 + x, 62 + y, 0, 142, 13);
	PutPixel(1 + x, 62 + y, 0, 146, 14);
	PutPixel(2 + x, 62 + y, 0, 146, 14);
	PutPixel(6 + x, 62 + y, 0, 146, 14);
	PutPixel(7 + x, 62 + y, 0, 146, 14);
	PutPixel(9 + x, 62 + y, 0, 146, 14);
	PutPixel(10 + x, 62 + y, 0, 146, 14);
	PutPixel(11 + x, 62 + y, 0, 146, 14);
	PutPixel(15 + x, 62 + y, 0, 70, 6);
	PutPixel(16 + x, 62 + y, 0, 146, 14);
	PutPixel(17 + x, 62 + y, 0, 146, 14);
	PutPixel(19 + x, 62 + y, 0, 146, 14);
	PutPixel(20 + x, 62 + y, 0, 146, 14);
	PutPixel(25 + x, 62 + y, 0, 146, 14);
	PutPixel(26 + x, 62 + y, 0, 146, 14);
	PutPixel(28 + x, 62 + y, 0, 146, 14);
	PutPixel(29 + x, 62 + y, 0, 146, 14);
	PutPixel(30 + x, 62 + y, 0, 146, 14);
	PutPixel(47 + x, 62 + y, 0, 146, 14);
	PutPixel(48 + x, 62 + y, 0, 146, 14);
	PutPixel(49 + x, 62 + y, 0, 146, 14);
	PutPixel(53 + x, 62 + y, 0, 3, 0);
	PutPixel(54 + x, 62 + y, 0, 146, 14);
	PutPixel(55 + x, 62 + y, 0, 146, 14);
	PutPixel(59 + x, 62 + y, 0, 146, 14);
	PutPixel(60 + x, 62 + y, 0, 146, 14);
	PutPixel(61 + x, 62 + y, 0, 146, 14);
	PutPixel(62 + x, 62 + y, 0, 146, 14);
	PutPixel(66 + x, 62 + y, 0, 146, 14);
	PutPixel(67 + x, 62 + y, 0, 146, 14);
	PutPixel(68 + x, 62 + y, 0, 146, 14);
	PutPixel(76 + x, 62 + y, 0, 146, 14);
	PutPixel(77 + x, 62 + y, 0, 146, 14);
	PutPixel(80 + x, 62 + y, 0, 146, 14);
	PutPixel(81 + x, 62 + y, 0, 146, 14);
	PutPixel(82 + x, 62 + y, 0, 146, 14);
	PutPixel(83 + x, 62 + y, 0, 70, 6);
	PutPixel(1 + x, 63 + y, 0, 142, 13);
	PutPixel(2 + x, 63 + y, 0, 146, 14);
	PutPixel(3 + x, 63 + y, 0, 146, 14);
	PutPixel(4 + x, 63 + y, 0, 146, 14);
	PutPixel(5 + x, 63 + y, 0, 146, 14);
	PutPixel(6 + x, 63 + y, 0, 146, 14);
	PutPixel(7 + x, 63 + y, 0, 146, 14);
	PutPixel(9 + x, 63 + y, 0, 146, 14);
	PutPixel(10 + x, 63 + y, 0, 146, 14);
	PutPixel(11 + x, 63 + y, 0, 146, 14);
	PutPixel(15 + x, 63 + y, 0, 25, 2);
	PutPixel(16 + x, 63 + y, 0, 146, 14);
	PutPixel(17 + x, 63 + y, 0, 146, 14);
	PutPixel(19 + x, 63 + y, 0, 146, 14);
	PutPixel(20 + x, 63 + y, 0, 146, 14);
	PutPixel(25 + x, 63 + y, 0, 146, 14);
	PutPixel(26 + x, 63 + y, 0, 146, 14);
	PutPixel(28 + x, 63 + y, 0, 146, 14);
	PutPixel(29 + x, 63 + y, 0, 146, 14);
	PutPixel(30 + x, 63 + y, 0, 146, 14);
	PutPixel(31 + x, 63 + y, 0, 146, 14);
	PutPixel(32 + x, 63 + y, 0, 146, 14);
	PutPixel(33 + x, 63 + y, 0, 146, 14);
	PutPixel(34 + x, 63 + y, 0, 146, 14);
	PutPixel(35 + x, 63 + y, 0, 146, 14);
	PutPixel(36 + x, 63 + y, 0, 146, 14);
	PutPixel(48 + x, 63 + y, 0, 146, 14);
	PutPixel(49 + x, 63 + y, 0, 146, 14);
	PutPixel(50 + x, 63 + y, 0, 146, 14);
	PutPixel(51 + x, 63 + y, 0, 146, 14);
	PutPixel(52 + x, 63 + y, 0, 146, 14);
	PutPixel(53 + x, 63 + y, 0, 146, 14);
	PutPixel(54 + x, 63 + y, 0, 146, 14);
	PutPixel(60 + x, 63 + y, 0, 146, 14);
	PutPixel(61 + x, 63 + y, 0, 146, 14);
	PutPixel(66 + x, 63 + y, 0, 146, 14);
	PutPixel(67 + x, 63 + y, 0, 146, 14);
	PutPixel(68 + x, 63 + y, 0, 146, 14);
	PutPixel(69 + x, 63 + y, 0, 146, 14);
	PutPixel(70 + x, 63 + y, 0, 146, 14);
	PutPixel(71 + x, 63 + y, 0, 146, 14);
	PutPixel(72 + x, 63 + y, 0, 146, 14);
	PutPixel(73 + x, 63 + y, 0, 146, 14);
	PutPixel(74 + x, 63 + y, 0, 146, 14);
	PutPixel(76 + x, 63 + y, 0, 146, 14);
	PutPixel(77 + x, 63 + y, 0, 146, 14);
	PutPixel(82 + x, 63 + y, 0, 146, 14);
	PutPixel(83 + x, 63 + y, 0, 146, 14);

}


//////////////////////////////////////////////////
//           Graphics Exception
Graphics::Exception::Exception( HRESULT hr,const std::wstring& note,const wchar_t* file,unsigned int line )
	:
	ChiliException( file,line,note ),
	hr( hr )
{}

std::wstring Graphics::Exception::GetFullMessage() const
{
	const std::wstring empty = L"";
	const std::wstring errorName = GetErrorName();
	const std::wstring errorDesc = GetErrorDescription();
	const std::wstring& note = GetNote();
	const std::wstring location = GetLocation();
	return    (!errorName.empty() ? std::wstring( L"Error: " ) + errorName + L"\n"
		: empty)
		+ (!errorDesc.empty() ? std::wstring( L"Description: " ) + errorDesc + L"\n"
			: empty)
		+ (!note.empty() ? std::wstring( L"Note: " ) + note + L"\n"
			: empty)
		+ (!location.empty() ? std::wstring( L"Location: " ) + location
			: empty);
}

std::wstring Graphics::Exception::GetErrorName() const
{
	return DXGetErrorString( hr );
}

std::wstring Graphics::Exception::GetErrorDescription() const
{
	std::array<wchar_t,512> wideDescription;
	DXGetErrorDescription( hr,wideDescription.data(),wideDescription.size() );
	return wideDescription.data();
}

std::wstring Graphics::Exception::GetExceptionType() const
{
	return L"Chili Graphics Exception";
}