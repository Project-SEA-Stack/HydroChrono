#include "guihelper_impl.h"
using namespace hydroc::gui;

#ifdef HYDROCHRONO_HAVE_VSG

using namespace chrono::vsg3d;

// -----------------------------------------------------------------------------

class MyComponentVSG : public chrono::vsg3d::ChGuiComponentVSG {
  public:
    MyComponentVSG(chrono::vsg3d::ChVisualSystemVSG* vsys, bool& buttonPressed)
        : m_vsys(vsys), pressed(buttonPressed) {}
    virtual void render(vsg::CommandBuffer& cb) override {
        ImGuiWindowFlags window_flags = 0;
        window_flags |= ImGuiWindowFlags_NoTitleBar;
        window_flags |= ImGuiWindowFlags_NoScrollbar;
        window_flags |= ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoResize;
        window_flags |= ImGuiWindowFlags_NoCollapse;
        window_flags |= ImGuiWindowFlags_NoNav;
        window_flags |= ImGuiWindowFlags_NoBackground;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(viewport->GetCenter().x, viewport->WorkPos.y + 20), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300, 0));
        ImGui::Begin("HydroChrono", NULL, window_flags);

        if (ImGui::Button(pressed ? "Playing" : "Paused", ImVec2(200, 40))) {
            pressed = !pressed;
        }

        ImGui::End();
    }

  private:
    chrono::vsg3d::ChVisualSystemVSG* m_vsys;
    bool& pressed;
};

// -----------------------------------------------------------------------------

GUIImplVSG::GUIImplVSG() : pVis(chrono_types::make_shared<chrono::vsg3d::ChVisualSystemVSG>()) {}

void GUIImplVSG::Init(UI& ui, chrono::ChSystem* system, const char* title) {
    pVis->AttachSystem(system);

    pVis->SetWindowTitle(title);
    pVis->SetWindowSize(1280, 720);
    pVis->SetWindowPosition(100, 100);
    pVis->SetWindowTitle(title);

    pVis->AddCamera(chrono::ChVector3d(10, -50, 10), chrono::ChVector3d(0, 0, 0));
    pVis->SetCameraVertical(chrono::CameraVerticalDir::Z);
    pVis->SetCameraAngleDeg(40.0);

    pVis->SetLightIntensity(1.0f);
    pVis->SetLightDirection(1.5 * chrono::CH_PI_2, chrono::CH_PI_4);
    pVis->EnableShadows();

    bool enable_grid = true;  // TODO: Make this configurable
    if (enable_grid) {
        pVis->AddGrid(1, 1, 30, 30,
                      chrono::ChCoordsys<>(chrono::ChVector3d(0, 0.0, 0), chrono::QuatFromAngleZ(chrono::CH_PI_2)),
                      chrono::ChColor(.1f, .1f, .1f));
    }

    pVis->AddGuiComponent(std::make_shared<MyComponentVSG>(pVis.get(), ui.simulationStarted));

    pVis->Initialize();
}

void GUIImplVSG::SetCamera(double x, double y, double z, double dirx, double diry, double dirz) {
    pVis->AddCamera({x, y, z}, {dirx, diry, dirz});
}

bool GUIImplVSG::IsRunning(double timestep) {
    if (pVis->Run() == false) return false;

    pVis->BeginScene();
    pVis->Render();
    pVis->EndScene();

    return true;
}

#endif  // HYDROCHRONO_HAVE_VSG
