/// @file
/// @author Todd Zupan <toddzupan@gmail.com>
/// @version $Revision: c85d467e8da7e856f4a8229e8bfbcc46299d722e $
/// @section LICENSE
/// https://en.wikipedia.org/wiki/MIT_License
/// @section DESCRIPTION
/// Last Changed: $Date:   Tue Oct 25 14:51:24 2016 +0000 $
#pragma once

#include "GEK/Utility/Context.hpp"
#include "GEK/Engine/Resources.hpp"
#include "GEK/System/VideoDevice.hpp"
#include "GEK/Shapes/Frustum.hpp"
#include <wink/signal.hpp>
#include <imgui.h>

namespace Gek
{
	namespace Plugin
    {
        GEK_PREDECLARE(Entity);

        GEK_INTERFACE(Renderer)
        {
            wink::signal<wink::slot<void(const Shapes::Frustum &viewFrustum, Math::Float4x4 const &viewMatrix, Math::Float4x4 const &projectionMatrix)>> onQueueDrawCalls;
            wink::signal<wink::slot<void(ImGuiContext * const guiContext)>> onShowUserInterface;

            virtual ~Renderer(void) = default;

            virtual void queueCamera(Math::Float4x4 const &viewMatrix, Math::Float4x4 const &projectionMatrix, float nearClip, float farClip, std::string const *name = nullptr, ResourceHandle cameraTarget = ResourceHandle(), std::string const &forceShader = String::Empty) = 0;
            virtual void queueDrawCall(VisualHandle plugin, MaterialHandle material, std::function<void(Video::Device::Context *)> &&draw) = 0;

            virtual void renderOverlay(Video::Device::Context *videoContext, ResourceHandle input, ResourceHandle target) = 0;
        };
    }; // namespace Engine
}; // namespace Gek
