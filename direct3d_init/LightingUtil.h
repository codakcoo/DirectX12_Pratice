#pragma once
#include <DirectXMath.h>
#include "MathHelper.h"

#define MaxLights 16

struct Light
{
	DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
	float FalloffStart = 1.0f;									// Á¡±¤/½ºÆÌ±¤¿ë
	DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };		// µð·º¼Å³Î/½ºÆÌ±¤¿ë
	float FalloffEnd = 10.0f;									// Á¡±¤/½ºÆÌ±¤¿ë
	DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };			// Á¡±¤/½ºÆÌ±¤¿ë
	float SpotPower = 64.0f;									// ½ºÆÌ±¤¿ë
};

struct MaterialConstants
{
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 0.25f;
};
