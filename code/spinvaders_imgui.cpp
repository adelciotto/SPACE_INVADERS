#include "spinvaders_imgui.h"
#include "spinvaders.h"

#include "spinvaders_shared.h"
#include "widgets/log.h"

struct UI_State {
  ImGuiIO *io;

  bool show_log;
  bool emulation_paused;

  LogWidget log_widget;
};

static UI_State s_ui_state = {};

void imgui_setup() {
  ImGuiIO *io = &ImGui::GetIO();
  s_ui_state.io = io;
  s_ui_state.show_log = false;
  ImGui::StyleColorsDark();

  adc_log_add_callback(log_handler, &s_ui_state.log_widget, ADC_LOG_DEBUG);
}

void imgui_shutdown() {
  ImGui::DestroyContext();
}

static void draw_menu() {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("Emulation")) {
      if (ImGui::MenuItem("Pause", nullptr, spinvaders_paused())) {
        spinvaders_set_pause(!spinvaders_paused());
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Debug")) {
      ImGui::MenuItem("Log", nullptr, &s_ui_state.show_log);
      ImGui::EndMenu();
    }

    int textw = ImGui::CalcTextSize("60.000f ms/frame (60.0f FPS)").x;
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - textw);
    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / s_ui_state.io->Framerate,
                s_ui_state.io->Framerate);

    ImGui::EndMainMenuBar();
  }
}

static void draw_log() {
  ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
  s_ui_state.log_widget.draw("Log", &s_ui_state.show_log);
}

void imgui_draw() {
  ImGui::NewFrame();

  draw_menu();
  if (s_ui_state.show_log)
    draw_log();

  ImGui::Render();
}
