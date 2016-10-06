﻿#include "GEK\Components\Filter.h"
#include "GEK\Context\ContextUser.h"
#include "GEK\Engine\ComponentMixin.h"
#include "GEK\Utility\String.h"

namespace Gek
{
    namespace Components
    {
        void Filter::save(Xml::Leaf &componentData) const
        {
            componentData.text.join(list, L',');
        }

        void Filter::load(const Xml::Leaf &componentData)
        {
            list = componentData.text.split(L',');
        }
    }; // namespace Components

    GEK_CONTEXT_USER(Filter)
        , public Plugin::ComponentMixin<Components::Filter, Editor::Component>
    {
    public:
        Filter(Context *context)
            : ContextRegistration(context)
        {
        }

        // Editor::Component
        void showEditor(ImGuiContext *guiContext, const Math::Float4x4 &viewMatrix, const Math::Float4x4 &projectionMatrix, Plugin::Component::Data *data)
        {
            ImGui::SetCurrentContext(guiContext);
            auto &filterComponent = *dynamic_cast<Components::Filter *>(data);
            if (ImGui::ListBoxHeader("Filters", filterComponent.list.size(), 5))
            {
                ImGuiListClipper clipper(filterComponent.list.size(), ImGui::GetTextLineHeightWithSpacing());
                while (clipper.Step())
                {
                    for (int filterIndex = clipper.DisplayStart; filterIndex < clipper.DisplayEnd; ++filterIndex)
                    {
                        ImGui::InputText("##", filterComponent.list[filterIndex]);
                    }
                };

                ImGui::ListBoxFooter();
            }

            ImGui::SetCurrentContext(nullptr);
        }

        // Plugin::Component
        const wchar_t * const getName(void) const
        {
            return L"filter";
        }
    };

    GEK_REGISTER_CONTEXT_USER(Filter);
}; // namespace Gek