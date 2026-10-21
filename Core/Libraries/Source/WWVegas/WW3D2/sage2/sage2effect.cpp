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

// Tiberium Project @feature SAGE 2 effect runtime (see sage2effect.h)
//
// File layout (fx_2_0): u32 magic FEFF0901, u32 offset; everything after is "base". At
// base + offset: parameter count, technique count, (unused), object count; the parameters and
// techniques (whose names, types and values are offsets into base); then the "small" objects
// (strings, sampler-to-texture mappings) and the "large" objects (shader bytecode and standalone
// pre-shaders), each tagged with the object it fills in.

#include "sage2effect.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// SAGE2_TRACE=1 in the environment prints what every Begin() uploads (debugging aid).
static bool Trace_Enabled() { static int on = -1; if (on < 0) { const char* e = getenv("SAGE2_TRACE"); on = (e && *e && *e != '0') ? 1 : 0; } return on != 0; }

namespace
{
	const unsigned EFFECT_MAGIC = 0xFEFF0901u;
	const unsigned STATE_VERTEXSHADER = 146;
	const unsigned STATE_PIXELSHADER = 147;

	// effect state table index -> D3DRENDERSTATETYPE (the order of the D3DRS enum, as D3DX lists them)
	const D3DRENDERSTATETYPE RENDER_STATE_TABLE[] =
	{
		D3DRS_ZENABLE, D3DRS_FILLMODE, D3DRS_SHADEMODE, D3DRS_ZWRITEENABLE, D3DRS_ALPHATESTENABLE,
		D3DRS_LASTPIXEL, D3DRS_SRCBLEND, D3DRS_DESTBLEND, D3DRS_CULLMODE, D3DRS_ZFUNC, D3DRS_ALPHAREF,
		D3DRS_ALPHAFUNC, D3DRS_DITHERENABLE, D3DRS_ALPHABLENDENABLE, D3DRS_FOGENABLE, D3DRS_SPECULARENABLE,
		D3DRS_FOGCOLOR, D3DRS_FOGTABLEMODE, D3DRS_FOGSTART, D3DRS_FOGEND, D3DRS_FOGDENSITY, D3DRS_RANGEFOGENABLE,
		D3DRS_STENCILENABLE, D3DRS_STENCILFAIL, D3DRS_STENCILZFAIL, D3DRS_STENCILPASS, D3DRS_STENCILFUNC,
		D3DRS_STENCILREF, D3DRS_STENCILMASK, D3DRS_STENCILWRITEMASK, D3DRS_TEXTUREFACTOR,
		D3DRS_WRAP0, D3DRS_WRAP1, D3DRS_WRAP2, D3DRS_WRAP3, D3DRS_WRAP4, D3DRS_WRAP5, D3DRS_WRAP6, D3DRS_WRAP7,
		D3DRS_CLIPPING, D3DRS_LIGHTING, D3DRS_AMBIENT, D3DRS_FOGVERTEXMODE, D3DRS_COLORVERTEX, D3DRS_LOCALVIEWER,
		D3DRS_NORMALIZENORMALS, D3DRS_DIFFUSEMATERIALSOURCE, D3DRS_SPECULARMATERIALSOURCE,
		D3DRS_AMBIENTMATERIALSOURCE, D3DRS_EMISSIVEMATERIALSOURCE, D3DRS_VERTEXBLEND, D3DRS_CLIPPLANEENABLE,
		D3DRS_POINTSIZE, D3DRS_POINTSIZE_MIN, D3DRS_POINTSPRITEENABLE, D3DRS_POINTSCALEENABLE, D3DRS_POINTSCALE_A,
		D3DRS_POINTSCALE_B, D3DRS_POINTSCALE_C, D3DRS_MULTISAMPLEANTIALIAS, D3DRS_MULTISAMPLEMASK,
		D3DRS_PATCHEDGESTYLE, D3DRS_DEBUGMONITORTOKEN, D3DRS_POINTSIZE_MAX, D3DRS_INDEXEDVERTEXBLENDENABLE,
		D3DRS_COLORWRITEENABLE, D3DRS_TWEENFACTOR, D3DRS_BLENDOP, D3DRS_POSITIONDEGREE, D3DRS_NORMALDEGREE,
		D3DRS_SCISSORTESTENABLE, D3DRS_SLOPESCALEDEPTHBIAS, D3DRS_ANTIALIASEDLINEENABLE, D3DRS_MINTESSELLATIONLEVEL,
		D3DRS_MAXTESSELLATIONLEVEL, D3DRS_ADAPTIVETESS_X, D3DRS_ADAPTIVETESS_Y, D3DRS_ADAPTIVETESS_Z,
		D3DRS_ADAPTIVETESS_W, D3DRS_ENABLEADAPTIVETESSELLATION, D3DRS_TWOSIDEDSTENCILMODE, D3DRS_CCW_STENCILFAIL,
		D3DRS_CCW_STENCILZFAIL, D3DRS_CCW_STENCILPASS, D3DRS_CCW_STENCILFUNC, D3DRS_COLORWRITEENABLE1,
		D3DRS_COLORWRITEENABLE2, D3DRS_COLORWRITEENABLE3, D3DRS_BLENDFACTOR, D3DRS_SRGBWRITEENABLE, D3DRS_DEPTHBIAS,
		D3DRS_SEPARATEALPHABLENDENABLE, D3DRS_SRCBLENDALPHA, D3DRS_DESTBLENDALPHA, D3DRS_BLENDOPALPHA,
	};
	const unsigned RENDER_STATE_COUNT = sizeof(RENDER_STATE_TABLE) / sizeof(RENDER_STATE_TABLE[0]);

	bool Is_Float_Render_State(D3DRENDERSTATETYPE rs)
	{
		switch (rs)
		{
			case D3DRS_FOGSTART: case D3DRS_FOGEND: case D3DRS_FOGDENSITY: case D3DRS_POINTSIZE:
			case D3DRS_POINTSIZE_MIN: case D3DRS_POINTSIZE_MAX: case D3DRS_POINTSCALE_A: case D3DRS_POINTSCALE_B:
			case D3DRS_POINTSCALE_C: case D3DRS_TWEENFACTOR: case D3DRS_SLOPESCALEDEPTHBIAS: case D3DRS_DEPTHBIAS:
			case D3DRS_MINTESSELLATIONLEVEL: case D3DRS_MAXTESSELLATIONLEVEL: case D3DRS_ADAPTIVETESS_X:
			case D3DRS_ADAPTIVETESS_Y: case D3DRS_ADAPTIVETESS_Z: case D3DRS_ADAPTIVETESS_W:
				return true;
			default:
				return false;
		}
	}

	unsigned U32(const unsigned char* p) { unsigned v; memcpy(&v, p, 4); return v; }
	float F32(const unsigned char* p) { float v; memcpy(&v, p, 4); return v; }
	unsigned short U16(const unsigned char* p) { unsigned short v; memcpy(&v, p, 2); return v; }

	std::string Read_String(const unsigned char* base, size_t baseLen, unsigned offset)
	{
		if (offset + 4 > baseLen)
			return std::string();
		unsigned n = U32(base + offset);
		if (n == 0 || offset + 4 + n > baseLen)
			return std::string();
		const char* s = (const char*)base + offset + 4;
		size_t len = strnlen(s, n);
		return std::string(s, len);
	}

	// Shader bytecode helpers
	bool Is_Comment(DWORD token, unsigned& length)
	{
		if ((token & 0xFFFF) != 0xFFFE)
			return false;
		length = (token >> 16) & 0x7FFF;
		return true;
	}
	DWORD FourCC(const char* s) { return (DWORD)s[0] | ((DWORD)s[1] << 8) | ((DWORD)s[2] << 16) | ((DWORD)s[3] << 24); }

	enum { CTAB_REGSET_BOOL = 0, CTAB_REGSET_INT4 = 1, CTAB_REGSET_FLOAT4 = 2, CTAB_REGSET_SAMPLER = 3 };
	enum { CTAB_CLASS_MATRIX_ROWS = 2, CTAB_CLASS_MATRIX_COLUMNS = 3 };

	enum { PRE_OPERAND_LITERAL = 1, PRE_OPERAND_INPUT = 2, PRE_OPERAND_OUTPUT = 4, PRE_OPERAND_TEMP = 7 };
}

//-------------------------------------------------------------------------------------------------
const Sage2EffectClass::Annotation* Sage2EffectClass::Param::Find_Annotation(const char* name) const
{
	for (size_t i = 0; i < annotations.size(); ++i)
		if (annotations[i].name == name)
			return &annotations[i];
	return nullptr;
}

const char* Sage2EffectClass::Param::Bind_Address() const
{
	const Annotation* a = Find_Annotation("SasBindAddress");
	return a ? a->text.c_str() : "";
}

//-------------------------------------------------------------------------------------------------
Sage2EffectClass::Sage2EffectClass() : m_currentTechnique(-1), m_currentPass(-1), m_savedVertexShader(nullptr), m_savedPixelShader(nullptr)
{
}

Sage2EffectClass::~Sage2EffectClass()
{
	Release();
}

void Sage2EffectClass::Release()
{
	for (size_t i = 0; i < m_shaders.size(); ++i)
	{
		if (m_shaders[i].vs) m_shaders[i].vs->Release();
		if (m_shaders[i].ps) m_shaders[i].ps->Release();
	}
	for (size_t i = 0; i < m_objects.size(); ++i)
		delete m_objects[i].selector;
	m_shaders.clear();
	m_objects.clear();
	m_params.clear();
	m_techniques.clear();
	m_paramIndex.clear();
}

//-------------------------------------------------------------------------------------------------
bool Sage2EffectClass::Read_Annotations(const unsigned char* base, size_t baseLen, const unsigned char*& p, unsigned count, std::vector<Annotation>& out)
{
	for (unsigned i = 0; i < count; ++i)
	{
		if ((size_t)(p - base) + 8 > baseLen)
			return false;
		unsigned typeOffset = U32(p), valueOffset = U32(p + 4);
		p += 8;
		Param v;
		if (!Read_Value(base, baseLen, typeOffset, valueOffset, v))
			return false;
		Annotation a;
		a.name = v.name;
		if (v.paramClass == CLASS_OBJECT)
			a.object = v.objects.empty() ? -1 : (int)v.objects[0];
		else
			a.numbers = v.values;
		out.push_back(a);
	}
	return true;
}

bool Sage2EffectClass::Read_Value(const unsigned char* base, size_t baseLen, unsigned typeOffset, unsigned valueOffset, Param& out)
{
	if ((size_t)typeOffset + 20 > baseLen)
		return false;
	const unsigned char* t = base + typeOffset;
	unsigned type = U32(t), cls = U32(t + 4), nameOff = U32(t + 8), semOff = U32(t + 12), elements = U32(t + 16);
	t += 20;
	out.type = (ParamType)type;
	out.paramClass = (ParamClass)cls;
	out.name = Read_String(base, baseLen, nameOff);
	out.semantic = Read_String(base, baseLen, semOff);
	out.elements = elements;
	out.rows = out.columns = 0;
	out.textureParam = -1;
	out.binding = 0;
	out.unmanaged = false;
	out.texture = nullptr;

	if (cls <= CLASS_MATRIX_COLS)
	{
		if ((size_t)(t - base) + 8 > baseLen)
			return false;
		out.columns = U32(t);
		out.rows = U32(t + 4);
		unsigned count = out.rows * (elements > 0 ? elements : 1);
		out.values.assign(count * 4, 0.0f);
		for (unsigned i = 0; i < count; ++i)
		{
			for (unsigned c = 0; c < out.columns && c < 4; ++c)
			{
				size_t off = (size_t)valueOffset + ((size_t)i * out.columns + c) * 4;
				if (off + 4 > baseLen)
					return false;
				if (type == TYPE_FLOAT)
					out.values[i * 4 + c] = F32(base + off);
				else
					out.values[i * 4 + c] = (float)(int)U32(base + off);
			}
		}
	}
	else if (cls == CLASS_OBJECT)
	{
		if (type >= TYPE_SAMPLER && type <= TYPE_SAMPLERCUBE)
		{
			if ((size_t)valueOffset + 4 > baseLen)
				return false;
			const unsigned char* v = base + valueOffset;
			unsigned nstates = U32(v);
			v += 4;
			for (unsigned i = 0; i < nstates; ++i)
			{
				if ((size_t)(v - base) + 16 > baseLen)
					return false;
				unsigned stype = U32(v) & ~0xA0u;
				unsigned so = U32(v + 8), vo = U32(v + 12);
				v += 16;
				Param sv;
				if (!Read_Value(base, baseLen, so, vo, sv))
					return false;
				if (stype == 4)	// Texture: the texture object this sampler reads
				{
					if (!sv.objects.empty())
					{
						unsigned obj = sv.objects[0];
						if (obj < m_objects.size())
							m_objects[obj].type = (ParamType)type;
						out.objects.push_back(obj);	// resolved to the texture parameter after all parameters are read
					}
				}
				else if (stype >= 5 && stype <= 17 && !sv.values.empty())
				{
					SamplerState ss;
					ss.state = stype - 4;	// D3DSAMPLERSTATETYPE
					ss.value = (DWORD)(int)sv.values[0];
					if (stype == 8)	// border colour: four floats -> ARGB
					{
						float r = sv.values.size() > 0 ? sv.values[0] : 0, g = sv.values.size() > 1 ? sv.values[1] : 0;
						float b = sv.values.size() > 2 ? sv.values[2] : 0, a = sv.values.size() > 3 ? sv.values[3] : 0;
						ss.value = D3DCOLOR_COLORVALUE(r, g, b, a);
					}
					out.samplerStates.push_back(ss);
				}
			}
		}
		else
		{
			unsigned n = elements > 0 ? elements : 1;
			if ((size_t)valueOffset + n * 4 > baseLen)
				return false;
			for (unsigned i = 0; i < n; ++i)
			{
				unsigned obj = U32(base + valueOffset + i * 4);
				if (obj < m_objects.size())
					m_objects[obj].type = (ParamType)type;
				out.objects.push_back(obj);
			}
		}
	}
	else	// struct: members; values are not used by the renderer (PointLight, Shroud)
	{
		if ((size_t)(t - base) + 4 > baseLen)
			return false;
		unsigned nmembers = U32(t);
		t += 4;
		unsigned size = 0;
		for (unsigned i = 0; i < nmembers; ++i)
		{
			if ((size_t)(t - base) + 28 > baseLen)
				return false;
			unsigned mrows = U32(t + 24), melem = U32(t + 16);
			size += mrows * (melem > 0 ? melem : 1);
			t += 28;
		}
		out.rows = size;
		out.columns = 4;
		out.values.assign((size_t)size * (elements > 0 ? elements : 1) * 4, 0.0f);
	}
	return true;
}

//-------------------------------------------------------------------------------------------------
bool Sage2EffectClass::Load(IDirect3DDevice9* device, const void* dataIn, size_t size, const char* name)
{
	Release();
	m_name = name ? name : "";
	m_error.clear();
	const unsigned char* data = (const unsigned char*)dataIn;
	if (size < 16 || U32(data) != EFFECT_MAGIC)
	{
		m_error = "not an fx_2_0 effect";
		return false;
	}
	unsigned offset = U32(data + 4);
	const unsigned char* base = data + 8;
	size_t baseLen = size - 8;
	if ((size_t)offset + 16 > baseLen)
	{
		m_error = "truncated effect";
		return false;
	}
	const unsigned char* p = base + offset;
	unsigned nparams = U32(p), ntech = U32(p + 4), nobjects = U32(p + 12);
	p += 16;
	m_objects.resize(nobjects);

	// parameters
	for (unsigned i = 0; i < nparams; ++i)
	{
		if ((size_t)(p - base) + 16 > baseLen) { m_error = "truncated parameters"; return false; }
		unsigned typeOffset = U32(p), valueOffset = U32(p + 4), nann = U32(p + 12);
		p += 16;
		Param param;
		std::vector<Annotation> ann;
		if (!Read_Annotations(base, baseLen, p, nann, ann) || !Read_Value(base, baseLen, typeOffset, valueOffset, param))
		{
			m_error = "bad parameter";
			return false;
		}
		param.annotations = ann;
		m_params.push_back(param);
	}
	for (size_t i = 0; i < m_params.size(); ++i)
		m_paramIndex[m_params[i].name] = (int)i;

	// techniques
	for (unsigned i = 0; i < ntech; ++i)
	{
		if ((size_t)(p - base) + 12 > baseLen) { m_error = "truncated techniques"; return false; }
		Technique tech;
		unsigned nameOff = U32(p), nann = U32(p + 4), npasses = U32(p + 8);
		p += 12;
		tech.name = Read_String(base, baseLen, nameOff);
		if (!Read_Annotations(base, baseLen, p, nann, tech.annotations)) { m_error = "bad technique"; return false; }
		for (unsigned j = 0; j < npasses; ++j)
		{
			if ((size_t)(p - base) + 12 > baseLen) { m_error = "truncated passes"; return false; }
			Pass pass;
			unsigned pnameOff = U32(p), pann = U32(p + 4), nstates = U32(p + 8);
			p += 12;
			pass.name = Read_String(base, baseLen, pnameOff);
			if (!Read_Annotations(base, baseLen, p, pann, pass.annotations)) { m_error = "bad pass"; return false; }
			for (unsigned k = 0; k < nstates; ++k)
			{
				if ((size_t)(p - base) + 16 > baseLen) { m_error = "truncated states"; return false; }
				State st;
				st.id = U32(p);
				unsigned so = U32(p + 8), vo = U32(p + 12);
				p += 16;
				Param sv;
				if (!Read_Value(base, baseLen, so, vo, sv)) { m_error = "bad state"; return false; }
				st.valueType = sv.type;
				st.values = sv.values;
				st.object = sv.objects.empty() ? ~0u : sv.objects[0];
				pass.states.push_back(st);
			}
			tech.passes.push_back(pass);
		}
		m_techniques.push_back(tech);
	}

	// objects
	if ((size_t)(p - base) + 8 > baseLen) { m_error = "truncated objects"; return false; }
	unsigned nsmall = U32(p), nlarge = U32(p + 4);
	p += 8;
	for (unsigned i = 0; i < nsmall; ++i)
	{
		if ((size_t)(p - base) + 8 > baseLen) { m_error = "truncated small objects"; return false; }
		unsigned index = U32(p), length = U32(p + 4);
		p += 8;
		if ((size_t)(p - base) + length > baseLen || index >= m_objects.size()) { m_error = "bad small object"; return false; }
		Object& o = m_objects[index];
		if (o.type == TYPE_STRING || (o.type >= TYPE_TEXTURE && o.type <= TYPE_SAMPLERCUBE))
			o.text.assign((const char*)p, strnlen((const char*)p, length));
		else
			o.bytes.assign(p, p + length);
		if (length)
			p += (length + 3) - ((length - 1) % 4);
	}
	for (unsigned i = 0; i < nlarge; ++i)
	{
		if ((size_t)(p - base) + 24 > baseLen) { m_error = "truncated large objects"; return false; }
		unsigned technique = U32(p), index = U32(p + 4), state = U32(p + 12), kind = U32(p + 16), length = U32(p + 20);
		p += 24;
		if ((size_t)(p - base) + length > baseLen) { m_error = "bad large object"; return false; }
		unsigned objectIndex = ~0u;
		if (technique == 0xFFFFFFFFu)
		{
			if (index < m_params.size() && state < m_params[index].objects.size())
				objectIndex = m_params[index].objects[state];
		}
		else if (technique < m_techniques.size() && index < m_techniques[technique].passes.size()
			&& state < m_techniques[technique].passes[index].states.size())
		{
			objectIndex = m_techniques[technique].passes[index].states[state].object;
		}
		if (objectIndex < m_objects.size())
		{
			Object& o = m_objects[objectIndex];
			if (kind == 2)
			{
				// standalone pre-shader: u32 name length, the array parameter's name (padded to 4), the pre-shader
				unsigned nameLen = U32(p);
				const char* arrayName = (const char*)p + 4;
				unsigned start = 4 + nameLen;
				o.selector = new Preshader;
				std::string an(arrayName, strnlen(arrayName, nameLen));
				o.arrayParam = Find_Param(an.c_str());
				if (start < length)
					Parse_Preshader((const DWORD*)(p + start), (length - start) / 4, *o.selector);
			}
			else if (o.type == TYPE_STRING || (o.type >= TYPE_TEXTURE && o.type <= TYPE_SAMPLERCUBE))
			{
				// a sampler's Texture state: the name of the texture parameter it samples
				o.text.assign((const char*)p, strnlen((const char*)p, length));
			}
			else
			{
				o.bytes.assign(p, p + length);
			}
		}
		if (length)
			p += (length + 3) - ((length - 1) % 4);
	}

	// samplers: the Texture state's object holds the name of the texture parameter it samples
	// ("sampler_state { Texture = <DiffuseTexture>; }" is stored as the string "DiffuseTexture")
	for (size_t i = 0; i < m_params.size(); ++i)
	{
		Param& s = m_params[i];
		if (s.type < TYPE_SAMPLER || s.type > TYPE_SAMPLERCUBE || s.objects.empty())
			continue;
		const std::string& texName = m_objects[s.objects[0]].text;
		std::map<std::string, int>::const_iterator it = m_paramIndex.find(texName);
		if (it != m_paramIndex.end() && m_params[it->second].type >= TYPE_TEXTURE && m_params[it->second].type <= TYPE_TEXTURECUBE)
			s.textureParam = it->second;
	}

	// annotation strings, 'unmanaged'
	for (size_t i = 0; i < m_params.size(); ++i)
	{
		Param& param = m_params[i];
		for (size_t j = 0; j < param.annotations.size(); ++j)
		{
			Annotation& a = param.annotations[j];
			if (a.object >= 0 && (size_t)a.object < m_objects.size())
				a.text = m_objects[a.object].text;
			if (a.name == "unmanaged" && !a.numbers.empty() && a.numbers[0] != 0.0f)
				param.unmanaged = true;
		}
	}

	// shaders
	for (size_t i = 0; i < m_objects.size(); ++i)
	{
		Object& o = m_objects[i];
		if ((o.type == TYPE_VERTEXSHADER || o.type == TYPE_PIXELSHADER) && o.bytes.size() >= 8 && o.selector == nullptr)
		{
			if (!Parse_Shader(device, o, (unsigned)i))
				return false;
		}
	}
	return true;
}

//-------------------------------------------------------------------------------------------------
bool Sage2EffectClass::Parse_Ctab(const DWORD* tokens, unsigned count, std::vector<CtabSymbol>& symbols)
{
	// tokens[0] is the CTAB fourcc; the table starts right after it, offsets relative to there
	if (count < 8)
		return false;
	const unsigned char* ctab = (const unsigned char*)(tokens + 1);
	size_t ctabLen = (size_t)(count - 1) * 4;
	unsigned nconst = U32(ctab + 12), infoOff = U32(ctab + 16);
	for (unsigned i = 0; i < nconst; ++i)
	{
		size_t off = (size_t)infoOff + (size_t)i * 20;
		if (off + 20 > ctabLen)
			return false;
		CtabSymbol s;
		unsigned nameOff = U32(ctab + off);
		s.name = (nameOff < ctabLen) ? std::string((const char*)ctab + nameOff, strnlen((const char*)ctab + nameOff, ctabLen - nameOff)) : "";
		s.registerSet = U16(ctab + off + 4);
		s.registerIndex = U16(ctab + off + 6);
		s.registerCount = U16(ctab + off + 8);
		s.param = Find_Param(s.name.c_str());
		// the constant's own layout, for matrices
		unsigned typeOff = U32(ctab + off + 12);
		if ((size_t)typeOff + 16 <= ctabLen)
		{
			unsigned cls = U16(ctab + typeOff);
			if (cls == CTAB_CLASS_MATRIX_COLUMNS)
				s.registerSet |= 0x100;	// flag: upload as columns
		}
		symbols.push_back(s);
	}
	return true;
}

bool Sage2EffectClass::Parse_Preshader(const DWORD* tokens, unsigned count, Preshader& out)
{
	out.valid = false;
	if (count < 1)
		return false;
	DWORD version = tokens[0];
	if ((version & 0xFFFF0000u) != 0x46580000u)
		return false;
	const DWORD* ctab = nullptr; unsigned ctabCount = 0;
	const DWORD* clit = nullptr; unsigned clitCount = 0;
	const DWORD* fxlc = nullptr; unsigned fxlcCount = 0;
	const DWORD* prsi = nullptr; unsigned prsiCount = 0;
	unsigned i = 1;
	while (i < count)
	{
		unsigned len;
		if (!Is_Comment(tokens[i], len))
		{
			if (tokens[i] == 0xFFFF)
				break;
			return false;
		}
		++i;
		if (i + len > count)
			return false;
		if (len > 0)
		{
			DWORD id = tokens[i];
			if (id == FourCC("CTAB")) { ctab = tokens + i; ctabCount = len; }
			else if (id == FourCC("CLIT")) { clit = tokens + i; clitCount = len; }
			else if (id == FourCC("FXLC")) { fxlc = tokens + i; fxlcCount = len; }
			else if (id == FourCC("PRSI")) { prsi = tokens + i; prsiCount = len; }
		}
		i += len;
	}
	if (!ctab || !fxlc || !clit)
		return false;

	if (clitCount >= 2)
	{
		unsigned nlit = clit[1];
		if (nlit > (clitCount - 2) / 2)
			return false;
		out.literals.resize(nlit);
		memcpy(out.literals.data(), clit + 2, (size_t)nlit * 8);
	}
	if (prsi && prsiCount >= 8)
	{
		unsigned n = prsi[7];
		if (8 + n * 2 <= prsiCount)
			out.outputMap.assign(prsi + 8, prsi + 8 + n * 2);
	}
	if (!Parse_Ctab(ctab, ctabCount, out.symbols))
		return false;
	out.inputRegisterCount = 0;
	for (size_t s = 0; s < out.symbols.size(); ++s)
	{
		unsigned end = out.symbols[s].registerIndex + out.symbols[s].registerCount;
		if (end > out.inputRegisterCount)
			out.inputRegisterCount = end;
	}

	if (fxlcCount < 2)
		return false;
	unsigned nops = fxlc[1];
	const DWORD* t = fxlc + 2;
	unsigned remaining = fxlcCount - 2;
	out.tempCount = 0;
	for (unsigned op = 0; op < nops; ++op)
	{
		if (remaining < 2)
			return false;
		PreshaderInstruction inst;
		inst.opcode = (t[0] >> 16) & 0xFFFF;
		inst.elements = t[0] & 0xFF;
		unsigned noperands = t[1] + 1;
		t += 2; remaining -= 2;
		for (unsigned k = 0; k < noperands; ++k)
		{
			if (remaining < 3)
				return false;
			PreshaderOperand operand;
			unsigned narrays = t[0];
			operand.type = (int)t[1];
			operand.index = t[2];
			t += 3; remaining -= 3;
			if (operand.type == PRE_OPERAND_INPUT && narrays > 0)
			{
				for (unsigned a = 0; a < narrays; ++a)
				{
					if (remaining < 2)
						return false;
					unsigned jmp = t[1];
					operand.arrayRegisters.push_back((jmp >> 4) * 4 + ((jmp >> 2) & 3));
					t += 2; remaining -= 2;
				}
			}
			if (operand.type == PRE_OPERAND_TEMP && operand.index + 4 > out.tempCount)
				out.tempCount = operand.index + 4;
			inst.operands.push_back(operand);
		}
		out.instructions.push_back(inst);
	}
	out.tempCount = (out.tempCount + 3) & ~3u;
	out.valid = true;
	return true;
}

bool Sage2EffectClass::Parse_Shader(IDirect3DDevice9* device, Object& object, unsigned objectIndex)
{
	const DWORD* tokens = (const DWORD*)object.bytes.data();
	unsigned count = (unsigned)(object.bytes.size() / 4);
	Shader shader;
	shader.type = object.type;

	// comment blocks: CTAB (constants), PRES (pre-shader); input declarations
	unsigned i = 1;
	while (i < count)
	{
		DWORD tok = tokens[i];
		unsigned len;
		if (tok == 0x0000FFFF)
			break;
		if (Is_Comment(tok, len))
		{
			if (i + 1 + len > count)
				break;
			if (len > 0)
			{
				if (tokens[i + 1] == FourCC("CTAB"))
					Parse_Ctab(tokens + i + 1, len, shader.symbols);
				else if (tokens[i + 1] == FourCC("PRES"))
					Parse_Preshader(tokens + i + 2, len - 1, shader.preshader);
			}
			i += 1 + len;
			continue;
		}
		unsigned opcode = tok & 0xFFFF;
		unsigned ilen = (tok >> 24) & 0xF;
		if (opcode == 0x1F && i + 2 < count && object.type == TYPE_VERTEXSHADER)	// dcl
		{
			DWORD usage = tokens[i + 1], dst = tokens[i + 2];
			unsigned regType = ((dst >> 28) & 7) | ((dst >> 9) & 0x18);
			if (regType == 1)	// input register
			{
				VertexInput in;
				in.usage = (BYTE)(usage & 0x1F);
				in.usageIndex = (BYTE)((usage >> 16) & 0xF);
				shader.inputs.push_back(in);
			}
		}
		i += 1 + ilen;
	}

	HRESULT hr;
	if (object.type == TYPE_VERTEXSHADER)
		hr = device->CreateVertexShader(tokens, &shader.vs);
	else
		hr = device->CreatePixelShader(tokens, &shader.ps);
	if (FAILED(hr))
	{
		char buf[128];
		snprintf(buf, sizeof(buf), "%s: shader object %u failed to create (%08x)", m_name.c_str(), objectIndex, (unsigned)hr);
		m_error = buf;
		return false;
	}
	object.shader = (int)m_shaders.size();
	m_shaders.push_back(shader);
	return true;
}

//-------------------------------------------------------------------------------------------------
int Sage2EffectClass::Find_Param(const char* name) const
{
	std::map<std::string, int>::const_iterator it = m_paramIndex.find(name);
	return it == m_paramIndex.end() ? -1 : it->second;
}

int Sage2EffectClass::Find_Technique(const char* name) const
{
	for (size_t i = 0; i < m_techniques.size(); ++i)
		if (m_techniques[i].name == name)
			return (int)i;
	return -1;
}

void Sage2EffectClass::Set_Float(int index, const float* values, unsigned count)
{
	if (index < 0 || index >= (int)m_params.size())
		return;
	Param& p = m_params[index];
	if (p.paramClass > CLASS_MATRIX_COLS)
		return;
	if ((p.paramClass == CLASS_SCALAR || p.paramClass == CLASS_VECTOR) && p.elements <= 1)
	{
		// a vector: the given floats are its components
		unsigned n = count < 4 ? count : 4;
		for (unsigned i = 0; i < n && i < p.values.size(); ++i)
			p.values[i] = values[i];
		return;
	}
	if (p.paramClass == CLASS_SCALAR || p.paramClass == CLASS_VECTOR)
	{
		// an array of vectors (WorldBones, PointLight colours ...): the floats come one full
		// register (four floats) per element, so a caller can hand over a packed float4 array
		unsigned n = count < p.values.size() ? count : (unsigned)p.values.size();
		for (unsigned i = 0; i < n; ++i)
			p.values[i] = values[i];
		return;
	}
	// a matrix (or array of matrices): the floats come row by row, 'columns' per row
	unsigned rows = (unsigned)(p.values.size() / 4);
	unsigned cols = p.columns ? p.columns : 4;
	unsigned k = 0;
	for (unsigned r = 0; r < rows && k < count; ++r)
		for (unsigned c = 0; c < cols && k < count; ++c)
			p.values[r * 4 + c] = values[k++];
}

void Sage2EffectClass::Set_Matrix(const char* name, const float* m)
{
	int index = Find_Param(name);
	if (index < 0)
		return;
	Param& p = m_params[index];
	if (p.paramClass != CLASS_MATRIX_ROWS && p.paramClass != CLASS_MATRIX_COLS)
		return;
	for (unsigned r = 0; r < p.rows && r < 4; ++r)
		for (unsigned c = 0; c < p.columns && c < 4; ++c)
			if (r * 4 + c < p.values.size())
				p.values[r * 4 + c] = m[r * 4 + c];
}

void Sage2EffectClass::Set_Texture(int index, IDirect3DBaseTexture9* texture)
{
	if (index < 0 || index >= (int)m_params.size())
		return;
	m_params[index].texture = texture;
}

//-------------------------------------------------------------------------------------------------
void Sage2EffectClass::Gather_Preshader_Inputs(const Preshader& pre, std::vector<float>& inputs) const
{
	inputs.assign((size_t)pre.inputRegisterCount * 4, 0.0f);
	for (size_t s = 0; s < pre.symbols.size(); ++s)
	{
		const CtabSymbol& sym = pre.symbols[s];
		if (sym.param < 0)
			continue;
		const Param& p = m_params[sym.param];
		size_t dst = (size_t)sym.registerIndex * 4;
		size_t n = (size_t)sym.registerCount * 4;
		for (size_t i = 0; i < n && i < p.values.size() && dst + i < inputs.size(); ++i)
			inputs[dst + i] = p.values[i];
	}
}

void Sage2EffectClass::Run_Preshader(const Preshader& pre, float* outputs, unsigned outputCount) const
{
	std::vector<float> inputs;
	Gather_Preshader_Inputs(pre, inputs);
	std::vector<double> temps(pre.tempCount + 4, 0.0);
	double src[3][4], dst[4];
	for (size_t n = 0; n < pre.instructions.size(); ++n)
	{
		const PreshaderInstruction& inst = pre.instructions[n];
		unsigned elems = inst.elements > 4 ? 4 : inst.elements;
		bool scalarOp = inst.opcode >= 0xA000;
		size_t nsrc = inst.operands.size() - 1;
		for (size_t o = 0; o < nsrc && o < 3; ++o)
		{
			const PreshaderOperand& op = inst.operands[o];
			bool scalar = scalarOp && o == 0;
			for (unsigned e = 0; e < 4; ++e)
				src[o][e] = 0.0;
			switch (op.type)
			{
				case PRE_OPERAND_LITERAL:
					for (unsigned e = 0; e < elems; ++e)
					{
						size_t li = scalar ? op.index : op.index + e;
						src[o][e] = li < pre.literals.size() ? pre.literals[li] : 0.0;
					}
					break;
				case PRE_OPERAND_INPUT:
					if (!op.arrayRegisters.empty())
					{
						// indexed array access: the value is an index, followed through the array registers
						size_t ri = ((size_t)(op.index >> 4) * 4) + ((op.index >> 2) & 3);
						int idx = ri < inputs.size() ? (int)inputs[ri] : 0;
						for (size_t a = 0; a < op.arrayRegisters.size(); ++a)
						{
							size_t r = (size_t)op.arrayRegisters[a] + (size_t)idx;
							idx = r < inputs.size() ? (int)inputs[r] : 0;
						}
						src[o][0] = idx;
					}
					else
						for (unsigned e = 0; e < elems; ++e)
						{
							size_t ii = scalar ? op.index : op.index + e;
							src[o][e] = ii < inputs.size() ? inputs[ii] : 0.0f;
						}
					break;
				case PRE_OPERAND_OUTPUT:
					for (unsigned e = 0; e < elems; ++e)
					{
						size_t oi = scalar ? op.index : op.index + e;
						src[o][e] = oi < outputCount ? outputs[oi] : 0.0f;
					}
					break;
				case PRE_OPERAND_TEMP:
					for (unsigned e = 0; e < elems; ++e)
					{
						size_t ti = scalar ? op.index : op.index + e;
						src[o][e] = ti < temps.size() ? temps[ti] : 0.0;
					}
					break;
				default:
					break;
			}
		}
		const double* s0 = src[0];
		const double* s1 = src[1];
		const double* s2 = src[2];
		for (unsigned e = 0; e < elems; ++e)
		{
			double a = s0[e], b = s1[e], c = s2[e];
			double sa = s0[0];
			double r = 0.0;
			switch (inst.opcode)
			{
				case 0x1000: r = a; break;
				case 0x1010: r = -a; break;
				case 0x1030: r = a != 0.0 ? 1.0 / a : 0.0; break;
				case 0x1040: r = a - floor(a); break;
				case 0x1050: r = exp(a); break;
				case 0x1060: r = a > 0.0 ? log(a) : 0.0; break;
				case 0x1070: r = a > 0.0 ? 1.0 / sqrt(a) : 0.0; break;
				case 0x1080: r = sin(a); break;
				case 0x1090: r = cos(a); break;
				case 0x10A0: r = asin(a); break;
				case 0x10B0: r = acos(a); break;
				case 0x10C0: r = atan(a); break;
				case 0x2000: r = a < b ? a : b; break;
				case 0x2010: r = a > b ? a : b; break;
				case 0x2020: r = a < b ? 1.0 : 0.0; break;
				case 0x2030: r = a >= b ? 1.0 : 0.0; break;
				case 0x2040: r = a + b; break;
				case 0x2050: r = a * b; break;
				case 0x2060: r = atan2(a, b); break;
				case 0x2080: r = b != 0.0 ? a / b : 0.0; break;
				case 0x3000: r = a >= 0.0 ? b : c; break;
				case 0x3010: r = a != 0.0 ? b : c; break;	// movc
				case 0x5000: { double d = 0; for (unsigned k = 0; k < elems; ++k) d += s0[k] * s1[k]; r = d; break; }
				case 0xA000: r = sa < b ? sa : b; break;
				case 0xA010: r = sa > b ? sa : b; break;
				case 0xA020: r = sa < b ? 1.0 : 0.0; break;
				case 0xA030: r = sa >= b ? 1.0 : 0.0; break;
				case 0xA040: r = sa + b; break;
				case 0xA050: r = sa * b; break;
				case 0xA060: r = atan2(sa, b); break;
				case 0xA080: r = b != 0.0 ? sa / b : 0.0; break;
				case 0xD000: r = sa * b; break;
				default: r = 0.0; break;
			}
			dst[e] = r;
		}
		const PreshaderOperand& out = inst.operands.back();
		if (out.type == PRE_OPERAND_TEMP)
		{
			for (unsigned e = 0; e < elems; ++e)
				if ((size_t)out.index + e < temps.size())
					temps[out.index + e] = dst[e];
		}
		else
		{
			for (unsigned e = 0; e < elems; ++e)
				if ((size_t)out.index + e < outputCount)
					outputs[out.index + e] = (float)dst[e];
		}
	}
}

int Sage2EffectClass::Select_Shader(const Object& arrayObject, const Preshader* selector) const
{
	if (selector == nullptr || !selector->valid || arrayObject.arrayParam < 0)
		return -1;
	float out[4] = { 0, 0, 0, 0 };
	Run_Preshader(*selector, out, 4);
	int index = (int)(out[0] + 0.5f);
	const Param& arr = m_params[arrayObject.arrayParam];
	if (index < 0 || (size_t)index >= arr.objects.size())
		index = 0;
	unsigned obj = arr.objects[index];
	return obj < m_objects.size() ? m_objects[obj].shader : -1;
}

//-------------------------------------------------------------------------------------------------
void Sage2EffectClass::Apply_Constants(IDirect3DDevice9* device, const Shader& shader)
{
	bool vs = shader.type == TYPE_VERTEXSHADER;
	float regs[4 * 4];
	for (size_t s = 0; s < shader.symbols.size(); ++s)
	{
		const CtabSymbol& sym = shader.symbols[s];
		if (sym.param < 0)
			continue;
		const Param& p = m_params[sym.param];
		unsigned set = sym.registerSet & 0xFF;
		if (set == CTAB_REGSET_SAMPLER)
			continue;
		if (set == CTAB_REGSET_BOOL)
		{
			BOOL b[16];
			unsigned n = sym.registerCount < 16 ? sym.registerCount : 16;
			for (unsigned i = 0; i < n; ++i)
				b[i] = (i * 4 < p.values.size() && p.values[i * 4] != 0.0f) ? TRUE : FALSE;
			if (vs) device->SetVertexShaderConstantB(sym.registerIndex, b, n);
			else device->SetPixelShaderConstantB(sym.registerIndex, b, n);
			continue;
		}
		if (set == CTAB_REGSET_INT4)
		{
			int iv[16 * 4];
			unsigned n = sym.registerCount < 16 ? sym.registerCount : 16;
			for (unsigned i = 0; i < n * 4; ++i)
				iv[i] = i < p.values.size() ? (int)p.values[i] : 0;
			if (vs) device->SetVertexShaderConstantI(sym.registerIndex, iv, n);
			else device->SetPixelShaderConstantI(sym.registerIndex, iv, n);
			continue;
		}
		// float4 registers. The effect stores every numeric value in register layout already (a
		// column-major float4x3 World is three rows of four), so rows go straight to registers.
		unsigned count = sym.registerCount;
		// rows / vectors / arrays: the values are already one register per row
		unsigned have = (unsigned)(p.values.size() / 4);
		unsigned n = count < have ? count : have;
		if (n == 0)
			continue;
		if (Trace_Enabled())
		{
			printf("  %s c%u[%u] %s =", vs ? "vs" : "ps", sym.registerIndex, n, p.name.c_str());
			for (unsigned i = 0; i < n * 4 && i < 16; ++i) printf(" %g", p.values[i]);
			printf("\n");
		}
		if (vs) device->SetVertexShaderConstantF(sym.registerIndex, p.values.data(), n);
		else device->SetPixelShaderConstantF(sym.registerIndex, p.values.data(), n);
	}

	if (shader.preshader.valid)
	{
		// constants the shader computes on the CPU, written to the registers its output map names
		unsigned maxReg = 0;
		for (size_t i = 0; i + 1 < shader.preshader.outputMap.size(); i += 2)
		{
			unsigned end = shader.preshader.outputMap[i] + shader.preshader.outputMap[i + 1];
			if (end > maxReg) maxReg = end;
		}
		for (size_t n = 0; n < shader.preshader.instructions.size(); ++n)
		{
			const PreshaderOperand& out = shader.preshader.instructions[n].operands.back();
			if (out.type == PRE_OPERAND_OUTPUT && out.index / 4 + 1 > maxReg)
				maxReg = out.index / 4 + 1;
		}
		std::vector<float> outputs((size_t)maxReg * 4, 0.0f);
		Run_Preshader(shader.preshader, outputs.data(), (unsigned)outputs.size());
		if (!shader.preshader.outputMap.empty())
		{
			for (size_t i = 0; i + 1 < shader.preshader.outputMap.size(); i += 2)
			{
				unsigned reg = shader.preshader.outputMap[i], cnt = shader.preshader.outputMap[i + 1];
				if (vs) device->SetVertexShaderConstantF(reg, &outputs[(size_t)reg * 4], cnt);
				else device->SetPixelShaderConstantF(reg, &outputs[(size_t)reg * 4], cnt);
			}
		}
		else
		{
			if (vs) device->SetVertexShaderConstantF(0, outputs.data(), maxReg);
			else device->SetPixelShaderConstantF(0, outputs.data(), maxReg);
		}
	}
}

void Sage2EffectClass::Apply_Samplers(IDirect3DDevice9* device, const Shader& shader)
{
	bool vs = shader.type == TYPE_VERTEXSHADER;
	for (size_t s = 0; s < shader.symbols.size(); ++s)
	{
		const CtabSymbol& sym = shader.symbols[s];
		if ((sym.registerSet & 0xFF) != CTAB_REGSET_SAMPLER || sym.param < 0)
			continue;
		const Param& sampler = m_params[sym.param];
		IDirect3DBaseTexture9* texture = nullptr;
		if (sampler.textureParam >= 0)
			texture = m_params[sampler.textureParam].texture;
		else if (sampler.type >= TYPE_TEXTURE && sampler.type <= TYPE_TEXTURECUBE)
			texture = sampler.texture;
		DWORD stage = sym.registerIndex + (vs ? D3DVERTEXTEXTURESAMPLER0 : 0);
		if (Trace_Enabled())
			printf("  %s s%u %s -> %s %p\n", vs ? "vs" : "ps", sym.registerIndex, sampler.name.c_str(),
				sampler.textureParam >= 0 ? m_params[sampler.textureParam].name.c_str() : "(none)", (void*)texture);
		SavedTexture savedTexture;
		savedTexture.stage = stage;
		savedTexture.texture = nullptr;
		device->GetTexture(stage, &savedTexture.texture);
		m_savedTextures.push_back(savedTexture);
		device->SetTexture(stage, texture);
		for (size_t i = 0; i < sampler.samplerStates.size(); ++i)
		{
			SavedSamplerState saved;
			saved.stage = stage;
			saved.state = (D3DSAMPLERSTATETYPE)sampler.samplerStates[i].state;
			device->GetSamplerState(stage, saved.state, &saved.value);
			if (saved.value == sampler.samplerStates[i].value)
				continue;
			m_savedSamplerStates.push_back(saved);
			device->SetSamplerState(stage, saved.state, sampler.samplerStates[i].value);
		}
	}
}

void Sage2EffectClass::Apply_Render_State(IDirect3DDevice9* device, const State& state)
{
	if (state.id >= RENDER_STATE_COUNT)
		return;	// texture stage / sampler states on passes are not used by SAGE 2 effects
	D3DRENDERSTATETYPE rs = RENDER_STATE_TABLE[state.id];
	float value = 0.0f;
	if (!state.values.empty())
		value = state.values[0];
	else if (state.object < m_objects.size() && m_objects[state.object].selector != nullptr)
	{
		float out[4] = { 0, 0, 0, 0 };
		Run_Preshader(*m_objects[state.object].selector, out, 4);
		value = out[0];
	}
	DWORD dw;
	if (Is_Float_Render_State(rs))
		memcpy(&dw, &value, 4);
	else
		dw = (DWORD)(int)value;
	SavedRenderState saved;
	saved.state = rs;
	device->GetRenderState(rs, &saved.value);
	if (saved.value == dw)
		return;
	m_savedRenderStates.push_back(saved);
	device->SetRenderState(rs, dw);
}

bool Sage2EffectClass::Begin(IDirect3DDevice9* device, int technique, int pass)
{
	if (technique < 0 || technique >= (int)m_techniques.size() || pass < 0 || pass >= (int)m_techniques[technique].passes.size())
		return false;
	const Pass& p = m_techniques[technique].passes[pass];
	m_currentTechnique = technique;
	m_currentPass = pass;
	m_savedRenderStates.clear();
	m_savedSamplerStates.clear();
	m_savedTextures.clear();
	device->GetVertexShader(&m_savedVertexShader);
	device->GetPixelShader(&m_savedPixelShader);
	for (size_t s = 0; s < p.states.size(); ++s)
	{
		const State& st = p.states[s];
		if (st.id == STATE_VERTEXSHADER || st.id == STATE_PIXELSHADER)
		{
			int shaderIndex = -1;
			if (st.object < m_objects.size())
			{
				const Object& o = m_objects[st.object];
				shaderIndex = o.selector ? Select_Shader(o, o.selector) : o.shader;
			}
			if (shaderIndex < 0)
				return false;
			const Shader& sh = m_shaders[shaderIndex];
			if (st.id == STATE_VERTEXSHADER)
				device->SetVertexShader(sh.vs);
			else
				device->SetPixelShader(sh.ps);
			Apply_Constants(device, sh);
			Apply_Samplers(device, sh);
		}
		else
		{
			Apply_Render_State(device, st);
		}
	}
	return true;
}

void Sage2EffectClass::End(IDirect3DDevice9* device)
{
	for (size_t i = m_savedRenderStates.size(); i-- > 0;)
		device->SetRenderState(m_savedRenderStates[i].state, m_savedRenderStates[i].value);
	for (size_t i = m_savedSamplerStates.size(); i-- > 0;)
		device->SetSamplerState(m_savedSamplerStates[i].stage, m_savedSamplerStates[i].state, m_savedSamplerStates[i].value);
	for (size_t i = m_savedTextures.size(); i-- > 0;)
	{
		device->SetTexture(m_savedTextures[i].stage, m_savedTextures[i].texture);
		if (m_savedTextures[i].texture)
			m_savedTextures[i].texture->Release();	// GetTexture added a reference
	}
	device->SetVertexShader(m_savedVertexShader);
	device->SetPixelShader(m_savedPixelShader);
	if (m_savedVertexShader) m_savedVertexShader->Release();
	if (m_savedPixelShader) m_savedPixelShader->Release();
	m_savedVertexShader = nullptr;
	m_savedPixelShader = nullptr;
	m_savedRenderStates.clear();
	m_savedSamplerStates.clear();
	m_savedTextures.clear();
	m_currentTechnique = m_currentPass = -1;
}

const std::vector<Sage2EffectClass::VertexInput>& Sage2EffectClass::Get_Vertex_Inputs(int technique, int pass)
{
	if (technique < 0 || technique >= (int)m_techniques.size() || pass < 0 || pass >= (int)m_techniques[technique].passes.size())
		return m_noInputs;
	const Pass& p = m_techniques[technique].passes[pass];
	for (size_t s = 0; s < p.states.size(); ++s)
	{
		if (p.states[s].id != STATE_VERTEXSHADER || p.states[s].object >= m_objects.size())
			continue;
		const Object& o = m_objects[p.states[s].object];
		int shaderIndex = o.selector ? Select_Shader(o, o.selector) : o.shader;
		if (shaderIndex >= 0)
			return m_shaders[shaderIndex].inputs;
	}
	return m_noInputs;
}
