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

// FILE: W3DParticleSys.h /////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameClient/ParticleSys.h"
#include "WW3D2/pointgr.h"
#include "WW3D2/streak.h"
#include "WW3D2/rinfo.h"
#include "WWLib/bittype.h"
#include "WWMath/vector3.h"
#include <vector>

class RenderObjClass;
class MeshClass;
class VertexMaterialClass;

//=============================================================================
/** W3D implementation of the game display which is responsible for creating
  * all interaction with the screen and updating the display
	*/
class W3DParticleSystemManager : public ParticleSystemManager
{

public:
	W3DParticleSystemManager();
	virtual ~W3DParticleSystemManager() override;

	virtual void reset() override;									///< Tiberium Project: also lets go of the model particles' copies

	virtual void doParticles(RenderInfoClass &rinfo) override;
	virtual void queueParticleRender() override;
	///< returns the number of particles shown on screen per frame
	virtual Int getOnScreenParticleCount() override { return m_onScreenParticleCount; }

private:
	Bool finishedBatch(const ParticleSystem& system, const RefCountPtr<TextureClass>& texture);
	void initializeBatch(const ParticleSystem& system, const RefCountPtr<TextureClass>& texture);
	void flushParticleBatch(RenderInfoClass& rinfo, UnsignedInt& pointCount);
	/// Tiberium Project @feature geometry particles: the system's particles as crossed quads (see
	/// ParticleSystemTemplate::ParticleGeometry), each colour times its light factor; returns how many
	UnsignedInt renderGeometryParticles(ParticleSystem& sys, const ParticleSystemTemplate& t, Bool lit, const Real sceneFactor[3]);

	/// Tiberium Project @feature model particles (see ParticleSystemTemplate's Model): a copy of each model for
	/// each of its particles drawn, kept from frame to frame. Each copy's meshes and materials are its own, so the
	/// fade and tint set on its materials hold whichever pass draws it (the sorted one too).
	struct ModelMaterial
	{
		VertexMaterialClass *material;
		Vector3 ambient, diffuse, emissive;		///< as the model has them
		float opacity;
		Int blend;								///< ModelBlend: how its mesh blends, so how it fades
	};
	enum ModelBlend
	{
		MODEL_BLEND_OPAQUE = 0,					///< not faded
		MODEL_BLEND_ALPHA,						///< faded by its opacity
		MODEL_BLEND_ADDITIVE					///< faded by its colours (an additive blend takes no opacity)
	};
	struct ModelCopy
	{
		RenderObjClass *robj;
		std::vector<MeshClass*> meshes;			///< (held by robj)
		std::vector<ModelMaterial> materials;
	};
	struct ModelPool
	{
		AsciiString name;
		std::vector<ModelCopy> copies;
		UnsignedInt used;						///< the copies drawn this frame
		Bool missing;							///< the model could not be made
	};
	enum { MAX_MODEL_COPIES = 256 };			///< of one model at once
	void renderModelParticles(RenderInfoClass &rinfo);
	ModelPool *findModelPool(const AsciiString &name);
	ModelCopy *takeModelCopy(ModelPool &pool);
	void releaseModelPools();
	std::vector<ModelPool*> m_modelPools;

	enum { MAX_POINTS_PER_GROUP = 512 };

	RefCountPtr<TextureClass> m_batchTexture;		///< the texture used as the drawing surface for batched particle draws
	PointGroupClass *m_pointGroup;							///< the point group that contains all of the particles
	StreakLineClass *m_streakLine;							///< the streak class that contains all of the streaks
	ShareBufferClass<Vector3> *m_posBuffer;			///< array of particle positions
	ShareBufferClass<Vector4> *m_RGBABuffer;		///< array of particle color and alpha
	ShareBufferClass<float> *m_sizeBuffer;			///< array of particle sizes
	ShareBufferClass<uint8> *m_angleBuffer;			///< array of particle orientations
	ShareBufferClass<uint8> *m_frameBuffer;			///< Tiberium Project @feature array of particle texture frames

	ParticleSystemInfo::ParticleShaderType m_batchShaderType;
	Bool m_readyToRender;											///< if true, it is OK to render
	ParticleSystemInfo::ParticleAlignmentType m_batchParticleAlignment;
	Bool m_batchBright;									///< Tiberium Project @feature the batch's colours drawn twice as bright (BrightColors)
	UnsignedInt m_batchGrid;								///< Tiberium Project @feature the batch texture's frame grid (1: none)
};
