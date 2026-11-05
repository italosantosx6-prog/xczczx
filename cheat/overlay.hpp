#pragma once
// language: C++, file: cheat/overlay.hpp, target: Windows 11 x64, MSVC
// DX11 transparent overlay + creative custom ImGui menu
// Theme: dark slate + red accent (Valorant palette)
// Features: tabs, custom toggles, color presets, animated tab indicator

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")
#include <dwmapi.h>
#include <cmath>
#include <cstdio>

#include "../external/imgui/imgui.h"
#include "../external/imgui/backends/imgui_impl_win32.h"
#include "../external/imgui/backends/imgui_impl_dx11.h"

#include "esp.hpp"

namespace overlay
{

// ─────────────────────────────────────────────────────────────────────────────
//  PALETTE
// ─────────────────────────────────────────────────────────────────────────────
namespace pal
{
    // backgrounds
    constexpr ImVec4 bg_deep   = { 0.047f, 0.067f, 0.090f, 0.97f }; // #0C1117
    constexpr ImVec4 bg_panel  = { 0.075f, 0.098f, 0.129f, 1.00f }; // #131921
    constexpr ImVec4 bg_item   = { 0.102f, 0.129f, 0.165f, 1.00f }; // #1A212A
    constexpr ImVec4 bg_hover  = { 0.133f, 0.165f, 0.208f, 1.00f }; // #222A35
    constexpr ImVec4 bg_active = { 0.161f, 0.196f, 0.247f, 1.00f }; // #29323F

    // accent — Valorant red
    constexpr ImVec4 accent    = { 1.000f, 0.271f, 0.333f, 1.00f }; // #FF4555
    constexpr ImVec4 accent_dim= { 0.600f, 0.161f, 0.200f, 1.00f }; // #992933
    constexpr ImVec4 accent_bg = { 1.000f, 0.271f, 0.333f, 0.15f }; // accent tinted bg

    // text
    constexpr ImVec4 text_hi   = { 0.918f, 0.925f, 0.937f, 1.00f }; // #EAECEF
    constexpr ImVec4 text_mid  = { 0.600f, 0.627f, 0.667f, 1.00f }; // #99A0AA
    constexpr ImVec4 text_lo   = { 0.337f, 0.365f, 0.400f, 1.00f }; // #565D66

    // separators / borders
    constexpr ImVec4 border    = { 0.173f, 0.208f, 0.255f, 1.00f }; // #2C3541

    inline ImU32 u32(ImVec4 v) { return ImGui::ColorConvertFloat4ToU32(v); }
}

// ─────────────────────────────────────────────────────────────────────────────
//  STATE
// ─────────────────────────────────────────────────────────────────────────────
static HWND                    g_hwnd      = nullptr;
static ID3D11Device*           g_dev       = nullptr;
static ID3D11DeviceContext*    g_ctx       = nullptr;
static IDXGISwapChain*         g_sc        = nullptr;
static ID3D11RenderTargetView* g_rtv       = nullptr;
static float                   g_sw        = 1920.f;
static float                   g_sh        = 1080.f;
static bool                    g_running   = true;

// ── ESP config ───────────────────────────────────────────────────────────────
struct EspConfig {
    bool  box           = true;
    bool  skeleton      = true;
    bool  head_dot      = true;
    bool  snap          = false;
    bool  health_bar    = true;
    bool  armor_bar     = true;
    bool  distance      = true;
    bool  name_tag      = false;
    bool  enemy_only    = true;
    float max_dist      = 150.f;
    float box_thick     = 1.5f;
    float skel_thick    = 1.2f;
    ImVec4 col_enemy    = { 1.f, 0.271f, 0.333f, 1.f };   // #FF4555
    ImVec4 col_team     = { 0.2f, 0.78f,  0.42f, 1.f };   // #33C76B
    ImVec4 col_low_hp   = { 1.f,  0.4f,  0.1f,  1.f };    // switch color below 30% hp
};
static EspConfig g_cfg;

// ─────────────────────────────────────────────────────────────────────────────
//  IMGUI STYLE SETUP
// ─────────────────────────────────────────────────────────────────────────────
static void SetupStyle()
{
    ImGuiStyle& s = ImGui::GetStyle();

    s.WindowRounding    = 8.f;
    s.ChildRounding     = 6.f;
    s.FrameRounding     = 4.f;
    s.GrabRounding      = 4.f;
    s.ScrollbarRounding = 4.f;
    s.PopupRounding     = 6.f;
    s.TabRounding       = 4.f;

    s.WindowPadding     = { 14.f, 14.f };
    s.FramePadding      = { 10.f,  5.f };
    s.ItemSpacing       = {  8.f,  6.f };
    s.ItemInnerSpacing  = {  6.f,  4.f };
    s.ScrollbarSize     = 10.f;
    s.GrabMinSize       = 10.f;
    s.WindowBorderSize  = 1.f;
    s.ChildBorderSize   = 1.f;
    s.FrameBorderSize   = 0.f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]           = pal::bg_deep;
    c[ImGuiCol_ChildBg]            = pal::bg_panel;
    c[ImGuiCol_PopupBg]            = pal::bg_panel;
    c[ImGuiCol_Border]             = pal::border;
    c[ImGuiCol_BorderShadow]       = { 0,0,0,0 };
    c[ImGuiCol_FrameBg]            = pal::bg_item;
    c[ImGuiCol_FrameBgHovered]     = pal::bg_hover;
    c[ImGuiCol_FrameBgActive]      = pal::bg_active;
    c[ImGuiCol_TitleBg]            = pal::bg_deep;
    c[ImGuiCol_TitleBgActive]      = pal::bg_deep;
    c[ImGuiCol_TitleBgCollapsed]   = pal::bg_deep;
    c[ImGuiCol_MenuBarBg]          = pal::bg_panel;
    c[ImGuiCol_ScrollbarBg]        = pal::bg_panel;
    c[ImGuiCol_ScrollbarGrab]      = pal::bg_active;
    c[ImGuiCol_ScrollbarGrabHovered]= pal::bg_hover;
    c[ImGuiCol_ScrollbarGrabActive] = pal::accent;
    c[ImGuiCol_CheckMark]          = pal::accent;
    c[ImGuiCol_SliderGrab]         = pal::accent;
    c[ImGuiCol_SliderGrabActive]   = pal::accent;
    c[ImGuiCol_Button]             = pal::bg_item;
    c[ImGuiCol_ButtonHovered]      = pal::bg_hover;
    c[ImGuiCol_ButtonActive]       = pal::accent_dim;
    c[ImGuiCol_Header]             = pal::accent_bg;
    c[ImGuiCol_HeaderHovered]      = pal::bg_hover;
    c[ImGuiCol_HeaderActive]       = pal::accent_bg;
    c[ImGuiCol_Separator]          = pal::border;
    c[ImGuiCol_SeparatorHovered]   = pal::accent;
    c[ImGuiCol_SeparatorActive]    = pal::accent;
    c[ImGuiCol_ResizeGrip]         = pal::bg_item;
    c[ImGuiCol_ResizeGripHovered]  = pal::accent;
    c[ImGuiCol_ResizeGripActive]   = pal::accent;
    c[ImGuiCol_Tab]                = pal::bg_item;
    c[ImGuiCol_TabHovered]         = pal::bg_hover;
    c[ImGuiCol_TabActive]          = pal::accent_bg;
    c[ImGuiCol_TabUnfocused]       = pal::bg_item;
    c[ImGuiCol_TabUnfocusedActive] = pal::bg_item;
    c[ImGuiCol_Text]               = pal::text_hi;
    c[ImGuiCol_TextDisabled]       = pal::text_lo;
    c[ImGuiCol_PlotLines]          = pal::accent;
    c[ImGuiCol_PlotLinesHovered]   = pal::accent;
    c[ImGuiCol_PlotHistogram]      = pal::accent;
    c[ImGuiCol_PlotHistogramHovered]= pal::accent;
}

// ─────────────────────────────────────────────────────────────────────────────
//  CUSTOM WIDGETS
// ─────────────────────────────────────────────────────────────────────────────

// Animated toggle switch (iOS-style pill)
// Returns true if value changed
static bool Toggle(const char* label, bool* val)
{
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      pos = ImGui::GetCursorScreenPos();

    float h      = 18.f;   // pill height
    float w      = 34.f;   // pill width
    float pad    = 2.f;
    float radius = h * 0.5f;

    // Invisible button for interaction
    ImGui::InvisibleButton(label, { w + ImGui::CalcTextSize(label).x + 10.f, h });
    bool changed = false;
    if (ImGui::IsItemClicked()) { *val = !*val; changed = true; }

    // Animate: lerp knob position
    static float anim[128] = {};
    static int   anim_id   = 0;
    // Use label hash as a simple stable id
    int hash = 0;
    for (const char* p = label; *p; p++) hash = hash*31 + *p;
    int slot = ((unsigned)hash) % 128;
    float target = *val ? 1.f : 0.f;
    anim[slot] += (target - anim[slot]) * 0.25f; // lerp factor (per-frame)

    float t = anim[slot];

    // Track background
    ImU32 bg_col = ImGui::ColorConvertFloat4ToU32({
        pal::accent.x * t + pal::bg_item.x * (1-t),
        pal::accent.y * t + pal::bg_item.y * (1-t),
        pal::accent.z * t + pal::bg_item.z * (1-t),
        1.f
    });
    dl->AddRectFilled({ pos.x, pos.y }, { pos.x+w, pos.y+h }, bg_col, radius);

    // Knob
    float knob_x = pos.x + pad + radius + t * (w - 2*pad - 2*radius);
    float knob_y = pos.y + h * 0.5f;
    dl->AddCircleFilled({ knob_x, knob_y }, radius - pad, IM_COL32(240,242,247,255));

    // Label
    ImGui::SameLine(0, 10.f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 1.f); // vertical center
    ImGui::TextColored(pal::text_hi, "%s", label);

    return changed;
}

// Section header with colored left bar
static void SectionHeader(const char* text)
{
    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2 pos      = ImGui::GetCursorScreenPos();
    float  bar_w    = 3.f;
    float  h        = ImGui::GetTextLineHeight();
    dl->AddRectFilled({ pos.x, pos.y }, { pos.x + bar_w, pos.y + h }, pal::u32(pal::accent));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + bar_w + 6.f);
    ImGui::TextColored(pal::text_hi, "%s", text);
    ImGui::Dummy({ 0, 2.f });
}

// Thin full-width separator
static void HSep()
{
    ImGui::Dummy({ 0, 2.f });
    ImGui::PushStyleColor(ImGuiCol_Separator, pal::border);
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Dummy({ 0, 2.f });
}

// Slim labeled slider (no default ImGui label noise)
static bool SlimSlider(const char* label, float* v, float mn, float mx, const char* fmt = "%.0f")
{
    ImGui::TextColored(pal::text_mid, "%s", label);
    ImGui::SameLine(130.f);
    ImGui::SetNextItemWidth(-1.f);
    char id[64]; snprintf(id, sizeof(id), "##sl_%s", label);
    return ImGui::SliderFloat(id, v, mn, mx, fmt);
}

// Color button + picker inline
static void ColorRow(const char* label, ImVec4* col)
{
    ImGui::TextColored(pal::text_mid, "%s", label);
    ImGui::SameLine(130.f);
    char id[64]; snprintf(id, sizeof(id), "##cp_%s", label);
    ImGui::SetNextItemWidth(-1.f);
    ImGui::ColorEdit3(id, (float*)col,
        ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoPicker |
        ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_InputRGB);
    // Full picker on click — just open the small swatch
    // For a full picker use: ImGuiColorEditFlags_NoSmallPreview and add ColorButton separately
}

// ─────────────────────────────────────────────────────────────────────────────
//  MENU WINDOW
// ─────────────────────────────────────────────────────────────────────────────
static bool g_show_menu = false;

struct TabDesc { const char* icon; const char* label; };
static TabDesc g_tabs[] = {
    { "[>]", "ESP"     },
    { "[*]", "COLORS"  },
    { "[i]", "INFO"    },
};
static int  g_active_tab = 0;
static float g_tab_anim  = 0.f; // animated tab indicator x pos

static void DrawMenu()
{
    ImGui::SetNextWindowSize({ 320.f, 440.f }, ImGuiCond_Once);
    ImGui::SetNextWindowPos({ 60.f, 60.f }, ImGuiCond_Once);
    ImGui::SetNextWindowBgAlpha(0.0f); // we draw our own bg

    ImGui::Begin("##vrd_menu", &g_show_menu,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImVec2 win_pos  = ImGui::GetWindowPos();
    ImVec2 win_size = ImGui::GetWindowSize();
    ImDrawList* dl  = ImGui::GetWindowDrawList();

    // ── Outer window background + border ────────────────────────────────────
    dl->AddRectFilled(win_pos,
        { win_pos.x + win_size.x, win_pos.y + win_size.y },
        pal::u32(pal::bg_deep), 10.f);
    dl->AddRect(win_pos,
        { win_pos.x + win_size.x, win_pos.y + win_size.y },
        pal::u32(pal::border), 10.f, 0, 1.f);

    // ── Accent top bar ───────────────────────────────────────────────────────
    dl->AddRectFilled(win_pos,
        { win_pos.x + win_size.x, win_pos.y + 3.f },
        pal::u32(pal::accent), 10.f, ImDrawFlags_RoundCornersTop);

    ImGui::SetCursorPos({ 14.f, 14.f });

    // ── Header ──────────────────────────────────────────────────────────────
    {
        ImGui::PushStyleColor(ImGuiCol_Text, pal::accent);
        ImGui::Text("VRD");
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 6.f);
        ImGui::TextColored(pal::text_hi, "ESP");
        ImGui::SameLine();
        ImGui::SetCursorPosX(win_size.x - 70.f);
        ImGui::TextColored(pal::text_lo, "v2.0");
        ImGui::SameLine();
        // ESC / END hint
        ImGui::TextColored(pal::text_lo, "[END]");
    }

    ImGui::Dummy({ 0, 4.f });

    // ── Tab bar ──────────────────────────────────────────────────────────────
    constexpr int TAB_COUNT = 3;
    float tab_w    = (win_size.x - 28.f) / TAB_COUNT;
    float tab_base = ImGui::GetCursorPosY();
    ImVec2 tab_start_abs = { win_pos.x + 14.f, win_pos.y + tab_base };

    // Animated indicator bar
    float ind_target = g_active_tab * tab_w;
    g_tab_anim += (ind_target - g_tab_anim) * 0.20f;
    dl->AddRectFilled(
        { tab_start_abs.x + g_tab_anim, tab_start_abs.y + 26.f },
        { tab_start_abs.x + g_tab_anim + tab_w, tab_start_abs.y + 28.f },
        pal::u32(pal::accent), 2.f
    );

    ImGui::SetCursorPosX(14.f);
    for (int i = 0; i < TAB_COUNT; i++) {
        bool active = (g_active_tab == i);
        if (active) ImGui::PushStyleColor(ImGuiCol_Text, pal::accent);
        else        ImGui::PushStyleColor(ImGuiCol_Text, pal::text_lo);

        char btn_id[32];
        snprintf(btn_id, sizeof(btn_id), "%s %s##tab%d",
                 g_tabs[i].icon, g_tabs[i].label, i);

        ImGui::PushStyleColor(ImGuiCol_Button,        { 0,0,0,0 });
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, pal::bg_item);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  pal::bg_item);
        if (ImGui::Button(btn_id, { tab_w, 26.f })) g_active_tab = i;
        ImGui::PopStyleColor(4);
        if (i < TAB_COUNT - 1) ImGui::SameLine(0, 0);
    }

    HSep();

    // ── Tab content ──────────────────────────────────────────────────────────
    ImGui::BeginChild("##tab_content", { 0, 0 }, false, ImGuiWindowFlags_NoScrollbar);

    if (g_active_tab == 0) // ── ESP TAB ──────────────────────────────────────
    {
        SectionHeader("DETECCAO");
        ImGui::Dummy({ 0, 4.f });

        Toggle("Somente inimigos",   &g_cfg.enemy_only);
        Toggle("Bounding box",       &g_cfg.box);
        Toggle("Esqueleto",          &g_cfg.skeleton);
        Toggle("Ponto na cabeca",    &g_cfg.head_dot);
        Toggle("Linha de snap",      &g_cfg.snap);

        HSep();
        SectionHeader("INFO VISUAL");
        ImGui::Dummy({ 0, 4.f });

        Toggle("Barra de vida",      &g_cfg.health_bar);
        Toggle("Barra de escudo",    &g_cfg.armor_bar);
        Toggle("Distancia",          &g_cfg.distance);
        Toggle("Nome do agente",     &g_cfg.name_tag);

        HSep();
        SectionHeader("AJUSTES");
        ImGui::Dummy({ 0, 4.f });

        SlimSlider("Dist. maxima (m)", &g_cfg.max_dist,  10.f, 300.f);
        SlimSlider("Espessura box",    &g_cfg.box_thick,  0.5f,   4.f, "%.1f");
        SlimSlider("Espessura skel.",  &g_cfg.skel_thick, 0.5f,   4.f, "%.1f");
    }
    else if (g_active_tab == 1) // ── COLORS TAB ───────────────────────────────
    {
        SectionHeader("COR DOS INIMIGOS");
        ImGui::Dummy({ 0, 6.f });
        ColorRow("Inimigo",    &g_cfg.col_enemy);
        ImGui::Dummy({ 0, 2.f });
        ColorRow("Aliado",     &g_cfg.col_team);
        ImGui::Dummy({ 0, 2.f });
        ColorRow("HP baixo",   &g_cfg.col_low_hp);

        HSep();
        SectionHeader("PRESETS RAPIDOS");
        ImGui::Dummy({ 0, 4.f });

        auto preset_btn = [](const char* label, ImVec4 col) -> bool {
            ImGui::PushStyleColor(ImGuiCol_Button,
                { col.x*0.4f, col.y*0.4f, col.z*0.4f, 0.9f });
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                { col.x*0.6f, col.y*0.6f, col.z*0.6f, 1.f });
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                { col.x, col.y, col.z, 1.f });
            bool clicked = ImGui::Button(label, { 70.f, 24.f });
            ImGui::PopStyleColor(3);
            return clicked;
        };

        if (preset_btn("Vermelho", { 1.f, 0.27f, 0.33f, 1.f }))
            g_cfg.col_enemy = { 1.f, 0.27f, 0.33f, 1.f };
        ImGui::SameLine();
        if (preset_btn("Branco", { 0.95f, 0.95f, 0.95f, 1.f }))
            g_cfg.col_enemy = { 0.95f, 0.95f, 0.95f, 1.f };
        ImGui::SameLine();
        if (preset_btn("Ciano", { 0.f, 0.9f, 1.f, 1.f }))
            g_cfg.col_enemy = { 0.f, 0.9f, 1.f, 1.f };
        ImGui::SameLine();
        if (preset_btn("Laranja", { 1.f, 0.55f, 0.f, 1.f }))
            g_cfg.col_enemy = { 1.f, 0.55f, 0.f, 1.f };
    }
    else if (g_active_tab == 2) // ── INFO TAB ─────────────────────────────────
    {
        SectionHeader("STATUS");
        ImGui::Dummy({ 0, 6.f });

        int total  = (int)g_frame.entities.size();
        int enemies = 0;
        for (auto& e : g_frame.entities)
            if (e.is_enemy && e.alive) enemies++;

        ImGui::TextColored(pal::text_mid, "Driver");
        ImGui::SameLine(130.f);
        ImGui::TextColored({ 0.2f, 0.9f, 0.4f, 1.f }, "ATIVO");

        ImGui::TextColored(pal::text_mid, "Jogo");
        ImGui::SameLine(130.f);
        ImGui::TextColored({ 0.2f, 0.9f, 0.4f, 1.f }, "ENCONTRADO");

        ImGui::TextColored(pal::text_mid, "Entidades");
        ImGui::SameLine(130.f);
        ImGui::TextColored(pal::text_hi, "%d", total);

        ImGui::TextColored(pal::text_mid, "Inimigos vis.");
        ImGui::SameLine(130.f);
        ImGui::TextColored(pal::accent, "%d", enemies);

        HSep();
        SectionHeader("CONTROLES");
        ImGui::Dummy({ 0, 4.f });

        auto key_row = [](const char* key, const char* desc) {
            ImGui::TextColored(pal::accent, "%-10s", key);
            ImGui::SameLine(80.f);
            ImGui::TextColored(pal::text_mid, "%s", desc);
        };

        key_row("[INSERT]", "Abrir / fechar menu");
        key_row("[END]",    "Fechar ESP");
        key_row("[F1]",     "Toggle enemy only");
        key_row("[F2]",     "Toggle esqueleto");

        HSep();
        SectionHeader("VERSAO");
        ImGui::Dummy({ 0, 2.f });
        ImGui::TextColored(pal::text_lo, "VRD ESP v2.0  |  patch 10.x");
        ImGui::TextColored(pal::text_lo, "Atualizar offsets apos cada patch");
    }

    ImGui::EndChild();
    ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
//  ESP DRAW HELPERS
// ─────────────────────────────────────────────────────────────────────────────
static void DrawText2D(ImDrawList* dl, Vec2 pos, const char* text, ImU32 col)
{
    dl->AddText({ pos.x+1, pos.y+1 }, IM_COL32(0,0,0,180), text);
    dl->AddText({ pos.x,   pos.y   }, col, text);
}

static void DrawBox(ImDrawList* dl, const EntityData& e, ImU32 col)
{
    ImVec2 tl = { e.box_tl.x, e.box_tl.y }, br = { e.box_br.x, e.box_br.y };
    // Drop shadow
    dl->AddRect({ tl.x-1.f, tl.y-1.f }, { br.x+1.f, br.y+1.f },
                IM_COL32(0,0,0,120), 2.f, 0, g_cfg.box_thick + 1.f);
    // Main box
    dl->AddRect(tl, br, col, 2.f, 0, g_cfg.box_thick);
    // Corner accents
    float cs = 6.f; // corner size
    ImU32 cc = IM_COL32(255,255,255,200);
    dl->AddLine(tl, { tl.x+cs, tl.y }, cc, g_cfg.box_thick);
    dl->AddLine(tl, { tl.x, tl.y+cs }, cc, g_cfg.box_thick);
    dl->AddLine({ br.x, tl.y }, { br.x-cs, tl.y }, cc, g_cfg.box_thick);
    dl->AddLine({ br.x, tl.y }, { br.x, tl.y+cs }, cc, g_cfg.box_thick);
    dl->AddLine({ tl.x, br.y }, { tl.x+cs, br.y }, cc, g_cfg.box_thick);
    dl->AddLine({ tl.x, br.y }, { tl.x, br.y-cs }, cc, g_cfg.box_thick);
    dl->AddLine(br, { br.x-cs, br.y }, cc, g_cfg.box_thick);
    dl->AddLine(br, { br.x, br.y-cs }, cc, g_cfg.box_thick);
}

static void DrawHealthBars(ImDrawList* dl, const EntityData& e)
{
    float pad = 3.f, bar_w = 4.f;
    float top = e.box_tl.y, bot = e.box_br.y;
    float height = bot - top;
    float hp_frac = (e.max_health > 0.f) ? e.health / e.max_health : 0.f;

    if (g_cfg.health_bar) {
        float lx = e.box_tl.x - pad - bar_w;
        dl->AddRectFilled({ lx, top }, { lx+bar_w, bot }, IM_COL32(0,0,0,160), 2.f);
        // Color: green → red as hp drops
        float r = 1.f - hp_frac, g2 = hp_frac;
        // Switch to low-hp color below 30%
        ImU32 hp_col = hp_frac < 0.3f
            ? ImGui::ColorConvertFloat4ToU32(g_cfg.col_low_hp)
            : IM_COL32((int)(r*230), (int)(g2*210), 30, 220);
        float fill_top = top + height * (1.f - hp_frac);
        dl->AddRectFilled({ lx, fill_top }, { lx+bar_w, bot }, hp_col, 2.f);
        dl->AddRect({ lx, top }, { lx+bar_w, bot }, IM_COL32(50,50,50,180), 2.f, 0, 0.8f);
    }

    if (g_cfg.armor_bar && e.armor > 0.f) {
        float rx = e.box_br.x + pad;
        float af = std::min(e.armor / 50.f, 1.f);
        float fill_top = top + height * (1.f - af);
        dl->AddRectFilled({ rx, top }, { rx+bar_w, bot }, IM_COL32(0,0,0,160), 2.f);
        dl->AddRectFilled({ rx, fill_top }, { rx+bar_w, bot },
                          IM_COL32(60, 160, 255, 210), 2.f);
        dl->AddRect({ rx, top }, { rx+bar_w, bot }, IM_COL32(50,50,50,180), 2.f, 0, 0.8f);
    }
}

static void DrawSkeleton(ImDrawList* dl, const EntityData& e, ImU32 col)
{
    static const int all_bones[] = {
        offsets::bone::Head,  offsets::bone::Neck,
        offsets::bone::Chest, offsets::bone::Stomach, offsets::bone::Pelvis,
        offsets::bone::LShoulder, offsets::bone::LElbow, offsets::bone::LWrist,
        offsets::bone::RShoulder, offsets::bone::RElbow, offsets::bone::RWrist,
        offsets::bone::LHip, offsets::bone::LKnee, offsets::bone::LAnkle,
        offsets::bone::RHip, offsets::bone::RKnee, offsets::bone::RAnkle,
    };
    for (int p = 0; p < g_bone_pair_count; p++) {
        int ia = -1, ib = -1;
        for (int b = 0; b < 17; b++) {
            if (all_bones[b] == g_bone_pairs[p][0]) ia = b;
            if (all_bones[b] == g_bone_pairs[p][1]) ib = b;
        }
        if (ia < 0 || ib < 0 || !e.bone_valid[ia] || !e.bone_valid[ib]) continue;
        dl->AddLine({ e.bones_screen[ia].x, e.bones_screen[ia].y },
                    { e.bones_screen[ib].x, e.bones_screen[ib].y },
                    col, g_cfg.skel_thick);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  DX11 INIT / WINDOW
// ─────────────────────────────────────────────────────────────────────────────
static bool CreateDX11(HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount        = 2;
    sd.BufferDesc.Format  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.Width   = (UINT)g_sw;
    sd.BufferDesc.Height  = (UINT)g_sh;
    sd.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow       = hwnd;
    sd.SampleDesc.Count   = 1;
    sd.Windowed           = TRUE;
    sd.SwapEffect         = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL feat;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION,
        &sd, &g_sc, &g_dev, &feat, &g_ctx);
    if (FAILED(hr)) return false;

    ID3D11Texture2D* back = nullptr;
    g_sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back);
    g_dev->CreateRenderTargetView(back, nullptr, &g_rtv);
    back->Release();
    return true;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return true;
    if (msg == WM_DESTROY) { g_running = false; PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool Init(float sw, float sh)
{
    g_sw = sw; g_sh = sh;

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"VrdOverlay";
    RegisterClassExW(&wc);

    g_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        L"VrdOverlay", L"",
        WS_POPUP, 0, 0, (int)sw, (int)sh,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!g_hwnd) return false;

    SetLayeredWindowAttributes(g_hwnd, 0, 255, LWA_ALPHA);
    MARGINS m = { -1,-1,-1,-1 };
    DwmExtendFrameIntoClientArea(g_hwnd, &m);

    if (!CreateDX11(g_hwnd)) return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize            = { sw, sh };
    io.ConfigFlags           |= ImGuiConfigFlags_NoMouseCursorChange;
    io.IniFilename            = nullptr; // no imgui.ini

    SetupStyle();
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_dev, g_ctx);

    // When menu is open: make window receive mouse/keyboard
    ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(g_hwnd);
    return true;
}

// Toggle click-through based on menu visibility
static void UpdateInputMode()
{
    LONG ex = GetWindowLongW(g_hwnd, GWL_EXSTYLE);
    if (g_show_menu) {
        // remove transparent (allow input)
        SetWindowLongW(g_hwnd, GWL_EXSTYLE, ex & ~WS_EX_TRANSPARENT);
        SetForegroundWindow(g_hwnd);
    } else {
        // restore click-through
        SetWindowLongW(g_hwnd, GWL_EXSTYLE, ex | WS_EX_TRANSPARENT);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  RENDER FRAME
// ─────────────────────────────────────────────────────────────────────────────
void RenderFrame()
{
    // Toggle menu
    static bool f1_prev = false, f2_prev = false, ins_prev = false;
    bool ins_now = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    bool f1_now  = (GetAsyncKeyState(VK_F1)     & 0x8000) != 0;
    bool f2_now  = (GetAsyncKeyState(VK_F2)     & 0x8000) != 0;

    if (ins_now && !ins_prev) { g_show_menu = !g_show_menu; UpdateInputMode(); }
    if (f1_now  && !f1_prev)  g_cfg.enemy_only = !g_cfg.enemy_only;
    if (f2_now  && !f2_prev)  g_cfg.skeleton   = !g_cfg.skeleton;
    ins_prev = ins_now; f1_prev = f1_now; f2_prev = f2_now;

    float clear[4] = { 0,0,0,0 };
    g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
    g_ctx->ClearRenderTargetView(g_rtv, clear);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // ── Full-screen transparent canvas for ESP drawing ───────────────────────
    ImGui::SetNextWindowPos({ 0,0 });
    ImGui::SetNextWindowSize({ g_sw, g_sh });
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::Begin("##canvas", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoInputs   | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings);

    ImDrawList* dl  = ImGui::GetWindowDrawList();
    Vec2 center     = { g_sw * 0.5f, g_sh * 0.5f };

    for (const auto& ent : g_frame.entities)
    {
        if (!ent.alive)                              continue;
        if (g_cfg.enemy_only && !ent.is_enemy)       continue;
        if (ent.distance > g_cfg.max_dist)           continue;

        ImVec4 cv  = ent.is_enemy ? g_cfg.col_enemy : g_cfg.col_team;
        ImU32  col = ImGui::ColorConvertFloat4ToU32(cv);

        // HP-based color override for enemy
        if (ent.is_enemy && ent.max_health > 0.f && ent.health / ent.max_health < 0.3f)
            col = ImGui::ColorConvertFloat4ToU32(g_cfg.col_low_hp);

        if (g_cfg.box        && ent.box_tl.x > 0.f) DrawBox(dl, ent, col);
        if (g_cfg.health_bar || g_cfg.armor_bar)     DrawHealthBars(dl, ent);
        if (g_cfg.skeleton)                          DrawSkeleton(dl, ent, col);

        // Head dot
        if (g_cfg.head_dot && ent.head_screen.y > 0.f)
            dl->AddCircleFilled({ ent.head_screen.x, ent.head_screen.y },
                                3.f, col);

        // Snap line to foot
        if (g_cfg.snap && ent.foot_screen.y > 0.f)
            dl->AddLine({ center.x, g_sh }, { ent.foot_screen.x, ent.foot_screen.y },
                        col, 1.f);

        // Distance label
        if (g_cfg.distance && ent.head_screen.y > 0.f) {
            char buf[48];
            snprintf(buf, sizeof(buf), "%.0fm  %.0fhp", ent.distance, ent.health);
            DrawText2D(dl, { ent.head_screen.x - 20.f, ent.head_screen.y - 15.f }, buf, col);
        }
    }

    // ── Minimal HUD corner ───────────────────────────────────────────────────
    if (!g_show_menu) {
        int enemies = 0;
        for (auto& e : g_frame.entities)
            if (e.is_enemy && e.alive) enemies++;
        char hud[64];
        snprintf(hud, sizeof(hud), "VRD  |  %d inimigos  |  [INS] menu", enemies);
        dl->AddText({ 10.f, 10.f }, IM_COL32(0,0,0,180), hud);
        dl->AddText({ 9.f,  9.f  }, pal::u32(pal::accent), hud);
    }

    ImGui::End(); // canvas

    // ── Menu ─────────────────────────────────────────────────────────────────
    if (g_show_menu) DrawMenu();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_sc->Present(0, 0);
}

// ─────────────────────────────────────────────────────────────────────────────
//  PUMP / SHUTDOWN
// ─────────────────────────────────────────────────────────────────────────────
bool PumpMessages()
{
    MSG msg = {};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (msg.message == WM_QUIT) return false;
    }
    return g_running;
}

void Shutdown()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
    if (g_sc)  { g_sc->Release();  g_sc  = nullptr; }
    if (g_ctx) { g_ctx->Release(); g_ctx = nullptr; }
    if (g_dev) { g_dev->Release(); g_dev = nullptr; }
    if (g_hwnd){ DestroyWindow(g_hwnd); g_hwnd = nullptr; }
}

} // namespace overlay
