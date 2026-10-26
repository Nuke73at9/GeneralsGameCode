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

// FILE: W3DDynamicsDraw.cpp //////////////////////////////////////////////////////////////////////
// Tiberium Project @feature A model that falls apart (see W3DDynamicsDraw.h). Each piece is a rigid
// box: thrown out from the middle of the wreck, spun, pulled down by gravity, and pushed out of the
// terrain with an impulse at the corner that touches it (so pieces land on an edge and tip over,
// skid with friction and come to rest). Client side only: the game logic never sees the pieces.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>

#include "Common/INI.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/ClientRandomValue.h"
#include "GameClient/Drawable.h"
#include "GameLogic/TerrainLogic.h"
#include "W3DDevice/GameClient/Module/W3DDynamicsDraw.h"
#include "WW3D2/htree.h"
#include "WW3D2/rendobj.h"
#include "WW3D2/ww3d.h"
#include "WWMath/quat.h"
#include "WWMath/wwmath.h"

namespace
{
	const Real RESTITUTION = 0.25f;		///< how much of its speed into the ground a piece keeps, bouncing
	const Real FRICTION = 0.6f;			///< sliding friction against the ground
	const Real EXPLODE_SPEED = 10.0f;	///< outwards, units a second per point of Explodiness
	const Real EXPLODE_LIFT = 20.0f;	///< upwards, units a second per point of Explodiness
	const Real MAX_STEP = 1.0f / 60.0f;	///< the longest physics step, seconds
	const Real REST_SPEED = 4.0f;		///< slower than this (and turning slowly) on the ground counts as still
	const Real REST_SPIN = 0.6f;
	const Real REST_TIME = 0.4f;		///< still this long, a piece stops being simulated

	Vector3 cross(const Vector3& a, const Vector3& b)
	{
		return Vector3(a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X);
	}

	Real dot(const Vector3& a, const Vector3& b)
	{
		return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
	}

	/// a transform's rotation turned further by `angle` radians about the world axis `axis` (unit)
	void rotateWorld(Matrix3D& m, const Vector3& axis, Real angle)
	{
		const Real c = WWMath::Cos(angle), s = WWMath::Sin(angle), t = 1.0f - c;
		const Real x = axis.X, y = axis.Y, z = axis.Z;
		const Real r[3][3] = {
			{ t * x * x + c,     t * x * y - s * z, t * x * z + s * y },
			{ t * x * y + s * z, t * y * y + c,     t * y * z - s * x },
			{ t * x * z - s * y, t * y * z + s * x, t * z * z + c     } };
		Real out[3][3];
		for (Int i = 0; i < 3; ++i)
			for (Int j = 0; j < 3; ++j)
				out[i][j] = r[i][0] * m[0][j] + r[i][1] * m[1][j] + r[i][2] * m[2][j];
		for (Int i = 0; i < 3; ++i)
			for (Int j = 0; j < 3; ++j)
				m[i][j] = out[i][j];
	}

	/// keeps a rotation orthonormal after many small turns
	void orthonormalize(Matrix3D& m)
	{
		Vector3 x(m[0][0], m[1][0], m[2][0]);
		Vector3 y(m[0][1], m[1][1], m[2][1]);
		x.Normalize();
		Vector3 z = cross(x, y);
		z.Normalize();
		y = cross(z, x);
		m[0][0] = x.X; m[1][0] = x.Y; m[2][0] = x.Z;
		m[0][1] = y.X; m[1][1] = y.Y; m[2][1] = y.Z;
		m[0][2] = z.X; m[1][2] = z.Y; m[2][2] = z.Z;
	}

	Vector3 transformPoint(const Matrix3D& m, const Vector3& p)
	{
		Vector3 out;
		Matrix3D::Transform_Vector(m, p, &out);
		return out;
	}

	Vector3 inverseTransformPoint(const Matrix3D& m, const Vector3& p)
	{
		Matrix3D inv;
		m.Get_Orthogonal_Inverse(inv);
		return transformPoint(inv, p);
	}
}

//-------------------------------------------------------------------------------------------------
W3DDynamicsDrawModuleData::W3DDynamicsDrawModuleData() :
	m_explodiness(1.0f),
	m_flingPerturbation(0.0f),
	m_flingMin(0.0f),
	m_flingMax(0.0f),
	m_flingPitchMin(PI * 0.5f),
	m_flingPitchMax(PI * 0.5f),
	m_gravity(250.0f),
	m_lifetimeDelay(4000),
	m_lifetimeFade(3000)
{
}

//-------------------------------------------------------------------------------------------------
W3DDynamicsDrawModuleData::~W3DDynamicsDrawModuleData()
{
}

//-------------------------------------------------------------------------------------------------
/// BoneVolume = <bone> Mass:<m> Spin:<radians a second> Center:x,y,z Half:x,y,z [Rotation:x,y,z,w]
void W3DDynamicsDrawModuleData::parseBoneVolume(INI* ini, void* instance, void* /*store*/, const void* /*userData*/)
{
	W3DDynamicsDrawModuleData* self = static_cast<W3DDynamicsDrawModuleData*>(instance);
	W3DDynamicsBoneVolume v;
	v.m_boneName = ini->getNextToken();
	v.m_mass = 1.0f;
	v.m_spin = 0.0f;
	v.m_center[0] = v.m_center[1] = v.m_center[2] = 0.0f;
	v.m_half[0] = v.m_half[1] = v.m_half[2] = 1.0f;
	v.m_rotation[0] = v.m_rotation[1] = v.m_rotation[2] = 0.0f;
	v.m_rotation[3] = 1.0f;
	for (const char* token = ini->getNextTokenOrNull(); token != nullptr; token = ini->getNextTokenOrNull())
	{
		float a = 0.0f, b = 0.0f, c = 0.0f, d = 0.0f;
		if (sscanf(token, "Mass:%f", &a) == 1)
			v.m_mass = a > 0.0f ? a : 1.0f;
		else if (sscanf(token, "Spin:%f", &a) == 1)
			v.m_spin = a;
		else if (sscanf(token, "Center:%f,%f,%f", &a, &b, &c) == 3)
		{
			v.m_center[0] = a; v.m_center[1] = b; v.m_center[2] = c;
		}
		else if (sscanf(token, "Half:%f,%f,%f", &a, &b, &c) == 3)
		{
			v.m_half[0] = a; v.m_half[1] = b; v.m_half[2] = c;
		}
		else if (sscanf(token, "Rotation:%f,%f,%f,%f", &a, &b, &c, &d) == 4)
		{
			v.m_rotation[0] = a; v.m_rotation[1] = b; v.m_rotation[2] = c; v.m_rotation[3] = d;
		}
		else
		{
			throw INI_INVALID_DATA;
		}
	}
	self->m_volumes.push_back(v);
}

//-------------------------------------------------------------------------------------------------
/// Fling = <force min> <force max> <pitch min> <pitch max>
void W3DDynamicsDrawModuleData::parseFling(INI* ini, void* instance, void* /*store*/, const void* /*userData*/)
{
	W3DDynamicsDrawModuleData* self = static_cast<W3DDynamicsDrawModuleData*>(instance);
	self->m_flingMin = INI::scanReal(ini->getNextToken());
	self->m_flingMax = INI::scanReal(ini->getNextToken());
	self->m_flingPitchMin = INI::scanReal(ini->getNextToken()) * PI / 180.0f;
	self->m_flingPitchMax = INI::scanReal(ini->getNextToken()) * PI / 180.0f;
}

//-------------------------------------------------------------------------------------------------
void W3DDynamicsDrawModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	W3DModelDrawModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "Explodiness", INI::parseReal, nullptr, offsetof(W3DDynamicsDrawModuleData, m_explodiness) },
		{ "FlingPerturbation", INI::parseReal, nullptr, offsetof(W3DDynamicsDrawModuleData, m_flingPerturbation) },
		{ "Fling", parseFling, nullptr, 0 },
		{ "Gravity", INI::parseReal, nullptr, offsetof(W3DDynamicsDrawModuleData, m_gravity) },
		{ "LifetimeDelay", INI::parseUnsignedInt, nullptr, offsetof(W3DDynamicsDrawModuleData, m_lifetimeDelay) },
		{ "LifetimeFade", INI::parseUnsignedInt, nullptr, offsetof(W3DDynamicsDrawModuleData, m_lifetimeFade) },
		{ "BoneVolume", parseBoneVolume, nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);
}

//-------------------------------------------------------------------------------------------------
W3DDynamicsDraw::W3DDynamicsDraw( Thing *thing, const ModuleData* moduleData ) : W3DModelDraw( thing, moduleData ),
	m_startedOn(nullptr),
	m_age(0.0f),
	m_started(FALSE)
{
}

//-------------------------------------------------------------------------------------------------
W3DDynamicsDraw::~W3DDynamicsDraw()
{
}

//-------------------------------------------------------------------------------------------------
void W3DDynamicsDraw::onRenderObjRecreated()
{
	// a new model (another condition state): its bones start where its own art has them
	m_pieces.clear();
	m_startedOn = nullptr;
	m_started = FALSE;
}

//-------------------------------------------------------------------------------------------------
/// Sets each piece at its place in the model and throws it.
void W3DDynamicsDraw::start(const Matrix3D& objectTm)
{
	const W3DDynamicsDrawModuleData* data = getW3DDynamicsDrawModuleData();
	RenderObjClass* robj = getRenderObject();
	m_pieces.clear();
	m_started = TRUE;
	m_startedOn = robj;
	m_age = 0.0f;
	if (robj == nullptr || data->m_volumes.empty())
		return;

	Matrix3D objectInv;
	objectTm.Get_Orthogonal_Inverse(objectInv);
	const HTreeClass* htree = robj->Get_HTree();

	Real totalMass = 0.0f;
	for (size_t i = 0; i < data->m_volumes.size(); ++i)
		totalMass += data->m_volumes[i].m_mass;
	const Real meanMass = totalMass / (Real)data->m_volumes.size();

	// the middle of the wreck, low down: the pieces fly out from here
	Vector3 middle(0.0f, 0.0f, 0.0f);
	for (size_t i = 0; i < data->m_volumes.size(); ++i)
		middle += Vector3(data->m_volumes[i].m_center[0], data->m_volumes[i].m_center[1], data->m_volumes[i].m_center[2]);
	middle /= (Real)data->m_volumes.size();
	middle.Z *= 0.4f;
	const Vector3 burst = transformPoint(objectTm, middle);

	// the whole wreck's throw (C&C4's FlingObject on the wreck)
	Vector3 fling(0.0f, 0.0f, 0.0f);
	if (data->m_flingMax > 0.0f)
	{
		const Real force = GameClientRandomValueReal(data->m_flingMin, data->m_flingMax) * LOGICFRAMES_PER_SECOND;
		const Real pitch = GameClientRandomValueReal(data->m_flingPitchMin, data->m_flingPitchMax);
		const Real yaw = GameClientRandomValueReal(0.0f, 2.0f * PI);
		fling.Set(force * WWMath::Cos(pitch) * WWMath::Cos(yaw), force * WWMath::Cos(pitch) * WWMath::Sin(yaw), force * WWMath::Sin(pitch));
	}

	for (size_t i = 0; i < data->m_volumes.size(); ++i)
	{
		const W3DDynamicsBoneVolume& v = data->m_volumes[i];
		const Int bone = robj->Get_Bone_Index(v.m_boneName.str());
		if (bone <= 0)
		{
			DEBUG_LOG(("W3DDynamicsDraw: no bone %s in %s", v.m_boneName.str(), robj->Get_Name()));
			continue;
		}
		Piece p;
		p.bone = bone;
		p.parentPiece = -1;
		p.world = robj->Get_Bone_Transform(bone);
		Matrix3D::Multiply(objectInv, p.world, &p.restObject);

		const Real mass = v.m_mass / meanMass;
		p.invMass = 1.0f / mass;
		const Real h2 = v.m_half[0] * v.m_half[0] + v.m_half[1] * v.m_half[1] + v.m_half[2] * v.m_half[2];
		p.invInertia = 1.0f / max(mass * h2 * (2.0f / 9.0f), 0.01f);

		// the box in the bone's space: centre and corners
		Matrix3D boxTm(true);
		Quaternion q(v.m_rotation[0], v.m_rotation[1], v.m_rotation[2], v.m_rotation[3]);
		q.Normalize();
		Build_Matrix3D(q, boxTm);
		boxTm.Set_Translation(Vector3(v.m_center[0], v.m_center[1], v.m_center[2]));
		Matrix3D boneInv;
		p.world.Get_Orthogonal_Inverse(boneInv);
		Matrix3D modelToBone;
		Matrix3D::Multiply(boneInv, objectTm, &modelToBone);
		p.comLocal = transformPoint(modelToBone, Vector3(v.m_center[0], v.m_center[1], v.m_center[2]));
		for (Int c = 0; c < 8; ++c)
		{
			const Vector3 corner((c & 1) ? v.m_half[0] : -v.m_half[0], (c & 2) ? v.m_half[1] : -v.m_half[1],
				(c & 4) ? v.m_half[2] : -v.m_half[2]);
			p.corners[c] = transformPoint(modelToBone, transformPoint(boxTm, corner));
		}

		// thrown up and out from the middle, lighter pieces faster
		const Vector3 com = transformPoint(p.world, p.comLocal);
		Vector3 out = com - burst;
		out.Z = 0.0f;
		if (out.Length2() > 0.0001f)
			out.Normalize();
		const Real light = WWMath::Clamp(WWMath::Sqrt(p.invMass), 0.5f, 1.8f);
		p.velocity = out * (data->m_explodiness * EXPLODE_SPEED * light * GameClientRandomValueReal(0.6f, 1.3f)) + fling;
		p.velocity.Z += data->m_explodiness * EXPLODE_LIFT * light * GameClientRandomValueReal(0.6f, 1.2f);
		p.velocity += Vector3(GameClientRandomValueReal(-1.0f, 1.0f), GameClientRandomValueReal(-1.0f, 1.0f),
			GameClientRandomValueReal(-0.5f, 1.0f)) * data->m_flingPerturbation;

		Vector3 axis(GameClientRandomValueReal(-1.0f, 1.0f), GameClientRandomValueReal(-1.0f, 1.0f), GameClientRandomValueReal(-1.0f, 1.0f));
		if (axis.Length2() < 0.0001f)
			axis.Set(0.0f, 0.0f, 1.0f);
		axis.Normalize();
		p.spin = axis * (v.m_spin * GameClientRandomValueReal(0.5f, 1.0f));
		p.restTime = 0.0f;
		p.asleep = FALSE;
		m_pieces.push_back(p);
	}

	// pieces hanging from other pieces follow their parent's motion through the hierarchy
	if (htree != nullptr)
	{
		for (size_t i = 0; i < m_pieces.size(); ++i)
		{
			const Int parentBone = htree->Get_Parent_Index(m_pieces[i].bone);
			for (size_t j = 0; j < m_pieces.size(); ++j)
				if (j != i && m_pieces[j].bone == parentBone)
					m_pieces[i].parentPiece = (Int)j;
		}
	}
	for (size_t i = 0; i < m_pieces.size(); ++i)
		robj->Capture_Bone(m_pieces[i].bone);
}

//-------------------------------------------------------------------------------------------------
/// Pushes a piece out of the ground: an impulse at each corner below it, with friction.
void W3DDynamicsDraw::collide(Piece& p)
{
	if (TheTerrainLogic == nullptr)
		return;
	Vector3 world[8];
	Real depth[8];
	Coord3D normals[8];
	Real deepest = 0.0f;
	for (Int c = 0; c < 8; ++c)
	{
		world[c] = transformPoint(p.world, p.corners[c]);
		const Real ground = TheTerrainLogic->getGroundHeight(world[c].X, world[c].Y, &normals[c]);
		depth[c] = ground - world[c].Z;
		deepest = max(deepest, depth[c]);
	}
	if (deepest <= 0.0f)
		return;

	// out of the ground first, then the bounce and the friction at each touching corner
	p.world.Adjust_Z_Translation(deepest);
	const Vector3 com = transformPoint(p.world, p.comLocal);
	for (Int c = 0; c < 8; ++c)
	{
		if (depth[c] <= 0.0f)
			continue;
		Vector3 n(normals[c].x, normals[c].y, normals[c].z);
		if (n.Length2() < 0.0001f)
			n.Set(0.0f, 0.0f, 1.0f);
		n.Normalize();
		const Vector3 r = world[c] + Vector3(0.0f, 0.0f, deepest) - com;
		const Vector3 vc = p.velocity + cross(p.spin, r);
		const Real vn = dot(vc, n);
		if (vn >= 0.0f)
			continue;
		const Vector3 rn = cross(r, n);
		const Real j = -(1.0f + RESTITUTION) * vn / (p.invMass + p.invInertia * dot(rn, rn));
		p.velocity += n * (j * p.invMass);
		p.spin += cross(r, n * j) * p.invInertia;

		Vector3 vt = vc - n * vn;
		const Real slide = vt.Length();
		if (slide > 0.0001f)
		{
			vt /= slide;
			const Vector3 rt = cross(r, vt);
			Real jt = slide / (p.invMass + p.invInertia * dot(rt, rt));
			jt = min(jt, FRICTION * j);
			p.velocity -= vt * (jt * p.invMass);
			p.spin -= cross(r, vt * jt) * p.invInertia;
		}
	}
	p.spin *= 0.98f;	// rolling resistance
}

//-------------------------------------------------------------------------------------------------
void W3DDynamicsDraw::step(Real dt)
{
	const W3DDynamicsDrawModuleData* data = getW3DDynamicsDrawModuleData();
	for (size_t i = 0; i < m_pieces.size(); ++i)
	{
		Piece& p = m_pieces[i];
		if (p.asleep)
			continue;
		p.velocity.Z -= data->m_gravity * dt;
		Vector3 com = transformPoint(p.world, p.comLocal);
		com += p.velocity * dt;
		const Real turn = p.spin.Length();
		if (turn > 0.0001f)
		{
			rotateWorld(p.world, p.spin / turn, turn * dt);
			orthonormalize(p.world);
		}
		// the transform's translation puts the box centre where it moved to
		Vector3 offset;
		Matrix3D rotation = p.world;
		rotation.Set_Translation(Vector3(0.0f, 0.0f, 0.0f));
		Matrix3D::Transform_Vector(rotation, p.comLocal, &offset);
		p.world.Set_Translation(com - offset);

		const Real before = p.world.Get_Z_Translation();
		collide(p);
		const Bool touching = p.world.Get_Z_Translation() != before;
		if (touching && p.velocity.Length() < REST_SPEED + data->m_gravity * dt && p.spin.Length() < REST_SPIN)
		{
			p.restTime += dt;
			if (p.restTime > REST_TIME)
			{
				p.asleep = TRUE;
				p.velocity.Set(0.0f, 0.0f, 0.0f);
				p.spin.Set(0.0f, 0.0f, 0.0f);
			}
		}
		else
		{
			p.restTime = 0.0f;
		}
	}
}

//-------------------------------------------------------------------------------------------------
/// Hands each piece's place to its bone: the hierarchy's own transform for the bone, times the
/// captured one, is the piece's transform.
void W3DDynamicsDraw::applyBones(const Matrix3D& objectTm)
{
	RenderObjClass* robj = getRenderObject();
	for (size_t i = 0; i < m_pieces.size(); ++i)
	{
		const Piece& p = m_pieces[i];
		Matrix3D rest;
		if (p.parentPiece >= 0)
		{
			const Piece& parent = m_pieces[p.parentPiece];
			Matrix3D parentRestInv, relative;
			parent.restObject.Get_Orthogonal_Inverse(parentRestInv);
			Matrix3D::Multiply(parentRestInv, p.restObject, &relative);
			Matrix3D::Multiply(parent.world, relative, &rest);
		}
		else
		{
			Matrix3D::Multiply(objectTm, p.restObject, &rest);
		}
		Matrix3D restInv, captured;
		rest.Get_Orthogonal_Inverse(restInv);
		Matrix3D::Multiply(restInv, p.world, &captured);
		robj->Control_Bone(p.bone, captured);
	}
}

//-------------------------------------------------------------------------------------------------
void W3DDynamicsDraw::doDrawModule(const Matrix3D* transformMtx)
{
	W3DModelDraw::doDrawModule(transformMtx);

	RenderObjClass* robj = getRenderObject();
	if (robj == nullptr || transformMtx == nullptr)
		return;
	if (!m_started || m_startedOn != robj)
		start(*transformMtx);
	if (m_pieces.empty())
		return;

	// the real time since the last frame (paused, nothing moves), in small steps
	Real dt = min(WW3D::Get_Sync_Frame_Time() * 0.001f, 0.1f);
	if (dt > 0.0f)
	{
		m_age += dt;
		while (dt > 0.0f)
		{
			const Real h = min(dt, MAX_STEP);
			step(h);
			dt -= h;
		}
	}
	applyBones(*transformMtx);

	// the pieces fade once they have lain a while
	const W3DDynamicsDrawModuleData* data = getW3DDynamicsDrawModuleData();
	const Real delay = data->m_lifetimeDelay * 0.001f;
	const Real fade = data->m_lifetimeFade * 0.001f;
	if (fade > 0.0f && m_age > delay)
		getDrawable()->setDrawableOpacity(max(0.0f, 1.0f - (m_age - delay) / fade));
}

//-------------------------------------------------------------------------------------------------
void W3DDynamicsDraw::crc( Xfer *xfer )
{
	W3DModelDraw::crc( xfer );
}

//-------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version (the pieces are not saved: a loaded wreck throws them again) */
//-------------------------------------------------------------------------------------------------
void W3DDynamicsDraw::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	W3DModelDraw::xfer( xfer );
}

//-------------------------------------------------------------------------------------------------
void W3DDynamicsDraw::loadPostProcess()
{
	W3DModelDraw::loadPostProcess();
	m_pieces.clear();
	m_startedOn = nullptr;
	m_started = FALSE;
}
