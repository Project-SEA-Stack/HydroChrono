#include <hydroc/config.h>
#include <hydroc/gui/guihelper.h>
#include <hydroc/logging.h>

#include "guihelper_impl.h"

using namespace hydroc::gui;

std::shared_ptr<hydroc::gui::UI> hydroc::gui::CreateUI(bool visualizationOn) {
    if (visualizationOn) {
        return std::make_shared<hydroc::gui::GUI>();
    } else {
        return std::make_shared<hydroc::gui::UI>();
    }
}

// -----------------------------------------------------------------------------

UI::UI() : pSystem(nullptr) {}

void UI::Init(chrono::ChSystem* system, const char* title) {
    pSystem = system;
}

void UI::SetCamera(double x, double y, double z, double dirx, double diry, double dirz) {}

bool UI::IsRunning(double timestep) {
    return true;
}

// -----------------------------------------------------------------------------

GUI::GUI() {
#if defined(HYDROCHRONO_HAVE_VSG)
    pImpl = std::make_shared<hydroc::gui::GUIImplVSG>();
#elif defined(HYDROCHRONO_HAVE_IRRLICHT)
    pImpl = std::make_shared<hydroc::gui::GUIImplIRR>();
#else
    pImpl = std::make_shared<hydroc::gui::GUIImpl>();
#endif

    simulationStarted = false;  // Simulation is paused
}

void GUI::Init(chrono::ChSystem* system, const char* title) {
    UI::Init(system, title);
    pImpl->Init(*this, system, title);
}

void GUI::SetCamera(double x, double y, double z, double dirx, double diry, double dirz) {
    pImpl->SetCamera(x, y, z, dirx, diry, dirz);
}

bool GUI::IsRunning(double timestep) {
    return pImpl->IsRunning(timestep);
}
