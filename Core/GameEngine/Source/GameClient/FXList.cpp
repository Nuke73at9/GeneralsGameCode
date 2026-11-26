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

// FILE: FXList.cpp ///////////////////////////////////////////////////////////////////////////////
// Author: Steven Johnson, December 2001
// Desc:   FXList descriptions
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameClient/FXList.h"

#include "Common/DrawModule.h"
#include "Common/GameAudio.h"
#include "Common/GameUtility.h"
#include "Common/INI.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/RandomValue.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingFactory.h"
#include "Common/TiberiumTest.h"	// Tiberium Project (the test: the dynamic decals)

#include "GameLogic/Object.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/TerrainLogic.h"
#include "GameClient/Display.h"
#include "GameClient/GameClient.h"
#include "GameClient/Drawable.h"
#include "GameClient/ParticleSys.h"
#include "GameClient/Shadow.h"	// Tiberium Project (the dynamic decals)
#include "GameLogic/PartitionManager.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC DATA ////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////


FXListStore *TheFXListStore = nullptr;					///< the FXList store definition

//-------------------------------------------------------------------------------------------------
static void adjustVector(Coord3D *vec, const Matrix3D* mtx)
{
	if (mtx)
	{
		Vector3 vectmp;
		vectmp.X = vec->x;
		vectmp.Y = vec->y;
		vectmp.Z = vec->z;
		vectmp = mtx->Rotate_Vector(vectmp);
		vec->x = vectmp.X;
		vec->y = vectmp.Y;
		vec->z = vectmp.Z;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE CLASSES ///////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
void FXNugget::doFXObj(const Object* primary, const Object* secondary) const
{
	const Coord3D* p = primary ? primary->getPosition() : nullptr;
	const Matrix3D* mtx = primary ? primary->getTransformMatrix() : nullptr;
	const Real speed = 0.0f;	// yes, that's right -- NOT the object's speed.
	const Coord3D* s = secondary ? secondary->getPosition() : nullptr;
	doFXPos(p, mtx, speed, s);
}

//-------------------------------------------------------------------------------------------------
class SoundFXNugget : public FXNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(SoundFXNugget, "SoundFXNugget")

public:

	virtual void doFXPos(const Coord3D *primary, const Matrix3D* /*primaryMtx*/, const Real /*primarySpeed*/, const Coord3D * /*secondary*/, const Real /*overrideRadius*/ ) const override
	{
		AudioEventRTS sound(m_soundName);

		if (primary)
		{
			sound.setPosition(primary);
		}

		TheAudio->addAudioEvent(&sound);
	}

	virtual void doFXObj(const Object* primary, const Object* secondary = nullptr) const override
	{
		AudioEventRTS sound(m_soundName);
		if (primary)
		{
			sound.setPlayerIndex(primary->getControllingPlayer()->getPlayerIndex());
			sound.setPosition(primary->getPosition());
		}

		TheAudio->addAudioEvent(&sound);
	}


	static void parse(INI *ini, void *instance, void* /*store*/, const void* /*userData*/)
	{
		static const FieldParse myFieldParse[] =
		{
			{ "Name",									INI::parseAsciiString,	nullptr, offsetof( SoundFXNugget, m_soundName ) },
			{ nullptr, nullptr, nullptr, 0 }
		};

		SoundFXNugget* nugget = newInstance(SoundFXNugget);
		ini->initFromINI(nugget, myFieldParse);
		((FXList*)instance)->addFXNugget(nugget);
	}

private:
	AsciiString		m_soundName;
};
EMPTY_DTOR(SoundFXNugget)

//-------------------------------------------------------------------------------------------------
static Real calcDist(const Coord3D& src, const Coord3D& dst)
{
  Real dx = dst.x - src.x;
  Real dy = dst.y - src.y;
  Real dz = dst.z - src.z;
  return sqrt(dx*dx + dy*dy + dz*dz);
}

//-------------------------------------------------------------------------------------------------
class TracerFXNugget : public FXNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(TracerFXNugget, "TracerFXNugget")
public:

	TracerFXNugget()
	{
		m_tracerName.set("GenericTracer");
    m_boneName.clear();
    m_speed = 0.0f; // means "use passed-in speed"
    m_decayAt = 1.0f;
		m_length = 10.0f;
		m_width = 1.0f;
		m_color.red = m_color.green = m_color.blue = 1.0f;
		m_probability = 1.0f;
		// Tiberium Project @feature C&C4 and Kane's Wrath's tracers (TracerModelDraw)
		m_count = 1.0f;
		m_spreadAngle = 0.0f;
		m_lifeFrames = 0;
		m_hitFX = nullptr;
		m_hitFXLookedUp = FALSE;
	}

	virtual void doFXPos(const Coord3D *primary, const Matrix3D* primaryMtx, const Real primarySpeed, const Coord3D *secondary, const Real /*overrideRadius*/ ) const override
	{
		if (m_probability <= GameClientRandomValueReal(0, 1))
			return;

		// Tiberium Project @feature tracers a shot: Count's whole part, and one more by its fraction
		Int count = 1;
		if (m_count != 1.0f)
		{
			count = REAL_TO_INT_FLOOR(m_count);
			if (GameClientRandomValueReal(0, 1) < m_count - (Real)count)
				++count;
		}
		for (Int k = 0; k < count; ++k)
			fireOne(primary, primarySpeed, secondary);
	}

	void fireOne(const Coord3D *primary, const Real primarySpeed, const Coord3D *secondary) const
	{
		if (primary && secondary)
		{
			Drawable *tracer = TheThingFactory->newDrawable(TheThingFactory->findTemplate(m_tracerName));
			if(!tracer)
				return;

			//Kris -- Redid this section Sept 18, 2002
			//Calculate tracer orientations to face from primary to secondary position. This
			//should be the direction that the projectile is being fired towards. It doesn't make
			//sense that the old stuff made use of the muzzle fx bone orientation (because it's a
			//subobject). It had other problems because of elevation variations the tracers would
			//stay on the ground.
			//tracer->setTransformMatrix(primaryMtx);
			Matrix3D tracerMtx;
			Vector3 pos( primary->x, primary->y, primary->z );
			Vector3 dir( secondary->x - primary->x, secondary->y - primary->y, secondary->z - primary->z );
			dir.Normalize(); //This is fantastically crucial for calling buildTransformMatrix!!!!!
			// Tiberium Project @feature a tracer's spread: up to SpreadAngle off the line to the target, evenly over the cone's disc
			if (m_spreadAngle > 0.0f)
			{
				Vector3 across, other;
				Vector3::Cross_Product( dir, fabs( dir.Z ) < 0.9f ? Vector3( 0.0f, 0.0f, 1.0f ) : Vector3( 1.0f, 0.0f, 0.0f ), &across );
				across.Normalize();
				Vector3::Cross_Product( dir, across, &other );
				const Real tilt = m_spreadAngle * sqrtf( GameClientRandomValueReal( 0.0f, 1.0f ) );
				const Real turn = GameClientRandomValueReal( 0.0f, 2.0f * PI );
				dir = dir * Cos( tilt ) + ( across * Cos( turn ) + other * Sin( turn ) ) * Sin( tilt );
				dir.Normalize();
			}
			tracerMtx.buildTransformMatrix( pos, dir );
			tracer->setTransformMatrix( &tracerMtx );
			tracer->setPosition(primary);

			Real speed = m_speed;
			if (speed == 0.0f)
			{
				speed = primarySpeed;
			}

			TracerDrawInterface* tdi = nullptr;
			for (DrawModule** d = tracer->getDrawModules(); *d; ++d)
			{
				if ((tdi = (*d)->getTracerDrawInterface()) != nullptr)
				{
					tdi->setTracerParms(speed, m_length, m_width, m_color, 1.0f);
				}
			}

			// estimate how long it will take us to get to the destination
			Real dist = calcDist(*primary, *secondary) - m_length;
			Real frames = (dist >= 0.0f && speed >= 0.0f) ? (dist / speed) : 1;
			Int framesAdjusted = REAL_TO_INT_CEIL(frames * m_decayAt);
			// Tiberium Project @feature a tracer lives no longer than LifeFrames, and plays HitFX where it lands
			const Bool cut = m_lifeFrames > 0 && framesAdjusted >= (Int)m_lifeFrames;
			if (cut)
				framesAdjusted = (Int)m_lifeFrames;
			if (!m_hitFXLookedUp)
			{
				m_hitFXLookedUp = TRUE;
				m_hitFX = m_hitFXName.isEmpty() ? nullptr : TheFXListStore->findFXList( m_hitFXName.str() );
			}
			if (m_hitFX != nullptr)
			{
				TracerDrawInterface* hit = nullptr;
				for (DrawModule** d = tracer->getDrawModules(); *d && hit == nullptr; ++d)
					hit = (*d)->getTracerDrawInterface();
				// (one frame more, for its head to come there while it is still drawn; one cut short by its life never does)
				if (hit != nullptr && !cut)
				{
					hit->setTracerHit( m_hitFX, calcDist(*primary, *secondary) );
					++framesAdjusted;
				}
			}
			tracer->setExpirationDate(TheGameLogic->getFrame() + framesAdjusted);
		}
		else
		{
			DEBUG_CRASH(("You must have a primary and secondary source for this effect"));
		}
	}

	static void parse(INI *ini, void *instance, void* /*store*/, const void* /*userData*/)
	{
		static const FieldParse myFieldParse[] =
		{
			{ "TracerName",			INI::parseAsciiString,			nullptr, offsetof( TracerFXNugget, m_tracerName ) },
			{ "BoneName",				INI::parseAsciiString,			nullptr, offsetof( TracerFXNugget, m_boneName ) },
      { "Speed",          INI::parseVelocityReal,     nullptr, offsetof( TracerFXNugget, m_speed ) },
      { "DecayAt",        INI::parseReal,             nullptr, offsetof( TracerFXNugget, m_decayAt ) },
      { "Length",					INI::parseReal,             nullptr, offsetof( TracerFXNugget, m_length ) },
      { "Width",					INI::parseReal,             nullptr, offsetof( TracerFXNugget, m_width ) },
      { "Color",					INI::parseRGBColor,					nullptr, offsetof( TracerFXNugget, m_color ) },
      { "Probability",		INI::parseReal,             nullptr, offsetof( TracerFXNugget, m_probability ) },
			// Tiberium Project @feature C&C4 and Kane's Wrath's tracers
			{ "Count",					INI::parseReal,							nullptr, offsetof( TracerFXNugget, m_count ) },
			{ "SpreadAngle",		INI::parseAngleReal,				nullptr, offsetof( TracerFXNugget, m_spreadAngle ) },
			{ "LifeFrames",			INI::parseUnsignedInt,			nullptr, offsetof( TracerFXNugget, m_lifeFrames ) },
			{ "HitFX",					INI::parseAsciiString,			nullptr, offsetof( TracerFXNugget, m_hitFXName ) },
			{ nullptr, nullptr, nullptr, 0 }
		};

		TracerFXNugget* nugget = newInstance( TracerFXNugget );
		ini->initFromINI(nugget, myFieldParse);
		((FXList*)instance)->addFXNugget(nugget);
	}

private:
	AsciiString			m_tracerName;
  AsciiString     m_boneName;
  Real            m_speed;
  Real            m_decayAt;
	Real						m_length;
	Real						m_width;
	RGBColor				m_color;
	Real						m_probability;
	// Tiberium Project @feature C&C4 and Kane's Wrath's tracers (TracerModelDraw)
	Real						m_count;				///< Count: tracers a shot on average (its fraction a chance of one more)
	Real						m_spreadAngle;	///< SpreadAngle: the most a tracer is off the line to the target
	UnsignedInt			m_lifeFrames;		///< LifeFrames: the longest a tracer lives (0: until it gets there)
	AsciiString			m_hitFXName;		///< HitFX: played where a tracer lands
	mutable const FXList *m_hitFX;
	mutable Bool		m_hitFXLookedUp;
};
EMPTY_DTOR(TracerFXNugget)

//-------------------------------------------------------------------------------------------------
class RayEffectFXNugget : public FXNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(RayEffectFXNugget, "RayEffectFXNugget")
public:

	RayEffectFXNugget()
	{
		m_templateName.clear();
		m_primaryOffset.x = m_primaryOffset.y = m_primaryOffset.z = 0;
		m_secondaryOffset.x = m_secondaryOffset.y = m_secondaryOffset.z = 0;
	}

	virtual void doFXPos(const Coord3D *primary, const Matrix3D* /*primaryMtx*/, const Real /*primarySpeed*/, const Coord3D * secondary, const Real /*overrideRadius*/ ) const override
	{
		const ThingTemplate* tmpl = TheThingFactory->findTemplate(m_templateName);
		DEBUG_ASSERTCRASH(tmpl, ("RayEffect %s not found",m_templateName.str()));
		if (primary && secondary && tmpl)
		{
			Coord3D sourcePos = *primary;
			sourcePos.x += m_primaryOffset.x;
			sourcePos.y += m_primaryOffset.y;
			sourcePos.z += m_primaryOffset.z;

			Coord3D targetPos = *secondary;
			targetPos.x += m_secondaryOffset.x;
			targetPos.y += m_secondaryOffset.y;
			targetPos.z += m_secondaryOffset.z;

			TheGameClient->createRayEffectByTemplate(&sourcePos, &targetPos, tmpl);
		}
		else
		{
			DEBUG_CRASH(("You must have a primary AND secondary source for this effect"));
		}
	}

	static void parse(INI *ini, void *instance, void* /*store*/, const void* /*userData*/)
	{
		static const FieldParse myFieldParse[] =
		{
			{ "Name",									INI::parseAsciiString,			nullptr, offsetof( RayEffectFXNugget, m_templateName ) },
			{ "PrimaryOffset",				INI::parseCoord3D,					nullptr, offsetof( RayEffectFXNugget, m_primaryOffset ) },
			{ "SecondaryOffset",			INI::parseCoord3D,					nullptr, offsetof( RayEffectFXNugget, m_secondaryOffset ) },
			{ nullptr, nullptr, nullptr, 0 }
		};

		RayEffectFXNugget* nugget = newInstance( RayEffectFXNugget );
		ini->initFromINI(nugget, myFieldParse);
		((FXList*)instance)->addFXNugget(nugget);
	}

private:
	AsciiString			m_templateName;
	Coord3D					m_primaryOffset;
	Coord3D					m_secondaryOffset;
};
EMPTY_DTOR(RayEffectFXNugget)

//-------------------------------------------------------------------------------------------------
class LightPulseFXNugget : public FXNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(LightPulseFXNugget, "LightPulseFXNugget")
public:

	LightPulseFXNugget() : m_radius(0), m_increaseFrames(0), m_decreaseFrames(0), m_boundingCirclePct(0)
	{
		m_color.red = m_color.green = m_color.blue = 0;
	}

	virtual void doFXObj(const Object* primary, const Object* /*secondary*/) const override
	{
		if (primary)
		{
			Real radius = m_radius;

			if (m_boundingCirclePct > 0)
				radius = (primary->getGeometryInfo().getBoundingCircleRadius() * m_boundingCirclePct);

			TheDisplay->createLightPulse(primary->getPosition(), &m_color, 1, radius, m_increaseFrames, m_decreaseFrames);
		}
		else
		{
			DEBUG_CRASH(("You must have a primary source for this effect"));
		}
	}

	virtual void doFXPos(const Coord3D *primary, const Matrix3D* /*primaryMtx*/, const Real /*primarySpeed*/, const Coord3D * /*secondary*/, const Real /*overrideRadius*/ ) const override
	{
		if (primary)
		{
			TheDisplay->createLightPulse(primary, &m_color, 1, m_radius, m_increaseFrames, m_decreaseFrames);
		}
		else
		{
			DEBUG_CRASH(("You must have a primary source for this effect"));
		}
	}

	static void parse(INI *ini, void *instance, void* /*store*/, const void* /*userData*/)
	{
		static const FieldParse myFieldParse[] =
		{
			{ "Color",						INI::parseRGBColor,								nullptr, offsetof( LightPulseFXNugget, m_color ) },
			{ "Radius",						INI::parseReal,										nullptr, offsetof( LightPulseFXNugget, m_radius ) },
			{ "RadiusAsPercentOfObjectSize",		INI::parsePercentToReal,	nullptr, offsetof( LightPulseFXNugget, m_boundingCirclePct ) },
			{ "IncreaseTime",			INI::parseDurationUnsignedInt,	nullptr, offsetof( LightPulseFXNugget, m_increaseFrames ) },
			{ "DecreaseTime",			INI::parseDurationUnsignedInt,	nullptr, offsetof( LightPulseFXNugget, m_decreaseFrames ) },
			{ nullptr, nullptr, nullptr, 0 }
		};

		LightPulseFXNugget* nugget = newInstance( LightPulseFXNugget );
		ini->initFromINI(nugget, myFieldParse);
		((FXList*)instance)->addFXNugget(nugget);
	}

private:
	RGBColor			m_color;
	Real					m_radius;
	Real					m_boundingCirclePct;
	UnsignedInt		m_increaseFrames;
	UnsignedInt		m_decreaseFrames;
};
EMPTY_DTOR(LightPulseFXNugget)

//-------------------------------------------------------------------------------------------------
class ViewShakeFXNugget : public FXNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ViewShakeFXNugget, "ViewShakeFXNugget")
public:

	ViewShakeFXNugget() : m_shake(View::SHAKE_NORMAL), m_duration(0.0f), m_radius(300.0f), m_amplitude(10.0f), m_localPlayerOnly(FALSE)
	{
	}

	virtual void doFXPos(const Coord3D *primary, const Matrix3D* /*primaryMtx*/, const Real /*primarySpeed*/, const Coord3D * /*secondary*/, const Real /*overrideRadius*/ ) const override
	{
		if (primary)
		{
			if (TheTacticalView)
			{
				// Tiberium Project @feature C&C4's CameraShakerVolume: the camera turned about (Amplitude degrees at the
				// most) for Duration, fading with time and with its distance from the spot (Radius); the engine's
				// own camera shaker (the scripts' "add camera shaker")
				if (m_duration > 0.0f)
				{
					// the shaker measures from the camera's eye; this one from where the camera looks, on the ground
					// (as Zero Hour's own shakes do): moved by the eye's offset from that spot
					const Coord3D eye = TheTacticalView->get3DCameraPosition();
					const Coord3D look = TheTacticalView->getPosition();
					Coord3D at = *primary;
					at.x += eye.x - look.x;
					at.y += eye.y - look.y;
					at.z = eye.z;
					TheTacticalView->Add_Camera_Shake(at, m_radius, m_duration * SECONDS_PER_LOGICFRAME_REAL, m_amplitude);
				}
				else
					TheTacticalView->shake(primary, m_shake);
			}
		}
		else
		{
			DEBUG_CRASH(("You must have a primary source for this effect"));
		}
	}

	// Tiberium Project @feature LocalPlayerOnly: only for the local player's own objects (a Crawler's drop pod)
	virtual void doFXObj(const Object* primary, const Object* secondary = nullptr) const override
	{
		if (m_localPlayerOnly && primary != nullptr && ThePlayerList != nullptr
			&& primary->getControllingPlayer() != ThePlayerList->getLocalPlayer())
			return;
		FXNugget::doFXObj(primary, secondary);
	}

	static void parse(INI *ini, void *instance, void* /*store*/, const void* /*userData*/)
	{
		static const FieldParse myFieldParse[] =
		{
			{ "Type",				parseShakeType,								nullptr, offsetof( ViewShakeFXNugget, m_shake ) },
			// Tiberium Project @feature C&C4's CameraShakerVolume
			{ "Duration",		INI::parseDurationReal,				nullptr, offsetof( ViewShakeFXNugget, m_duration ) },
			{ "Radius",			INI::parseReal,								nullptr, offsetof( ViewShakeFXNugget, m_radius ) },
			{ "Amplitude",	INI::parseReal,								nullptr, offsetof( ViewShakeFXNugget, m_amplitude ) },
			{ "LocalPlayerOnly",	INI::parseBool,					nullptr, offsetof( ViewShakeFXNugget, m_localPlayerOnly ) },
			{ nullptr, nullptr, nullptr, 0 }
		};

		ViewShakeFXNugget* nugget = newInstance( ViewShakeFXNugget );
		ini->initFromINI(nugget, myFieldParse);
		((FXList*)instance)->addFXNugget(nugget);
	}

protected:
	static void parseShakeType( INI* ini, void *instance, void *store, const void* /*userData*/ )
	{
		static const LookupListRec shakeTypeNames[] =
		{
			{ "SUBTLE", View::SHAKE_SUBTLE },
			{ "NORMAL", View::SHAKE_NORMAL },
			{ "STRONG", View::SHAKE_STRONG },
			{ "SEVERE", View::SHAKE_SEVERE },
			{ "CINE_EXTREME", View::SHAKE_CINE_EXTREME },
			{ "CINE_INSANE",  View::SHAKE_CINE_INSANE },
			{ nullptr, 0 }
		};
		static_assert(ARRAY_SIZE(shakeTypeNames) == View::SHAKE_COUNT + 1, "Incorrect array size");

		*(Int *)store = INI::scanLookupList(ini->getNextToken(), shakeTypeNames);
	}

private:
	View::CameraShakeType m_shake;
	Real m_duration;				///< Tiberium Project: logic frames (0: Zero Hour's shake of its Type)
	Real m_radius;
	Real m_amplitude;				///< degrees
	Bool m_localPlayerOnly;

};
EMPTY_DTOR(ViewShakeFXNugget)

//-------------------------------------------------------------------------------------------------
class TerrainScorchFXNugget : public FXNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(TerrainScorchFXNugget, "TerrainScorchFXNugget")
public:

	TerrainScorchFXNugget() : m_scorch(-1), m_radius(0)
	{
	}

	virtual void doFXPos(const Coord3D *primary, const Matrix3D* /*primaryMtx*/, const Real /*primarySpeed*/, const Coord3D * /*secondary*/, const Real /*overrideRadius*/ ) const override
	{
		if (primary)
		{
			Int scorch = m_scorch;
			if (scorch < 0)
			{
				scorch = GameClientRandomValue( SCORCH_1, SCORCH_4 );
			}
			TheGameClient->addScorch(primary, m_radius, (Scorches)scorch);
		}
		else
		{
			DEBUG_CRASH(("You must have a primary source for this effect"));
		}
	}

	static void parse(INI *ini, void *instance, void* /*store*/, const void* /*userData*/)
	{
		static const FieldParse myFieldParse[] =
		{
			{ "Type",				parseScorchType,			nullptr, offsetof( TerrainScorchFXNugget, m_scorch ) },
			{ "Radius",			INI::parseReal,				nullptr, offsetof( TerrainScorchFXNugget, m_radius ) },
			{ nullptr, nullptr, nullptr, 0 }
		};

		TerrainScorchFXNugget* nugget = newInstance( TerrainScorchFXNugget );
		ini->initFromINI(nugget, myFieldParse);
		((FXList*)instance)->addFXNugget(nugget);
	}

protected:

	static void parseScorchType( INI* ini, void *instance, void *store, const void* /*userData*/ )
	{
		static const LookupListRec scorchTypeNames[] =
		{
			{ "SCORCH_1",				SCORCH_1 },
			{ "SCORCH_2",				SCORCH_2 },
			{ "SCORCH_3",				SCORCH_3 },
			{ "SCORCH_4",				SCORCH_4 },
			{ "SHADOW_SCORCH",	SHADOW_SCORCH },
			{ "RANDOM",					-1 },
			{ nullptr, 0 }
		};
		static_assert(ARRAY_SIZE(scorchTypeNames) == SCORCH_COUNT + 2, "Incorrect array size");

		*(Int *)store = INI::scanLookupList(ini->getNextToken(), scorchTypeNames);
	}

private:
	Int		m_scorch;
	Real	m_radius;
};
EMPTY_DTOR(TerrainScorchFXNugget)

//-------------------------------------------------------------------------------------------------
// Tiberium Project @feature C&C4's DynamicDecal: a texture laid on the ground for a time (its scorch and blast marks,
// the Ion Cannon's glow), its opacity from OpacityStart to OpacityPeak over FadeIn, held for PeakTime, to OpacityEnd
// over FadeOut, after StartingDelay; gone after Lifetime (counted from the effect). Drawn as the engine's decals
// (alpha blended, or added with Additive), on the logic's time (still while paused). Drawing only.
namespace
{
	struct DynamicDecalSpec
	{
		AsciiString texture;
		Real size;
		Bool additive;
		Real opacityStart, opacityPeak, opacityEnd;
		UnsignedInt fadeIn, peakTime, fadeOut, delay, lifetime;
	};

	struct DynamicDecal
	{
		const DynamicDecalSpec* spec;
		Coord3D pos;
		UnsignedInt start;			///< the logic frame of the effect
		Shadow* decal;					///< made when its delay is over
		Real angle;
	};

	std::vector<DynamicDecal> s_dynamicDecals;
	const size_t DYNAMIC_DECALS_MOST = 128;	///< (a walker's footprints last seconds each: several walkers' fit)

	Real dynamicDecalOpacity( const DynamicDecalSpec& d, UnsignedInt age )
	{
		if( age < d.fadeIn )
			return d.opacityStart + ( d.opacityPeak - d.opacityStart ) * ( (Real)age / (Real)d.fadeIn );
		age -= d.fadeIn;
		if( age < d.peakTime )
			return d.opacityPeak;
		age -= d.peakTime;
		if( age < d.fadeOut )
			return d.opacityPeak + ( d.opacityEnd - d.opacityPeak ) * ( (Real)age / (Real)d.fadeOut );
		return d.opacityEnd;
	}

	void releaseDynamicDecal( DynamicDecal& d )
	{
		if( d.decal != nullptr )
			d.decal->release();
		d.decal = nullptr;
	}
}

//-------------------------------------------------------------------------------------------------
void FXList::updateDynamicDecals()
{
	if( s_dynamicDecals.empty() || TheGameLogic == nullptr )
		return;
	const UnsignedInt now = TheGameLogic->getFrame();
	for( size_t i = 0; i < s_dynamicDecals.size(); )
	{
		DynamicDecal& d = s_dynamicDecals[i];
		const DynamicDecalSpec& spec = *d.spec;
		const UnsignedInt age = now >= d.start ? now - d.start : 0;
		if( age >= spec.lifetime )
		{
			if( TiberiumTest::isActive() )
				TiberiumTest::log( "frame %d: dynamic decal %s gone", (int)now, spec.texture.str() );
			releaseDynamicDecal( d );
			s_dynamicDecals.erase( s_dynamicDecals.begin() + i );
			continue;
		}
		if( age >= spec.delay )
		{
			if( d.decal == nullptr && TheProjectedShadowManager != nullptr )
			{
				Shadow::ShadowTypeInfo info;
				info.allowUpdates = FALSE;
				info.allowWorldAlign = TRUE;
				info.m_type = spec.additive ? SHADOW_ADDITIVE_DECAL : SHADOW_ALPHA_DECAL;
				strlcpy( info.m_ShadowName, spec.texture.str(), ARRAY_SIZE( info.m_ShadowName ) );
				info.m_sizeX = spec.size;
				info.m_sizeY = spec.size;
				d.decal = TheProjectedShadowManager->addDecal( &info );
				if( d.decal != nullptr )
				{
					d.decal->setAngle( d.angle );
					d.decal->setPosition( d.pos.x, d.pos.y, d.pos.z );
				}
				if( TiberiumTest::isActive() )
					TiberiumTest::log( "frame %d: dynamic decal %s %s at %.0f %.0f, %.0f across", (int)now, spec.texture.str(),
						d.decal != nullptr ? "laid" : "not laid", d.pos.x, d.pos.y, spec.size );
			}
			if( d.decal != nullptr )
			{
				Real o = dynamicDecalOpacity( spec, age - spec.delay );
				o = o < 0.0f ? 0.0f : ( o > 1.0f ? 1.0f : o );
				const UnsignedInt v = (UnsignedInt)REAL_TO_INT( o * 255.0f );
				// white, at the opacity: alpha for an alpha-blended decal, brightness for an added one
				d.decal->setDiffuseDirect( spec.additive ? ( 0xFF000000u | ( v << 16 ) | ( v << 8 ) | v ) : ( ( v << 24 ) | 0x00FFFFFFu ) );
			}
		}
		++i;
	}
}

//-------------------------------------------------------------------------------------------------
void FXList::resetDynamicDecals()
{
	for( size_t i = 0; i < s_dynamicDecals.size(); ++i )
		releaseDynamicDecal( s_dynamicDecals[i] );
	s_dynamicDecals.clear();
}

//-------------------------------------------------------------------------------------------------
class DynamicDecalFXNugget : public FXNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(DynamicDecalFXNugget, "DynamicDecalFXNugget")
public:

	DynamicDecalFXNugget() : m_onlyIfOnLand(FALSE)
	{
		m_spec.size = 50.0f;
		m_spec.additive = FALSE;
		m_spec.opacityStart = 0.0f;
		m_spec.opacityPeak = 1.0f;
		m_spec.opacityEnd = 0.0f;
		m_spec.fadeIn = 0;
		m_spec.peakTime = 0;
		m_spec.fadeOut = 0;
		m_spec.delay = 0;
		m_spec.lifetime = LOGICFRAMES_PER_SECOND * 10;
	}

	virtual void doFXPos(const Coord3D *primary, const Matrix3D* /*primaryMtx*/, const Real /*primarySpeed*/, const Coord3D * /*secondary*/, const Real /*overrideRadius*/ ) const override
	{
		if (primary == nullptr || m_spec.texture.isEmpty() || TheGameLogic == nullptr)
			return;
		if (m_onlyIfOnLand && TheTerrainLogic != nullptr && TheTerrainLogic->isUnderwater(primary->x, primary->y))
			return;
		if (s_dynamicDecals.size() >= DYNAMIC_DECALS_MOST)
		{
			releaseDynamicDecal( s_dynamicDecals.front() );
			s_dynamicDecals.erase( s_dynamicDecals.begin() );
		}
		DynamicDecal d;
		d.spec = &m_spec;
		d.pos = *primary;
		d.start = TheGameLogic->getFrame();
		d.decal = nullptr;
		d.angle = GameClientRandomValueReal( 0.0f, 2.0f * PI );	// each its own turn (inferred)
		s_dynamicDecals.push_back( d );
		FXList::updateDynamicDecals();
	}

	static void parse(INI *ini, void *instance, void* /*store*/, const void* /*userData*/)
	{
		static const FieldParse myFieldParse[] =
		{
			{ "Texture",				INI::parseAsciiString,					nullptr, offsetof( DynamicDecalFXNugget, m_spec ) + offsetof( DynamicDecalSpec, texture ) },
			{ "Size",						INI::parseReal,									nullptr, offsetof( DynamicDecalFXNugget, m_spec ) + offsetof( DynamicDecalSpec, size ) },
			{ "Additive",				INI::parseBool,									nullptr, offsetof( DynamicDecalFXNugget, m_spec ) + offsetof( DynamicDecalSpec, additive ) },
			{ "OpacityStart",		INI::parsePercentToReal,				nullptr, offsetof( DynamicDecalFXNugget, m_spec ) + offsetof( DynamicDecalSpec, opacityStart ) },
			{ "OpacityPeak",		INI::parsePercentToReal,				nullptr, offsetof( DynamicDecalFXNugget, m_spec ) + offsetof( DynamicDecalSpec, opacityPeak ) },
			{ "OpacityEnd",			INI::parsePercentToReal,				nullptr, offsetof( DynamicDecalFXNugget, m_spec ) + offsetof( DynamicDecalSpec, opacityEnd ) },
			{ "FadeIn",					INI::parseDurationUnsignedInt,	nullptr, offsetof( DynamicDecalFXNugget, m_spec ) + offsetof( DynamicDecalSpec, fadeIn ) },
			{ "PeakTime",				INI::parseDurationUnsignedInt,	nullptr, offsetof( DynamicDecalFXNugget, m_spec ) + offsetof( DynamicDecalSpec, peakTime ) },
			{ "FadeOut",				INI::parseDurationUnsignedInt,	nullptr, offsetof( DynamicDecalFXNugget, m_spec ) + offsetof( DynamicDecalSpec, fadeOut ) },
			{ "StartingDelay",	INI::parseDurationUnsignedInt,	nullptr, offsetof( DynamicDecalFXNugget, m_spec ) + offsetof( DynamicDecalSpec, delay ) },
			{ "Lifetime",				INI::parseDurationUnsignedInt,	nullptr, offsetof( DynamicDecalFXNugget, m_spec ) + offsetof( DynamicDecalSpec, lifetime ) },
			{ "OnlyIfOnLand",		INI::parseBool,									nullptr, offsetof( DynamicDecalFXNugget, m_onlyIfOnLand ) },
			{ nullptr, nullptr, nullptr, 0 }
		};

		DynamicDecalFXNugget* nugget = newInstance( DynamicDecalFXNugget );
		ini->initFromINI(nugget, myFieldParse);
		((FXList*)instance)->addFXNugget(nugget);
	}

private:
	DynamicDecalSpec m_spec;
	Bool m_onlyIfOnLand;
};
EMPTY_DTOR(DynamicDecalFXNugget)

//-------------------------------------------------------------------------------------------------
class ParticleSystemFXNugget : public FXNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ParticleSystemFXNugget, "ParticleSystemFXNugget")
public:

	ParticleSystemFXNugget()
	{
		m_name.clear();
		m_count = 1;
		m_radius.setRange(0, 0, GameClientRandomVariable::CONSTANT);
		m_height.setRange(0, 0, GameClientRandomVariable::CONSTANT);
		// -1 means "don't mess with it, just accept the particle-system's defaults"
		m_delay.setRange(-1, -1, GameClientRandomVariable::CONSTANT);
		m_offset.x = m_offset.y = m_offset.z = 0;
		m_orientToObject = false;
		m_attachToObject = false;
		m_createAtGroundHeight = FALSE;
		m_useCallersRadius = FALSE;
		m_useTarget = FALSE;
		m_ricochet = FALSE;	// Tiberium Project @bugfix was left unset (only a Ricochet line set it)
		m_rotateX = m_rotateY = m_rotateZ = 0;
	}

	virtual void doFXPos(const Coord3D *primary, const Matrix3D* primaryMtx, const Real /*primarySpeed*/, const Coord3D * secondary, const Real overrideRadius ) const override
	{
		if (primary)
		{
			// Tiberium Project @feature UseTarget (C&C4's): the system at the other end (a weapon's victim), turned
			// along the line of fire when it is to orient or ricochet (sparks thrown on past the hit)
			if (m_useTarget && secondary)
			{
				Matrix3D aimingMatrix(1);
				aimingMatrix.Rotate_Z( WWMath::Atan2( secondary->y - primary->y, secondary->x - primary->x ) );
				reallyDoFX(secondary, (m_orientToObject || m_ricochet) ? &aimingMatrix : nullptr, nullptr, overrideRadius, primary);
			}
			else
			{
				reallyDoFX(primary, primaryMtx, nullptr, overrideRadius, secondary);
			}
		}
		else
		{
			DEBUG_CRASH(("You must have a primary source for this effect"));
		}
	}

	virtual void doFXObj(const Object* primary, const Object* secondary) const override
	{
		// Tiberium Project @feature UseTarget: the system on the other object (see doFXPos)
		if (m_useTarget && primary && secondary)
		{
			reallyDoFX(secondary->getPosition(), secondary->getTransformMatrix(), secondary, 0.0f, primary->getPosition());
			return;
		}

		if (primary)
		{

			if (m_ricochet && secondary)
			{
				// HERE WE MUST BUILD A MATRIX WHICH WILL ORIENT THE NEW PARTICLE SYSTEM TO FACE AWAY FROM THE SECONDARY OBJECT
				// THE RESULT SHOULD LOOK LIKE THE DIRECTION OF THE "ATTACK" IS CARRIED THROUGH LIKE A RICOCHET
				Real deltaX = primary->getPosition()->x - secondary->getPosition()->x;
				Real deltaY = primary->getPosition()->y - secondary->getPosition()->y;
				Real aimingAngle = atan2(deltaY, deltaX);
				Matrix3D aimingMatrix(1);
				aimingMatrix.Rotate_Z( aimingAngle );

				reallyDoFX(primary->getPosition(), &aimingMatrix, primary, 0.0f, secondary->getPosition());
			}
			else
				// if we have an object, then adjust the offset and direction by the object's transformation
				// matrix, so that (say) an offset of +10 in the z axis "follows" the orientation of the object.
				reallyDoFX(primary->getPosition(), primary->getTransformMatrix(), primary, 0.0f, secondary ? secondary->getPosition() : nullptr);
		}
		else
		{
			DEBUG_CRASH(("You must have a primary source for this effect"));
		}
	}

	static void parse(INI *ini, void *instance, void* /*store*/, const void* /*userData*/)
	{
		static const FieldParse myFieldParse[] =
		{
			{ "Name",									INI::parseAsciiString,			nullptr, offsetof( ParticleSystemFXNugget, m_name ) },
			{ "Count",								INI::parseInt,							nullptr, offsetof( ParticleSystemFXNugget, m_count ) },
			{ "Offset",								INI::parseCoord3D,					nullptr, offsetof( ParticleSystemFXNugget, m_offset ) },
			{ "Radius",								INI::parseGameClientRandomVariable,		nullptr, offsetof( ParticleSystemFXNugget, m_radius ) },
			{ "Height",								INI::parseGameClientRandomVariable,		nullptr, offsetof( ParticleSystemFXNugget, m_height ) },
			{ "InitialDelay",					INI::parseGameClientRandomVariable,		nullptr, offsetof( ParticleSystemFXNugget, m_delay ) },
			{ "RotateX",							INI::parseAngleReal,				nullptr, offsetof( ParticleSystemFXNugget, m_rotateX ) },
			{ "RotateY",							INI::parseAngleReal,				nullptr, offsetof( ParticleSystemFXNugget, m_rotateY ) },
			{ "RotateZ",							INI::parseAngleReal,				nullptr, offsetof( ParticleSystemFXNugget, m_rotateZ ) },
			{ "OrientToObject",				INI::parseBool,							nullptr, offsetof( ParticleSystemFXNugget, m_orientToObject ) },
			{ "Ricochet",				      INI::parseBool,							nullptr, offsetof( ParticleSystemFXNugget, m_ricochet ) },
			{ "AttachToObject",				INI::parseBool,							nullptr, offsetof( ParticleSystemFXNugget, m_attachToObject ) },
			{ "CreateAtGroundHeight",	INI::parseBool,							nullptr, offsetof( ParticleSystemFXNugget, m_createAtGroundHeight ) },
			{ "UseCallersRadius",			INI::parseBool,							nullptr, offsetof( ParticleSystemFXNugget, m_useCallersRadius ) },
			{ "UseTarget",						INI::parseBool,							nullptr, offsetof( ParticleSystemFXNugget, m_useTarget ) },	// Tiberium Project
			{ nullptr, nullptr, nullptr, 0 }
		};

		ParticleSystemFXNugget* nugget = newInstance( ParticleSystemFXNugget );
		ini->initFromINI(nugget, myFieldParse);
		((FXList*)instance)->addFXNugget(nugget);
	}

protected:

	// otherEnd: the effect's other end, where a lightning system's bolt goes (Tiberium Project)
	void reallyDoFX(const Coord3D *primary, const Matrix3D* mtx, const Object* thingToAttachTo, Real overrideRadius, const Coord3D *otherEnd = nullptr ) const
	{
		Coord3D offset = m_offset;
		if (mtx)
		{
			adjustVector(&offset, mtx);
		}

		const ParticleSystemTemplate *tmp = TheParticleSystemManager->findTemplate(m_name);
		DEBUG_ASSERTCRASH(tmp, ("ParticleSystem %s not found",m_name.str()));
		if (tmp)
		{
			for (Int i = 0; i < m_count; i++ )
			{
				ParticleSystem *sys = TheParticleSystemManager->createParticleSystem(tmp);
				if (sys)
				{
					Coord3D newPos;
					Real radius = m_radius.getValue();
					Real angle = GameClientRandomValueReal( 0.0f, 2.0f * PI );

					newPos.x = primary->x + offset.x + radius * cos(angle);
					newPos.y = primary->y + offset.y + radius * sin(angle);
					newPos.z = primary->z + offset.z;
					if( m_createAtGroundHeight && TheTerrainLogic )
					{
						//old way:
						//newPos.z = TheTerrainLogic->getGrsoundHeight( newPos.x, newPos.y ) + 1;// The plus one prevents scissoring with terrain

						//new way: now we allow bridges in the GroundHeight.
						PathfindLayerEnum layer = TheTerrainLogic->getLayerForDestination(&newPos);
						newPos.z = TheTerrainLogic->getLayerHeight( newPos.x, newPos.y, layer );
					}
					else
						newPos.z += m_height.getValue();


					if (m_orientToObject && mtx)
					{
						sys->setLocalTransform(mtx);
					}
					if (m_rotateX != 0.0f)
						sys->rotateLocalTransformX(m_rotateX);
					if (m_rotateY != 0.0f)
						sys->rotateLocalTransformY(m_rotateY);
					if (m_rotateZ != 0.0f)
						sys->rotateLocalTransformZ(m_rotateZ);

					if (m_attachToObject && thingToAttachTo)
						sys->attachToObject(thingToAttachTo);
					else
						sys->setPosition( &newPos );

					Real delayInMsec = m_delay.getValue();
					if (delayInMsec >= 0.0f)
					{
						UnsignedInt delayInFrames = REAL_TO_INT_CEIL(ConvertDurationFromMsecsToFrames(delayInMsec));
						sys->setInitialDelay(delayInFrames);
					}

					// Tiberium Project @feature a lightning system's bolt runs to the effect's other end
					if (otherEnd != nullptr && tmp->isLightning())
						sys->setLightningTarget(otherEnd);

					if( m_useCallersRadius  &&  overrideRadius )
					{
						ParticleSystemInfo::EmissionVolumeType type = sys->getEmisionVolumeType();

						if( type == ParticleSystemInfo::EmissionVolumeType::SPHERE )
							sys->setEmissionVolumeSphereRadius( overrideRadius );
						else if( type == ParticleSystemInfo::EmissionVolumeType::CYLINDER )
							sys->setEmissionVolumeCylinderRadius( overrideRadius );
					}
				}
			}
		}
	}


private:
	AsciiString			m_name;
	Int							m_count;
	Coord3D					m_offset;
	GameClientRandomVariable	m_radius;
	GameClientRandomVariable	m_height;
	GameClientRandomVariable	m_delay;
	Real						m_rotateX, m_rotateY, m_rotateZ;
	Bool						m_orientToObject;
	Bool						m_attachToObject;
	Bool						m_createAtGroundHeight;
	Bool						m_useCallersRadius;
	Bool						m_ricochet;
	Bool						m_useTarget;	///< Tiberium Project: made at the effect's other end (C&C4's UseTarget)
};
EMPTY_DTOR(ParticleSystemFXNugget)

//-------------------------------------------------------------------------------------------------
class FXListAtBonePosFXNugget : public FXNugget
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(FXListAtBonePosFXNugget, "FXListAtBonePosFXNugget")
public:

	FXListAtBonePosFXNugget()
	{
		m_fx = nullptr;
    m_boneName.clear();
    m_orientToBone = true;
	}

	virtual void doFXPos(const Coord3D *primary, const Matrix3D* primaryMtx, const Real /*primarySpeed*/, const Coord3D * /*secondary*/, const Real /*overrideRadius*/ ) const override
	{
		// Tiberium Project @tweak no object, so no bones: the effect where it is played (C&C4's effects name a bone
		// for their particles, and the same effect may be played at a spot)
		if (primary)
			FXList::doFXPos(m_fx, primary, primaryMtx, 0.0f, nullptr, 0.0f);
	}

	virtual void doFXObj(const Object* primary, const Object* /*secondary*/) const override
	{
		if (primary)
		{
      // first, try the unadorned name.
			doFxAtBones(primary, 0);

      // then, try the 01,02,03...etc names.
			doFxAtBones(primary, 1);
		}
		else
		{
			DEBUG_CRASH(("You must have a primary source for this effect"));
		}
	}

	static void parse(INI *ini, void *instance, void* /*store*/, const void* /*userData*/)
	{
		static const FieldParse myFieldParse[] =
		{
			{ "FX",								  	INI::parseFXList,			    nullptr, offsetof( FXListAtBonePosFXNugget, m_fx ) },
			{ "BoneName",							INI::parseAsciiString,		nullptr, offsetof( FXListAtBonePosFXNugget, m_boneName ) },
			{ "OrientToBone",					INI::parseBool,					  nullptr, offsetof( FXListAtBonePosFXNugget, m_orientToBone ) },
			{ nullptr, nullptr, nullptr, 0 }
		};

		FXListAtBonePosFXNugget* nugget = newInstance( FXListAtBonePosFXNugget );
		ini->initFromINI(nugget, myFieldParse);
		((FXList*)instance)->addFXNugget(nugget);
	}

protected:

	void doFxAtBones(const Object* obj, Int start) const
	{
    Coord3D bonePos[MAX_BONE_POINTS];
    Matrix3D boneMtx[MAX_BONE_POINTS];

    Drawable* draw = obj->getDrawable();
		if (draw)
		{
			// yes, BONEPOS_CURRENT_CLIENT_ONLY -- this is client-only, so should be safe to do.
			Int count = draw->getCurrentClientBonePositions(m_boneName.str(), start, bonePos, boneMtx, MAX_BONE_POINTS);
			for (Int i = 0; i < count; ++i)
			{
				Coord3D p;
				Matrix3D m;
				obj->convertBonePosToWorldPos(&bonePos[i], &boneMtx[i], &p, &m);
				FXList::doFXPos(m_fx, &p, &m, 0.0f, nullptr, 0.0f);
			}
		}
	}

private:

	enum { MAX_BONE_POINTS = 40 };

	const FXList*		m_fx;
  AsciiString     m_boneName;
  Bool            m_orientToBone;
};
EMPTY_DTOR(FXListAtBonePosFXNugget)

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

static const FieldParse TheFXListFieldParse[] =
{
	{ "Sound",											SoundFXNugget::parse, nullptr, 0},
	{ "RayEffect",									RayEffectFXNugget::parse, nullptr, 0},
	{ "Tracer",											TracerFXNugget::parse, nullptr, 0},
	{ "LightPulse",									LightPulseFXNugget::parse, nullptr, 0},
	{ "ViewShake",									ViewShakeFXNugget::parse, nullptr, 0},
	{ "TerrainScorch",							TerrainScorchFXNugget::parse, nullptr, 0},
	{ "DynamicDecal",								DynamicDecalFXNugget::parse, nullptr, 0},	// Tiberium Project
	{ "ParticleSystem",							ParticleSystemFXNugget::parse, nullptr, 0},
	{ "FXListAtBonePos",						FXListAtBonePosFXNugget::parse, nullptr, 0},
	{ nullptr, nullptr, nullptr, 0 }
};

//-------------------------------------------------------------------------------------------------
FXList::FXList()
{
}

//-------------------------------------------------------------------------------------------------
FXList::~FXList()
{
	clear();
}

//-------------------------------------------------------------------------------------------------
void FXList::clear()
{
	for (FXNuggetList::iterator it = m_nuggets.begin(); it != m_nuggets.end(); ++it)
	{
		deleteInstance(*it);
	}
	m_nuggets.clear();
}

//-------------------------------------------------------------------------------------------------
void FXList::doFXPos(const Coord3D *primary, const Matrix3D* primaryMtx, const Real primarySpeed, const Coord3D *secondary, const Real overrideRadius ) const
{
	const Int playerIndex = rts::getObservedOrLocalPlayer()->getPlayerIndex();

	if (ThePartitionManager->getShroudStatusForPlayer(playerIndex, primary) != CELLSHROUD_CLEAR)
		return;

	for (FXNuggetList::const_iterator it = m_nuggets.begin(); it != m_nuggets.end(); ++it)
	{
		(*it)->doFXPos(primary, primaryMtx, primarySpeed, secondary, overrideRadius);
	}
}

//-------------------------------------------------------------------------------------------------
void FXList::doFXObj(const Object* primary, const Object* secondary) const
{
	const Int playerIndex = rts::getObservedOrLocalPlayer()->getPlayerIndex();

	if (primary && primary->getShroudedStatus(playerIndex) > OBJECTSHROUD_PARTIAL_CLEAR)
		return;	//the primary object is fogged or shrouded so don't bother with the effect.

	for (FXNuggetList::const_iterator it = m_nuggets.begin(); it != m_nuggets.end(); ++it)
	{

		// HERE THE PRIMARY IS THE GUY RECEIVING THE FX, AND SECONDARY MIGHT BE THE GUY DEALING IT
		(*it)->doFXObj(primary, secondary);
	}
}



//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
FXListStore::FXListStore()
{
}

//-------------------------------------------------------------------------------------------------
FXListStore::~FXListStore()
{
	m_fxmap.clear();
}

//-------------------------------------------------------------------------------------------------
const FXList *FXListStore::findFXList(const char* name) const
{
	if (stricmp(name, "None") == 0)
		return nullptr;

  FXListMap::const_iterator it = m_fxmap.find(NAMEKEY(name));
  if (it != m_fxmap.end())
	{
		return &(*it).second;
	}
	return nullptr;
}

//-------------------------------------------------------------------------------------------------
/*static */ void FXListStore::parseFXListDefinition(INI *ini)
{
	// read the FXList name
	const char *c = ini->getNextToken();
	NameKeyType key = TheNameKeyGenerator->nameToKey(c);
	FXList& fxl = TheFXListStore->m_fxmap[key];
	fxl.clear();
	ini->initFromINI(&fxl, TheFXListFieldParse);
}

//-------------------------------------------------------------------------------------------------
/*static*/ void INI::parseFXListDefinition(INI *ini)
{
	FXListStore::parseFXListDefinition(ini);
}
