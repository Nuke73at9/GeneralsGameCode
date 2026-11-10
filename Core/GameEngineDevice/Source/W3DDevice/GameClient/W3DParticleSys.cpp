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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// W3DParticleSys.cpp
// W3D Particle System implementation
// Author: Michael S. Booth, November 2001

#include "Common/FramePacer.h"
#include "Common/GlobalData.h"
#include "GameClient/Color.h"
#include "W3DDevice/GameClient/W3DParticleSys.h"
#include "W3DDevice/GameClient/W3DAssetManager.h"
#include "W3DDevice/GameClient/W3DDisplay.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "W3DDevice/GameClient/W3DSmudge.h"
#include "W3DDevice/GameClient/W3DSnow.h"
#include "W3DDevice/GameClient/W3DScene.h"
#include "WW3D2/camera.h"
#include "WW3D2/pointgr.h"
// Tiberium Project @feature model particles
#include "WW3D2/dx8renderer.h"
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/matinfo.h"
#include "WW3D2/mesh.h"
#include "WW3D2/meshmdl.h"
#include "WW3D2/vertmaterial.h"
#include "WW3D2/w3d_file.h"
#include "WW3D2/ww3d.h"
#ifdef RTS_RENDERER_D3D9
#include <math.h>
#include <vector>
#include "WW3D2/dx8wrapper.h"
#include "WW3D2/sage2/sage2globals.h"	// Tiberium Project @feature lit smoke
#include "WW3D2/sage2/sage2lighting.h"
#include "WW3D2/sage2/sage2scene.h"
#endif


//------------------------------------------------------------------------------ Performance Timers
//#include "Common/PerfMetrics.h"
//#include "Common/PerfTimer.h"

//-------------------------------------------------------------------------------------------------


W3DParticleSystemManager::W3DParticleSystemManager()
{
	m_batchParticleAlignment = ParticleSystemInfo::PARTICLE_ALIGNMENT_BILLBOARD;
	m_batchShaderType = ParticleSystemInfo::INVALID_SHADER;

	m_pointGroup = nullptr;
	m_streakLine = nullptr;
	m_posBuffer = nullptr;
	m_RGBABuffer = nullptr;
	m_sizeBuffer = nullptr;
	m_angleBuffer = nullptr;
	m_frameBuffer = nullptr;
	m_batchGrid = 1;
	m_batchBright = FALSE;
	m_readyToRender = false;

	m_onScreenParticleCount = 0;

	m_pointGroup = NEW PointGroupClass();
	//m_streakLine = nullptr;
	m_streakLine = NEW StreakLineClass();

	m_posBuffer = NEW_REF( ShareBufferClass<Vector3>, (MAX_POINTS_PER_GROUP, "W3DParticleSystemManager::m_posBuffer") );
	m_RGBABuffer = NEW_REF( ShareBufferClass<Vector4>, (MAX_POINTS_PER_GROUP, "W3DParticleSystemManager::m_RGBABuffer") );
	m_sizeBuffer = NEW_REF( ShareBufferClass<float>, (MAX_POINTS_PER_GROUP, "W3DParticleSystemManager::m_sizeBuffer") );
	m_angleBuffer = NEW_REF( ShareBufferClass<uint8>, (MAX_POINTS_PER_GROUP, "W3DParticleSystemManager::m_angleBuffer") );
	m_frameBuffer = NEW_REF( ShareBufferClass<uint8>, (MAX_POINTS_PER_GROUP, "W3DParticleSystemManager::m_frameBuffer") );
}

W3DParticleSystemManager::~W3DParticleSystemManager()
{
	// Tiberium Project @feature model particles: the copies are let go while W3D is up (at shutdown W3D may be
	// gone first, its buffers with it: then they are left to the process's end)
	if (WW3D::Is_Initted())
		releaseModelPools();
	else
	{
		for (size_t i = 0; i < m_modelPools.size(); ++i)
			delete m_modelPools[i];
		m_modelPools.clear();
	}

	delete m_pointGroup;

//	W3DDisplay::m_3DScene->Remove_Render_Object( m_streakLine );

	if (m_streakLine)
	{
		REF_PTR_RELEASE(m_streakLine);
	}

	REF_PTR_RELEASE(m_posBuffer);
	REF_PTR_RELEASE(m_RGBABuffer);
	REF_PTR_RELEASE(m_sizeBuffer);
	REF_PTR_RELEASE(m_angleBuffer);
	REF_PTR_RELEASE(m_frameBuffer);
}

/**
 * Hack because DoParticles is called from Flush(), which is called
 * multiple times per frame.  We only want to render once.
 * @todo Clean up the flag/Flush hack.
 */
void W3DParticleSystemManager::queueParticleRender()
{
	m_readyToRender = true;
}

// Tiberium Project @feature model particles: the copies are let go between games
void W3DParticleSystemManager::reset()
{
	releaseModelPools();
	ParticleSystemManager::reset();
}

//-------------------------------------------------------------------------------------------------
// Tiberium Project @feature model particles (see ParticleSystemTemplate's Model)
//-------------------------------------------------------------------------------------------------
// the meshes of a render object: itself, or its sub-objects'
static void collectModelMeshes(RenderObjClass *robj, std::vector<MeshClass*> &meshes)
{
	if (robj->Class_ID() == RenderObjClass::CLASSID_MESH)
	{
		meshes.push_back(static_cast<MeshClass*>(robj));
		return;
	}
	for (Int i = 0; i < robj->Get_Num_Sub_Objects(); ++i)
	{
		RenderObjClass *sub = robj->Get_Sub_Object(i);
		if (sub != nullptr)
		{
			collectModelMeshes(sub, meshes);
			sub->Release_Ref();
		}
	}
}

W3DParticleSystemManager::ModelPool *W3DParticleSystemManager::findModelPool(const AsciiString &name)
{
	for (size_t i = 0; i < m_modelPools.size(); ++i)
	{
		if (m_modelPools[i]->name == name)
			return m_modelPools[i];
	}
	ModelPool *pool = NEW ModelPool;
	pool->name = name;
	pool->used = 0;
	pool->missing = FALSE;
	m_modelPools.push_back(pool);
	return pool;
}

// the next copy of the pool's model this frame, made when there are not enough
W3DParticleSystemManager::ModelCopy *W3DParticleSystemManager::takeModelCopy(ModelPool &pool)
{
	if (pool.used < pool.copies.size())
		return &pool.copies[pool.used++];
	if (pool.missing || pool.copies.size() >= MAX_MODEL_COPIES)
		return nullptr;

	RenderObjClass *robj = WW3DAssetManager::Get_Instance()->Create_Render_Obj(pool.name.str());
	if (robj == nullptr)
	{
		fprintf(stderr, "Tiberium: model particles: no model %s; its systems draw nothing\n", pool.name.str());
		fflush(stderr);
		pool.missing = TRUE;
		return nullptr;
	}
	ModelCopy copy;
	copy.robj = robj;
	collectModelMeshes(robj, copy.meshes);
	for (size_t m = 0; m < copy.meshes.size(); ++m)
	{
		// its own mesh model and materials, so this copy's fade and tint are its own
		MeshClass *mesh = copy.meshes[m];
		mesh->Make_Unique(true);
		MeshModelClass *model = mesh->Peek_Model();
		if (model == nullptr)
			continue;
		// (drawn now, with the particles: W3D's static sort lists were drawn before them)
		if (mesh->Get_Sort_Level() != SORT_LEVEL_NONE)
			mesh->Set_Sort_Level(SORT_LEVEL_NONE);
		const ShaderClass shader = model->Has_Shader_Array(0) ? model->Get_Shader(0, 0) : model->Get_Single_Shader(0);
		Int blend = MODEL_BLEND_OPAQUE;
		if (shader.Get_Dst_Blend_Func() == ShaderClass::DSTBLEND_ONE)
			blend = MODEL_BLEND_ADDITIVE;
		else if (shader.Get_Dst_Blend_Func() != ShaderClass::DSTBLEND_ZERO)
			blend = MODEL_BLEND_ALPHA;
		MaterialInfoClass *info = mesh->Get_Material_Info();
		if (info == nullptr)
			continue;
		for (Int i = 0; i < info->Vertex_Material_Count(); ++i)
		{
			VertexMaterialClass *material = info->Peek_Vertex_Material(i);
			if (material == nullptr)
				continue;
			ModelMaterial look;
			look.material = material;
			material->Get_Ambient(&look.ambient);
			material->Get_Diffuse(&look.diffuse);
			material->Get_Emissive(&look.emissive);
			look.opacity = material->Get_Opacity();
			look.blend = blend;
			copy.materials.push_back(look);
		}
		info->Release_Ref();
	}
	if (pool.copies.empty())
	{
		fprintf(stderr, "Tiberium: model particles: %s, %d meshes, %d materials\n", pool.name.str(), (int)copy.meshes.size(), (int)copy.materials.size());
		fflush(stderr);
	}
	pool.copies.push_back(copy);
	return &pool.copies[pool.used++];
}

void W3DParticleSystemManager::releaseModelPools()
{
	for (size_t i = 0; i < m_modelPools.size(); ++i)
	{
		ModelPool *pool = m_modelPools[i];
		for (size_t c = 0; c < pool->copies.size(); ++c)
			REF_PTR_RELEASE(pool->copies[c].robj);
		delete pool;
	}
	m_modelPools.clear();
}

// each particle of the systems that draw models: a copy of the model where it is, turned, sized, faded and tinted
void W3DParticleSystemManager::renderModelParticles(RenderInfoClass &rinfo)
{
	for (size_t i = 0; i < m_modelPools.size(); ++i)
		m_modelPools[i]->used = 0;

	// (the particles have moved on into the logic frame this far: their sizes and angles follow)
	const Real phase = TheFramePacer != nullptr ? TheFramePacer->getLogicFramePhase() : 0.0f;
	LightEnvironmentClass *lights = W3DDisplay::m_3DScene != nullptr ? &W3DDisplay::m_3DScene->getDefaultLightEnv() : nullptr;
	Bool drew = FALSE;

	ParticleSystemManager::ParticleSystemList &systems = TheParticleSystemManager->getAllParticleSystems();
	for (ParticleSystemManager::ParticleSystemListIt it = systems.begin(); it != systems.end(); ++it)
	{
		ParticleSystem *sys = *it;
		if (sys == nullptr || !sys->isUsingDrawables())
			continue;
		const ParticleSystemTemplate *t = sys->getTemplate();
		if (t == nullptr || t->getModelName().isEmpty())
			continue;
		ModelPool *pool = findModelPool(t->getModelName());
		if (pool->missing)
			continue;
		// (an additive system keeps no alpha: its colours fade it instead)
		const Bool fades = sys->getShaderType() != ParticleSystemInfo::ADDITIVE;
		for (Particle *p = sys->getFirstParticle(); p != nullptr; p = p->m_systemNext)
		{
			const Real alpha = fades ? p->getAlpha() : 1.0f;
			if (alpha < 0.004f)
				continue;
			const Real age = (Real)p->getElapsedFrames() + phase;
			Real scale[3];
			t->getModelScale(p->getPersonality(), age, scale);
			if (scale[0] < 0.0001f || scale[1] < 0.0001f || scale[2] < 0.0001f)
				continue;
			ModelCopy *copy = takeModelCopy(*pool);
			if (copy == nullptr)
				break;

			// turned as its template says
			Matrix3D tm(true);
			switch (t->getModelRotation())
			{
				case ParticleSystemTemplate::MODEL_ROTATION_IN_WORLD:
				{
					Real angles[2];
					t->getModelAngles(p->getPersonality(), age, angles);
					tm.Rotate_Z(p->getAngle());
					tm.Rotate_Y(angles[1]);
					tm.Rotate_X(angles[0]);
					break;
				}
				case ParticleSystemTemplate::MODEL_ROTATION_AROUND_VELOCITY:
				{
					Vector3 along(p->m_vel.x, p->m_vel.y, p->m_vel.z);
					if (along.Length2() > 0.000001f)
					{
						along.Normalize();
						Vector3 side;
						Vector3::Cross_Product(Vector3(0.0f, 0.0f, 1.0f), along, &side);
						if (side.Length2() < 0.000001f)
							side.Set(0.0f, 1.0f, 0.0f);
						side.Normalize();
						Vector3 up;
						Vector3::Cross_Product(along, side, &up);
						tm.Set(along, side, up, Vector3(0.0f, 0.0f, 0.0f));
					}
					Real angles[2];
					t->getModelAngles(p->getPersonality(), age, angles);
					tm.Rotate_X(angles[0]);
					break;
				}
				default:
				{
					// as its system is turned (a muzzle flash along its barrel), its axes kept unit length
					const Matrix3D *sysTm = sys->getTransform();
					Vector3 axes[3];
					for (Int a = 0; a < 3; ++a)
					{
						axes[a].Set((*sysTm)[0][a], (*sysTm)[1][a], (*sysTm)[2][a]);
						if (axes[a].Length2() < 0.000001f)
							axes[a].Set(a == 0 ? 1.0f : 0.0f, a == 1 ? 1.0f : 0.0f, a == 2 ? 1.0f : 0.0f);
						axes[a].Normalize();
					}
					tm.Set(axes[0], axes[1], axes[2], Vector3(0.0f, 0.0f, 0.0f));
					break;
				}
			}
			// sized on each axis, where the particle is
			for (Int row = 0; row < 3; ++row)
			{
				tm[row][0] *= scale[0];
				tm[row][1] *= scale[1];
				tm[row][2] *= scale[2];
			}
			const Coord3D *pos = p->getPosition();
			tm.Set_Translation(Vector3(pos->x, pos->y, pos->z));
			copy->robj->Set_Transform(tm);
			// (its bounds and normals at its largest size)
			Real largest = scale[0] > scale[1] ? scale[0] : scale[1];
			if (scale[2] > largest)
				largest = scale[2];
			copy->robj->Set_ObjectScale(largest);
			for (size_t m = 0; m < copy->meshes.size(); ++m)
				copy->meshes[m]->Set_ObjectScale(largest);

			// faded by its alpha, tinted by its colour
			Vector3 tint(1.0f, 1.0f, 1.0f);
			if (t->isModelTint())
			{
				const RGBColor *color = p->getColor();
				tint.Set(color->red, color->green, color->blue);
			}
			for (size_t m = 0; m < copy->materials.size(); ++m)
			{
				const ModelMaterial &look = copy->materials[m];
				const Real k = look.blend == MODEL_BLEND_ADDITIVE ? alpha : 1.0f;
				look.material->Set_Ambient(look.ambient.X * tint.X * k, look.ambient.Y * tint.Y * k, look.ambient.Z * tint.Z * k);
				look.material->Set_Diffuse(look.diffuse.X * tint.X * k, look.diffuse.Y * tint.Y * k, look.diffuse.Z * tint.Z * k);
				look.material->Set_Emissive(look.emissive.X * tint.X * k, look.emissive.Y * tint.Y * k, look.emissive.Z * tint.Z * k);
				look.material->Set_Opacity(look.blend == MODEL_BLEND_ALPHA ? look.opacity * alpha : look.opacity);
			}
			rinfo.light_environment = lights;
			copy->robj->Render(rinfo);
			drew = TRUE;
		}
	}
	rinfo.light_environment = nullptr;
	if (drew)
	{
		// (the copies' materials have changed since W3D last set them)
		DX8Wrapper::Set_Material(nullptr);
		TheDX8MeshRenderer.Flush();
	}
}

/**
 * Nasty hack to render particles last. Called directly by WW3D::Flush()
 */
void DoParticles( RenderInfoClass &rinfo )
{
	if (TheParticleSystemManager)
		TheParticleSystemManager->doParticles(rinfo);
}

//-------------------------------------------------------------------------------------------------
// Tiberium Project @feature sprite-sheet particles: the frame grid of a system's texture (1: none;
// the point groups take square grids of 2, 4, 8 or 16), and a particle's frame in it
static UnsignedInt textureGrid(const ParticleSystem& sys)
{
	const ParticleSystemTemplate* t = sys.getTemplate();
	if (t == nullptr || t->getTextureFrames() <= 1)
		return 1;
	const UnsignedInt grid = t->getTextureGrid();
	return (grid == 2 || grid == 4 || grid == 8 || grid == 16) ? grid : 1;
}

// Tiberium Project @feature colours drawn twice as bright (the template's BrightColors)
static Bool isBright(const ParticleSystem& sys)
{
	const ParticleSystemTemplate* t = sys.getTemplate();
	return t != nullptr && t->isBrightColors();
}

// the sprite shader with the texture times twice the colour
static ShaderClass brightShader(const ShaderClass& preset)
{
	ShaderClass shader(preset);
	shader.Set_Primary_Gradient(ShaderClass::GRADIENT_MODULATE2X);
	return shader;
}

static uint8 particleFrame(const ParticleSystemTemplate& t, Particle& p, UnsignedInt grid)
{
	UnsignedInt frames = t.getTextureFrames();
	if (frames > grid * grid)
		frames = grid * grid;
	UnsignedInt perRow = t.getTextureFramesPerRow();
	if (perRow == 0 || perRow > frames)
		perRow = grid < frames ? grid : frames;
	const UnsignedInt rows = (frames + perRow - 1) / perRow;
	UnsignedInt seed = p.getPersonality() * 2654435761u;	// (the personality counts up; scatter it)
	seed ^= seed >> 15;
	const Real age = (Real)p.getElapsedFrames() * t.getTextureFrameSpeed();
	const UnsignedInt step = age > 0.0f ? (UnsignedInt)age : 0;
	UnsignedInt frame = 0;
	switch (t.getTextureFrameMode())
	{
		case ParticleSystemTemplate::TEXTURE_FRAMES_RANDOM:
			frame = seed % frames;
			break;
		case ParticleSystemTemplate::TEXTURE_FRAMES_ROW:
		case ParticleSystemTemplate::TEXTURE_FRAMES_RANDOM_ROW:
		{
			UnsignedInt row = t.getTextureFrameMode() == ParticleSystemTemplate::TEXTURE_FRAMES_ROW
				? t.getTextureFrameRow() : (seed >> 8) % rows;
			if (row >= rows)
				row = rows - 1;
			frame = row * perRow + (step < perRow ? step : perRow - 1);
			break;
		}
		default:
			frame = step % frames;
			break;
	}
	return (uint8)(frame < frames ? frame : frames - 1);
}

void W3DParticleSystemManager::doParticles(RenderInfoClass &rinfo)
{

	if (m_readyToRender == false)
		return;

	// external mechanism must tell us when it's OK to render again...
	m_readyToRender = false;

	// Tiberium Project @feature model particles: their models first, as the scene's translucent objects are drawn
	renderModelParticles(rinfo);

	//reset each frame
	/// @todo lorenzen sez: this should be debug only:
	m_onScreenParticleCount = 0;

 	const FrustumClass & frustum = rinfo.Camera.Get_Frustum();
	AABoxClass bbox;

	//Get a bounding box around our visible universe.  Bounded by terrain and the sky
	//so much tighter fitting volume than what's actually visible.  This will cull
	//particles falling under the ground.

 	TheTerrainRenderObject->getMaximumVisibleBox(frustum, &bbox, TRUE);

	//@todo lorenzen sez: put these in registers for sure
	Real bcX = bbox.Center.X;
	Real bcY = bbox.Center.Y;
	Real bcZ = bbox.Center.Z;
	Real beX = bbox.Extent.X;
	Real beY = bbox.Extent.Y;
	Real beZ = bbox.Extent.Z;

	unsigned int personalities[MAX_POINTS_PER_GROUP];
	Int lightningBolts[MAX_POINTS_PER_GROUP];	// Tiberium Project


	m_fieldParticleCount = 0;

#ifdef RTS_RENDERER_D3D9
	// Tiberium Project @feature Lit smoke, as C&C4 and RA3 light their particles
	// (GPUParticleLitAccumLight.fx: the colour times the scene's directional lights): smoke, dust
	// and debris (alpha-blended and alpha-tested systems; the additive ones glow on their own) take
	// the scene's light against RA3's afternoon, for which Zero Hour's particle colours read right,
	// so they dim and tint at evening and night instead of glowing grey, and they take the light of
	// the fires, explosions and lamps near them: the smoke over a fire is lit from below.
	Bool litParticles = FALSE;
	Real sceneLight[3] = { 1.0f, 1.0f, 1.0f };
	Real referenceLight[3] = { 1.0f, 1.0f, 1.0f };
	Real sceneFactor[3] = { 1.0f, 1.0f, 1.0f };
	const Bool pointLit = Sage2SceneClass::Get_Point_Light_Count() > 0;
	const Int timeOfDay = TheGlobalData != nullptr ? (Int)TheGlobalData->m_timeOfDay : -1;
	if (Sage2SceneClass::Is_Particle_Lighting_Enabled() && Sage2LightingClass::Uses_Presets()
		&& timeOfDay >= TIME_OF_DAY_FIRST && timeOfDay < TIME_OF_DAY_COUNT)
	{
		// a puff takes the ambient and, on average, half of each light
		const GlobalData::TerrainLighting *lights = TheGlobalData->m_terrainObjectsLighting[timeOfDay];
		const float ambient[3] = { lights[0].ambient.red, lights[0].ambient.green, lights[0].ambient.blue };
		float diffuse[3][3] = { { 0.0f } };
		for (Int j = 0; j < 3 && j < MAX_GLOBAL_LIGHTS; ++j)
		{
			diffuse[j][0] = lights[j].diffuse.red;
			diffuse[j][1] = lights[j].diffuse.green;
			diffuse[j][2] = lights[j].diffuse.blue;
		}
		Sage2LightingClass::Get_Particle_Light(ambient, diffuse, sceneLight, referenceLight, sceneFactor);
		litParticles = TRUE;
		static Int s_loggedTimeOfDay = -1;
		if (s_loggedTimeOfDay != timeOfDay)
		{
			s_loggedTimeOfDay = timeOfDay;
			Sage2_Log("lit particles: smoke and dust at (%.2f %.2f %.2f) of their colour (time of day %d, the light %.2f %.2f %.2f of RA3's afternoon)",
				sceneFactor[0], sceneFactor[1], sceneFactor[2], timeOfDay, sceneLight[0], sceneLight[1], sceneLight[2]);
		}
	}
#endif

	const Bool drawSmudge = TheSmudgeManager && TheSmudgeManager->getHardwareSupport() && TheGlobalData->m_useHeatEffects;

	if (drawSmudge)
	{
		TheSmudgeManager->resetDraw();
	}

	// Number of particles/points being rendered.
	UnsignedInt pointCount = 0;

	ParticleSystemManager::ParticleSystemList &particleSysList = TheParticleSystemManager->getAllParticleSystems();
	for( ParticleSystemManager::ParticleSystemListIt it = particleSysList.begin(); it != particleSysList.end(); ++it)
	{
		ParticleSystem *sys = (*it);
		if (!sys) {
			continue;
		}

		// only look at particle/point style systems
		if (sys->isUsingDrawables())
			continue;

		// TheSuperHackers @performance Mauller 16/08/2026 Skip processing particle system if no particles are in view.
		UnsignedInt particleCount = 0;
		for (Particle* vp = sys->getFirstParticle(); vp; vp = vp->m_systemNext)
		{
			const Coord3D* pos = vp->getPosition();
			const Real psize = vp->getSize();

			//Test if particle is at the screen or terrain edges.
			if (WWMath::Fabs(pos->x - bcX) > (beX + psize) ||
				WWMath::Fabs(pos->y - bcY) > (beY + psize) ||
				WWMath::Fabs(pos->z - bcZ) > (beZ + psize))
			{
				vp->setIsCulled(true);
				continue;
			}

			vp->setIsCulled(false);
			particleCount++;
		}

		// Particle system has no particles on screen
		if (particleCount == 0)
			continue;

		// Handle smudge type particles
		if (sys->isUsingSmudge())
		{
			if (!drawSmudge)
				continue;

			for (Particle *p = sys->getFirstParticle(); p; p = p->m_systemNext)
			{
				if (p->isCulled())
					continue;

				if (Smudge *smudge = TheSmudgeManager->findSmudge(p))
				{
					// The particle is in view. Draw the smudge!
					smudge->m_draw = true;
				}
			}
			continue;
		}

		// Tiberium Project @feature geometry particles (C&C4's muzzle flashes, sparks and flame blades):
		// crossed quads along each particle's velocity instead of camera-facing sprites
		if (sys->isUsingParticles() && sys->getTemplate() != nullptr
			&& sys->getTemplate()->getGeometry() != ParticleSystemTemplate::PARTICLE_GEOMETRY_NONE)
		{
			flushParticleBatch(rinfo, pointCount);
			Bool lit = FALSE;
			Real factor[3] = { 1.0f, 1.0f, 1.0f };
#ifdef RTS_RENDERER_D3D9
			const Int geometryLighting = sys->getTemplate()->getLighting();
			lit = litParticles && geometryLighting != ParticleSystemTemplate::PARTICLE_LIGHTING_UNLIT
				&& (geometryLighting == ParticleSystemTemplate::PARTICLE_LIGHTING_LIT
					|| sys->getShaderType() == ParticleSystemInfo::ALPHA || sys->getShaderType() == ParticleSystemInfo::ALPHA_TEST);
			for (Int c = 0; c < 3; ++c)
				factor[c] = sceneFactor[c];
#endif
			m_onScreenParticleCount += renderGeometryParticles(*sys, *sys->getTemplate(), lit, factor);
			continue;
		}

		// TheSuperHackers @performance Ronin/Mauller 09/08/2026 Implement batched rendering for similar particles.
		// Particles with the same properties will now be batched onto a single texture surface before being drawn.
		// If a different particle type appears before the batch is filled, the previous batch will be drawn first.
		RefCountPtr<TextureClass> texture;
		texture.Assign_No_Add_Ref(W3DDisplay::m_assetManager->Get_Texture(sys->getParticleTypeName().str()));

		const Bool canBatch = sys->isUsingParticles();
		const Bool batchDone = finishedBatch(*sys, texture);
		if (!canBatch || batchDone)
		{
			flushParticleBatch(rinfo, pointCount);
		}

		// setup a new particle batch texture if prior batch was flushed.
		if (canBatch && m_batchTexture == nullptr)
		{
			initializeBatch(*sys, texture);
		}

		UnsignedInt startCount = pointCount;

		// build W3D particle buffer
		Vector3 *posArray = m_posBuffer->Get_Array();
		Real *sizeArray = m_sizeBuffer->Get_Array();
		Vector4 *RGBAArray = m_RGBABuffer->Get_Array();
		uint8 *angleArray = m_angleBuffer->Get_Array();
		uint8 *frameArray = m_frameBuffer->Get_Array();
		const UnsignedInt grid = textureGrid(*sys);
		const ParticleSystemTemplate *sysTemplate = sys->getTemplate();
		const Coord3D *pos;
		const RGBColor *color;
		Real psize;
#ifdef RTS_RENDERER_D3D9
		// Tiberium Project @feature a system can say whether it takes the light (Lighting)
		const Int lighting = sysTemplate != nullptr ? sysTemplate->getLighting() : (Int)ParticleSystemTemplate::PARTICLE_LIGHTING_AUTO;
		const Bool litSystem = litParticles && lighting != ParticleSystemTemplate::PARTICLE_LIGHTING_UNLIT
			&& (lighting == ParticleSystemTemplate::PARTICLE_LIGHTING_LIT
				|| sys->getShaderType() == ParticleSystemInfo::ALPHA || sys->getShaderType() == ParticleSystemInfo::ALPHA_TEST);
#endif

		//set-up all the per-particle
		for (Particle *p = sys->getFirstParticle(); p; p = p->m_systemNext)
		{
			if (p->isCulled())
				continue;

			pos = p->getPosition();
			psize = p->getSize();

			m_fieldParticleCount += ( sys->getPriority() == AREA_EFFECT && sys->isFieldParticle() );

			//@todo lorenzen sez: use pointer arithmetic for these arrays
			personalities[pointCount] = p->getPersonality();
			lightningBolts[pointCount] = p->m_lightningBolt;	// Tiberium Project: which bolt a lightning point is of

			posArray[pointCount].X = pos->x;
			posArray[pointCount].Y = pos->y;
			posArray[pointCount].Z = pos->z;

			sizeArray[pointCount] = psize;

			color = p->getColor();
			RGBAArray[pointCount].X = color->red;
			RGBAArray[pointCount].Y = color->green;
			RGBAArray[pointCount].Z = color->blue;
			RGBAArray[pointCount].W = p->getAlpha();
#ifdef RTS_RENDERER_D3D9
			if (litSystem)
			{
				Real factor[3] = { sceneFactor[0], sceneFactor[1], sceneFactor[2] };
				if (pointLit)
				{
					const float where[3] = { pos->x, pos->y, pos->z };
					float nearLight[3];	// (not "near": a macro in the Windows headers)
					Sage2SceneClass::Get_Point_Light_Particle(where, nearLight);
					if (nearLight[0] + nearLight[1] + nearLight[2] > 0.0f)
					{
						for (Int c = 0; c < 3; ++c)
						{
							// in light, then to the display: up to half as bright again as by day
							Real light = sceneLight[c] + nearLight[c] / referenceLight[c];
							light = light < 0.0f ? 0.0f : (light > 1.5f ? 1.5f : light);
							factor[c] = powf(light, 1.0f / 2.2f);
						}
					}
				}
				RGBAArray[pointCount].X *= factor[0];
				RGBAArray[pointCount].Y *= factor[1];
				RGBAArray[pointCount].Z *= factor[2];
			}
#endif

			angleArray[pointCount] = (uint8)(p->getAngle() * 255.0f / (2.0f * PI));
			frameArray[pointCount] = grid > 1 ? particleFrame(*sysTemplate, *p, grid) : 0;

			if (++pointCount == MAX_POINTS_PER_GROUP)
			{
				if (!canBatch)
				{
					break;
				}

				// TheSuperHackers @info The Buffer is full mid-system so draw what we have and carry on with the SAME system.
				// This prevents particles being dropped. Bank the stats first as the flush resets count to 0.
				m_onScreenParticleCount += (pointCount - startCount);
				flushParticleBatch(rinfo, pointCount);
				initializeBatch(*sys, texture);
				startCount = 0;
			}
		}

		if (pointCount == startCount)
		{
			continue;	//this system has no particles to render
		}

		const UnsignedInt volumeParticleDepth = sys->getVolumeParticleDepth();

		// Handle drawing streak type particles.
		if ( sys->isUsingStreak() && (pointCount >= 2) )
		{
			m_streakLine->Reset_Line();

			m_streakLine->Set_Texture( texture.Peek() );
			switch( sys->getShaderType() )
			{
				case ParticleSystemInfo::ADDITIVE:
					m_streakLine->Set_Shader( ShaderClass::_PresetAdditiveSpriteShader );
					break;
				case ParticleSystemInfo::ALPHA:
					m_streakLine->Set_Shader( ShaderClass::_PresetAlphaSpriteShader );
					break;
				case ParticleSystemInfo::ALPHA_TEST:
					m_streakLine->Set_Shader( ShaderClass::_PresetATestSpriteShader );
					break;
				case ParticleSystemInfo::MULTIPLY:
					m_streakLine->Set_Shader( ShaderClass::_PresetMultiplicativeSpriteShader );
					break;
			}

			// Tiberium Project @feature lightning: each bolt (a burst's points, in order) a line of its own
			if (sysTemplate != nullptr && sysTemplate->isLightning())
			{
				UnsignedInt runStart = startCount;
				for (UnsignedInt i = startCount + 1; i <= pointCount; ++i)
				{
					if (i < pointCount && lightningBolts[i] == lightningBolts[runStart])
						continue;
					if (i - runStart >= 2)
					{
						m_streakLine->Reset_Line();
						m_streakLine->Set_LocsWidthsColors(
							i - runStart,
							m_posBuffer->Get_Array() + runStart,
							m_sizeBuffer->Get_Array() + runStart,
							m_RGBABuffer->Get_Array() + runStart,
							&personalities[runStart]
							);
						m_streakLine->Render( rinfo );
					}
					runStart = i;
				}
			}
			else
			{
			//UPDATE THE STREAK'S ARRAYS
			m_streakLine->Set_LocsWidthsColors(
				pointCount,
				m_posBuffer->Get_Array(),
				m_sizeBuffer->Get_Array(),
				m_RGBABuffer->Get_Array(),
				&personalities[0]
				);

			//WWASSERT( m_streakLine->Get_Num_Points() == pointCount );

			// This is the happy place for this!
			RGBAArray[0].X = 0;//eliminates the scissor edge on the trailing edge of the streak
			RGBAArray[0].Y = 0;
			RGBAArray[0].Z = 0;
			RGBAArray[0].W = 0;


			//RENDER STREAK!
			m_streakLine->Render( rinfo );
			}
			m_onScreenParticleCount += (pointCount - startCount);
			pointCount = startCount;
		}
		// Handle lone streak type particles by drawing them as regular particles.
		else if (sys->isUsingStreak() && (pointCount == 1))
		{
			m_onScreenParticleCount += (pointCount - startCount);
			initializeBatch(*sys, texture);
			flushParticleBatch(rinfo, pointCount);
			startCount = 0;
		}
		// Handle volumetric type particle systems.
		else if( sys->isUsingVolumeParticles() && volumeParticleDepth > DEFAULT_VOLUME_PARTICLE_DEPTH )
		{
			m_pointGroup->Set_Texture( texture.Peek() );
			m_pointGroup->Set_Flag( PointGroupClass::TRANSFORM, true );	// transform to screen space

			switch( sys->getShaderType() )
			{
				case ParticleSystemInfo::ADDITIVE:
					m_pointGroup->Set_Shader( ShaderClass::_PresetAdditiveSpriteShader );
					break;
				case ParticleSystemInfo::ALPHA:
					m_pointGroup->Set_Shader( ShaderClass::_PresetAlphaSpriteShader );
					break;
				case ParticleSystemInfo::ALPHA_TEST:
					m_pointGroup->Set_Shader( ShaderClass::_PresetATestSpriteShader );
					break;
				case ParticleSystemInfo::MULTIPLY:
					m_pointGroup->Set_Shader( ShaderClass::_PresetMultiplicativeSpriteShader );
					break;
			}

			/// @todo Use both QUADS and TRIS for particles
			m_pointGroup->Set_Point_Mode( PointGroupClass::QUADS );
			m_pointGroup->Set_Frame_Row_Column_Count_Log2( (unsigned char)(grid >= 16 ? 4 : grid >= 8 ? 3 : grid >= 4 ? 2 : grid >= 2 ? 1 : 0) );
			m_pointGroup->Set_Arrays( m_posBuffer, m_RGBABuffer, nullptr, m_sizeBuffer, m_angleBuffer, grid > 1 ? m_frameBuffer : nullptr, pointCount );
			m_pointGroup->Set_Billboard(sys->getParticleAlignment() == ParticleSystemInfo::PARTICLE_ALIGNMENT_BILLBOARD);

			// Tiberium Project @feature animated texture particles: the frames above (sprite sheets)
			m_pointGroup->Set_Point_Frame( 0 );

			m_pointGroup->RenderVolumeParticle( rinfo, volumeParticleDepth);
			m_onScreenParticleCount += (pointCount - startCount);
			pointCount = startCount;
		}


		/// @todo lorenzen sez: this should be debug only:
		//add particle count to total
		m_onScreenParticleCount += (pointCount - startCount);

	/*
		// draw the wind vector for this particle system on the screen
		UnsignedInt width = TheDisplay->getWidth();
		UnsignedInt height = TheDisplay->getHeight();
		Coord3D worldStart, worldEnd;
		ICoord2D pixelStart, pixelEnd;
		sys->getPosition( &worldStart );
		worldEnd.x = Cos( sys->getWindAngle() ) * 50.0f + worldStart.x;
		worldEnd.y = Sin( sys->getWindAngle() ) * 50.0f + worldStart.y;
		worldEnd.z = worldStart.z;
		TheTacticalView->worldToScreen( &worldStart, &pixelStart );
		TheTacticalView->worldToScreen( &worldEnd, &pixelEnd );
		Color colorStart = GameMakeColor( 255, 255, 255, 255 );
		Color colorEnd = GameMakeColor( 255, 128, 128, 255 );
		TheDisplay->drawLine( pixelStart.x, pixelStart.y, pixelEnd.x, pixelEnd.y, 1.0f, colorStart, colorEnd );
	*/


	}

	// TheSuperHackers @info Flush the last batch if one is pending.
	flushParticleBatch(rinfo, pointCount);

		/// @todo lorenzen sez: this should be debug only:
	TheParticleSystemManager->setOnScreenParticleCount(m_onScreenParticleCount);

	//Draw any particles belonging to weather effects
	if (TheSnowManager)
		((W3DSnowManager *)TheSnowManager)->render(rinfo);

	//Now process screen smudges which are particles that distort the background behind them.
	if(TheSmudgeManager)
	{
		((W3DSmudgeManager *)TheSmudgeManager)->render(rinfo);
	}
}

// Tiberium Project @feature geometry particles. Each particle: two quads crossing along its axis
// (its velocity, or turned in the world), one in the plane of the axis and its side, one in the plane
// of the axis and its up; as long as the template's length course, as wide as the particle's size.
// The texture's u runs along the axis (1 at the pivot, 0 at the far end), its v across, as C&C4's
// GpuParticlePerpendicularBottom/Center.fx lay it out.
UnsignedInt W3DParticleSystemManager::renderGeometryParticles(ParticleSystem& sys, const ParticleSystemTemplate& t, Bool lit, const Real sceneFactor[3])
{
	static std::vector<Vector3> s_loc;
	static std::vector<Vector4> s_color;
	static std::vector<Vector2> s_uv;
	s_loc.clear();
	s_color.clear();
	s_uv.clear();

	const UnsignedInt grid = textureGrid(sys);
	const Bool bottom = t.getGeometry() == ParticleSystemTemplate::PARTICLE_GEOMETRY_BOTTOM;
	const Int rotation = t.getGeometryRotation();
	Real radiusSum = 0.0f;
	UnsignedInt drawn = 0;
	for (Particle* p = sys.getFirstParticle(); p; p = p->m_systemNext)
	{
		if (p->isCulled())
			continue;
		const Coord3D* pos = p->getPosition();
		const Real width = p->getSize();
		const Real length = t.getGeometryLength(p->getPersonality(), p->getElapsedFrames());
		if (width <= 0.0f && length <= 0.0f)
			continue;

		// the particle's axis and the two directions across it
		Vector3 axis(1.0f, 0.0f, 0.0f), side(0.0f, 1.0f, 0.0f), up(0.0f, 0.0f, 1.0f);
		const Real angle = p->getAngle();
		if (rotation == ParticleSystemTemplate::PARTICLE_GEOMETRY_AROUND_VELOCITY)
		{
			Vector3 v(p->m_vel.x, p->m_vel.y, p->m_vel.z);
			if (v.Length2() > 1.0e-8f)
			{
				v.Normalize();
				axis = v;
				const Real flat = WWMath::Sqrt(v.X * v.X + v.Y * v.Y);
				side = flat > 1.0e-4f ? Vector3(-v.Y / flat, v.X / flat, 0.0f) : Vector3(0.0f, 1.0f, 0.0f);
				Vector3::Cross_Product(axis, side, &up);
			}
			const Real c = WWMath::Cos(angle), s = WWMath::Sin(angle);
			const Vector3 turnedSide = side * c + up * s;
			up = up * c - side * s;
			side = turnedSide;
		}
		else if (rotation == ParticleSystemTemplate::PARTICLE_GEOMETRY_IN_WORLD)
		{
			const Real c = WWMath::Cos(angle), s = WWMath::Sin(angle);
			axis.Set(c, s, 0.0f);
			side.Set(-s, c, 0.0f);
		}

		const Vector3 at(pos->x, pos->y, pos->z);
		const Vector3 nearEnd = bottom ? at : at - axis * (length * 0.5f);
		const Vector3 farEnd = nearEnd + axis * length;
		const Vector3 across[2] = { side * (width * 0.5f), up * (width * 0.5f) };

		// the texture: the whole of it, or the particle's frame of the sheet
		Real u0 = 0.0f, v0 = 0.0f, cell = 1.0f;
		if (grid > 1)
		{
			const UnsignedInt frame = particleFrame(t, *p, grid);
			cell = 1.0f / (Real)grid;
			u0 = (Real)(frame % grid) * cell;
			v0 = (Real)(frame / grid) * cell;
		}

		const RGBColor* color = p->getColor();
		Vector4 rgba(color->red, color->green, color->blue, p->getAlpha());
		if (lit)
		{
			rgba.X *= sceneFactor[0];
			rgba.Y *= sceneFactor[1];
			rgba.Z *= sceneFactor[2];
		}
		for (Int q = 0; q < 2; ++q)
		{
			s_loc.push_back(farEnd + across[q]);
			s_loc.push_back(farEnd - across[q]);
			s_loc.push_back(nearEnd - across[q]);
			s_loc.push_back(nearEnd + across[q]);
			s_uv.push_back(Vector2(u0, v0));
			s_uv.push_back(Vector2(u0, v0 + cell));
			s_uv.push_back(Vector2(u0 + cell, v0 + cell));
			s_uv.push_back(Vector2(u0 + cell, v0));
			for (Int k = 0; k < 4; ++k)
				s_color.push_back(rgba);
		}
		radiusSum += 0.5f * (width > length ? width : length);
		++drawn;
	}
	if (drawn == 0)
		return 0;

	RefCountPtr<TextureClass> texture;
	texture.Assign_No_Add_Ref(W3DDisplay::m_assetManager->Get_Texture(sys.getParticleTypeName().str()));
	const Bool bright = isBright(sys);
	ShaderClass shader = ShaderClass::_PresetAdditiveSpriteShader;
	switch (sys.getShaderType())
	{
		case ParticleSystemInfo::ALPHA:
			shader = bright ? brightShader(ShaderClass::_PresetAlphaSpriteShader) : ShaderClass::_PresetAlphaSpriteShader;
			break;
		case ParticleSystemInfo::ALPHA_TEST:
			shader = bright ? brightShader(ShaderClass::_PresetATestSpriteShader) : ShaderClass::_PresetATestSpriteShader;
			break;
		case ParticleSystemInfo::MULTIPLY:
			shader = ShaderClass::_PresetMultiplicativeSpriteShader;
			break;
		default:
			shader = bright ? brightShader(ShaderClass::_PresetAdditiveSpriteShader) : ShaderClass::_PresetAdditiveSpriteShader;
			break;
	}
#ifdef RTS_RENDERER_D3D9
	DX8Wrapper::Set_Soft_Particle(true, radiusSum / (Real)drawn);
#endif
	PointGroupClass::Render_World_Quads(&s_loc[0], &s_color[0], &s_uv[0], (int)s_loc.size(), shader, texture.Peek());
#ifdef RTS_RENDERER_D3D9
	DX8Wrapper::Set_Soft_Particle(false);
#endif
	return drawn;
}

Bool W3DParticleSystemManager::finishedBatch(const ParticleSystem& system, const RefCountPtr<TextureClass>& texture)
{
	return texture.Peek() != m_batchTexture.Peek() ||
		system.getShaderType() != m_batchShaderType ||
		system.getParticleAlignment() != m_batchParticleAlignment ||
		textureGrid(system) != m_batchGrid ||
		isBright(system) != m_batchBright;
}

void W3DParticleSystemManager::initializeBatch(const ParticleSystem& system, const RefCountPtr<TextureClass>& texture)
{
	m_batchTexture = texture;
	m_batchShaderType = system.getShaderType();
	m_batchParticleAlignment = system.getParticleAlignment();
	m_batchGrid = textureGrid(system);
	m_batchBright = isBright(system);
}

void W3DParticleSystemManager::flushParticleBatch(RenderInfoClass& rinfo, UnsignedInt& pointCount)
{
	if (pointCount > 0)
	{
		m_pointGroup->Set_Texture(m_batchTexture.Peek());

		switch (m_batchShaderType)
		{
		case ParticleSystemInfo::ADDITIVE:
			m_pointGroup->Set_Shader(m_batchBright ? brightShader(ShaderClass::_PresetAdditiveSpriteShader) : ShaderClass::_PresetAdditiveSpriteShader);
			break;
		case ParticleSystemInfo::ALPHA:
			m_pointGroup->Set_Shader(m_batchBright ? brightShader(ShaderClass::_PresetAlphaSpriteShader) : ShaderClass::_PresetAlphaSpriteShader);
			break;
		case ParticleSystemInfo::ALPHA_TEST:
			m_pointGroup->Set_Shader(m_batchBright ? brightShader(ShaderClass::_PresetATestSpriteShader) : ShaderClass::_PresetATestSpriteShader);
			break;
		case ParticleSystemInfo::MULTIPLY:
			m_pointGroup->Set_Shader(ShaderClass::_PresetMultiplicativeSpriteShader);
			break;
		}

		m_pointGroup->Set_Flag(PointGroupClass::TRANSFORM, true);
		m_pointGroup->Set_Point_Mode(PointGroupClass::QUADS);
		// Tiberium Project @feature sprite-sheet particles: each particle's frame of the batch's grid
		const UnsignedInt grid = m_batchGrid;
		m_pointGroup->Set_Frame_Row_Column_Count_Log2((unsigned char)(grid >= 16 ? 4 : grid >= 8 ? 3 : grid >= 4 ? 2 : grid >= 2 ? 1 : 0));
		m_pointGroup->Set_Arrays(m_posBuffer, m_RGBABuffer, nullptr, m_sizeBuffer, m_angleBuffer, grid > 1 ? m_frameBuffer : nullptr, pointCount);
		m_pointGroup->Set_Billboard(m_batchParticleAlignment == ParticleSystemInfo::PARTICLE_ALIGNMENT_BILLBOARD);
		m_pointGroup->Set_Point_Frame(0);
#ifdef RTS_RENDERER_D3D9
		// Tiberium Project @feature soft particles: billboards fade where they meet the scene, as
		// C&C4's particles do (ground-aligned ones lie on it and stay as they are)
		// Tiberium Project @tweak over the batch's particles' average radius (C&C4 fades by size)
		{
			const float *sizes = m_sizeBuffer->Get_Array();
			float sum = 0.0f;
			for (UnsignedInt i = 0; i < pointCount; ++i)
				sum += sizes[i];
			DX8Wrapper::Set_Soft_Particle(m_batchParticleAlignment == ParticleSystemInfo::PARTICLE_ALIGNMENT_BILLBOARD,
				0.5f * sum / (float)pointCount);
		}
#endif
		m_pointGroup->Render(rinfo);
#ifdef RTS_RENDERER_D3D9
		DX8Wrapper::Set_Soft_Particle(false);
#endif

		pointCount = 0;
	}

	m_batchTexture.Clear();
	m_batchParticleAlignment = ParticleSystemInfo::PARTICLE_ALIGNMENT_BILLBOARD;
	m_batchShaderType = ParticleSystemInfo::INVALID_SHADER;
	m_batchGrid = 1;
	m_batchBright = FALSE;
}
