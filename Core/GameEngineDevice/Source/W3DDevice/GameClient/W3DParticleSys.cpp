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

#include "Common/GlobalData.h"
#include "GameClient/Color.h"
#include "W3DDevice/GameClient/W3DParticleSys.h"
#include "W3DDevice/GameClient/W3DAssetManager.h"
#include "W3DDevice/GameClient/W3DDisplay.h"
#include "W3DDevice/GameClient/HeightMap.h"
#include "W3DDevice/GameClient/W3DSmudge.h"
#include "W3DDevice/GameClient/W3DSnow.h"
#include "WW3D2/camera.h"
#ifdef RTS_RENDERER_D3D9
#include <math.h>
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
