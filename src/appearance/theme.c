#include "theme.h"

#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

static const char browser_styles[] =
    ".ca-titlebar { height: 32px; padding: 0 8px; background: var(--ca-bg-void); border-bottom-width: 0; }"
    ".ca-titlebar-title { font-size: 12px; font-weight: normal; color: var(--ca-text-medium); }"
    ".browser { background: var(--ca-bg-void); }"
    ".dock-lane { width: 100%; height: 56px; padding: 0 8px 8px; justify-content: center;"
    "             align-items: center; flex-shrink: 0; }"
    ".page-overlap .dock-lane { position: absolute; left: 0; bottom: 0; z-index: 2; }"
    ".toolbar { width: 460px; height: 38px; padding: 3px 5px; gap: 4px; align-items: center;"
    "           flex-shrink: 1; background: var(--browser-panel-surface); border-radius: 19px; }"
    ".toolbar-wing { flex-basis: 62px; flex-shrink: 1; align-items: center; }"
    ".toolbar-tail { flex-basis: 34px; justify-content: flex-end; }"
    ".history-controls { height: 30px; gap: 2px; padding: 1px; align-items: center;"
    "                    background: transparent; border-radius: 10px; }"
    ".icon-button { width: 28px; height: 28px; padding: 0; text-align: center;"
    "               font-size: 16px; color: var(--ca-text-medium); background: transparent; border-radius: 7px; }"
    ".icon-button:hover, .icon-button:active { color: var(--ca-text-bright); background: var(--ca-bg-surface); }"
    ".icon-button:disabled { color: var(--ca-text-dim); }"
    ".icon-button:focus, .menu-item:focus, .theme-choice:focus { border: 1px solid var(--ca-accent); }"
    ".menu-toggle { width: 29px; height: 29px; background: var(--ca-bg-elevated); border-radius: 8px; }"
    ".menu-toggle.selected, .menu-toggle.selected:hover {"
    "    color: var(--ca-on-accent); background: var(--ca-accent); }"
    ".address-shell { flex-grow: 1; flex-basis: 0; min-width: 26px; flex-shrink: 1; overflow: hidden;"
    "                 height: 30px; padding: 0 3px 0 7px; align-items: center; gap: 4px;"
    "                 border-radius: 12px; background: var(--ca-bg-surface); }"
    ".address-symbol { font-size: 15px; color: var(--ca-text-muted); flex-shrink: 0; }"
    ".address { height: 30px; flex-grow: 1; flex-basis: 0px; min-width: 0;"
    "           color: var(--ca-text-bright); background: transparent; padding: 0 2px; font-size: 13px; }"
    ".address-shell:focus-within { border: 1px solid var(--ca-accent); }"
    ".workspace, .workspace-main, .page-surface, .page-frame, .page"
    "    { flex-grow: 1; min-width: 0; min-height: 0; align-items: stretch; }"
    ".workspace, .page-surface, .page-frame, .page { width: 100%; }"
    ".workspace { gap: 6px; }"
    ".workspace-main { flex-basis: 0px; padding: 1px; border-radius: 15px;"
    "                  background: var(--browser-page-edge); }"
    ".page-surface { border-radius: 14px; background: var(--ca-bg-base); }"
    ".start { width: 100%; flex-grow: 1; align-items: center; justify-content: center; padding: 18px 16px 54px; }"
    ".start-row { width: 100%; justify-content: center; }"
    ".start-card { width: 380px; flex-shrink: 1; gap: 11px; align-items: center; }"
    ".start-mark { width: 46px; height: 46px; align-items: center; justify-content: center;"
    "              background: var(--ca-bg-surface); border-radius: 14px; }"
    ".start-icon { font-size: 25px; color: var(--ca-accent); }"
    ".start-title { color: var(--ca-text-bright); font-size: 27px; font-weight: bold; }"
    ".start-search { width: 100%; height: 40px; padding: 3px 6px 3px 10px; gap: 6px; align-items: center;"
    "                background: var(--ca-bg-elevated); border: 1px solid var(--ca-separator); border-radius: 12px; }"
    ".start-search:focus-within { border-color: var(--ca-accent); }"
    ".search-go { width: 30px; height: 30px; background: var(--ca-bg-surface); }"
    ".status { flex-shrink: 0; color: var(--ca-warning); background: var(--ca-bg-base); padding: 8px 12px; }"
    ".page-overlap .status { margin-bottom: 56px; }"
    ".unavailable { flex-grow: 1; align-items: center; justify-content: center; }";

static const char menu_styles[] =
    ".app-menu { width: 226px; flex-direction: column; flex-shrink: 0; padding: 12px; gap: 9px; overflow-y: auto;"
    "            background: var(--ca-bg-base); border-radius: 15px; }"
    ".app-menu.compact { width: 100%; }"
    ".page-overlap .app-menu { padding-bottom: 62px; }"
    ".menu-heading { font-size: 12px; font-weight: bold; color: var(--ca-text-muted); padding: 1px 4px; }"
    ".menu-group { width: 100%; align-items: stretch; gap: 3px; flex-shrink: 0; }"
    ".menu-item { height: 34px; color: var(--ca-text-bright); font-size: 13px; text-align: left;"
    "             padding: 0 9px; background: transparent; border-radius: 7px; }"
    ".menu-item:hover, .menu-item:active { background: var(--ca-bg-surface); }"
    ".menu-item:disabled { color: var(--ca-text-dim); }"
    ".menu-segment { width: 100%; height: 34px; padding: 2px; gap: 2px; align-items: center;"
    "                background: var(--ca-bg-elevated); border-radius: 9px; }"
    ".theme-choice { height: 30px; min-width: 0; flex-grow: 1; flex-basis: 0px; text-align: center;"
    "                padding: 0 2px; color: var(--ca-text-medium); background: transparent; border-radius: 7px; font-size: 12px; }"
    ".theme-choice:hover, .theme-choice:active { background: var(--ca-bg-surface); color: var(--ca-text-bright); }"
    ".theme-choice.selected, .theme-choice.selected:hover, .theme-choice.selected:active {"
    "    background: var(--ca-accent); color: var(--ca-on-accent); }"
    ".theme-choice.selected:focus { border-color: var(--ca-on-accent); }"
    ".zoom-value { flex-grow: 1; min-width: 0; font-size: 13px; }";

static const char rail_styles[] =
    ".browser-content { flex-grow: 1; flex-basis: 0; min-width: 0; align-items: stretch; padding: 0 4px 4px 0; }"
    ".tab-pages { flex-grow: 1; width: 100%; min-height: 0; overflow: hidden; }"
    ".tab-rail-slot { width: 46px; flex-direction: column; padding: 7px 3px; flex-shrink: 0; align-items: stretch; }"
    ".tab-rail-slot.expanded { width: 214px; padding: 7px 5px; }"
    ".tab-rail-slot.expanded.full { width: 100%; }"
    ".tab-rail { flex-direction: column; flex-grow: 1; min-height: 0; padding: 4px 2px; gap: 6px; align-items: stretch;"
    "            background: var(--ca-bg-void); }"
    ".tab-rail-head, .tab-rail-footer { height: 32px; gap: 4px; align-items: center; flex-shrink: 0; }"
    ".tab-rail-title { flex-grow: 1; font-size: 13px; font-weight: bold; color: var(--ca-text-bright); padding: 0 4px; }"
    ".rail-button, .tab-tile { width: 32px; height: 32px; padding: 0; text-align: center; font-size: 16px;"
    "                          color: var(--ca-text-medium); background: var(--ca-bg-elevated); border-radius: 8px; }"
    ".rail-button:hover, .tab-tile:hover { background: var(--ca-bg-surface); color: var(--ca-text-bright); }"
    ".rail-button:focus, .tab-tile:focus, .tab-label:focus, .tab-group-label:focus, .group-choice:focus"
    "    { border: 1px solid var(--ca-accent); }"
    ".rail-add, .rail-toggle { width: 100%; height: 32px; }"
    ".rail-toggle { background: transparent; }"
    ".tab-list { flex-grow: 1; min-height: 0; overflow-y: auto; gap: 4px; align-items: stretch; }"
    ".tab-tile-wrap { width: 100%; height: 34px; flex-shrink: 0; align-items: center; }"
    ".tab-tile { width: 100%; height: 34px; flex-shrink: 0; border-radius: 9px; }"
    ".tab-tile.active, .tab-tile.active:hover { background: var(--ca-bg-overlay); color: var(--ca-accent); }"
    ".group-tile { background: var(--ca-bg-surface); }"
    ".group-tile.group-active { color: var(--ca-accent); }"
    ".tab-section { width: 100%; gap: 4px; flex-shrink: 0; }"
    ".tab-group-caption { font-size: 11px; color: var(--ca-text-muted); padding: 5px 5px 3px; }"
    ".tab-group-head, .tab-row { width: 100%; height: 34px; gap: 2px; align-items: center; flex-shrink: 0;"
    "                            overflow: hidden; border-radius: 8px; }"
    ".tab-group-head { background: var(--ca-bg-elevated); }"
    ".tab-row.active { background: var(--ca-bg-surface); border-left: 2px solid var(--ca-accent); }"
    ".tab-label, .tab-group-label, .group-choice { height: 32px; min-width: 0; padding: 0 6px;"
    "    text-align: left; font-size: 12px; color: var(--ca-text-medium); background: transparent; }"
    ".tab-label, .tab-group-label { flex-grow: 1; flex-basis: 0; overflow: hidden; }"
    ".tab-group-label { font-weight: bold; }"
    ".tab-label:hover, .tab-group-label:hover, .group-choice:hover { background: var(--ca-bg-overlay); }"
    ".active .tab-label { color: var(--ca-text-bright); }"
    ".tab-close { width: 25px; height: 25px; flex-shrink: 0; font-size: 14px; background: transparent; }"
    ".tab-group-editor { height: 34px; gap: 4px; flex-shrink: 0; align-items: center; }"
    ".group-create { width: 52px; font-size: 12px; }"
    ".group-choice { width: 100%; }"
    ".group-name { height: 32px; flex-grow: 1; flex-basis: 0; min-width: 0; padding: 0 6px; font-size: 12px;"
    "              color: var(--ca-text-bright); background: var(--ca-bg-surface); border-radius: 7px; }";

static const uint32_t canvas_rgb[BROWSER_THEME_COUNT] = {
    [BROWSER_THEME_DARK] = 0x171b22,
    [BROWSER_THEME_LIGHT] = 0xf0f3f8,
    [BROWSER_THEME_DUSK] = 0x211b28,
};

static const uint32_t panel_color[BROWSER_THEME_COUNT] = {
    [BROWSER_THEME_DARK] = 0x222832e8,
    [BROWSER_THEME_LIGHT] = 0xffffffe8,
    [BROWSER_THEME_DUSK] = 0x302638e8,
};

static const uint32_t page_edge_color[BROWSER_THEME_COUNT] = {
    [BROWSER_THEME_DARK] = 0x303947ff,
    [BROWSER_THEME_LIGHT] = 0xd7dfeaff,
    [BROWSER_THEME_DUSK] = 0x493c55ff,
};

static const struct {
    const char *variable;
    uint32_t rgb[BROWSER_THEME_COUNT];
} palette[] = {
    /* CSS color role          Dark       Light      Dusk */
    { "--ca-bg-base",       { 0x191e26,  0xf7f9fc,  0x282030 } },
    { "--ca-bg-elevated",   { 0x222832,  0xffffff,  0x302638 } },
    { "--ca-bg-surface",    { 0x2c3441,  0xe8edf5,  0x3d3048 } },
    { "--ca-bg-overlay",    { 0x3e4a5c,  0xd7e2f0,  0x51405f } },
    { "--ca-separator",     { 0x354050,  0xd2dbe8,  0x4b3c56 } },
    { "--ca-text-bright",   { 0xeaf0f8,  0x172130,  0xf4eaf8 } },
    { "--ca-text-medium",   { 0xc6d0df,  0x38465a,  0xdfcce8 } },
    { "--ca-text-muted",    { 0xa1afc2,  0x55647a,  0xbda8ca } },
    { "--ca-text-dim",      { 0x7f8da1,  0x67768a,  0x9e8aac } },
    { "--ca-accent",        { 0xa3c8ff,  0x2458a8,  0xdfb8ff } },
    { "--ca-on-accent",     { 0x12233b,  0xffffff,  0x301344 } },
    { "--ca-success",       { 0x92d6ad,  0x176348,  0x9dd6b2 } },
    { "--ca-warning",       { 0xf2c18c,  0x8a4700,  0xf7c694 } },
    { "--ca-danger",        { 0xad3c4e,  0xaf3042,  0xa74065 } },
    { "--ca-on-danger",     { 0xffffff,  0xffffff,  0xffffff } },
};

Ca_Instance *browser_create_ui_instance(const char *app_name)
{
    Ca_InstanceDesc desc = {.app_name = app_name};
#if defined(__APPLE__)
    desc.font_path = "/System/Library/Fonts/Supplemental/Arial.ttf";
    desc.bold_font_path = "/System/Library/Fonts/Supplemental/Arial Bold.ttf";
#elif defined(_WIN32)
    char directory[MAX_PATH];
    char regular[MAX_PATH + 24], bold[MAX_PATH + 24];
    UINT length = GetWindowsDirectoryA(directory, sizeof(directory));
    if (length && length < sizeof(directory)) {
        snprintf(regular, sizeof(regular), "%s\\Fonts\\segoeui.ttf", directory);
        snprintf(bold, sizeof(bold), "%s\\Fonts\\segoeuib.ttf", directory);
        desc.font_path = regular;
        desc.bold_font_path = bold;
    }
#elif defined(__linux__)
    static const char *const fonts[][2] = {
        {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
         "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"},
        {"/usr/share/fonts/TTF/DejaVuSans.ttf",
         "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf"},
        {"/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf",
         "/usr/share/fonts/liberation-sans/LiberationSans-Bold.ttf"},
    };
    for (size_t i = 0; i < sizeof(fonts) / sizeof(fonts[0]); ++i) {
        if (access(fonts[i][0], R_OK) == 0 && access(fonts[i][1], R_OK) == 0) {
            desc.font_path = fonts[i][0];
            desc.bold_font_path = fonts[i][1];
            break;
        }
    }
#endif
    return ca_instance_create(&desc);
}

const char *browser_theme_name(BrowserTheme theme)
{
    static const char *const names[BROWSER_THEME_COUNT] = {
        [BROWSER_THEME_DARK] = "Dark",
        [BROWSER_THEME_LIGHT] = "Light",
        [BROWSER_THEME_DUSK] = "Dusk",
    };
    return names[theme];
}

Ca_Stylesheet *browser_theme_create(BrowserTheme theme, uint32_t *canvas_color)
{
    char css[sizeof(browser_styles) + sizeof(menu_styles) + sizeof(rail_styles) - 2];
    memcpy(css, browser_styles, sizeof(browser_styles) - 1);
    memcpy(css + sizeof(browser_styles) - 1, menu_styles, sizeof(menu_styles) - 1);
    memcpy(css + sizeof(browser_styles) + sizeof(menu_styles) - 2, rail_styles, sizeof(rail_styles));
    Ca_Stylesheet *stylesheet = ca_css_parse(css);
    if (!stylesheet)
        return NULL;

    /* Causality stores colors as RRGGBBAA. These palette entries are opaque. */
    uint32_t canvas = (canvas_rgb[theme] << 8) | 0xffu;
    if (!ca_css_set_color_var(stylesheet, "--ca-bg-void", canvas) ||
        !ca_css_set_color_var(stylesheet, "--browser-panel-surface", panel_color[theme]) ||
        !ca_css_set_color_var(stylesheet, "--browser-page-edge", page_edge_color[theme])) {
        ca_css_destroy(stylesheet);
        return NULL;
    }
    for (size_t i = 0; i < sizeof(palette) / sizeof(palette[0]); ++i) {
        uint32_t color = (palette[i].rgb[theme] << 8) | 0xffu;
        if (!ca_css_set_color_var(stylesheet, palette[i].variable, color)) {
            ca_css_destroy(stylesheet);
            return NULL;
        }
    }
    *canvas_color = canvas;
    return stylesheet;
}
