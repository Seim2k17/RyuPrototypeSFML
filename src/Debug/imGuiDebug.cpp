#include <Ryu/Character/CharacterIchi.h>
#include <Ryu/Debug/imGuiDebug.h>
#include <Ryu/Events/EventEnums.h>

#include <box2d/b2_math.h>
#include <fmt/core.h>
#include <imgui.h>
#include <iostream>
#include <memory>

namespace RyuDebug {

DebugWidgets::DebugWidgets(std::weak_ptr<CharacterIchi> character)
    : playerCharacter(character) {}

void DebugWidgets::ShowWidgets() {
    ImGuiIO &io = ImGui::GetIO();
    if (debugData.readMousePos) {
        debugData.mousPos[0] = io.MousePos.x;
        debugData.mousPos[1] = io.MousePos.y;

        if (io.MouseClicked[0]) {
            debugData.readMousePos = false;
            notify(*playerCharacter.lock(), Ryu::EEvent::TeleportDo);
        }
    }

    ImGui::Text("Ryu Debug Viewer\n"
                "(right-click to change position)");
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Basic")) {
        if (ImGui::IsMousePosValid())
            ImGui::Text("Mouse Position: %f/%f", io.MousePos.x, io.MousePos.y);
        else
            ImGui::Text("Mouse Position: <invalid>");
    }
    ImGui::Separator();
    if (ImGui::CollapsingHeader("World")) {
        ImGui::SliderFloat("WorldUpdateFactor: ", &debugData.worldUpdateFactor, 0.f, 2.f, "%.3f");
    }
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Character")) {
        // ImGui::Text("CharacterState: %s ",characterState);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
                           "CharacterFalling: %s",
                           debugData.characterIsFalling ? "true" : "false");
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "State: %s",
                           debugData.characterState);
        ImGui::Text("AniFrames: %i ", debugData.numFrames);

        ImGui::Text("AniFrames in Vector: %i ", debugData.numFramesVector);
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "JumpForce: {%f,%f}",
                           debugData.charJumpForce.x,
                           debugData.charJumpForce.y);
        ImGui::Separator();

        if (ImGui::InputFloat2("JumpForceForward",
                               debugData.jumpImpulseForwardVec)) {
            debugData.jumpImpulseForward.x = debugData.jumpImpulseForwardVec[0];
            debugData.jumpImpulseForward.y = debugData.jumpImpulseForwardVec[1];
            fmt::print("Set jumpForceForward {}/{}\n",
                       debugData.jumpImpulseForward.x,
                       debugData.jumpImpulseForward.y);
            notify(*playerCharacter.lock(), Ryu::EEvent::DebugValuesChanged);
        }
        if (ImGui::InputFloat2("JumpForceUp", debugData.jumpImpulseUpVec)) {
            debugData.jumpImpulseUp.x = debugData.jumpImpulseUpVec[0];
            debugData.jumpImpulseUp.y = debugData.jumpImpulseUpVec[1];
            fmt::print("Set jumpForceUp\n");
            notify(*playerCharacter.lock(), Ryu::EEvent::DebugValuesChanged);
        }
        if (ImGui::InputFloat2("Masscenter", debugData.massCenterVec)) {
            fmt::print("Set center of mass\n");
            notify(*playerCharacter.lock(), Ryu::EEvent::DebugValuesChanged);
        }
        if (ImGui::Button("Teleport")) {
            debugData.readMousePos = true;
            notify(*playerCharacter.lock(), Ryu::EEvent::TeleportWait);
        }
        if (ImGui::Button("Reset Values")) {
            debugData.jumpImpulseForwardVec[0] = 150;
            debugData.jumpImpulseForwardVec[1] = -250;
            debugData.jumpImpulseForward = {150, -250};
            debugData.jumpImpulseUpVec[0] = 0;
            debugData.jumpImpulseUpVec[1] = -200;
            debugData.jumpImpulseUp = {0, -200};
            notify(*playerCharacter.lock(), Ryu::EEvent::DebugValuesChanged);
        }

        if (ImGui::InputInt2("PositionCross-Offset", debugData.positionCross)) {
            notify(*playerCharacter.lock(), Ryu::EEvent::DebugValuesChanged);
        }
        DebugSpritesheetInfo();
    }
    ImGui::Separator();
    ImGui::Text("For ImGui-Demo-Window press '-' ");
}

void DebugWidgets::DebugSpritesheetInfo() {
    ImGui::Separator();
    ImGui::Text("PivotAbs (%d/%d)",
                playerCharacter.lock()->getSpriteAnimation().getPivotAbs().x,
                playerCharacter.lock()->getSpriteAnimation().getPivotAbs().y);
    ImGui::Separator();
    ImGui::Text("Origin (%f/%f)",
                playerCharacter.lock()->getSpriteAnimation().getOrigin().x,
                playerCharacter.lock()->getSpriteAnimation().getOrigin().y);
    ImGui::Separator();
    ImGui::Text("FrameSize: (%d/%d)",
                playerCharacter.lock()->getSpriteAnimation().getFrameSize().x,
                playerCharacter.lock()->getSpriteAnimation().getFrameSize().y);
    ImGui::Separator();
    ImGui::Text("LocalBounds: (%f/%f)",
                playerCharacter.lock()->getSpriteAnimation().getLocalBounds().size.y,
                playerCharacter.lock()->getSpriteAnimation().getLocalBounds().size.x);
    ImGui::Separator();
    ImGui::Text("GlobalBounds: (%f/%f)",
                playerCharacter.lock()->getSpriteAnimation().getGlobalBounds().size.y,
                playerCharacter.lock()->getSpriteAnimation().getGlobalBounds().size.x);
    // ImGui::Text("Spritesheetname: %s", playerCharacter->
    ImGui::Separator();
}

void DebugWidgets::CreateDebugGui() {
    if (debugData.showImGuiDemoWindow) {
        ImGui::ShowDemoWindow(&debugData.showImGuiDemoWindow);
    }

    if (debugData.activateRyuDebug) {

        static int location = -1;
        ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
        if (location >= 0) {
            const float PAD = 10.0f;
            const ImGuiViewport *viewport = ImGui::GetMainViewport();
            ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid
                                                 // menu-bar/task-bar, if any!
            ImVec2 work_size = viewport->WorkSize;
            ImVec2 window_pos, window_pos_pivot;
            window_pos.x = (location & 1) ? (work_pos.x + work_size.x - PAD)
                                          : (work_pos.x + PAD);
            window_pos.y = (location & 2) ? (work_pos.y + work_size.y - PAD)
                                          : (work_pos.y + PAD);
            window_pos_pivot.x = (location & 1) ? 1.0f : 0.0f;
            window_pos_pivot.y = (location & 2) ? 1.0f : 0.0f;
            ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always,
                                    window_pos_pivot);
            window_flags |= ImGuiWindowFlags_NoMove;
        } else if (location == -2) {
            // Center window
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                                    ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            window_flags |= ImGuiWindowFlags_NoMove;
        }
        ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
        if (ImGui::Begin("RyuDebugOverlay", &debugData.activateRyuDebug,
                         window_flags)) {
            ShowWidgets();
            if (ImGui::BeginPopupContextWindow()) {
                if (ImGui::MenuItem("Custom", NULL, location == -1))
                    location = -1;
                if (ImGui::MenuItem("Center", NULL, location == -2))
                    location = -2;
                if (ImGui::MenuItem("Top-left", NULL, location == 0))
                    location = 0;
                if (ImGui::MenuItem("Top-right", NULL, location == 1))
                    location = 1;
                if (ImGui::MenuItem("Bottom-left", NULL, location == 2))
                    location = 2;
                if (ImGui::MenuItem("Bottom-right", NULL, location == 3))
                    location = 3;
                if (debugData.activateRyuDebug && ImGui::MenuItem("Close"))
                    debugData.activateRyuDebug = false;
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }
}
} // namespace RyuDebug
