﻿#include "GEK/Utility/Exceptions.hpp"
#include "GEK/Utility/String.hpp"
#include "GEK/Utility/FileSystem.hpp"
#include "GEK/Utility/Timer.hpp"
#include "GEK/Utility/ContextUser.hpp"
#include "GEK/GUI/Utilities.hpp"
#include "GEK/Engine/Core.hpp"
#include "GEK/Engine/Population.hpp"
#include "GEK/Engine/Resources.hpp"
#include "GEK/Engine/Renderer.hpp"
#include <concurrent_unordered_map.h>
#include <algorithm>
#include <queue>
#include <ppl.h>

namespace Gek
{
    namespace Implementation
    {
        GEK_CONTEXT_USER(Core, Window *)
            , public Plugin::Core
            , public Plugin::Core::Log
        {
        public:
            struct Command
            {
                std::string function;
                std::vector<std::string> parameterList;
            };

        private:
            WindowPtr window;
            bool windowActive = false;
            bool engineRunning = false;

            int currentDisplayMode = 0;
			int previousDisplayMode = 0;
            Video::DisplayModeList displayModeList;
            std::vector<std::string> displayModeStringList;
            bool fullScreen = false;
           
            JSON::Object configuration;
            ShuntingYard shuntingYard;

            Timer timer;
            float mouseSensitivity = 0.5f;

            Video::DevicePtr videoDevice;
            Plugin::RendererPtr renderer;
            Engine::ResourcesPtr resources;
            std::vector<Plugin::ProcessorPtr> processorList;
            Plugin::PopulationPtr population;

            ImGui::PanelManager panelManager;
            Video::TexturePtr consoleButton;
            Video::TexturePtr performanceButton;
            Video::TexturePtr settingsButton;
			bool showModeChange = false;
			float modeChangeTimer = 0.0f;
            bool consoleActive = false;

            struct EventHistory
            {
                float current = Math::NotANumber;
                float minimum = 0.0f;
                float maximum = 0.0f;
                std::vector<float> data;
            };

            static const uint32_t HistoryLength = 100;
            using EventHistoryMap = concurrency::concurrent_unordered_map<std::string, EventHistory>;
            using SystemHistoryMap = concurrency::concurrent_unordered_map<std::string, EventHistoryMap>;

            SystemHistoryMap systemHistoryMap;

        public:
            Core(Context *context, Window *_window)
                : ContextRegistration(context)
                , window(_window)
            {
                std::cout << "Starting GEK Engine" << std::endl;

                if (!window)
                {
                    Window::Description description;
                    description.className = "GEK_Engine_Demo";
                    description.windowName = "GEK Engine Demo";
                    window = getContext()->createClass<Window>("Default::System::Window", description);
                }

                window->onClose.connect<Core, &Core::onClose>(this);
                window->onActivate.connect<Core, &Core::onActivate>(this);
                window->onSizeChanged.connect<Core, &Core::onSizeChanged>(this);
                window->onKeyPressed.connect<Core, &Core::onKeyPressed>(this);
                window->onCharacter.connect<Core, &Core::onCharacter>(this);
                window->onSetCursor.connect<Core, &Core::onSetCursor>(this);
                window->onMouseClicked.connect<Core, &Core::onMouseClicked>(this);
                window->onMouseWheel.connect<Core, &Core::onMouseWheel>(this);
                window->onMousePosition.connect<Core, &Core::onMousePosition>(this);
                window->onMouseMovement.connect<Core, &Core::onMouseMovement>(this);

                configuration = JSON::Load(getContext()->getRootFileName("config.json"));
                previousDisplayMode = currentDisplayMode = JSON::From(JSON::Get(JSON::Get(configuration, "display"), "mode"), ShuntingYard(), 0);
                configuration["display"]["mode"] = currentDisplayMode;

                HRESULT resultValue = CoInitialize(nullptr);
                if (FAILED(resultValue))
                {
                    //throw InitializationFailed("Failed call to CoInitialize");
                }

                Video::Device::Description deviceDescription;
                videoDevice = getContext()->createClass<Video::Device>("Default::Device::Video", window.get(), deviceDescription);
                displayModeList = videoDevice->getDisplayModeList(deviceDescription.displayFormat);
                for (const auto &displayMode : displayModeList)
                {
                    std::string displayModeString(String::Format("%vx%v, %vhz", displayMode.width, displayMode.height, uint32_t(std::ceil(float(displayMode.refreshRate.numerator) / float(displayMode.refreshRate.denominator)))));
                    switch (displayMode.aspectRatio)
                    {
                    case Video::DisplayMode::AspectRatio::_4x3:
                        displayModeString.append(" (4x3)");
                        break;

                    case Video::DisplayMode::AspectRatio::_16x9:
                        displayModeString.append(" (16x9)");
                        break;

                    case Video::DisplayMode::AspectRatio::_16x10:
                        displayModeString.append(" (16x10)");
                        break;
                    };

                    displayModeStringList.push_back(displayModeString);
                }

                auto baseFileName(getContext()->getRootFileName("data", "gui"));
                consoleButton = videoDevice->loadTexture(FileSystem::GetFileName(baseFileName, "console.png"), 0);
                performanceButton = videoDevice->loadTexture(FileSystem::GetFileName(baseFileName, "performance.png"), 0);
                settingsButton = videoDevice->loadTexture(FileSystem::GetFileName(baseFileName, "settings.png"), 0);

                auto propertiesPane = panelManager.addPane(ImGui::PanelManager::RIGHT, "PropertiesPanel##PropertiesPanel");
                if (propertiesPane)
                {
                    propertiesPane->previewOnHover = false;
                }

                auto consolePane = panelManager.addPane(ImGui::PanelManager::BOTTOM, "ConsolePanel##ConsolePanel");
                if (consolePane)
                {
                    consolePane->previewOnHover = false;

                    consolePane->addButtonAndWindow(
                        ImGui::Toolbutton("Console", (Video::Object *)consoleButton.get(), ImVec2(0, 0), ImVec2(1, 1), ImVec2(32, 32)),
                        ImGui::PanelManagerPaneAssociatedWindow("Console", -1, [](ImGui::PanelManagerWindowData &windowData) -> void
                    {
                        ((Core *)windowData.userData)->drawConsole(windowData);
                    }, this, ImGuiWindowFlags_NoScrollbar));

                    consolePane->addButtonAndWindow(
                        ImGui::Toolbutton("Performance", (Video::Object *)performanceButton.get(), ImVec2(0, 0), ImVec2(1, 1), ImVec2(32, 32)),
                        ImGui::PanelManagerPaneAssociatedWindow("Performance", -1, [](ImGui::PanelManagerWindowData &windowData) -> void
                    {
                        ((Core *)windowData.userData)->drawPerformance(windowData);
                    }, this, ImGuiWindowFlags_NoScrollbar));

                    consolePane->addButtonAndWindow(
                        ImGui::Toolbutton("Settings", (Video::Object *)settingsButton.get(), ImVec2(0, 0), ImVec2(1, 1), ImVec2(32, 32)),
                        ImGui::PanelManagerPaneAssociatedWindow("Settings", -1, [](ImGui::PanelManagerWindowData &windowData) -> void
                    {
                        ((Core *)windowData.userData)->drawSettings(windowData);
                    }, this, ImGuiWindowFlags_NoScrollbar));
                }

                setDisplayMode(currentDisplayMode);
                population = getContext()->createClass<Plugin::Population>("Engine::Population", (Plugin::Core *)this);
                resources = getContext()->createClass<Engine::Resources>("Engine::Resources", (Plugin::Core *)this);
                renderer = getContext()->createClass<Plugin::Renderer>("Engine::Renderer", (Plugin::Core *)this);
                renderer->onShowUserInterface.connect<Core, &Core::onShowUserInterface>(this);

                std::cout << "Loading processor plugins" << std::endl;

                std::vector<std::string> processorNameList;
                getContext()->listTypes("ProcessorType", [&](std::string const &className) -> void
                {
                    processorNameList.push_back(className);
                });

                processorList.reserve(processorNameList.size());
                for (const auto &processorName : processorNameList)
                {
                    std::cout << "Processor found: " << processorName << std::endl;
                    processorList.push_back(getContext()->createClass<Plugin::Processor>(processorName, (Plugin::Core *)this));
                }

                for (auto &processor : processorList)
                {
                    processor->onInitialized();
                }

                ImGuiIO &imGuiIo = ImGui::GetIO();
                imGuiIo.KeyMap[ImGuiKey_Tab] = VK_TAB;
                imGuiIo.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
                imGuiIo.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
                imGuiIo.KeyMap[ImGuiKey_UpArrow] = VK_UP;
                imGuiIo.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
                imGuiIo.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
                imGuiIo.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
                imGuiIo.KeyMap[ImGuiKey_Home] = VK_HOME;
                imGuiIo.KeyMap[ImGuiKey_End] = VK_END;
                imGuiIo.KeyMap[ImGuiKey_Delete] = VK_DELETE;
                imGuiIo.KeyMap[ImGuiKey_Backspace] = VK_BACK;
                imGuiIo.KeyMap[ImGuiKey_Enter] = VK_RETURN;
                imGuiIo.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
                imGuiIo.KeyMap[ImGuiKey_A] = 'A';
                imGuiIo.KeyMap[ImGuiKey_C] = 'C';
                imGuiIo.KeyMap[ImGuiKey_V] = 'V';
                imGuiIo.KeyMap[ImGuiKey_X] = 'X';
                imGuiIo.KeyMap[ImGuiKey_Y] = 'Y';
                imGuiIo.KeyMap[ImGuiKey_Z] = 'Z';
                imGuiIo.MouseDrawCursor = false;

                windowActive = true;
                engineRunning = true;

                window->setVisibility(true);

				std::cout << "Starting engine" << std::endl;

                population->load("demo");
            }

            ~Core(void)
            {
                renderer->onShowUserInterface.disconnect<Core, &Core::onShowUserInterface>(this);
                window->onClose.disconnect<Core, &Core::onClose>(this);
                window->onActivate.disconnect<Core, &Core::onActivate>(this);
                window->onSizeChanged.disconnect<Core, &Core::onSizeChanged>(this);
                window->onKeyPressed.disconnect<Core, &Core::onKeyPressed>(this);
                window->onCharacter.disconnect<Core, &Core::onCharacter>(this);
                window->onSetCursor.disconnect<Core, &Core::onSetCursor>(this);
                window->onMouseClicked.disconnect<Core, &Core::onMouseClicked>(this);
                window->onMouseWheel.disconnect<Core, &Core::onMouseWheel>(this);
                window->onMousePosition.disconnect<Core, &Core::onMousePosition>(this);
                window->onMouseMovement.disconnect<Core, &Core::onMouseMovement>(this);

                panelManager.clear();
                consoleButton = nullptr;
                performanceButton = nullptr;
                settingsButton = nullptr;

                processorList.clear();
                renderer = nullptr;
                resources = nullptr;
                population = nullptr;
                videoDevice = nullptr;
                window = nullptr;
                JSON::Save(getContext()->getRootFileName("config.json"), configuration);
                CoUninitialize();
            }

			void setDisplayMode(uint32_t displayMode)
            {
                auto &displayModeData = displayModeList[displayMode];
				std::cout << "Setting display mode: " << displayModeData.width << "x" << displayModeData.height << std::endl;
                if (displayMode < displayModeList.size())
                {
                    currentDisplayMode = displayMode;
                    videoDevice->setDisplayMode(displayModeData);
                    window->move();
                    onResize.emit();
                }
            }

            // ImGui
            void drawConsole(ImGui::PanelManagerWindowData &windowData)
            {
                auto listBoxSize = (windowData.size - (ImGui::GetStyle().WindowPadding * 2.0f));
                listBoxSize.y -= ImGui::GetTextLineHeightWithSpacing();
                if (ImGui::ListBoxHeader("##empty", listBoxSize))
                {
                    const auto logCount = 0;// logList.size();
                    ImGuiListClipper clipper(logCount, ImGui::GetTextLineHeightWithSpacing());
                    while (clipper.Step())
                    {
                        for (int logIndex = clipper.DisplayStart; logIndex < clipper.DisplayEnd; ++logIndex)
                        {
                            ImGui::PushID(logIndex);
                            //ImGui::TextColored(color, String::Format("%v: %v", system, message).c_str());
                            ImGui::PopID();
                        }
                    };

                    ImGui::ListBoxFooter();
                }
            }

            std::string selectedEvent;
            std::string selectedSystem;
            void drawPerformance(ImGui::PanelManagerWindowData &windowData)
            {
                if (systemHistoryMap.empty())
                {
                    return;
                }

                auto clientSize = (windowData.size - (ImGui::GetStyle().WindowPadding * 2.0f));
                clientSize.y -= ImGui::GetTextLineHeightWithSpacing();

                ImGui::BeginGroup();
                ImVec2 eventSize(300.0f, clientSize.y);
                if (ImGui::BeginChildFrame(ImGui::GetCurrentWindow()->GetID("##systemEventTree"), eventSize))
                {
                    for (const auto &systemPair : systemHistoryMap)
                    {
                        uint32_t flags = ImGuiTreeNodeFlags_CollapsingHeader;
                        if (systemPair.first == selectedSystem)
                        {
                            flags |= ImGuiTreeNodeFlags_DefaultOpen;
                        }

                        if (ImGui::TreeNodeEx(systemPair.first.c_str(), flags))
                        {
                            for (const auto &eventPair : systemPair.second)
                            {
                                bool isSelected = (eventPair.first == selectedEvent);
                                isSelected = ImGui::Selectable(eventPair.first.c_str(), &isSelected);
                                if (isSelected)
                                {
                                    selectedSystem = systemPair.first;
                                    selectedEvent = eventPair.first;
                                }
                            }

                            ImGui::TreePop();
                        }
                    }

                    ImGui::EndChildFrame();
                }

                ImGui::EndGroup();
                auto systemSearch = systemHistoryMap.find(selectedSystem);
                if (systemSearch != std::end(systemHistoryMap))
                {
                    auto &eventHistoryMap = systemSearch->second;
                    auto eventSearch = eventHistoryMap.find(selectedEvent);
                    if (eventSearch != std::end(eventHistoryMap))
                    {
                        auto &selectedEvent = eventSearch->second;
                        if (!selectedEvent.data.empty())
                        {
                            ImGui::SameLine();
                            ImVec2 historySize((clientSize.x - 300.0f - ImGui::GetStyle().WindowPadding.x), clientSize.y);
                            GUI::PlotHistogram("##historyHistogram", [](void *userData, int index) -> float
                            {
                                auto &data = *(std::vector<float> *)userData;
                                return (index < int(data.size()) ? data[index] : 0.0f);
                            }, &selectedEvent.data, HistoryLength, 0, 0.0f, selectedEvent.maximum, historySize);
                        }
                    }
                }
            }

            void drawSettings(ImGui::PanelManagerWindowData &windowData)
            {
                if (ImGui::Checkbox("FullScreen", &fullScreen))
                {
                    if (fullScreen)
                    {
                        window->move(Math::Int2::Zero);
                    }

                    videoDevice->setFullScreenState(fullScreen);
                    onResize.emit();
                    if (!fullScreen)
                    {
                        window->move();
                    }
                }

                ImGui::PushItemWidth(350.0f);
                if (GUI::ListBox("Display Mode", &currentDisplayMode, [](void *data, int index, const char **text) -> bool
                {
                    Core *core = static_cast<Core *>(data);
                    auto &mode = core->displayModeStringList[index];
                    (*text) = mode.c_str();
                    return true;
                }, this, displayModeStringList.size(), 5))
                {
                    configuration["display"]["mode"] = currentDisplayMode;
                    setDisplayMode(currentDisplayMode);
                    showModeChange = true;
                    modeChangeTimer = 10.0f;
                }

                ImGui::PopItemWidth();

                OnSettingsPanel.emit(ImGui::GetCurrentContext(), windowData);
            }

            // Window slots
            void onClose(void)
            {
                engineRunning = false;
                onExit.emit();
            }

            void onActivate(bool isActive)
            {
                windowActive = isActive;
            }

            void onSetCursor(bool &showCursor)
            {
                showCursor = false;
            }

            void onSizeChanged(bool isMinimized)
            {
                if (videoDevice && !isMinimized)
                {
                    videoDevice->handleResize();
                    onResize.emit();
                }
            }

            void onCharacter(wchar_t character)
            {
                ImGuiIO &imGuiIo = ImGui::GetIO();
                imGuiIo.AddInputCharacter(character);
            }

            void onKeyPressed(uint16_t key, bool state)
            {
                ImGuiIO &imGuiIo = ImGui::GetIO();
                imGuiIo.KeysDown[key] = state;
                if (state)
                {
                    switch (key)
                    {
                    case VK_ESCAPE:
                        imGuiIo.MouseDrawCursor = !imGuiIo.MouseDrawCursor;
                        break;
                    };

                    if (population)
                    {
                        switch (key)
                        {
                        case VK_F5:
                            population->save("autosave");
                            break;

                        case VK_F6:
                            population->load("autosave");
                            break;
                        };
                    }
                }

                if (!imGuiIo.MouseDrawCursor && population)
                {
                    switch (key)
                    {
                    case 'W':
                    case VK_UP:
                        population->action(Plugin::Population::Action("move_forward", state));
                        break;

                    case 'S':
                    case VK_DOWN:
                        population->action(Plugin::Population::Action("move_backward", state));
                        break;

                    case 'A':
                    case VK_LEFT:
                        population->action(Plugin::Population::Action("strafe_left", state));
                        break;

                    case 'D':
                    case VK_RIGHT:
                        population->action(Plugin::Population::Action("strafe_right", state));
                        break;

                    case VK_SPACE:
                        population->action(Plugin::Population::Action("jump", state));
                        break;

                    case VK_LCONTROL:
                        population->action(Plugin::Population::Action("crouch", state));
                        break;
                    };
                }
            }

            void onMouseClicked(Window::Button button, bool state)
            {
                ImGuiIO &imGuiIo = ImGui::GetIO();
                switch(button)
                {
                case Window::Button::Left:
                    imGuiIo.MouseDown[0] = state;
                    break;

                case Window::Button::Middle:
                    imGuiIo.MouseDown[2] = state;
                    break;

                case Window::Button::Right:
                    imGuiIo.MouseDown[1] = state;
                    break;
                };
            }

            void onMouseWheel(int32_t offset)
            {
                ImGuiIO &imGuiIo = ImGui::GetIO();
                imGuiIo.MouseWheel += (offset > 0 ? +1.0f : -1.0f);
            }

            void onMousePosition(int32_t xPosition, int32_t yPosition)
            {
                ImGuiIO &imGuiIo = ImGui::GetIO();
                imGuiIo.MousePos.x = xPosition;
                imGuiIo.MousePos.y = yPosition;
            }

            void onMouseMovement(int32_t xMovement, int32_t yMovement)
            {
                if (population)
                {
                    population->action(Plugin::Population::Action("turn", xMovement * mouseSensitivity));
                    population->action(Plugin::Population::Action("tilt", yMovement * mouseSensitivity));
                }
            }

            // Plugin::Core::Log
            void beginEvent(std::string const &system, std::string const &name)
            {
                auto &eventData = systemHistoryMap[system][name];
                eventData.current = timer.getImmediateTime();
            }

            void endEvent(std::string const &system, std::string const &name)
            {
                auto &eventData = systemHistoryMap[system][name];
                eventData.current = (timer.getImmediateTime() - eventData.current) * 1000.0f;
            }

            void setValue(std::string const &system, std::string const &name, float value)
            {
                auto &eventData = systemHistoryMap[system][name];
                eventData.current = value;
            }

            void adjustValue(std::string const &system, std::string const &name, float value)
            {
                auto &eventData = systemHistoryMap[system][name];
                eventData.current = ((std::isnan(eventData.current) ? 0.0f : eventData.current) + value);
            }

            // Renderer
            void onShowUserInterface(ImGuiContext * const guiContext)
            {
                ImGuiIO &imGuiIo = ImGui::GetIO();
                panelManager.setDisplayPortion(ImVec4(0, 0, imGuiIo.DisplaySize.x, imGuiIo.DisplaySize.y));

                float frameTime = timer.getUpdateTime();
                if (windowActive)
                {
                    if (imGuiIo.MouseDrawCursor)
                    {
                        if (showModeChange)
                        {
                            ImGui::SetNextWindowPosCenter();
                            ImGui::Begin("Keep Display Mode", nullptr, ImGuiWindowFlags_ShowBorders | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysUseWindowPadding);
                            ImGui::Text("Keep Display Mode?");

                            if (ImGui::Button("Yes"))
                            {
                                showModeChange = false;
                                previousDisplayMode = currentDisplayMode;
                            }

                            ImGui::SameLine();
                            modeChangeTimer -= frameTime;
                            if (modeChangeTimer <= 0.0f || ImGui::Button("No"))
                            {
                                showModeChange = false;
                                setDisplayMode(previousDisplayMode);
                            }

                            ImGui::Text(String::Format("(Revert in %v seconds)", uint32_t(modeChangeTimer)).c_str());

                            ImGui::End();
                        }
                    }
                    else
                    {
                        auto rectangle = window->getScreenRectangle();
                        window->setCursorPosition(Math::Int2(
                            int(Math::Interpolate(float(rectangle.minimum.x), float(rectangle.maximum.x), 0.5f)),
                            int(Math::Interpolate(float(rectangle.minimum.y), float(rectangle.maximum.y), 0.5f))));
                    }
                }
                else
                {
                    ImGui::SetNextWindowPosCenter();
                    ImGui::Begin("GEK Engine##Paused", nullptr, ImGuiWindowFlags_ShowBorders | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NoCollapse);
                    ImGui::Dummy(ImVec2(200, 0));
                    ImGui::Text("Paused");
                    ImGui::End();
                }

                panelManager.render();
            }

            // Plugin::Core
            bool update(void)
            {
                Core::Scope function(this, "Core", "Update Time");

                window->readEvents();

                timer.update();

                // Read keyboard modifiers inputs
                ImGuiIO &imGuiIo = ImGui::GetIO();
                imGuiIo.KeyCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                imGuiIo.KeyShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                imGuiIo.KeyAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;
                imGuiIo.KeySuper = false;
                // imGuiIo.KeysDown : filled by WM_KEYDOWN/WM_KEYUP events
                // imGuiIo.MousePos : filled by WM_MOUSEMOVE events
                // imGuiIo.MouseDown : filled by WM_*BUTTON* events
                // imGuiIo.MouseWheel : filled by WM_MOUSEWHEEL events

                if (windowActive)
                {
                    float frameTime = timer.getUpdateTime();
                    if (imGuiIo.MouseDrawCursor)
                    {
                        population->update(0.0f);
                    }
                    else
                    {
                        population->update(frameTime);
                    }

                    for (auto &systemPair : systemHistoryMap)
                    {
                        auto &historyMap = systemPair.second;
                        concurrency::parallel_for_each(std::begin(historyMap), std::end(historyMap), [&](EventHistoryMap::value_type &eventPair) -> void
                        {
                            static const auto adapt = [](float current, float target, float frameTime) -> float
                            {
                                return (current + ((target - current) * (1.0 - exp(-frameTime * 1.25f))));
                            };

                            auto &eventData = eventPair.second;
                            if (!std::isnan(eventData.current))
                            {
                                eventData.data.push_back(eventPair.second.current);
                                if (eventData.data.size() > HistoryLength)
                                {
                                    auto iterator = std::begin(eventData.data);
                                    eventData.data.erase(iterator);
                                }

                                if (eventData.data.size() == 1)
                                {
                                    eventData.maximum = eventData.minimum = eventData.current;
                                }
                                else
                                {
                                    auto minmax = std::minmax_element(std::begin(eventData.data), std::end(eventData.data));
                                    eventData.minimum = adapt(eventData.minimum, *minmax.first, frameTime);
                                    eventData.maximum = adapt(eventData.maximum, *minmax.second, frameTime);
                                }

                                eventData.current = Math::NotANumber;
                            }
                        });
                    }
                }

                return engineRunning;
            }

            JSON::Object getOption(std::string const &system, std::string const &name)
            {
                return JSON::Get(JSON::Get(configuration, system), name);
            }

            void setOption(std::string const &system, std::string const &name, JSON::Object const &value)
            {
                configuration[system][name] = value;
            }

            void deleteOption(std::string const &system, std::string const &name)
            {
                if (configuration.has_member(system))
                {
                    configuration[system].erase(name);
                }
            }

            Log * getLog(void) const
            {
                return (Plugin::Core::Log *)this;
            }

            Window * getWindow(void) const
            {
                return window.get();
            }

            Video::Device * getVideoDevice(void) const
            {
                return videoDevice.get();
            }

            Plugin::Population * getPopulation(void) const
            {
                return population.get();
            }

            Plugin::Resources * getResources(void) const
            {
                return resources.get();
            }

            Plugin::Renderer * getRenderer(void) const
            {
                return renderer.get();
            }

            ImGui::PanelManager * getPanelManager(void)
            {
                return &panelManager;
            }

            void listProcessors(std::function<void(Plugin::Processor *)> onProcessor)
            {
                for (const auto &processor : processorList)
                {
                    onProcessor(processor.get());
                }
            }
        };

        GEK_REGISTER_CONTEXT_USER(Core);
    }; // namespace Implementation
}; // namespace Gek
