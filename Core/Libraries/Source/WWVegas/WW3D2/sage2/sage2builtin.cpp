/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Tiberium Project @feature The built-in approximation of the C&C4 object look (see sage2builtin.h)

#include "sage2builtin.h"

#include <map>
#include <string.h>

#include "assetmgr.h"
#include "damagelook.h"
#include "dx8caps.h"
#include "dx8wrapper.h"
#include "mesh.h"
#include "meshmdl.h"
#include "texture.h"
#include "vertmaterial.h"
#include "WWLib/ffactory.h"
#include "WWLib/wwstring.h"

#include "../shaders/sage2object_vs.h"	// from shaders/sage2object.hlsl (shaders/build_shaders.py)
#include "../shaders/sage2object_ps.h"

namespace
{
	enum InitState { NOT_TRIED, READY, UNAVAILABLE };

	InitState					s_state = NOT_TRIED;
	IDirect3DVertexShader9*	s_vertexShader = nullptr;
	DWORD						s_pixelShader = 0;

	// Texture stages the shader reads besides W3D's own two
	const unsigned NOISE_STAGE		= 2;
	const unsigned NORMAL_STAGE	= 3;
	const unsigned SPEC_STAGE		= 4;

	// Look tuning (C&C4's specular maps are faint; these bring them up to its shine)
	const float SHINE_STRENGTH		= 2.5f;
	const float SHINE_POWER			= 24.0f;
	const float REFLECTION_STRENGTH	= 0.6f;
	const float SELF_ILLUMINATION	= 0.8f;
	const float NORMAL_STRENGTH		= 1.0f;

	struct Maps
	{
		TextureClass* normal;
		TextureClass* spec;
	};
	// base texture -> its normal / specular maps (normal == nullptr: none, drawn the old way). The base
	// texture is held, so its address can't be reused by another texture while it is a key here.
	std::map<TextureClass*, Maps> s_maps;

	bool File_Exists(const char* name)
	{
		file_auto_ptr file(_TheFileFactory, name);
		return file->Is_Available();
	}

	TextureClass* Load_Sibling(const StringClass& baseName, const char* suffix)
	{
		StringClass name = baseName;
		const char* dot = strrchr(name.Peek_Buffer(), '.');
		if (dot != nullptr)
			name.Erase((int)(dot - name.Peek_Buffer()), name.Get_Length() - (int)(dot - name.Peek_Buffer()));
		name += suffix;
		name += ".dds";
		if (!File_Exists(name))
			return nullptr;
		return WW3DAssetManager::Get_Instance()->Get_Texture(name);
	}

	const Maps& Find_Maps(TextureClass* base)
	{
		std::map<TextureClass*, Maps>::iterator it = s_maps.find(base);
		if (it != s_maps.end())
			return it->second;
		Maps maps;
		maps.normal = Load_Sibling(base->Get_Texture_Name(), "_NRM");
		maps.spec = maps.normal ? Load_Sibling(base->Get_Texture_Name(), "_SPM") : nullptr;
		base->Add_Ref();
		return s_maps.insert(std::make_pair(base, maps)).first->second;
	}

	bool Init()
	{
		s_state = UNAVAILABLE;
		IDirect3DDevice8* device = DX8Wrapper::_Get_D3D_Device8();
		if (device == nullptr
			|| DX8Wrapper::Get_Current_Caps()->Get_Pixel_Shader_Major_Version() < 3
			|| DX8Wrapper::Get_Current_Caps()->Get_Vertex_Shader_Major_Version() < 3)
			return false;
		if (FAILED(device->Get_D3D9()->CreateVertexShader(Sage2ObjectVertexShader, &s_vertexShader)))
		{
			s_vertexShader = nullptr;
			return false;
		}
		if (FAILED(device->CreatePixelShader(Sage2ObjectPixelShader, &s_pixelShader)))
		{
			s_pixelShader = 0;
			return false;
		}
		s_state = READY;
		return true;
	}

	Vector4 Color_To_Vector(D3DCOLOR c)
	{
		return Vector4(((c >> 16) & 0xFF) / 255.0f, ((c >> 8) & 0xFF) / 255.0f, (c & 0xFF) / 255.0f, 1.0f);
	}

	float Bits_To_Float(unsigned bits)
	{
		float f;
		memcpy(&f, &bits, sizeof(f));
		return f;
	}
}

//-------------------------------------------------------------------------------------------------
bool Sage2BuiltinMaterialClass::Begin(MeshClass* mesh, TextureClass* base, TextureClass* damageSheet, const VertexMaterialClass* material)
{
	if (mesh == nullptr || base == nullptr || material == nullptr || !material->Get_Lighting())
		return false;
	MeshModelClass* model = mesh->Peek_Model();
	if (model == nullptr || !model->Needs_Vertex_Normals() || model->Get_UV_Array_Count() < 1)
		return false;

	if (s_state == NOT_TRIED)
		Init();
	if (s_state != READY)
		return false;

	const Maps& maps = Find_Maps(base);
	if (maps.normal == nullptr)
		return false;

	//--------------------------------------------------------------------------- vertex constants
	Matrix4x4 world, view, projection;
	DX8Wrapper::Get_Transform(D3DTS_WORLD, world);
	DX8Wrapper::Get_Transform(D3DTS_VIEW, view);
	DX8Wrapper::Get_Transform(D3DTS_PROJECTION, projection);
	Matrix4x4 worldView = view * world;

	float fog[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	if (DX8Wrapper::Get_Fog_Enable())
	{
		float start = Bits_To_Float(DX8Wrapper::Get_DX8_Render_State(D3DRS_FOGSTART));
		float end = Bits_To_Float(DX8Wrapper::Get_DX8_Render_State(D3DRS_FOGEND));
		if (end > start)
		{
			fog[0] = start;
			fog[1] = 1.0f / (end - start);
			fog[2] = 1.0f;
		}
	}
	DX8Wrapper::Set_Vertex_Shader_Constant(0, &worldView, 4);
	DX8Wrapper::Set_Vertex_Shader_Constant(4, &projection, 4);
	DX8Wrapper::Set_Vertex_Shader_Constant(8, &world, 4);
	DX8Wrapper::Set_Vertex_Shader_Constant(12, fog, 1);

	//---------------------------------------------------------------------------- pixel constants
	Vector3 matAmbient, matDiffuse, matEmissive;
	material->Get_Ambient(&matAmbient);
	material->Get_Diffuse(&matDiffuse);
	material->Get_Emissive(&matEmissive);

	Vector4 c[20];
	memset(c, 0, sizeof(c));
	Vector4 sceneAmbient = Color_To_Vector(DX8Wrapper::Get_DX8_Render_State(D3DRS_AMBIENT));
	c[0] = Vector4(sceneAmbient.X * matAmbient.X + matEmissive.X,
		sceneAmbient.Y * matAmbient.Y + matEmissive.Y,
		sceneAmbient.Z * matAmbient.Z + matEmissive.Z, 1.0f);
	for (int l = 0; l < 4; ++l)
	{
		if (!DX8Wrapper::Is_Light_Enabled(l))
			continue;
		const D3DLIGHT8& light = DX8Wrapper::Peek_Light(l);
		if (light.Type == D3DLIGHT_DIRECTIONAL)
		{
			Vector3 toLight(-light.Direction.x, -light.Direction.y, -light.Direction.z);
			toLight.Normalize();
			c[1 + l] = Vector4(toLight.X, toLight.Y, toLight.Z, 0.0f);
		}
		else
		{
			c[1 + l] = Vector4(light.Position.x, light.Position.y, light.Position.z, 1.0f);
			c[9 + l] = Vector4(light.Attenuation0, light.Attenuation1, light.Attenuation2, light.Range);
		}
		c[5 + l] = Vector4(light.Diffuse.r * matDiffuse.X, light.Diffuse.g * matDiffuse.Y, light.Diffuse.b * matDiffuse.Z, 1.0f);
	}
	Matrix4x4 viewInverse = view.Inverse();
	c[13] = Vector4(viewInverse[0][3], viewInverse[1][3], viewInverse[2][3], 1.0f);
	c[14] = Vector4(SHINE_STRENGTH, SHINE_POWER, REFLECTION_STRENGTH, SELF_ILLUMINATION);
	Vector4 fogColor = Color_To_Vector(DX8Wrapper::Get_Fog_Color());
	c[15] = Vector4(fogColor.X, fogColor.Y, fogColor.Z, material->Get_Opacity());

	float damage[3][4];
	bool damaged = damageSheet != nullptr && model->Get_UV_Array_Count() >= 2
		&& DamageLookClass::Get_Constants(DamageLookClass::Get_Amount(mesh), damage)
		&& DamageLookClass::Bind_Noise(NOISE_STAGE);
	if (damaged)
		memcpy(&c[16], damage, sizeof(damage));
	else
		c[16].X = -1.0f;	// no damage
	c[19] = Vector4(NORMAL_STRENGTH, damaged ? 1.0f : 0.0f, 0.0f, 0.0f);
	// the wrapper caches only the first MAX_PIXEL_SHADER_CONSTANTS registers: set those through it, the
	// rest directly
	DX8Wrapper::Set_Pixel_Shader_Constant(0, c, MAX_PIXEL_SHADER_CONSTANTS);
	DX8Wrapper::_Get_D3D_Device8()->Get_D3D9()->SetPixelShaderConstantF(MAX_PIXEL_SHADER_CONSTANTS,
		&c[MAX_PIXEL_SHADER_CONSTANTS].X, 20 - MAX_PIXEL_SHADER_CONSTANTS);

	//----------------------------------------------------------------------------------- textures
	maps.normal->Apply(NORMAL_STAGE);
	if (maps.spec != nullptr)
		maps.spec->Apply(SPEC_STAGE);
	else
		TextureBaseClass::Apply_Null(SPEC_STAGE);

	DX8Wrapper::Set_Pixel_Shader(s_pixelShader);
	DX8Wrapper::_Get_D3D_Device8()->Set_Vertex_Shader_Override(s_vertexShader);
	return true;
}

//-------------------------------------------------------------------------------------------------
void Sage2BuiltinMaterialClass::End()
{
	DX8Wrapper::_Get_D3D_Device8()->Set_Vertex_Shader_Override(nullptr);
	DX8Wrapper::Set_Pixel_Shader(0);
	TextureBaseClass::Apply_Null(NORMAL_STAGE);
	TextureBaseClass::Apply_Null(SPEC_STAGE);
	DX8Wrapper::Set_DX8_Texture(NOISE_STAGE, nullptr);
}

//-------------------------------------------------------------------------------------------------
void Sage2BuiltinMaterialClass::Shutdown()
{
	for (std::map<TextureClass*, Maps>::iterator it = s_maps.begin(); it != s_maps.end(); ++it)
	{
		REF_PTR_RELEASE(it->second.normal);
		REF_PTR_RELEASE(it->second.spec);
		it->first->Release_Ref();
	}
	s_maps.clear();

	IDirect3DDevice8* device = DX8Wrapper::_Get_D3D_Device8();
	if (device != nullptr)
	{
		device->Set_Vertex_Shader_Override(nullptr);
		if (s_pixelShader != 0)
		{
			DX8Wrapper::Set_Pixel_Shader(0);
			device->DeletePixelShader(s_pixelShader);
		}
	}
	if (s_vertexShader != nullptr)
		s_vertexShader->Release();
	s_vertexShader = nullptr;
	s_pixelShader = 0;
	s_state = NOT_TRIED;
}
