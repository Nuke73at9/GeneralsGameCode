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

// Tiberium Project @feature SAGE 2 (C&C4 / RA3) effect runtime for the Direct3D 9 renderer.
//
// Loads a compiled Direct3D 9 effect (fx_2_0 .fxo as those games ship them): parameters with
// their annotations, techniques, passes with render states, the vertex and pixel shaders, and
// the pre-shaders that pick shaders from arrays and compute constants on the CPU. Then draws
// with it: Begin(technique, pass) applies everything from the current parameter values.
//
// Pure Direct3D 9; no D3DX. Only depends on <d3d9.h> and the standard library, so the same
// code runs in the offline harness and in the engine.

#pragma once

#include <d3d9.h>
#include <map>
#include <string>
#include <vector>

class Sage2EffectClass
{
public:
	enum ParamClass { CLASS_SCALAR, CLASS_VECTOR, CLASS_MATRIX_ROWS, CLASS_MATRIX_COLS, CLASS_OBJECT, CLASS_STRUCT };
	enum ParamType { TYPE_VOID, TYPE_BOOL, TYPE_INT, TYPE_FLOAT, TYPE_STRING, TYPE_TEXTURE, TYPE_TEXTURE1D,
		TYPE_TEXTURE2D, TYPE_TEXTURE3D, TYPE_TEXTURECUBE, TYPE_SAMPLER, TYPE_SAMPLER1D, TYPE_SAMPLER2D,
		TYPE_SAMPLER3D, TYPE_SAMPLERCUBE, TYPE_PIXELSHADER, TYPE_VERTEXSHADER };

	struct Annotation
	{
		std::string name;
		std::string text;			// string annotations (SasBindAddress, UIName, ResourceName ...)
		std::vector<float> numbers;	// numeric ones
		int object;					// string annotations: the string object (resolved into 'text' at load)
		Annotation() : object(-1) {}
	};

	struct SamplerState
	{
		DWORD state;				// D3DSAMPLERSTATETYPE
		DWORD value;
	};

	struct Param
	{
		std::string name;
		std::string semantic;
		ParamClass paramClass;
		ParamType type;
		unsigned rows, columns, elements;
		std::vector<float> values;		// numeric parameters: rows * max(elements, 1) float4s (ints and bools as float)
		std::vector<Annotation> annotations;
		std::vector<SamplerState> samplerStates;
		int textureParam;				// samplers: index of the texture parameter they sample, or -1
		std::vector<unsigned> objects;	// object parameters: the object indices (shader arrays have several)
		bool unmanaged;					// 'unmanaged' annotation: a per-draw engine global
		IDirect3DBaseTexture9* texture;	// textures: what is bound, or nullptr
		int binding;					// for the engine: which global value feeds this parameter (0: none)

		const Annotation* Find_Annotation(const char* name) const;
		const char* Bind_Address() const;	// SasBindAddress annotation or ""
	};

	Sage2EffectClass();
	~Sage2EffectClass();

	/// Parses the effect and creates its shaders. Returns false (with Get_Error()) on failure.
	bool Load(IDirect3DDevice9* device, const void* data, size_t size, const char* name = "");
	void Release();
	const std::string& Get_Error() const { return m_error; }
	const std::string& Get_Name() const { return m_name; }

	//---------------------------------------------------------------------------- parameters
	int Find_Param(const char* name) const;
	int Get_Param_Count() const { return (int)m_params.size(); }
	const Param& Get_Param(int index) const { return m_params[index]; }
	/// Numeric parameters: 'count' floats, laid out as the parameter is (row by row, four floats a row).
	void Set_Float(int index, const float* values, unsigned count);
	void Set_Float(const char* name, const float* values, unsigned count) { int i = Find_Param(name); if (i >= 0) Set_Float(i, values, count); }
	void Set_Float1(const char* name, float v) { Set_Float(name, &v, 1); }
	void Set_Int(const char* name, int v) { float f = (float)v; Set_Float(name, &f, 1); }
	void Set_Bool(const char* name, bool v) { float f = v ? 1.0f : 0.0f; Set_Float(name, &f, 1); }
	/// Matrices: 16 floats, m[row][col], in the register layout the effect stores (for SAGE 2's
	/// row-vector HLSL, that is the column-vector matrix: World rows = Matrix3D rows, ViewProjection
	/// rows = rows of projection * view).
	void Set_Matrix(const char* name, const float* rowMajor16);
	void Set_Texture(int index, IDirect3DBaseTexture9* texture);
	void Set_Binding(int index, int binding) { if (index >= 0 && index < (int)m_params.size()) m_params[index].binding = binding; }
	void Set_Texture(const char* name, IDirect3DBaseTexture9* texture) { int i = Find_Param(name); if (i >= 0) Set_Texture(i, texture); }

	//---------------------------------------------------------------------------- techniques
	int Find_Technique(const char* name) const;
	int Get_Technique_Count() const { return (int)m_techniques.size(); }
	const char* Get_Technique_Name(int t) const { return m_techniques[t].name.c_str(); }
	int Get_Pass_Count(int t) const { return (int)m_techniques[t].passes.size(); }

	/// Applies the pass: shaders (after running the selection pre-shaders), constants (after the
	/// shaders' own pre-shaders), samplers and textures, render states. The caller draws, then
	/// calls End(), which puts back every device state the pass changed (render states, sampler
	/// states, textures, shaders), so a caller that caches device state stays right.
	bool Begin(IDirect3DDevice9* device, int technique, int pass);
	void End(IDirect3DDevice9* device);

	/// Vertex input semantics the pass's vertex shader declares (D3DDECLUSAGE, usage index), so the
	/// caller can build a matching vertex declaration.
	struct VertexInput { BYTE usage; BYTE usageIndex; };
	const std::vector<VertexInput>& Get_Vertex_Inputs(int technique, int pass);

	//------------------------------------------------------------------------------ pre-shader
	struct PreshaderOperand { int type; unsigned index; std::vector<unsigned> arrayRegisters; };
	struct PreshaderInstruction { unsigned opcode; unsigned elements; std::vector<PreshaderOperand> operands; };
	struct CtabSymbol { std::string name; unsigned registerSet, registerIndex, registerCount; int param; };
	struct Preshader
	{
		std::vector<double> literals;
		std::vector<PreshaderInstruction> instructions;
		std::vector<CtabSymbol> symbols;			// inputs: parameter -> input registers
		std::vector<unsigned> outputMap;			// (register, count) pairs, when the outputs go to shader constants
		unsigned inputRegisterCount;
		unsigned tempCount;
		bool valid;
		Preshader() : inputRegisterCount(0), tempCount(0), valid(false) {}
	};

private:
	struct Shader
	{
		ParamType type;
		IDirect3DVertexShader9* vs;
		IDirect3DPixelShader9* ps;
		std::vector<CtabSymbol> symbols;
		Preshader preshader;					// PRES block inside the shader, if any
		std::vector<VertexInput> inputs;
		Shader() : type(TYPE_VOID), vs(nullptr), ps(nullptr) {}
	};
	struct Object
	{
		ParamType type;
		std::string text;						// strings, sampler mappings
		std::vector<unsigned char> bytes;		// shader bytecode
		int shader;								// index into m_shaders, or -1
		Preshader* selector;					// standalone pre-shader: picks the shader from an array (or computes a state)
		int arrayParam;							// the array parameter the selector indexes, or -1
		Object() : type(TYPE_VOID), shader(-1), selector(nullptr), arrayParam(-1) {}
	};
	struct State { unsigned id; std::vector<float> values; unsigned object; ParamType valueType; };
	struct Pass { std::string name; std::vector<State> states; std::vector<Annotation> annotations; };
	struct Technique { std::string name; std::vector<Pass> passes; std::vector<Annotation> annotations; };

	bool Read_Value(const unsigned char* base, size_t baseLen, unsigned typeOffset, unsigned valueOffset, Param& out);
	bool Read_Annotations(const unsigned char* base, size_t baseLen, const unsigned char*& p, unsigned count, std::vector<Annotation>& out);
	bool Parse_Shader(IDirect3DDevice9* device, Object& object, unsigned objectIndex);
	bool Parse_Ctab(const DWORD* tokens, unsigned count, std::vector<CtabSymbol>& symbols);
	bool Parse_Preshader(const DWORD* tokens, unsigned count, Preshader& out);
	void Run_Preshader(const Preshader& pre, float* outputs, unsigned outputCount) const;
	int Select_Shader(const Object& arrayObject, const Preshader* selector) const;
	void Apply_Constants(IDirect3DDevice9* device, const Shader& shader);
	void Apply_Samplers(IDirect3DDevice9* device, const Shader& shader);
	void Apply_Render_State(IDirect3DDevice9* device, const State& state);
	void Gather_Preshader_Inputs(const Preshader& pre, std::vector<float>& inputs) const;

	std::string m_name;
	std::string m_error;
	std::vector<Param> m_params;
	std::vector<Technique> m_techniques;
	std::vector<Object> m_objects;
	std::vector<Shader> m_shaders;
	std::map<std::string, int> m_paramIndex;
	std::vector<VertexInput> m_noInputs;
	int m_currentTechnique, m_currentPass;

	// what Begin() changed on the device, for End() to put back
	struct SavedRenderState { D3DRENDERSTATETYPE state; DWORD value; };
	struct SavedSamplerState { DWORD stage; D3DSAMPLERSTATETYPE state; DWORD value; };
	struct SavedTexture { DWORD stage; IDirect3DBaseTexture9* texture; };
	std::vector<SavedRenderState> m_savedRenderStates;
	std::vector<SavedSamplerState> m_savedSamplerStates;
	std::vector<SavedTexture> m_savedTextures;
	IDirect3DVertexShader9* m_savedVertexShader;
	IDirect3DPixelShader9* m_savedPixelShader;
};
