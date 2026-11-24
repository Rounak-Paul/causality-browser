#include "window.h"
#include "address.h"
#include "tabs.h"
#include "../appearance/theme.h"
#include "../chrome/page_overlay.h"

#include <ca_icons.h>
#include <ca_webview.h>
#include <causality_config.h>

#ifdef __APPLE__
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BROWSER_NAME "Causality Browser"
#define SEARCH_PLACEHOLDER "Search or enter a website"

static const int zoom_levels[] = {50, 67, 80, 90, 100, 110, 125, 150, 175, 200, 250, 300};

/** Owns one window's retained chrome, theme, and WebView state. */
typedef struct BrowserWindow {
    Ca_Window *window;
    BrowserPageOverlay *overlay;
    Ca_Stylesheet *stylesheet;
    BrowserTabs tabs;
    Ca_Div *content;
    Ca_Div *dock_panel;
    Ca_Div *main;
    Ca_Div *page_surface;
    Ca_Div *start;
    Ca_Div *menu;
    Ca_TextInput *address;
    Ca_TextInput *start_search;
    Ca_Button *back;
    Ca_Button *forward;
    Ca_Button *address_action;
    Ca_Tooltip *address_tooltip;
    Ca_Button *menu_toggle;
    Ca_Button *copy_address;
    Ca_Button *zoom_out;
    Ca_Button *zoom_value;
    Ca_Button *zoom_in;
    Ca_Button *theme_choices[BROWSER_THEME_COUNT];
    Ca_Label *status;
    BrowserTheme theme;
    BrowserTheme requested_theme;
    size_t zoom;
    bool menu_open;
    bool compact_menu;
    bool showing_start;
    bool address_edited;
    bool loading;
    bool content_hidden;
    bool page_overlap;
} BrowserWindow;

/** Returns the classes for a theme choice in its current selection state. */
static const char *theme_choice_style(BrowserTheme choice, BrowserTheme active)
{
    return choice == active ? "theme-choice selected" : "theme-choice";
}

/** Applies a requested palette to the retained UI and saves the selection. */
static void apply_theme(BrowserWindow *browser)
{
    BrowserTheme theme = browser->requested_theme;
    uint32_t canvas;
    Ca_Stylesheet *stylesheet = browser_theme_create(theme, &canvas);
    if (!stylesheet) {
        browser->requested_theme = browser->theme;
        fputs("Could not change the browser theme.\n", stderr);
        return;
    }

    ca_instance_set_stylesheet(ca_window_instance(browser->window), stylesheet);
    ca_window_set_canvas_color(browser->window, canvas);
    ca_css_destroy(browser->stylesheet);
    browser->stylesheet = stylesheet;
    browser->theme = theme;
    for (BrowserTheme choice = 0; choice < BROWSER_THEME_COUNT; ++choice) {
        Ca_Button *button = browser->theme_choices[choice];
        if (button)
            ca_set_style(button, theme_choice_style(choice, theme));
    }
    if (!browser_theme_save(theme))
        fputs("Could not save the browser theme preference.\n", stderr);
}

/** Shows a load or address error above the browsing area. */
static void show_status(BrowserWindow *browser, const char *message)
{
    if (browser->status) {
        ca_set_text(browser->status, message);
        ca_set_hidden(browser->status, !*message);
    }
}

/** Tests a main-frame address before navigation or copying it from the page. */
static bool allow_navigation(Ca_WebView *page, const char *url, void *data)
{
    (void)page;
    (void)data;
    if (!url)
        return false;
    if (strcmp(url, BROWSER_START_URL) == 0)
        return true;

    char resolved[CA_INPUT_TEXT_MAX];
    return browser_address_resolve(url, resolved, sizeof(resolved)) &&
           strcmp(resolved, url) == 0;
}

/** Returns the active tab's native page, if one is available. */
static Ca_WebView *current_page(const BrowserWindow *browser)
{
    return browser->tabs.active ? browser->tabs.active->view : NULL;
}

/** Sets the address field's trailing action for the current editing and load state. */
static void update_address_action(BrowserWindow *browser)
{
    if (!browser->address_action)
        return;

    bool go = browser->address_edited || (browser->showing_start && !browser->loading);
    const char *icon = go ? CA_ICON_NF_COD_ARROW_RIGHT
        : browser->loading ? CA_ICON_NF_COD_CLOSE : CA_ICON_NF_COD_REFRESH;
    const char *hint = go ? "Go" : browser->loading ? "Stop loading" : "Reload page";
    ca_set_text(browser->address_action, icon);
    if (browser->address_tooltip)
        ca_tooltip_set_text(browser->address_tooltip, hint);
}

/** Swaps the start surface and page frame while keeping the native view alive. */
static void show_start(BrowserWindow *browser, bool show)
{
    browser->showing_start = show;
    if (browser->start)
        ca_set_hidden(browser->start, !show);
    browser_tabs_show_start(&browser->tabs, show);
    update_address_action(browser);
}

/** Keeps the menu and native WebView in separate layout regions. */
static void update_menu_layout(BrowserWindow *browser)
{
    int width = 0;
    float rail_width = 0;
    if (browser->menu_open) {
        ca_window_get_rect(browser->window, NULL, NULL, &width, NULL);
        ca_div_screen_rect(browser->tabs.rail_slot, NULL, NULL, &rail_width, NULL);
    }
    bool compact = browser->menu_open && (float)width - rail_width < 566.0f;
    if (compact == browser->compact_menu)
        return;

    browser->compact_menu = compact;
    if (browser->main)
        ca_set_hidden(browser->main, compact);
    ca_set_style(browser->menu, compact ? "app-menu compact" : "app-menu");
}

/** Shows or closes the browser menu and updates its toolbar button. */
static void set_menu_open(BrowserWindow *browser, bool open)
{
    if (!browser->menu || browser->menu_open == open)
        return;
    browser->menu_open = open;
    ca_set_hidden(browser->menu, !open);
    ca_set_style(browser->menu_toggle, open ? "icon-button menu-toggle selected" : "icon-button menu-toggle");
    if (!open && browser->copy_address)
        ca_set_text(browser->copy_address, CA_ICON_NF_COD_LINK "   Copy page address");
    update_menu_layout(browser);
}

/** Sets page commands that depend on a loaded document. */
static void update_page_commands(BrowserWindow *browser, const char *url)
{
    bool has_address = *url && strcmp(url, BROWSER_START_URL) != 0;
    if (browser->copy_address) {
        ca_set_disabled(browser->copy_address, !has_address);
        ca_set_text(browser->copy_address, CA_ICON_NF_COD_LINK "   Copy page address");
    }
}

/** Projects native browser events into the chrome without rebuilding the page. */
static void on_page_event(const Ca_WebViewEvent *event, void *data)
{
    BrowserWindow *browser = data;
    if (browser->loading != event->loading) {
        browser->loading = event->loading;
        update_address_action(browser);
    }

    switch (event->type) {
    case CA_WEBVIEW_EVENT_URL_CHANGED: {
        bool at_start = strcmp(event->url, BROWSER_START_URL) == 0;
        if (*event->url)
            show_start(browser, at_start);
        if (browser->address && !ca_input_is_focused(browser->address)) {
            ca_set_text(browser->address, at_start ? "" : event->url);
            browser->address_edited = false;
            update_address_action(browser);
        }
        update_page_commands(browser, event->url);
        break;
    }
    case CA_WEBVIEW_EVENT_TITLE_CHANGED:
        ca_window_set_title(browser->window, *event->title ? event->title : BROWSER_NAME);
        break;
    case CA_WEBVIEW_EVENT_HISTORY_CHANGED:
        if (browser->back) ca_set_disabled(browser->back, !event->can_go_back);
        if (browser->forward) ca_set_disabled(browser->forward, !event->can_go_forward);
        break;
    case CA_WEBVIEW_EVENT_LOAD_STARTED:
        show_status(browser, "");
        break;
    case CA_WEBVIEW_EVENT_LOAD_FINISHED:
        show_status(browser, "");
        break;
    case CA_WEBVIEW_EVENT_LOAD_FAILED:
        show_status(browser, *event->detail ? event->detail : "The page could not load.");
        break;
    case CA_WEBVIEW_EVENT_PROCESS_TERMINATED:
        show_status(browser, "The page process exited. Reload to try again.");
        break;
    default:
        break;
    }
}

/** Forwards tab-creation failures to the window's status line. */
static void on_tab_error(const char *message, void *data)
{
    show_status(data, message);
}

/** Opens the address or search query from the given input. */
static void submit_address(BrowserWindow *browser, const char *text)
{
    if (!current_page(browser) || !text || !*text)
        return;

    char url[CA_INPUT_TEXT_MAX];
    if (!browser_address_or_search(text, url, sizeof(url)) ||
        !ca_webview_load_url(current_page(browser), url)) {
        show_status(browser, "Enter a web address or a shorter search query.");
        return;
    }

    ca_set_text(browser->address, url);
    if (browser->start_search)
        ca_set_text(browser->start_search, "");
    browser->address_edited = false;
    set_menu_open(browser, false);
    show_start(browser, false);
    ca_window_clear_focus(browser->window);
    ca_webview_focus(current_page(browser));
}

/** Focuses the Causality address field and selects its contents. */
static void focus_address(BrowserWindow *browser)
{
    if (current_page(browser) && browser->address) {
        browser_tabs_reveal_page(&browser->tabs);
        ca_webview_blur(current_page(browser));
        ca_input_focus(browser->address);
    }
}

/** Syncs the rounded native viewport and dock cutout with the Causality layout. */
static void update_chrome(BrowserWindow *browser)
{
    if (!browser->overlay)
        return;

    BrowserSurfaceRect page = {.radius = 14.0f};
    BrowserSurfaceRect dock = {0};
    if (!browser->content_hidden && !browser->compact_menu && !browser->showing_start &&
        current_page(browser)) {
        ca_div_screen_rect(browser->page_surface, &page.x, &page.y, &page.width, &page.height);
    }
    if (browser->page_overlap && !browser->content_hidden) {
        ca_div_screen_rect(browser->dock_panel, &dock.x, &dock.y, &dock.width, &dock.height);
        dock.radius = dock.height * 0.5f;
    }
    browser_page_overlay_update(browser->overlay, page.width > 0.0f && page.height > 0.0f ? &page : NULL,
                                dock.width > 0.0f && dock.height > 0.0f ? &dock : NULL);
}

/** Applies pending changes and submits the focused search field on Enter. */
static void on_frame(void *data)
{
    BrowserWindow *browser = data;
    browser_tabs_tick(&browser->tabs);
    if (browser->content_hidden != browser->tabs.full_width) {
        browser->content_hidden = browser->tabs.full_width;
        ca_set_hidden(browser->content, browser->content_hidden);
    }
    if (browser->requested_theme != browser->theme)
        apply_theme(browser);
    if (browser->menu_open)
        update_menu_layout(browser);

    Ca_TextInput *input = ca_input_is_focused(browser->address) ? browser->address
        : ca_input_is_focused(browser->start_search) ? browser->start_search : NULL;
    if (input && (ca_input_key_pressed(input, CA_KEY_ENTER) ||
                  ca_input_key_pressed(input, CA_KEY_KP_ENTER))) {
        submit_address(browser, ca_input_text(input));
    }
    update_chrome(browser);
}

/** Handles browser shortcuts received by the Causality window. */
static void on_key(const Ca_Event *event, void *data)
{
    BrowserWindow *browser = data;
    if (event->window != browser->window || event->key.action != CA_PRESS)
        return;

    if (event->key.key == CA_KEY_ESCAPE && browser->menu_open) {
        set_menu_open(browser, false);
        return;
    }
#ifdef __APPLE__
    const int shortcut = CA_MOD_SUPER;
#else
    const int shortcut = CA_MOD_CONTROL;
#endif
    if ((event->key.mods & (CA_MOD_CONTROL | CA_MOD_SUPER | CA_MOD_ALT | CA_MOD_SHIFT)) != shortcut)
        return;

    if (event->key.key == CA_KEY_T) {
        browser_tabs_new(&browser->tabs);
    } else if (event->key.key == CA_KEY_W) {
        browser_tabs_close_active(&browser->tabs);
    } else if (event->key.key == CA_KEY_L) {
        set_menu_open(browser, false);
        focus_address(browser);
    } else if (event->key.key == CA_KEY_R && current_page(browser) && !browser->showing_start) {
        ca_webview_reload(current_page(browser));
    }
}

/** Switches the trailing address action to Go when text is edited. */
static void on_address_edit(Ca_TextInput *input, void *data)
{
    (void)input;
    BrowserWindow *browser = data;
    browser->address_edited = true;
    update_address_action(browser);
}

/** Traverses to the preceding page in this WebView's history. */
static void on_back(Ca_Button *button, void *data)
{
    (void)button;
    BrowserWindow *browser = data;
    ca_webview_go_back(current_page(browser));
}

/** Traverses to the next page in this WebView's history. */
static void on_forward(Ca_Button *button, void *data)
{
    (void)button;
    BrowserWindow *browser = data;
    ca_webview_go_forward(current_page(browser));
}

/** Submits an edited address, stops a load, or reloads the current page. */
static void on_address_action(Ca_Button *button, void *data)
{
    (void)button;
    BrowserWindow *browser = data;
    if (browser->address_edited || (browser->showing_start && !browser->loading)) {
        if (*ca_input_text(browser->address))
            submit_address(browser, ca_input_text(browser->address));
        else
            focus_address(browser);
    } else if (browser->loading) {
        ca_webview_stop(current_page(browser));
        browser->loading = false;
        browser->tabs.active->loading = false;
        update_address_action(browser);
    } else {
        ca_webview_reload(current_page(browser));
    }
}

/** Submits the native Start Page's search field. */
static void on_start_search(Ca_Button *button, void *data)
{
    (void)button;
    BrowserWindow *browser = data;
    submit_address(browser, ca_input_text(browser->start_search));
}

/** Toggles the browser menu from its toolbar button. */
static void on_menu_toggle(Ca_Button *button, void *data)
{
    (void)button;
    BrowserWindow *browser = data;
    set_menu_open(browser, !browser->menu_open);
}

/** Opens the Start Page through WebView history, preserving Back and Forward. */
static void on_home(Ca_Button *button, void *data)
{
    (void)button;
    BrowserWindow *browser = data;
    set_menu_open(browser, false);
    if (!browser->showing_start && !ca_webview_load_url(current_page(browser), BROWSER_START_URL))
        show_status(browser, "The Start Page could not open.");
}

/** Copies the current page address and confirms the action in the menu. */
static void on_copy_address(Ca_Button *button, void *data)
{
    BrowserWindow *browser = data;
    const char *url = ca_webview_url(current_page(browser));
    if (*url && strcmp(url, BROWSER_START_URL) != 0) {
        ca_clipboard_set_text(browser->window, url);
        ca_set_text(button, CA_ICON_NF_COD_CHECK "   Address copied");
    }
}

/** Applies a page zoom level and updates its menu controls. */
static void set_zoom(BrowserWindow *browser, size_t level)
{
    const size_t count = sizeof(zoom_levels) / sizeof(zoom_levels[0]);
    if (level >= count)
        return;
    browser->zoom = level;
    if (browser->tabs.active)
        browser->tabs.active->zoom = level;
    ca_webview_set_zoom(current_page(browser), zoom_levels[level] / 100.0);

    char label[16];
    snprintf(label, sizeof(label), "%d%%", zoom_levels[level]);
    if (browser->zoom_value) ca_set_text(browser->zoom_value, label);
    if (browser->zoom_out) ca_set_disabled(browser->zoom_out, level == 0);
    if (browser->zoom_in) ca_set_disabled(browser->zoom_in, level == count - 1);
}

/** Moves down one zoom level, resets to 100 percent, or moves up one level. */
static void on_zoom(Ca_Button *button, void *data)
{
    BrowserWindow *browser = data;
    if (button == browser->zoom_value)
        set_zoom(browser, BROWSER_DEFAULT_ZOOM);
    else if (button == browser->zoom_in)
        set_zoom(browser, browser->zoom + 1);
    else if (browser->zoom > 0)
        set_zoom(browser, browser->zoom - 1);
}

/** Projects the selected tab's retained navigation state into the browser chrome. */
static void on_tab_select(void *data)
{
    BrowserWindow *browser = data;
    browser_page_overlay_sync_pages(browser->overlay);
    BrowserTab *tab = browser->tabs.active;
    const char *url = ca_webview_url(tab->view);
    const char *title = ca_webview_title(tab->view);

    set_menu_open(browser, false);
    ca_window_clear_focus(browser->window);
    browser->address_edited = false;
    browser->loading = tab->loading;
    ca_set_text(browser->address, tab->start ? "" : url);
    if (browser->start_search)
        ca_set_text(browser->start_search, "");
    ca_window_set_title(browser->window, *title ? title : BROWSER_NAME);
    if (browser->back) ca_set_disabled(browser->back, !ca_webview_can_go_back(tab->view));
    if (browser->forward) ca_set_disabled(browser->forward, !ca_webview_can_go_forward(tab->view));
    show_status(browser, tab->error ? tab->error : "");
    show_start(browser, tab->start);
    update_page_commands(browser, url);
    set_zoom(browser, tab->zoom);
    if (tab->start)
        focus_address(browser);
}

/** Routes browser shortcuts intercepted by the native window. */
static bool on_overlay_action(BrowserPageOverlayAction action, void *data)
{
    BrowserWindow *browser = data;
    switch (action) {
    case BROWSER_OVERLAY_FOCUS:
        set_menu_open(browser, false);
        focus_address(browser);
        break;
    case BROWSER_OVERLAY_BACK:
        on_back(NULL, browser);
        break;
    case BROWSER_OVERLAY_FORWARD:
        on_forward(NULL, browser);
        break;
    case BROWSER_OVERLAY_RELOAD:
        if (current_page(browser) && !browser->showing_start)
            ca_webview_reload(current_page(browser));
        break;
    case BROWSER_OVERLAY_ESCAPE:
        if (browser->menu_open) {
            set_menu_open(browser, false);
        } else if (ca_input_is_focused(browser->address)) {
            const char *url = browser->showing_start ? "" : ca_webview_url(current_page(browser));
            ca_set_text(browser->address, url);
            browser->address_edited = false;
            update_address_action(browser);
            ca_window_clear_focus(browser->window);
        } else if (browser->tabs.full_width) {
            browser_tabs_reveal_page(&browser->tabs);
        } else {
            return false;
        }
        break;
    case BROWSER_OVERLAY_NEW_TAB:
        browser_tabs_new(&browser->tabs);
        break;
    case BROWSER_OVERLAY_CLOSE_TAB:
        browser_tabs_close_active(&browser->tabs);
        break;
    }
    ca_instance_wake();
    return true;
}

/** Queues the palette associated with a menu selection. */
static void on_theme_choice(Ca_Button *button, void *data)
{
    BrowserWindow *browser = data;
    for (BrowserTheme theme = 0; theme < BROWSER_THEME_COUNT; ++theme) {
        if (button == browser->theme_choices[theme]) {
            browser->requested_theme = theme;
            return;
        }
    }
}

/** Adds a Causality button and attaches an optional tooltip. */
static Ca_Button *build_button(BrowserWindow *browser, const char *text, const char *style,
                               const char *hint, Ca_ClickFn on_click, bool disabled)
{
    Ca_Button *button = ca_btn_begin(&(Ca_BtnDesc){
        .text = text,
        .style = style,
        .disabled = disabled,
        .on_click = on_click,
        .click_data = browser,
    });
    ca_btn_end();
    if (button && hint)
        ca_tooltip_for_widget(button, &(Ca_TooltipDesc){.text = hint});
    return button;
}

/** Builds the centered address field and compact navigation and menu controls. */
static void build_toolbar(BrowserWindow *browser, const char *initial_url, bool supported)
{
    ca_div_begin(&(Ca_DivDesc){
        .direction = CA_HORIZONTAL, .style = "dock-lane", .no_hover = browser->page_overlap,
    });
    browser->dock_panel = ca_div_begin(&(Ca_DivDesc){.direction = CA_HORIZONTAL, .style = "toolbar"});
    ca_div_begin(&(Ca_DivDesc){.direction = CA_HORIZONTAL, .style = "toolbar-wing"});
    ca_div_begin(&(Ca_DivDesc){.direction = CA_HORIZONTAL, .style = "history-controls"});
    browser->back = build_button(browser, CA_ICON_NF_COD_CHEVRON_LEFT, "icon-button", "Go back", on_back, true);
    browser->forward = build_button(browser, CA_ICON_NF_COD_CHEVRON_RIGHT, "icon-button", "Go forward", on_forward, true);
    ca_div_end();
    ca_div_end();

    ca_div_begin(&(Ca_DivDesc){.direction = CA_HORIZONTAL, .style = "address-shell"});
    ca_text(&(Ca_TextDesc){.text = CA_ICON_NF_COD_SEARCH, .style = "address-symbol"});
    browser->address = ca_input(&(Ca_InputDesc){
        .text = initial_url,
        .placeholder = SEARCH_PLACEHOLDER,
        .style = "address",
        .on_change = on_address_edit,
        .change_data = browser,
        .disabled = !supported,
    });
    browser->address_action = build_button(browser, CA_ICON_NF_COD_REFRESH, "icon-button",
                                           NULL, on_address_action, !supported);
    if (browser->address_action) {
        browser->address_tooltip = ca_tooltip_for_widget(browser->address_action,
                                                       &(Ca_TooltipDesc){.text = "Reload page"});
    }
    ca_div_end();

    ca_div_begin(&(Ca_DivDesc){.direction = CA_HORIZONTAL, .style = "toolbar-wing toolbar-tail"});
    browser->menu_toggle = build_button(browser, CA_ICON_NF_COD_ELLIPSIS, "icon-button menu-toggle",
                                        "Browser menu", on_menu_toggle, false);
    ca_div_end();
    ca_div_end();
    ca_div_end();
}

/** Builds the native Start Page and its search entry. */
static void build_start_page(BrowserWindow *browser)
{
    browser->start = ca_div_begin(&(Ca_DivDesc){
        .direction = CA_VERTICAL, .style = "start", .hidden = !browser->showing_start,
    });
    ca_div_begin(&(Ca_DivDesc){.direction = CA_HORIZONTAL, .style = "start-row"});
    ca_div_begin(&(Ca_DivDesc){.direction = CA_VERTICAL, .style = "start-card"});
    ca_div_begin(&(Ca_DivDesc){.direction = CA_VERTICAL, .style = "start-mark"});
    ca_text(&(Ca_TextDesc){.text = CA_ICON_NF_COD_COMPASS, .style = "start-icon"});
    ca_div_end();
    ca_h1(&(Ca_TextDesc){.text = "Causality", .style = "start-title"});
    ca_div_begin(&(Ca_DivDesc){.direction = CA_HORIZONTAL, .style = "start-search"});
    ca_text(&(Ca_TextDesc){.text = CA_ICON_NF_COD_SEARCH, .style = "address-symbol"});
    browser->start_search = ca_input(&(Ca_InputDesc){.placeholder = SEARCH_PLACEHOLDER, .style = "address"});
    build_button(browser, CA_ICON_NF_COD_ARROW_RIGHT, "icon-button search-go", "Go", on_start_search, false);
    ca_div_end();
    ca_div_end();
    ca_div_end();
    ca_div_end();
}

/** Builds the browser command menu beside the browsing area. */
static void build_menu(BrowserWindow *browser, bool supported)
{
    browser->menu = ca_div_begin(&(Ca_DivDesc){.direction = CA_VERTICAL, .style = "app-menu", .hidden = true});
    if (supported) {
        ca_div_begin(&(Ca_DivDesc){.direction = CA_VERTICAL, .style = "menu-group"});
        ca_text(&(Ca_TextDesc){.text = "Page", .style = "menu-heading"});
        build_button(browser, CA_ICON_NF_COD_HOME "   Start Page", "menu-item", NULL, on_home, false);
        browser->copy_address = build_button(browser, CA_ICON_NF_COD_LINK "   Copy page address",
                                              "menu-item", NULL, on_copy_address, true);
        ca_div_end();

        ca_div_begin(&(Ca_DivDesc){.direction = CA_VERTICAL, .style = "menu-group"});
        ca_text(&(Ca_TextDesc){.text = "Page zoom", .style = "menu-heading"});
        ca_div_begin(&(Ca_DivDesc){.direction = CA_HORIZONTAL, .style = "menu-segment"});
        browser->zoom_out = build_button(browser, CA_ICON_NF_COD_REMOVE, "icon-button", "Zoom out", on_zoom, false);
        browser->zoom_value = build_button(browser, "100%", "icon-button zoom-value", "Reset zoom", on_zoom, false);
        browser->zoom_in = build_button(browser, CA_ICON_NF_COD_ADD, "icon-button", "Zoom in", on_zoom, false);
        ca_div_end();
        ca_div_end();
    }

    ca_div_begin(&(Ca_DivDesc){.direction = CA_VERTICAL, .style = "menu-group"});
    ca_text(&(Ca_TextDesc){.text = "Appearance", .style = "menu-heading"});
    ca_div_begin(&(Ca_DivDesc){.direction = CA_HORIZONTAL, .style = "menu-segment"});
    for (BrowserTheme theme = 0; theme < BROWSER_THEME_COUNT; ++theme) {
        browser->theme_choices[theme] = build_button(browser, browser_theme_name(theme),
                                                    theme_choice_style(theme, browser->theme),
                                                    NULL, on_theme_choice, false);
    }
    ca_div_end();
    ca_div_end();
    ca_div_end();
}

/** Creates the rail, shared chrome, and initial browsing surface. */
static void build_window(BrowserWindow *browser, const char *initial_url)
{
    bool supported = ca_webview_supported();
    browser->tabs = (BrowserTabs){
        .window = browser->window,
        .supported = supported,
        .on_select = on_tab_select,
        .on_event = on_page_event,
        .on_error = on_tab_error,
        .allow_navigation = allow_navigation,
        .data = browser,
    };
    browser->showing_start = !initial_url;
    browser->page_overlap = browser_page_overlay_floats_dock(browser->overlay);
    ca_ui_begin(browser->window, &(Ca_DivDesc){
        .direction = CA_HORIZONTAL,
        .style = browser->page_overlap ? "browser page-overlap" : "browser",
    });
    browser_tabs_build_rail(&browser->tabs);
    browser->content = ca_div_begin(&(Ca_DivDesc){.direction = CA_VERTICAL, .style = "browser-content"});
    Ca_Div *workspace = ca_div_begin(&(Ca_DivDesc){.direction = CA_HORIZONTAL, .style = "workspace"});
    ca_div_end();
    browser->status = ca_text(&(Ca_TextDesc){.style = "status", .hidden = true, .wrap = true});
    build_toolbar(browser, initial_url, supported);

    ca_reconcile_begin(workspace);
    browser->main = ca_div_begin(&(Ca_DivDesc){.direction = CA_VERTICAL, .style = "workspace-main"});
    browser->page_surface = ca_div_begin(&(Ca_DivDesc){
        .direction = CA_VERTICAL, .style = "page-surface", .clip_content = true,
    });
    if (supported && browser->address)
        build_start_page(browser);
    browser->tabs.supported = supported && browser->address != NULL;
    browser_tabs_build_pages(&browser->tabs, initial_url);

    if (!current_page(browser)) {
        if (browser->address) ca_set_disabled(browser->address, true);
        if (browser->address_action) ca_set_disabled(browser->address_action, true);
        if (browser->start) ca_set_hidden(browser->start, true);
        ca_set_hidden(browser->tabs.pages, true);
        ca_div_begin(&(Ca_DivDesc){.style = "unavailable"});
        ca_text(&(Ca_TextDesc){
            .text = supported ? "The native WebView could not start."
                              : "Causality WebView is unavailable on this platform.",
            .style = "menu-heading",
            .wrap = true,
        });
        ca_div_end();
    }
    ca_div_end();
    ca_div_end();
    build_menu(browser, current_page(browser) != NULL);
    ca_div_end();
    ca_div_end();
    ca_ui_end();

    if (current_page(browser))
        update_page_commands(browser, ca_webview_url(current_page(browser)));
    update_address_action(browser);
    ca_window_set_on_frame(browser->window, on_frame, browser);
}

/** Detaches AppKit views before Causality tears down their window. */
static void on_window_close(Ca_Window *window, void *data)
{
    (void)window;
    BrowserWindow *browser = data;
    browser_page_overlay_destroy(browser->overlay);
    browser->overlay = NULL;
}

int browser_run(const char *initial_address)
{
    char initial_url[CA_INPUT_TEXT_MAX];
    if (initial_address &&
        !browser_address_resolve(initial_address, initial_url, sizeof(initial_url))) {
        fputs("Enter a valid HTTP or HTTPS address.\n", stderr);
        return EXIT_FAILURE;
    }

#ifdef __APPLE__
    glfwInitVulkanLoader(vkGetInstanceProcAddr);
#endif

    Ca_Instance *instance = browser_create_ui_instance(BROWSER_NAME);
    if (!instance) {
        fputs("Could not initialize Causality. A Vulkan 1.3-capable graphics device is required.\n", stderr);
        return EXIT_FAILURE;
    }

    BrowserTheme theme = browser_theme_load();
    uint32_t canvas;
    Ca_Stylesheet *stylesheet = browser_theme_create(theme, &canvas);
    if (!stylesheet) {
        ca_instance_destroy(instance);
        fputs("Could not initialize the browser stylesheet.\n", stderr);
        return EXIT_FAILURE;
    }
    ca_instance_set_stylesheet(instance, stylesheet);

    BrowserWindow browser = {
        .stylesheet = stylesheet,
        .theme = theme,
        .requested_theme = theme,
        .zoom = BROWSER_DEFAULT_ZOOM,
    };
    browser.window = browser_page_overlay_create_window(instance, &(Ca_WindowDesc){
        .title = BROWSER_NAME,
        .width = 1200,
        .height = 800,
        .native_frame = false,
        .on_close = on_window_close,
        .on_close_data = &browser,
    }, &browser.overlay, on_overlay_action, &browser);
    int result = EXIT_FAILURE;
    if (browser.window) {
        if (!browser_page_overlay_install(browser.overlay)) {
            browser_page_overlay_destroy(browser.overlay);
            browser.overlay = NULL;
        }
        ca_window_set_canvas_color(browser.window, canvas);
        build_window(&browser, initial_address ? initial_url : NULL);
        ca_event_set_handler(instance, CA_EVENT_KEY, on_key, &browser);
        while (ca_instance_tick(instance)) {}
        result = EXIT_SUCCESS;
    } else {
        fputs("Could not create the browser window.\n", stderr);
    }
    ca_instance_destroy(instance);
    browser_tabs_destroy(&browser.tabs);
    ca_css_destroy(browser.stylesheet);
    return result;
}
