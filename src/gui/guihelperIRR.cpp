#include "guihelper_impl.h"
using namespace hydroc::gui;

#ifdef HYDROCHRONO_HAVE_IRRLICHT

    #include <IEventReceiver.h>  // irrlicht
    #include <chrono_irrlicht/ChIrrMeshTools.h>

using namespace chrono::irrlicht;

// -----------------------------------------------------------------------------

using irr::EEVENT_TYPE;
using irr::s32;
using irr::core::rect;
using irr::gui::EGUI_EVENT_TYPE;

///@brief Define a class to manage user inputs via the GUI (i.e. play/pause button)
class hydroc::gui::GUIImplIRR::MyActionReceiver : public irr::IEventReceiver {
  public:
    MyActionReceiver(bool& buttonPressed);
    bool OnEvent(const irr::SEvent& event);
    void Init(chrono::irrlicht::ChVisualSystemIrrlicht* vsys);
    void SetCamera(double x, double y, double z, double dirx, double diry, double dirz);

  private:
    chrono::irrlicht::ChVisualSystemIrrlicht* vis;
    irr::gui::IGUIButton* pauseButton;
    irr::gui::IGUIStaticText* buttonText;

    bool& pressed;
};

hydroc::gui::GUIImplIRR::MyActionReceiver::MyActionReceiver(bool& buttonPressed) : pressed(buttonPressed) {}

/// @brief Initialize Action with System
/// @param vsys
void hydroc::gui::GUIImplIRR::MyActionReceiver::Init(chrono::irrlicht::ChVisualSystemIrrlicht* vsys) {
    // store pointer application
    vis = vsys;

    // ..add a GUI button to control pause/play
    pauseButton = vis->GetGUIEnvironment()->addButton(rect<s32>(510, 20, 650, 35));
    buttonText  = vis->GetGUIEnvironment()->addStaticText(L"Paused", rect<s32>(560, 20, 600, 35), false);
}

bool hydroc::gui::GUIImplIRR::MyActionReceiver::OnEvent(const irr::SEvent& event) {
    // check if user clicked button
    if (event.EventType == EEVENT_TYPE::EET_GUI_EVENT) {
        switch (event.GUIEvent.EventType) {
            case EGUI_EVENT_TYPE::EGET_BUTTON_CLICKED:
                pressed = !pressed;
                if (pressed) {
                    buttonText->setText(L"Playing");
                } else {
                    buttonText->setText(L"Paused");
                }
                return pressed;
                break;
            default:
                break;
        }
    }
    return false;
}

// -----------------------------------------------------------------------------

GUIImplIRR::GUIImplIRR() : pVis(chrono_types::make_shared<chrono::irrlicht::ChVisualSystemIrrlicht>()) {}

void GUIImplIRR::InitReceiver(bool& theSimulationStarted) {
    receiver = std::make_shared<MyActionReceiver>(theSimulationStarted);
}

void GUIImplIRR::Init(UI& ui, chrono::ChSystem* system, const char* title) {
    // ========== TEMPORARY DIAGNOSTIC CODE FOR GUI CRASH DEBUGGING ==========
    // TODO: Remove this diagnostic block once GUI crash is resolved

    try {
        hydroc::debug::LogDebug("🔍 GUIImplIRR::Init - Attaching system to visualization...");
        pVis->AttachSystem(system);
        hydroc::debug::LogDebug("✅ System attached successfully");
    } catch (const std::exception& e) {
        hydroc::cli::LogError(std::string("🔥 Exception during AttachSystem: ") + e.what());
        throw;  // Re-throw to be caught by outer guard
    } catch (...) {
        hydroc::cli::LogError("🔥 Unknown exception during AttachSystem");
        throw;  // Re-throw to be caught by outer guard
    }

    try {
        hydroc::debug::LogDebug("🔍 GUIImplIRR::Init - Setting window properties...");
        pVis->SetWindowSize(1280, 720);
        pVis->SetWindowTitle(title);
        pVis->SetCameraVertical(chrono::CameraVerticalDir::Z);
        hydroc::debug::LogDebug("✅ Window properties set successfully");
    } catch (const std::exception& e) {
        hydroc::cli::LogError(std::string("🔥 Exception during window setup: ") + e.what());
        throw;  // Re-throw to be caught by outer guard
    } catch (...) {
        hydroc::cli::LogError("🔥 Unknown exception during window setup");
        throw;  // Re-throw to be caught by outer guard
    }

    try {
        hydroc::debug::LogDebug("🔍 GUIImplIRR::Init - Initializing visualization system...");
        pVis->Initialize();
        hydroc::debug::LogDebug("✅ Visualization system initialized successfully");
    } catch (const std::exception& e) {
        hydroc::cli::LogError(std::string("🔥 Exception during Initialize: ") + e.what());
        throw;  // Re-throw to be caught by outer guard
    } catch (...) {
        hydroc::cli::LogError("🔥 Unknown exception during Initialize");
        throw;  // Re-throw to be caught by outer guard
    }

    try {
        hydroc::debug::LogDebug("🔍 GUIImplIRR::Init - Setting up event receiver...");
        InitReceiver(ui.simulationStarted);
        receiver->Init(pVis.get());
        pVis->AddUserEventReceiver(receiver.get());
        hydroc::debug::LogDebug("✅ Event receiver set up successfully");
    } catch (const std::exception& e) {
        hydroc::cli::LogError(std::string("🔥 Exception during receiver setup: ") + e.what());
        // Don't re-throw here, receiver is optional
    } catch (...) {
        hydroc::cli::LogError("🔥 Unknown exception during receiver setup");
        // Don't re-throw here, receiver is optional
    }

    // ========== GUARDED VISUAL ASSET CREATION ==========
    // TODO: Remove guards once GUI crash is resolved

    // Guard visual asset creation - can be easily toggled off for debugging
    bool enable_visual_assets = true;  // TODO: Make this configurable

    if (enable_visual_assets) {
        try {
            hydroc::debug::LogDebug("🔍 GUIImplIRR::Init - Adding logo...");
            pVis->AddLogo();
            hydroc::debug::LogDebug("✅ Logo added successfully");
        } catch (const std::exception& e) {
            hydroc::cli::LogError(std::string("🔥 Exception during AddLogo: ") + e.what());
            hydroc::cli::LogWarning("⚠️ Continuing without logo");
        } catch (...) {
            hydroc::cli::LogError("🔥 Unknown exception during AddLogo");
            hydroc::cli::LogWarning("⚠️ Continuing without logo");
        }

        try {
            hydroc::debug::LogDebug("🔍 GUIImplIRR::Init - Adding skybox...");
            pVis->AddSkyBox();
            hydroc::debug::LogDebug("✅ Skybox added successfully");
        } catch (const std::exception& e) {
            hydroc::cli::LogError(std::string("🔥 Exception during AddSkyBox: ") + e.what());
            hydroc::cli::LogWarning("⚠️ Continuing without skybox");
        } catch (...) {
            hydroc::cli::LogError("🔥 Unknown exception during AddSkyBox");
            hydroc::cli::LogWarning("⚠️ Continuing without skybox");
        }

        try {
            hydroc::debug::LogDebug("🔍 GUIImplIRR::Init - Adding camera...");
            pVis->AddCamera(chrono::ChVector3d(8, -25, 15), chrono::ChVector3d(0, 0, 0));
            hydroc::debug::LogDebug("✅ Camera added successfully");
        } catch (const std::exception& e) {
            hydroc::cli::LogError(std::string("🔥 Exception during AddCamera: ") + e.what());
            hydroc::cli::LogWarning("⚠️ Continuing without camera");
        } catch (...) {
            hydroc::cli::LogError("🔥 Unknown exception during AddCamera");
            hydroc::cli::LogWarning("⚠️ Continuing without camera");
        }

        try {
            hydroc::debug::LogDebug("🔍 GUIImplIRR::Init - Adding lights...");
            pVis->AddTypicalLights();
            hydroc::debug::LogDebug("✅ Lights added successfully");
        } catch (const std::exception& e) {
            hydroc::cli::LogError(std::string("🔥 Exception during AddTypicalLights: ") + e.what());
            hydroc::cli::LogWarning("⚠️ Continuing without lights");
        } catch (...) {
            hydroc::cli::LogError("🔥 Unknown exception during AddTypicalLights");
            hydroc::cli::LogWarning("⚠️ Continuing without lights");
        }
    } else {
        hydroc::cli::LogWarning("⚠️ Visual assets disabled for debugging");
    }

    hydroc::debug::LogDebug("✅ GUIImplIRR::Init completed");
    // ========== END TEMPORARY DIAGNOSTIC CODE ==========
}

void GUIImplIRR::SetCamera(double x, double y, double z, double dirx, double diry, double dirz) {
    pVis->AddCamera({x, y, z}, {dirx, diry, dirz});
}

bool GUIImplIRR::IsRunning(double timestep) {
    // ========== TEMPORARY DIAGNOSTIC CODE FOR GUI CRASH DEBUGGING ==========
    // TODO: Remove this diagnostic block once GUI crash is resolved

    try {
        if (pVis->Run() == false) return false;
    } catch (const std::exception& e) {
        hydroc::cli::LogError(std::string("🔥 Exception during pVis->Run(): ") + e.what());
        return false;  // Stop the simulation loop
    } catch (...) {
        hydroc::cli::LogError("🔥 Unknown exception during pVis->Run()");
        return false;  // Stop the simulation loop
    }

    try {
        pVis->BeginScene();
    } catch (const std::exception& e) {
        hydroc::cli::LogError(std::string("🔥 Exception during BeginScene: ") + e.what());
        return false;  // Stop the simulation loop
    } catch (...) {
        hydroc::cli::LogError("🔥 Unknown exception during BeginScene");
        return false;  // Stop the simulation loop
    }

    try {
        pVis->Render();
    } catch (const std::exception& e) {
        hydroc::cli::LogError(std::string("🔥 Exception during Render: ") + e.what());
        // Try to end scene gracefully and return false
        try {
            pVis->EndScene();
        } catch (...) {
        }
        return false;  // Stop the simulation loop
    } catch (...) {
        hydroc::cli::LogError("🔥 Unknown exception during Render");
        // Try to end scene gracefully and return false
        try {
            pVis->EndScene();
        } catch (...) {
        }
        return false;  // Stop the simulation loop
    }

    // Guard grid drawing - optional visual element that can be disabled
    bool enable_grid = true;  // TODO: Make this configurable
    if (enable_grid) {
        try {
            // Add grid to materialize horizontal plane
            tools::drawGrid(
                pVis.get(), 1, 1, 30, 30,
                chrono::ChCoordsys<>(chrono::ChVector3d(0, 0.0, 0), chrono::QuatFromAngleZ(chrono::CH_PI_2)),
                chrono::ChColor(.1f, .1f, .1f), true);
        } catch (const std::exception& e) {
            hydroc::cli::LogError(std::string("🔥 Exception during drawGrid: ") + e.what());
            hydroc::cli::LogWarning("⚠️ Continuing without grid");
        } catch (...) {
            hydroc::cli::LogError("🔥 Unknown exception during drawGrid");
            hydroc::cli::LogWarning("⚠️ Continuing without grid");
        }
    }

    try {
        pVis->EndScene();
    } catch (const std::exception& e) {
        hydroc::cli::LogError(std::string("🔥 Exception during EndScene: ") + e.what());
        return false;  // Stop the simulation loop
    } catch (...) {
        hydroc::cli::LogError("🔥 Unknown exception during EndScene");
        return false;  // Stop the simulation loop
    }

    return true;
    // ========== END TEMPORARY DIAGNOSTIC CODE ==========
}

#endif  // HYDROCHRONO_HAVE_IRRLICHT
