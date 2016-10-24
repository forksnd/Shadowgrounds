#include "precompiled.h"
#include "spherical_joint.h"
#include "NxPhysics.h"

namespace frozenbyte {
namespace physics {

SphericalJoint::SphericalJoint(NxScene &scene, const NxSphericalJointDesc &desc, std::shared_ptr<ActorBase> &a, std::shared_ptr<ActorBase> &b)
:	JointBase(scene, a, b)
{
	joint = scene.createJoint(desc);
}

SphericalJoint::~SphericalJoint()
{
}

bool SphericalJoint::isValid() const
{
	if(joint)
		return true;

	return false;
}

} // physics
} // frozenbyte
