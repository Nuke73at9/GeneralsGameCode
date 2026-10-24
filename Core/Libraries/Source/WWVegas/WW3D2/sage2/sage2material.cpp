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

// Tiberium Project @feature Meshes drawn with their SAGE 2 effect (see sage2material.h)

#include "sage2material.h"

#include <ctype.h>
#include <map>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <vector>

#include "assetmgr.h"
#include "damagelook.h"
#include "dx8caps.h"
#include "dx8polygonrenderer.h"
#include "dx8vertexbuffer.h"
#include "dx8wrapper.h"
#include "htree.h"
#include "mesh.h"
#include "meshmdl.h"
#include "sage2builtin.h"
#include "sage2effect.h"
#include "sage2globals.h"
#include "sage2scene.h"
#include "sage2shadow.h"
#include "texture.h"
#include "textureloader.h"
#include "vertmaterial.h"
#include "WWLib/ffactory.h"
#include "WWLib/WWFILE.h"

namespace
{
	enum InitState { NOT_TRIED, READY, UNAVAILABLE };
	InitState s_state = NOT_TRIED;

	const char* EFFECT_DIRECTORY = "Art\\Shaders\\";
	const unsigned SIDE_STREAM = 1;
	const unsigned SIDE_STRIDE = 32;	// tangent 3, binormal 3, blend indices 4 bytes, colour 4 bytes

	//------------------------------------------------------------------------------------ effects
	struct EffectEntry
	{
		Sage2EffectClass* effect;	// nullptr: the file is missing or broken
	};
	std::map<std::string, EffectEntry> s_effects;

	std::string Lower(const char* s)
	{
		std::string out(s);
		for (size_t i = 0; i < out.size(); ++i)
			out[i] = (char)tolower((unsigned char)out[i]);
		return out;
	}

	Sage2EffectClass* Load_Effect(const std::string& name)
	{
		std::map<std::string, EffectEntry>::iterator it = s_effects.find(name);
		if (it != s_effects.end())
			return it->second.effect;
		EffectEntry entry;
		entry.effect = nullptr;
		std::string path = EFFECT_DIRECTORY + name + ".fxo";
		file_auto_ptr file(_TheFileFactory, path.c_str());
		if (file->Is_Available() && file->Open())
		{
			std::vector<unsigned char> data((size_t)file->Size());
			if (!data.empty() && file->Read(&data[0], (int)data.size()) == (int)data.size())
			{
				Sage2EffectClass* effect = new Sage2EffectClass;
				if (effect->Load(DX8Wrapper::_Get_D3D_Device8()->Get_D3D9(), &data[0], data.size(), name.c_str()))
				{
					Sage2GlobalsClass::Resolve_All(*effect);
					entry.effect = effect;
					Sage2_Log("effect %s: loaded, %d parameters, %d techniques", path.c_str(), effect->Get_Param_Count(), effect->Get_Technique_Count());
				}
				else
				{
					Sage2_Log("effect %s: %s", path.c_str(), effect->Get_Error().c_str());
					delete effect;
				}
			}
			else
				Sage2_Log("effect %s: could not read %u bytes", path.c_str(), (unsigned)data.size());
			file->Close();
		}
		else
		{
			Sage2_Log("effect %s: not found", path.c_str());
		}
		s_effects[name] = entry;
		return entry.effect;
	}

	//---------------------------------------------------------------------------- mesh materials
	struct MaterialTexture { int param; TextureClass* texture; };
	struct MaterialValue { int param; std::vector<float> values; };
	struct MeshMaterial
	{
		Sage2EffectClass* effect;
		int technique;			// the material's technique (deferred-lit when it is a High one)
		int forwardTechnique;	// its "_M" variant (per-pixel lighting without the deferred light buffers), or the same
		int prepassTechnique;	// _CreateDepthNormalMap, or -1
		int shadowTechnique;	// _CreateShadowMap (the mesh as a shadow caster), or -1
		std::vector<MaterialTexture> textures;
		std::vector<MaterialValue> values;
		MeshMaterial() : effect(nullptr), technique(-1), forwardTechnique(-1), prepassTechnique(-1), shadowTechnique(-1) {}
	};
	// mesh model (held) -> its material; a model without one is entered with effect == nullptr
	std::map<MeshModelClass*, MeshMaterial> s_materials;

	void Trim(std::string& s)
	{
		size_t a = 0, b = s.size();
		while (a < b && isspace((unsigned char)s[a])) ++a;
		while (b > a && isspace((unsigned char)s[b - 1])) --b;
		s = s.substr(a, b - a);
	}

	bool Parse_Number_List(const std::string& text, std::vector<float>& out)
	{
		out.clear();
		std::string lower = Lower(text.c_str());
		if (lower == "true" || lower == "yes") { out.push_back(1.0f); return true; }
		if (lower == "false" || lower == "no") { out.push_back(0.0f); return true; }
		const char* p = text.c_str();
		while (*p)
		{
			while (*p && (isspace((unsigned char)*p) || *p == ',' || *p == ';'))
				++p;
			if (!*p)
				break;
			char* end = nullptr;
			float v = (float)strtod(p, &end);
			if (end == p)
				return false;
			out.push_back(v);
			p = end;
		}
		return !out.empty();
	}

	const MeshMaterial& Find_Material(MeshModelClass* model)
	{
		std::map<MeshModelClass*, MeshMaterial>::iterator it = s_materials.find(model);
		if (it != s_materials.end())
			return it->second;
		model->Add_Ref();
		MeshMaterial& mat = s_materials[model];

		const char* text = model->Get_User_Text();
		if (text == nullptr || strstr(text, "Sage2Effect") == nullptr)
			return mat;

		// first pass: the effect
		std::string effectName, techniqueName = "Default";
		std::vector<std::pair<std::string, std::string> > lines;
		const char* p = text;
		while (*p)
		{
			const char* nl = strchr(p, '\n');
			std::string line = nl ? std::string(p, nl - p) : std::string(p);
			p = nl ? nl + 1 : p + strlen(p);
			size_t eq = line.find('=');
			if (eq == std::string::npos)
				continue;
			std::string key = line.substr(0, eq), value = line.substr(eq + 1);
			Trim(key);
			Trim(value);
			if (key == "Sage2Effect")
				effectName = Lower(value.c_str());
			else if (key == "Sage2Technique")
				techniqueName = value;
			else
				lines.push_back(std::make_pair(key, value));
		}
		if (effectName.empty())
			return mat;
		Sage2EffectClass* effect = Load_Effect(effectName);
		if (effect == nullptr)
			return mat;
		int technique = effect->Find_Technique(techniqueName.c_str());
		if (technique < 0)
			technique = effect->Find_Technique("Default");
		if (technique < 0)
			technique = 0;

		// second pass: the material's own parameters
		for (size_t i = 0; i < lines.size(); ++i)
		{
			int param = effect->Find_Param(lines[i].first.c_str());
			if (param < 0)
				continue;
			const Sage2EffectClass::Param& pr = effect->Get_Param(param);
			if (pr.type >= Sage2EffectClass::TYPE_TEXTURE && pr.type <= Sage2EffectClass::TYPE_TEXTURECUBE)
			{
				MaterialTexture mt;
				mt.param = param;
				mt.texture = WW3DAssetManager::Get_Instance()->Get_Texture(lines[i].second.c_str());
				if (mt.texture != nullptr)
					mat.textures.push_back(mt);
			}
			else if (pr.paramClass <= Sage2EffectClass::CLASS_MATRIX_COLS)
			{
				MaterialValue mv;
				mv.param = param;
				if (Parse_Number_List(lines[i].second, mv.values))
					mat.values.push_back(mv);
			}
		}
		mat.effect = effect;
		mat.technique = technique;
		// the High techniques light from the deferred light buffers; until the scene renders those,
		// the Medium ones (forward per-pixel lighting) give the look
		std::string forward = std::string(effect->Get_Technique_Name(technique)) + "_M";
		mat.forwardTechnique = effect->Find_Technique(forward.c_str());
		if (mat.forwardTechnique < 0)
			mat.forwardTechnique = technique;
		mat.prepassTechnique = mat.forwardTechnique != technique ? effect->Find_Technique("_CreateDepthNormalMap") : -1;
		mat.shadowTechnique = effect->Find_Technique("_CreateShadowMap");
		Sage2_Log("mesh %s: %s, technique %s (forward %s, pre-pass %d), %u textures, %u values", model->Get_Name(), effectName.c_str(),
			effect->Get_Technique_Name(technique), effect->Get_Technique_Name(mat.forwardTechnique), mat.prepassTechnique,
			(unsigned)mat.textures.size(), (unsigned)mat.values.size());
		return mat;
	}

	//------------------------------------------------------------------------------ side streams
	// W3D's vertex buffers hold position, normal, colour and texture coordinates. The second
	// stream adds, per vertex of the same shared buffer, the tangent frame and the blend index.
	struct SideVertex
	{
		float tangent[3];
		float binormal[3];
		unsigned char blendIndices[4];
		DWORD color;			// opaque white: the effects read the vertex alpha as "how intact"
	};
	struct SideStream
	{
		IDirect3DVertexBuffer9* buffer;
		unsigned vertexCount;
		std::map<MeshModelClass*, unsigned> filled;	// model -> base vertex offset it was written at
		SideStream() : buffer(nullptr), vertexCount(0) {}
	};
	std::map<VertexBufferClass*, SideStream> s_sideStreams;	// shared vertex buffer (held) -> side stream
	std::map<MeshModelClass*, std::vector<SideVertex> > s_bindFrames;	// model -> its tangent frame at rest
	const unsigned VERTEX_BUFFER_OVERFLOW_OFFSET = 0xffff;	// dx8renderer: the skin did not fit this frame

	void Build_Tangent_Frame(MeshModelClass* model, std::vector<SideVertex>& out)
	{
		int nv = model->Get_Vertex_Count();
		const Vector3* pos = model->Get_Vertex_Array();
		const Vector3* nrm = model->Get_Vertex_Normal_Array();
		const Vector2* uv = model->Get_UV_Array_Count() > 0 ? model->Get_UV_Array(0, 0) : nullptr;
		const TriIndex* tris = model->Get_Polygon_Array();
		int np = model->Get_Polygon_Count();
		std::vector<Vector3> du((size_t)nv, Vector3(0, 0, 0)), dv((size_t)nv, Vector3(0, 0, 0));
		if (uv != nullptr)
		{
			for (int t = 0; t < np; ++t)
			{
				unsigned a = tris[t].I, b = tris[t].J, c = tris[t].K;
				if (a >= (unsigned)nv || b >= (unsigned)nv || c >= (unsigned)nv)
					continue;
				Vector3 e1 = pos[b] - pos[a], e2 = pos[c] - pos[a];
				Vector2 d1 = uv[b] - uv[a], d2 = uv[c] - uv[a];
				float det = d1.X * d2.Y - d2.X * d1.Y;
				if (WWMath::Fabs(det) < 1e-12f)
					continue;
				float r = 1.0f / det;
				Vector3 tu = (e1 * d2.Y - e2 * d1.Y) * r;	// dP/du
				Vector3 tv = (e2 * d1.X - e1 * d2.X) * r;	// dP/dv
				du[a] += tu; du[b] += tu; du[c] += tu;
				dv[a] += tv; dv[b] += tv; dv[c] += tv;
			}
		}
		out.resize((size_t)nv);
		for (int i = 0; i < nv; ++i)
		{
			Vector3 n = nrm ? nrm[i] : Vector3(0, 0, 1);
			// C&C4 art stores dP/dv as the "tangent" and dP/du as the "binormal" (its shaders build
			// the tangent frame as (-binormal, -tangent, normal)); keep to that
			Vector3 t = dv[i] - n * Vector3::Dot_Product(n, dv[i]);
			Vector3 b = du[i] - n * Vector3::Dot_Product(n, du[i]);
			if (t.Length2() < 1e-12f || b.Length2() < 1e-12f)
			{
				// no usable texture mapping: any frame around the normal
				Vector3 helper = WWMath::Fabs(n.Z) < 0.9f ? Vector3(0, 0, 1) : Vector3(1, 0, 0);
				Vector3::Cross_Product(n, helper, &b);
				Vector3::Cross_Product(b, n, &t);
			}
			t.Normalize();
			b.Normalize();
			out[i].tangent[0] = t.X; out[i].tangent[1] = t.Y; out[i].tangent[2] = t.Z;
			out[i].binormal[0] = b.X; out[i].binormal[1] = b.Y; out[i].binormal[2] = b.Z;
			out[i].blendIndices[0] = out[i].blendIndices[1] = out[i].blendIndices[2] = out[i].blendIndices[3] = 0;
			out[i].color = 0xFFFFFFFF;
		}
	}

	const std::vector<SideVertex>& Bind_Frame(MeshModelClass* model)
	{
		std::map<MeshModelClass*, std::vector<SideVertex> >::iterator it = s_bindFrames.find(model);
		if (it == s_bindFrames.end())
		{
			it = s_bindFrames.insert(std::make_pair(model, std::vector<SideVertex>())).first;
			Build_Tangent_Frame(model, it->second);
		}
		return it->second;
	}

	// Skins are deformed on the CPU into a dynamic buffer every frame (world = identity); their
	// tangent frame turns with the bones the same way.
	void Deformed_Frame(MeshModelClass* model, const HTreeClass* htree, std::vector<SideVertex>& out)
	{
		const std::vector<SideVertex>& rest = Bind_Frame(model);
		const uint16* bones = model->Get_Vertex_Bone_Links();
		out = rest;
		if (bones == nullptr || htree == nullptr)
			return;
		for (size_t i = 0; i < out.size(); ++i)
		{
			const Matrix3D& tm = htree->Get_Transform(bones[i]);
			Vector3 t = tm.Rotate_Vector(Vector3(rest[i].tangent[0], rest[i].tangent[1], rest[i].tangent[2]));
			Vector3 b = tm.Rotate_Vector(Vector3(rest[i].binormal[0], rest[i].binormal[1], rest[i].binormal[2]));
			out[i].tangent[0] = t.X; out[i].tangent[1] = t.Y; out[i].tangent[2] = t.Z;
			out[i].binormal[0] = b.X; out[i].binormal[1] = b.Y; out[i].binormal[2] = b.Z;
		}
	}

	/// The side stream for the shared vertex buffer the mesh sits in, with the mesh's vertices
	/// written at its offset (a skin's every frame, with its current pose; a rigid mesh's once).
	IDirect3DVertexBuffer9* Side_Stream_For(VertexBufferClass* vb, MeshModelClass* model, unsigned baseVertex, const HTreeClass* skinTree, bool dynamic)
	{
		if (vb == nullptr)
			return nullptr;
		std::map<VertexBufferClass*, SideStream>::iterator it = s_sideStreams.find(vb);
		if (it == s_sideStreams.end())
		{
			vb->Add_Ref();
			it = s_sideStreams.insert(std::make_pair(vb, SideStream())).first;
		}
		SideStream& side = it->second;
		unsigned count = vb->Get_Vertex_Count();
		if (side.buffer == nullptr || side.vertexCount != count)
		{
			if (side.buffer) side.buffer->Release();
			side.buffer = nullptr;
			side.filled.clear();
			side.vertexCount = count;
			IDirect3DDevice9* device = DX8Wrapper::_Get_D3D_Device8()->Get_D3D9();
			if (FAILED(device->CreateVertexBuffer(count * SIDE_STRIDE, D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &side.buffer, nullptr)))
			{
				side.buffer = nullptr;
				return nullptr;
			}
		}
		unsigned nv = (unsigned)model->Get_Vertex_Count();
		if (baseVertex + nv > count)
			return nullptr;
		std::map<MeshModelClass*, unsigned>::iterator f = side.filled.find(model);
		if (dynamic || f == side.filled.end() || f->second != baseVertex)
		{
			std::vector<SideVertex> frame;
			if (skinTree != nullptr)
				Deformed_Frame(model, skinTree, frame);
			else
				frame = Bind_Frame(model);
			void* p = nullptr;
			if (FAILED(side.buffer->Lock(baseVertex * SIDE_STRIDE, nv * SIDE_STRIDE, &p, 0)))
				return nullptr;
			memcpy(p, &frame[0], nv * SIDE_STRIDE);
			side.buffer->Unlock();
			side.filled[model] = baseVertex;
		}
		return side.buffer;
	}

	//------------------------------------------------------------------------------ declarations
	// one per fixed-function vertex format: its elements on stream 0, the side stream on stream 1
	std::map<DWORD, IDirect3DVertexDeclaration9*> s_declarations;

	IDirect3DVertexDeclaration9* Declaration_For(DWORD fvf)
	{
		std::map<DWORD, IDirect3DVertexDeclaration9*>::iterator it = s_declarations.find(fvf);
		if (it != s_declarations.end())
			return it->second;
		std::vector<D3DVERTEXELEMENT9> el;
		WORD offset = 0;
		D3DVERTEXELEMENT9 e;
		e.Stream = 0; e.Method = D3DDECLMETHOD_DEFAULT; e.UsageIndex = 0;
		DWORD position = fvf & D3DFVF_POSITION_MASK;
		if (position == D3DFVF_XYZ) { e.Offset = offset; e.Type = D3DDECLTYPE_FLOAT3; e.Usage = D3DDECLUSAGE_POSITION; el.push_back(e); offset += 12; }
		else if (position == D3DFVF_XYZRHW) { e.Offset = offset; e.Type = D3DDECLTYPE_FLOAT4; e.Usage = D3DDECLUSAGE_POSITIONT; el.push_back(e); offset += 16; }
		else { e.Offset = offset; e.Type = D3DDECLTYPE_FLOAT3; e.Usage = D3DDECLUSAGE_POSITION; el.push_back(e); offset += 12; offset += (WORD)(4 * ((position - D3DFVF_XYZB1) / 2 + 1)); }
		if (fvf & D3DFVF_NORMAL) { e.Offset = offset; e.Type = D3DDECLTYPE_FLOAT3; e.Usage = D3DDECLUSAGE_NORMAL; el.push_back(e); offset += 12; }
		if (fvf & D3DFVF_PSIZE) { e.Offset = offset; e.Type = D3DDECLTYPE_FLOAT1; e.Usage = D3DDECLUSAGE_PSIZE; el.push_back(e); offset += 4; }
		// W3D's diffuse colour is skipped: the effects read COLOR0's alpha as the damage blend, and
		// that must be 1 whatever W3D put there (the side stream provides it), and a missing input
		// would be whatever the driver defaults to
		if (fvf & D3DFVF_DIFFUSE) offset += 4;
		if (fvf & D3DFVF_SPECULAR) { e.Offset = offset; e.Type = D3DDECLTYPE_D3DCOLOR; e.Usage = D3DDECLUSAGE_COLOR; e.UsageIndex = 1; el.push_back(e); offset += 4; }
		unsigned texCount = (fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
		for (unsigned t = 0; t < texCount; ++t)
		{
			unsigned size = (fvf >> (16 + t * 2)) & 3;	// D3DFVF_TEXCOORDSIZEn: 0 = 2 floats, 3 = 1, 1 = 3, 2 = 4
			static const BYTE types[4] = { D3DDECLTYPE_FLOAT2, D3DDECLTYPE_FLOAT3, D3DDECLTYPE_FLOAT4, D3DDECLTYPE_FLOAT1 };
			static const WORD sizes[4] = { 8, 12, 16, 4 };
			e.Offset = offset; e.Type = types[size]; e.Usage = D3DDECLUSAGE_TEXCOORD; e.UsageIndex = (BYTE)t;
			el.push_back(e);
			offset += sizes[size];
		}
		// a mesh with one set of texture coordinates still has to feed TEXCOORD1 (damage): the same set
		if (texCount == 1)
		{
			e = el.back(); e.UsageIndex = 1; el.push_back(e);
		}
		e.Stream = SIDE_STREAM; e.UsageIndex = 0;
		e.Offset = 0; e.Type = D3DDECLTYPE_FLOAT3; e.Usage = D3DDECLUSAGE_TANGENT; el.push_back(e);
		e.Offset = 12; e.Type = D3DDECLTYPE_FLOAT3; e.Usage = D3DDECLUSAGE_BINORMAL; el.push_back(e);
		e.Offset = 24; e.Type = D3DDECLTYPE_UBYTE4; e.Usage = D3DDECLUSAGE_BLENDINDICES; el.push_back(e);
		e.Offset = 28; e.Type = D3DDECLTYPE_D3DCOLOR; e.Usage = D3DDECLUSAGE_COLOR; e.UsageIndex = 0; el.push_back(e);
		D3DVERTEXELEMENT9 end = D3DDECL_END();
		el.push_back(end);
		IDirect3DVertexDeclaration9* decl = nullptr;
		if (FAILED(DX8Wrapper::_Get_D3D_Device8()->Get_D3D9()->CreateVertexDeclaration(&el[0], &decl)))
			decl = nullptr;
		s_declarations[fvf] = decl;
		return decl;
	}

	//---------------------------------------------------------------------------------- the draw
	bool s_builtin = false;		// the current draw went to the built-in approximation
	bool s_srgb = false;		// the current draw writes sRGB
	Sage2EffectClass* s_current = nullptr;

	bool Init()
	{
		s_state = UNAVAILABLE;
		IDirect3DDevice8* device = DX8Wrapper::_Get_D3D_Device8();
		Sage2_Log("device %p, pixel shader %d.x, vertex shader %d.x, deferred %s, sRGB write %s", (void*)device,
			device ? DX8Wrapper::Get_Current_Caps()->Get_Pixel_Shader_Major_Version() : 0,
			device ? DX8Wrapper::Get_Current_Caps()->Get_Vertex_Shader_Major_Version() : 0,
			device && Sage2SceneClass::Is_Available() ? "available" : "unavailable",
			device && Sage2SceneClass::Supports_SRGB_Write() ? "yes" : "no");
		if (device == nullptr
			|| DX8Wrapper::Get_Current_Caps()->Get_Pixel_Shader_Major_Version() < 3
			|| DX8Wrapper::Get_Current_Caps()->Get_Vertex_Shader_Major_Version() < 3)
			return false;
		s_state = READY;
		return true;
	}

	IDirect3DBaseTexture9* D3D_Texture(TextureClass* texture)
	{
		if (texture == nullptr)
			return nullptr;
		if (!texture->Is_Initialized())
			TextureLoader::Request_Foreground_Loading(texture);
		return texture->Peek_D3D_Base_Texture();
	}
}

namespace
{
	bool Begin(MeshClass* mesh, TextureClass* base, TextureClass* damageSheet, const VertexMaterialClass* material, DX8PolygonRendererClass* renderer);
	void End();
}

//-------------------------------------------------------------------------------------------------
Sage2EffectClass* Sage2MaterialClass::Get_Effect(const char* name)
{
	if (DX8Wrapper::_Get_D3D_Device8() == nullptr)
		return nullptr;
	return Load_Effect(Lower(name));
}

//-------------------------------------------------------------------------------------------------
bool Sage2MaterialClass::Render(MeshClass* mesh, TextureClass* base, TextureClass* damageSheet, const VertexMaterialClass* material,
	DX8PolygonRendererClass* renderer)
{
	if (!Begin(mesh, base, damageSheet, material, renderer))
		return false;
	renderer->Render(mesh->Get_Base_Vertex_Offset());
	End();
	return true;
}

namespace
{
bool Begin(MeshClass* mesh, TextureClass* base, TextureClass* damageSheet, const VertexMaterialClass* material, DX8PolygonRendererClass* renderer)
{
	if (mesh == nullptr || material == nullptr)
		return false;
	MeshModelClass* model = mesh->Peek_Model();
	if (model == nullptr)
		return false;
	if (s_state == NOT_TRIED)
		Init();
	if (s_state != READY)
		return false;

	const MeshMaterial& mat = Find_Material(model);
	bool skin = model->Get_Flag(MeshGeometryClass::SKIN) != 0;
	const HTreeClass* skinTree = (skin && mesh->Get_Container()) ? mesh->Get_Container()->Get_HTree() : nullptr;
	if (mat.effect == nullptr || (skin && (skinTree == nullptr || (unsigned)mesh->Get_Base_Vertex_Offset() == VERTEX_BUFFER_OVERFLOW_OFFSET)))
	{
		if (Sage2ShadowClass::Is_Rendering())
			return false;	// a caster through W3D's fixed-function draw
		s_builtin = Sage2BuiltinMaterialClass::Begin(mesh, base, damageSheet, material);
		return s_builtin;
	}
	s_builtin = false;

	// W3D's own state first (textures, shader states, vertex and index buffers), so what the effect
	// sets on top is what the draw uses, and End() can put W3D's state back
	DX8Wrapper::Apply_Render_State_Changes();
	IDirect3DDevice8* device8 = DX8Wrapper::_Get_D3D_Device8();
	IDirect3DDevice9* device = device8->Get_D3D9();

	RenderStateStruct state;
	DX8Wrapper::Get_Render_State(state);
	unsigned baseVertex = (unsigned)mesh->Get_Base_Vertex_Offset();
	static const bool trace = getenv("SAGE2_TRACE") != nullptr;
	if (trace)
		printf("sage2 mesh %s: skin %d, %d verts, base %u, fvf %x, vb %p (%u verts, type %d), index base %u vba %u\n", mesh->Get_Name(),
			model->Get_Flag(MeshGeometryClass::SKIN) ? 1 : 0, model->Get_Vertex_Count(), baseVertex, (unsigned)device8->Get_Current_FVF(),
			(void*)state.vertex_buffers[0], state.vertex_buffers[0] ? (unsigned)state.vertex_buffers[0]->Get_Vertex_Count() : 0u,
			state.vertex_buffer_types[0], (unsigned)state.index_base_offset, (unsigned)state.vba_offset);
	// a skin sits in the dynamic buffer at its offset plus the accessor's; sorting (translucent)
	// meshes are copied into a dynamic buffer only when drawn, so they keep the built-in look
	bool dynamic = state.vertex_buffer_types[0] == BUFFER_TYPE_DYNAMIC_DX8;
	bool direct = dynamic || state.vertex_buffer_types[0] == BUFFER_TYPE_DX8;
	if (dynamic)
		baseVertex += (unsigned)state.vba_offset;
	IDirect3DVertexBuffer9* side = direct ? Side_Stream_For(state.vertex_buffers[0], model, baseVertex, skin ? skinTree : nullptr, dynamic) : nullptr;
	DWORD fvf = device8->Get_Current_FVF();
	IDirect3DVertexDeclaration9* decl = fvf ? Declaration_For(fvf) : nullptr;
	if (side == nullptr || decl == nullptr)
	{
		Sage2_Log("mesh %s: no side stream (%p) or declaration (%p, fvf %x, buffer type %d)", model->Get_Name(), (void*)side, (void*)decl, (unsigned)fvf, state.vertex_buffer_types[0]);
		s_builtin = Sage2BuiltinMaterialClass::Begin(mesh, base, damageSheet, material);
		return s_builtin;
	}

	// the draw's values
	Sage2DrawState draw;
	draw.opacity = material->Get_Opacity();
	draw.damage = DamageLookClass::Get_Amount(mesh);
	RenderObjClass* top = mesh;
	while (top->Get_Container() != nullptr)
		top = top->Get_Container();
	draw.houseColor = top->Get_ObjectColor() & 0x00FFFFFF;
	Sage2GlobalsClass::Apply(*mat.effect, draw);

	// the material's values
	for (size_t i = 0; i < mat.textures.size(); ++i)
		mat.effect->Set_Texture(mat.textures[i].param, D3D_Texture(mat.textures[i].texture));
	for (size_t i = 0; i < mat.values.size(); ++i)
		mat.effect->Set_Float(mat.values[i].param, &mat.values[i].values[0], (unsigned)mat.values[i].values.size());

	device8->Set_Vertex_Declaration_Override(decl);
	device->SetStreamSource(SIDE_STREAM, side, 0, SIDE_STRIDE);

	// the High technique lights from the light buffers the scene computes for this mesh (in linear
	// space, so the frame buffer takes sRGB writes for it); otherwise the forward-lit one. In the
	// shadow pass the mesh is a caster: its effect's own depth-only, alpha-tested technique
	int technique = mat.forwardTechnique;
	s_srgb = false;
	if (Sage2ShadowClass::Is_Rendering())
	{
		if (mat.shadowTechnique < 0)
		{
			// no caster technique: W3D's fixed-function draw writes the depth
			device->SetStreamSource(SIDE_STREAM, nullptr, 0, 0);
			device8->Set_Vertex_Declaration_Override(nullptr);
			DX8Wrapper::Invalidate_Shader_Constants();
			return false;
		}
		technique = mat.shadowTechnique;
		Sage2ShadowClass::Count_Draw(false);
	}
	else if (mat.prepassTechnique >= 0 && Sage2SceneClass::Is_Enabled() && Sage2SceneClass::Supports_SRGB_Write())
	{
		IDirect3DVertexBuffer9* meshBuffer = static_cast<DX8VertexBufferClass*>(state.vertex_buffers[0])->Get_DX8_Vertex_Buffer();
		unsigned meshStride = state.vertex_buffers[0]->FVF_Info().Get_FVF_Size();
		if (Sage2SceneClass::Light_Mesh(*mat.effect, mat.prepassTechnique, mesh, renderer, meshBuffer, meshStride, decl))
		{
			technique = mat.technique;
			s_srgb = true;
			device->SetRenderState(D3DRS_SRGBWRITEENABLE, TRUE);
		}
	}
	static std::map<MeshModelClass*, int> s_loggedPath;
	if (!Sage2ShadowClass::Is_Rendering() && s_loggedPath[model] != (s_srgb ? 2 : 1))
	{
		s_loggedPath[model] = s_srgb ? 2 : 1;
		Sage2_Log("mesh %s: drawn %s with technique %s", model->Get_Name(), s_srgb ? "deferred-lit" : "forward-lit", mat.effect->Get_Technique_Name(technique));
	}
	if (!mat.effect->Begin(device, technique, 0))
	{
		Sage2_Log("mesh %s: Begin failed for technique %s", model->Get_Name(), mat.effect->Get_Technique_Name(technique));
		if (s_srgb) device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
		s_srgb = false;
		device->SetStreamSource(SIDE_STREAM, nullptr, 0, 0);
		device8->Set_Vertex_Declaration_Override(nullptr);
		DX8Wrapper::Invalidate_Shader_Constants();
		s_builtin = Sage2BuiltinMaterialClass::Begin(mesh, base, damageSheet, material);
		return s_builtin;
	}
	s_current = mat.effect;
	return true;
}

void End()
{
	if (s_builtin)
	{
		Sage2BuiltinMaterialClass::End();
		s_builtin = false;
		return;
	}
	if (s_current == nullptr)
		return;
	IDirect3DDevice8* device8 = DX8Wrapper::_Get_D3D_Device8();
	s_current->End(device8->Get_D3D9());
	if (s_srgb)
		device8->Get_D3D9()->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
	s_srgb = false;
	device8->Get_D3D9()->SetStreamSource(SIDE_STREAM, nullptr, 0, 0);
	device8->Set_Vertex_Declaration_Override(nullptr);
	// the effect wrote shader constants the wrapper's cache knows nothing about
	DX8Wrapper::Invalidate_Shader_Constants();
	s_current = nullptr;
}
}	// namespace

//-------------------------------------------------------------------------------------------------
void Sage2MaterialClass::Shutdown()
{
	Sage2BuiltinMaterialClass::Shutdown();
	Sage2SceneClass::Shutdown();
	Sage2GlobalsClass::Shutdown();

	for (std::map<MeshModelClass*, MeshMaterial>::iterator it = s_materials.begin(); it != s_materials.end(); ++it)
	{
		for (size_t i = 0; i < it->second.textures.size(); ++i)
			REF_PTR_RELEASE(it->second.textures[i].texture);
		it->first->Release_Ref();
	}
	s_materials.clear();
	for (std::map<VertexBufferClass*, SideStream>::iterator it = s_sideStreams.begin(); it != s_sideStreams.end(); ++it)
	{
		if (it->second.buffer) it->second.buffer->Release();
		it->first->Release_Ref();
	}
	s_sideStreams.clear();
	s_bindFrames.clear();
	for (std::map<DWORD, IDirect3DVertexDeclaration9*>::iterator it = s_declarations.begin(); it != s_declarations.end(); ++it)
		if (it->second) it->second->Release();
	s_declarations.clear();
	for (std::map<std::string, EffectEntry>::iterator it = s_effects.begin(); it != s_effects.end(); ++it)
		delete it->second.effect;
	s_effects.clear();
	s_current = nullptr;
	s_builtin = false;
	s_state = NOT_TRIED;
}
