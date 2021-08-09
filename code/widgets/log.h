#ifndef _SPINVADERS_WIDGETS_LOG_H_
#define _SPINVADERS_WIDGETS_LOG_H_

#include "lib/imgui/imgui.h"
#include "spinvaders_shared.h"

struct LogWidget {
  ImGuiTextBuffer buf;
  ImVector<int> line_offsets;
  bool auto_scroll;

  LogWidget() {
    auto_scroll = true;
    clear();
  }

  void clear() {
    buf.clear();
    line_offsets.clear();
    line_offsets.push_back(0);
  }

  void vadd(const char *fmt, va_list args) {
    int old_size = buf.size();
    buf.appendfv(fmt, args);
    for (int new_size = buf.size(); old_size < new_size; old_size++)
      if (buf[old_size] == '\n')
        line_offsets.push_back(old_size + 1);
  }

  void add(const char *fmt, ...) IM_FMTARGS(2) {
    va_list args;
    va_start(args, fmt);
    vadd(fmt, args);
    va_end(args);
  }

  void draw(const char *title, bool *p_open = nullptr) {
    if (!ImGui::Begin(title, p_open)) {
      ImGui::End();
      return;
    }

    bool clear_selected = ImGui::Button("Clear");
    ImGui::SameLine();
    bool copy_selected = ImGui::Button("Copy");

    ImGui::Separator();
    ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    if (clear_selected)
      clear();
    if (copy_selected)
      ImGui::LogToClipboard();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    const char *buf_begin = buf.begin();
    const char *buf_end = buf.end();
    ImGuiListClipper clipper;
    clipper.Begin(line_offsets.Size);
    while (clipper.Step()) {
      for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++) {
        const char *line_start = buf_begin + line_offsets[line_no];
        const char *line_end = (line_no + 1 < line_offsets.Size)
                                   ? (buf_begin + line_offsets[line_no + 1] - 1)
                                   : buf_end;
        ImGui::TextUnformatted(line_start, line_end);
      }
    }
    clipper.End();
    ImGui::PopStyleVar();

    if (auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
      ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
    ImGui::End();
  }
};

static void log_handler(adc_log_msg *msg) {
  char timestr[16];
  timestr[strftime(timestr, sizeof(timestr), "%H:%M:%S", msg->time)] = '\0';

  LogWidget *log_widget = (LogWidget *)msg->userdata;
  assert(log_widget);
  log_widget->add("%s %s %s:%d: ", adc_log_level_string(msg->level), timestr, msg->file, msg->line);
  log_widget->vadd(msg->fmt, msg->args);
  log_widget->add("\n");
}

#endif // _SPINVADERS_WIDGETS_LOG_H_
