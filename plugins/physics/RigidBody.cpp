#include "GEK\Math\Common.h"
#include "GEK\Math\Float4x4.h"
#include "GEK\Utility\String.h"
#include "GEK\Context\ContextUser.h"
#include "GEK\Engine\ComponentMixin.h"
#include "GEK\Components\Transform.h"
#include "GEK\Newton\Base.h"
#include <Newton.h>

namespace Gek
{
    namespace Newton
    {
        class RigidBody
            : public Newton::Entity
        {
        private:
            Newton::World *world;

            Plugin::Entity *entity;
            NewtonBody *newtonBody;

        public:
            RigidBody(NewtonWorld *newtonWorld, const NewtonCollision* const newtonCollision, Plugin::Entity *entity)
                : world(static_cast<Newton::World *>(NewtonWorldGetUserData(newtonWorld)))
                , entity(entity)
                , newtonBody(nullptr)
            {
                GEK_REQUIRE(world);
                GEK_REQUIRE(entity);

                auto &physical = entity->getComponent<Components::Physical>();
                auto &transform = entity->getComponent<Components::Transform>();

                Math::Float4x4 matrix(transform.getMatrix());
                newtonBody = NewtonCreateDynamicBody(newtonWorld, newtonCollision, matrix.data);
                if (newtonBody == nullptr)
                {
                    throw Newton::UnableToCreateCollision();
                }

                NewtonBodySetUserData(newtonBody, dynamic_cast<Newton::Entity *>(this));
                NewtonBodySetMassProperties(newtonBody, physical.mass, newtonCollision);
            }

            ~RigidBody(void)
            {
                NewtonDestroyBody(newtonBody);
            }

            // Newton::Entity
            Plugin::Entity * const getEntity(void) const
            {
                return entity;
            }

            NewtonBody * const getNewtonBody(void) const
            {
                return newtonBody;
            }

            uint32_t getSurface(const Math::Float3 &position, const Math::Float3 &normal)
            {
                return 0;
            }

            void onPreUpdate(int threadHandle)
            {
                auto &physical = entity->getComponent<Components::Physical>();
                auto &transform = entity->getComponent<Components::Transform>();

                NewtonCollisionSetScale(NewtonBodyGetCollision(newtonBody), transform.scale.x, transform.scale.y, transform.scale.z);

                Math::Float3 gravity(world->getGravity(transform.position));
                NewtonBodyAddForce(newtonBody, (gravity * physical.mass).data);
            }

            void onSetTransform(const float* const matrixData, int threadHandle)
            {
                auto &transform = entity->getComponent<Components::Transform>();

                Math::Float4x4 matrix(matrixData);
                transform.position = matrix.translation;
                transform.rotation = matrix.getQuaternion();
            }
        };

        Newton::EntityPtr createRigidBody(NewtonWorld *newtonWorld, const NewtonCollision* const newtonCollision, Plugin::Entity *entity)
        {
            return std::make_shared<RigidBody>(newtonWorld, newtonCollision, entity);
        }
    }; // namespace Newton
}; // namespace Gek