#include "ui/ui_theme.hpp"

namespace nerv {

void UITheme::apply_nerv_theme() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Rounding & borders
    style.WindowRounding = 8.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 5.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;

    style.WindowPadding = ImVec2(12.0f, 12.0f);
    style.FramePadding = ImVec2(8.0f, 6.0f);
    style.ItemSpacing = ImVec2(8.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
    style.ScrollbarSize = 12.0f;

    ImVec4* colors = style.Colors;

    // Palette:
    // Background: #05070D
    // Deep Navy: #0B1220
    // Electric Cyan: #5DEBFF
    // Neural Violet: #9A7BFF
    // Active Green: #63F5B0
    // Text: #EDF3FF

    ImVec4 bg_black   = ImVec4(0.020f, 0.027f, 0.051f, 0.94f);
    ImVec4 bg_black_o = ImVec4(0.020f, 0.027f, 0.051f, 1.00f);
    ImVec4 deep_navy  = ImVec4(0.043f, 0.071f, 0.125f, 0.90f);
    ImVec4 navy_card  = ImVec4(0.065f, 0.105f, 0.180f, 0.85f);
    ImVec4 cyan       = ImVec4(0.365f, 0.922f, 1.000f, 1.00f);
    ImVec4 cyan_hover = ImVec4(0.480f, 0.950f, 1.000f, 0.85f);
    ImVec4 cyan_alpha = ImVec4(0.365f, 0.922f, 1.000f, 0.25f);
    ImVec4 violet     = ImVec4(0.604f, 0.482f, 1.000f, 1.00f);
    ImVec4 text_white = ImVec4(0.929f, 0.953f, 1.000f, 1.00f);
    ImVec4 text_dim   = ImVec4(0.620f, 0.670f, 0.780f, 1.00f);
    ImVec4 border_col = ImVec4(0.120f, 0.180f, 0.280f, 0.65f);

    colors[ImGuiCol_Text]                  = text_white;
    colors[ImGuiCol_TextDisabled]          = text_dim;
    colors[ImGuiCol_WindowBg]              = bg_black;
    colors[ImGuiCol_ChildBg]               = deep_navy;
    colors[ImGuiCol_PopupBg]               = bg_black_o;
    colors[ImGuiCol_Border]                = border_col;
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_FrameBg]               = navy_card;
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.090f, 0.140f, 0.240f, 0.90f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.120f, 0.180f, 0.300f, 1.00f);
    colors[ImGuiCol_TitleBg]               = deep_navy;
    colors[ImGuiCol_TitleBgActive]         = deep_navy;
    colors[ImGuiCol_TitleBgCollapsed]      = deep_navy;
    colors[ImGuiCol_MenuBarBg]             = bg_black;
    colors[ImGuiCol_ScrollbarBg]           = deep_navy;
    colors[ImGuiCol_ScrollbarGrab]         = border_col;
    colors[ImGuiCol_ScrollbarGrabHovered]  = cyan_alpha;
    colors[ImGuiCol_ScrollbarGrabActive]   = cyan;
    colors[ImGuiCol_CheckMark]             = cyan;
    colors[ImGuiCol_SliderGrab]            = cyan;
    colors[ImGuiCol_SliderGrabActive]      = violet;
    colors[ImGuiCol_Button]                = navy_card;
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.100f, 0.170f, 0.280f, 0.95f);
    colors[ImGuiCol_ButtonActive]          = cyan_alpha;
    colors[ImGuiCol_Header]                = navy_card;
    colors[ImGuiCol_HeaderHovered]         = cyan_alpha;
    colors[ImGuiCol_HeaderActive]          = cyan_alpha;
    colors[ImGuiCol_Separator]             = border_col;
    colors[ImGuiCol_SeparatorHovered]      = cyan;
    colors[ImGuiCol_SeparatorActive]       = violet;
    colors[ImGuiCol_ResizeGrip]            = border_col;
    colors[ImGuiCol_ResizeGripHovered]     = cyan;
    colors[ImGuiCol_ResizeGripActive]      = violet;
    colors[ImGuiCol_Tab]                   = navy_card;
    colors[ImGuiCol_TabHovered]            = cyan_hover;
    colors[ImGuiCol_TabSelected]           = cyan_alpha;
    colors[ImGuiCol_PlotLines]             = cyan;
    colors[ImGuiCol_PlotLinesHovered]      = violet;
    colors[ImGuiCol_PlotHistogram]         = cyan;
    colors[ImGuiCol_PlotHistogramHovered]  = violet;
    colors[ImGuiCol_TableHeaderBg]         = deep_navy;
    colors[ImGuiCol_TableBorderStrong]     = border_col;
    colors[ImGuiCol_TableBorderLight]      = border_col;
}

} // namespace nerv
