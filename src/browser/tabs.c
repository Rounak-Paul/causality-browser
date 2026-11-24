#include "tabs.h"

#include <ca_icons.h>
#include <ctype.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { GROUP_NAME_MAX = 64 };

/** Stores a named rail section and its expansion state. */
struct BrowserTabGroup {
    BrowserTabGroup *next;
    BrowserTabs *tabs;
    Ca_Div *rail_header;
    char key[32];
    char name[GROUP_NAME_MAX + 1];
    bool collapsed;
};

/** Builds a rail button with an optional tooltip. */
static Ca_Button *rail_button(const char *text, const char *style, const char *hint,
                              Ca_ClickFn click, void *data, bool disabled)
{
    Ca_Button *button = ca_btn_begin(&(Ca_BtnDesc){
        .text = text, .style = style, .on_click = click, .click_data = data, .disabled = disabled,
    });
    ca_btn_end();
    if (button && hint)
        ca_tooltip_for_widget(button, &(Ca_TooltipDesc){.text = hint});
    return button;
}

/** Updates the rail's width and toggles its expanded-only header controls. */
static void sync_layout(BrowserTabs *tabs, bool force)
{
    int width = 0;
    ca_window_get_rect(tabs->window, NULL, NULL, &width, NULL);
    bool full = tabs->expanded && width < 570;
    if (!force && full == tabs->full_width)
        return;
    tabs->full_width = full;
    ca_set_style(tabs->rail_slot, full ? "tab-rail-slot expanded full" :
                 tabs->expanded ? "tab-rail-slot expanded" : "tab-rail-slot");
    if (tabs->title) ca_set_hidden(tabs->title, !tabs->expanded);
    if (tabs->groups_button) ca_set_hidden(tabs->groups_button, !tabs->expanded);
    if (tabs->add)
        ca_set_style(tabs->add, tabs->expanded ? "rail-button" : "rail-button rail-add");
    if (tabs->toggle)
        ca_set_text(tabs->toggle, tabs->expanded ? CA_ICON_NF_COD_CHEVRON_LEFT : CA_ICON_NF_COD_CHEVRON_RIGHT);
}

/** Opens or closes the group editor attached to the rail. */
static void show_group_picker(BrowserTabs *tabs, bool show)
{
    tabs->picking_group = show;
    ca_set_hidden(tabs->editor, !show);
    if (!show && tabs->group_name)
        ca_set_text(tabs->group_name, "");
    tabs->list_dirty = true;
}

/** Changes the rail between tile and labeled list views. */
static void on_rail_toggle(Ca_Button *button, void *data)
{
    (void)button;
    BrowserTabs *tabs = data;
    tabs->expanded = !tabs->expanded;
    if (!tabs->expanded && tabs->picking_group)
        show_group_picker(tabs, false);
    sync_layout(tabs, true);
    tabs->list_dirty = true;
}

void browser_tabs_reveal_page(BrowserTabs *tabs)
{
    if (tabs->full_width)
        on_rail_toggle(NULL, tabs);
}

/** Toggles group assignment and creation for the active tab. */
static void on_group_picker(Ca_Button *button, void *data)
{
    (void)button;
    BrowserTabs *tabs = data;
    if (tabs->active)
        show_group_picker(tabs, !tabs->picking_group);
}

/** Shows only the active document and refreshes its shared chrome. */
static void select_tab(BrowserTabs *tabs, BrowserTab *tab)
{
    if (tabs->active == tab)
        return;
    if (tabs->active) {
        ca_webview_blur(tabs->active->view);
        ca_set_hidden(tabs->active->frame, true);
    }
    tabs->active = tab;
    browser_tabs_show_start(tabs, tab->start);
    if (tabs->picking_group)
        show_group_picker(tabs, false);
    if (tabs->full_width) {
        tabs->expanded = false;
        sync_layout(tabs, true);
    }
    tabs->list_dirty = true;
    tabs->on_select(tabs->data);
}

void browser_tabs_show_start(BrowserTabs *tabs, bool start)
{
    BrowserTab *tab = tabs->active;
    if (!tab)
        return;
    tab->start = start;
    ca_set_hidden(tabs->pages, start);
    if (tab->frame)
        ca_set_hidden(tab->frame, start);
}

void browser_tabs_new(BrowserTabs *tabs)
{
    if (!tabs->active)
        return;
    tabs->add_tab = true;
    tabs->add_group = NULL;
    tabs->pending_url[0] = '\0';
}

void browser_tabs_close_active(BrowserTabs *tabs)
{
    tabs->close_tab = tabs->active;
}

/** Records page state and forwards events from the selected tab. */
static void on_tab_event(Ca_WebView *view, const Ca_WebViewEvent *event, void *data)
{
    BrowserTab *tab = data;
    BrowserTabs *tabs = tab->tabs;
    tab->loading = event->loading;
    switch (event->type) {
    case CA_WEBVIEW_EVENT_URL_CHANGED:
        if (*event->url)
            tab->start = strcmp(event->url, BROWSER_START_URL) == 0;
        tabs->list_dirty = true;
        break;
    case CA_WEBVIEW_EVENT_TITLE_CHANGED:
        tabs->list_dirty = true;
        break;
    case CA_WEBVIEW_EVENT_LOAD_STARTED:
    case CA_WEBVIEW_EVENT_LOAD_FINISHED:
        free(tab->error);
        tab->error = NULL;
        break;
    case CA_WEBVIEW_EVENT_LOAD_FAILED:
    case CA_WEBVIEW_EVENT_PROCESS_TERMINATED: {
        const char *message = event->type == CA_WEBVIEW_EVENT_PROCESS_TERMINATED
            ? "The page process exited. Reload to try again."
            : *event->detail ? event->detail : "The page could not load.";
        size_t length = strlen(message) + 1;
        free(tab->error);
        tab->error = malloc(length);
        if (tab->error)
            memcpy(tab->error, message, length);
        break;
    }
    case CA_WEBVIEW_EVENT_NEW_WINDOW:
        if (tab == tabs->active && tabs->allow_navigation(view, event->detail, NULL)) {
            tabs->add_tab = true;
            tabs->add_group = tab->group;
            memcpy(tabs->pending_url, event->detail, strlen(event->detail) + 1);
            ca_instance_wake();
        }
        return;
    default:
        break;
    }
    if (tab == tabs->active)
        tabs->on_event(event, tabs->data);
}

/** Reuses keyed WebView wrappers and mounts the newly appended tab. */
static void sync_pages(BrowserTabs *tabs, BrowserTab *added, const char *url)
{
    ca_reconcile_begin(tabs->pages);
    for (BrowserTab *tab = tabs->first; tab; tab = tab->next) {
        ca_reconcile_key(tab->key);
        tab->frame = ca_div_begin(&(Ca_DivDesc){
            .direction = CA_VERTICAL, .style = "page-frame", .clip_content = true,
            .hidden = tab != tabs->active || tab->start,
        });
        tab->view = ca_webview(&(Ca_WebViewDesc){
            .url = tab == added ? url : NULL, .style = "page", .id = "document",
            .on_event = on_tab_event, .on_navigate = tabs->allow_navigation, .user_data = tab,
        });
        ca_div_end();
    }
    ca_div_end();
}

/** Creates a tab and selects it only when the native WebView starts. */
static BrowserTab *add_tab(BrowserTabs *tabs, BrowserTabGroup *group, const char *url)
{
    BrowserTab *tab = calloc(1, sizeof(*tab));
    if (!tab) {
        tabs->on_error("Could not create a tab.", tabs->data);
        return NULL;
    }
    tab->tabs = tabs;
    tab->group = group;
    tab->zoom = BROWSER_DEFAULT_ZOOM;
    tab->start = !url || strcmp(url, BROWSER_START_URL) == 0;
    snprintf(tab->key, sizeof(tab->key), "tab-%" PRIu64, ++tabs->next_key);
    BrowserTab **tail = &tabs->first;
    while (*tail)
        tail = &(*tail)->next;
    *tail = tab;
    sync_pages(tabs, tab, url ? url : BROWSER_START_URL);
    if (!tab->view) {
        ca_div_destroy(tab->frame);
        *tail = NULL;
        free(tab->error);
        free(tab);
        tabs->on_error("Could not open the native WebView.", tabs->data);
        return NULL;
    }
    if (group)
        group->collapsed = false;
    select_tab(tabs, tab);
    return tab;
}

/** Closes a tab and selects its neighbor, or creates a fresh Start Page for the last tab. */
static void close_tab(BrowserTabs *tabs, BrowserTab *tab)
{
    BrowserTab **link = &tabs->first;
    BrowserTab *previous = NULL;
    while (*link && *link != tab) {
        previous = *link;
        link = &(*link)->next;
    }
    if (!*link)
        return;

    if (tabs->active == tab) {
        BrowserTab *replacement = tab->next ? tab->next : previous;
        if (replacement)
            select_tab(tabs, replacement);
        else if (!add_tab(tabs, NULL, NULL))
            return;
    }
    *link = tab->next;
    ca_div_destroy(tab->frame);
    free(tab->error);
    free(tab);
    tabs->list_dirty = true;
}

/** Queues a new blank tab when the rail add button is clicked. */
static void on_add(Ca_Button *button, void *data)
{
    (void)button;
    browser_tabs_new(data);
}

/** Queues a new tab inside the specified group. */
static void on_add_to_group(Ca_Button *button, void *data)
{
    (void)button;
    BrowserTabGroup *group = data;
    browser_tabs_new(group->tabs);
    group->tabs->add_group = group;
}

/** Displays the chosen tab, restoring the page area if the rail fills the window. */
static void activate_tab_row(BrowserTab *tab)
{
    if (tab->tabs->active == tab && tab->tabs->full_width)
        on_rail_toggle(NULL, tab->tabs);
    else
        select_tab(tab->tabs, tab);
}

/** Activates keyboard selections immediately and lets mouse gestures finish first. */
static void on_select_tab(Ca_Button *button, void *data)
{
    BrowserTab *tab = data;
    if (ca_button_get_click_pos(button, NULL, NULL))
        tab->pointer_select = true;
    else
        activate_tab_row(tab);
}

/** Queues closing the chosen tab after input processing. */
static void on_close_tab(Ca_Button *button, void *data)
{
    (void)button;
    BrowserTab *tab = data;
    tab->tabs->close_tab = tab;
}

/** Adds or moves the active tab to the chosen group. */
static void assign_group(BrowserTabs *tabs, BrowserTabGroup *group)
{
    if (tabs->active) {
        tabs->active->group = group;
        if (group)
            group->collapsed = false;
    }
    show_group_picker(tabs, false);
}

/** Moves the active tab to the clicked group. */
static void on_assign_group(Ca_Button *button, void *data)
{
    (void)button;
    BrowserTabGroup *group = data;
    assign_group(group->tabs, group);
}

/** Removes the active tab's group assignment. */
static void on_ungroup(Ca_Button *button, void *data)
{
    (void)button;
    assign_group(data, NULL);
}

/** Shows or hides the group's rows in the expanded rail. */
static void on_toggle_group(Ca_Button *button, void *data)
{
    (void)button;
    BrowserTabGroup *group = data;
    group->collapsed = !group->collapsed;
    group->tabs->list_dirty = true;
}

/** Removes a group while keeping all its tabs in the ungrouped list. */
static void remove_group(BrowserTabs *tabs, BrowserTabGroup *group)
{
    BrowserTabGroup **link = &tabs->groups;
    while (*link != group)
        link = &(*link)->next;
    *link = group->next;
    for (BrowserTab *tab = tabs->first; tab; tab = tab->next) {
        if (tab->group == group)
            tab->group = NULL;
    }
    if (tabs->add_group == group)
        tabs->add_group = NULL;
    free(group);
    tabs->list_dirty = true;
}

/** Queues removal of the clicked group after input processing. */
static void on_remove_group(Ca_Button *button, void *data)
{
    (void)button;
    BrowserTabGroup *group = data;
    group->tabs->remove_group = group;
}

/** Appends a named group to the rail, reporting allocation failure in the shared chrome. */
static BrowserTabGroup *create_group(BrowserTabs *tabs, const char *name, size_t length)
{
    BrowserTabGroup *group = calloc(1, sizeof(*group));
    if (!group) {
        tabs->on_error("Could not create a tab group.", tabs->data);
        return NULL;
    }
    group->tabs = tabs;
    snprintf(group->key, sizeof(group->key), "group-%" PRIu64, ++tabs->next_key);
    memcpy(group->name, name, length);
    BrowserTabGroup **tail = &tabs->groups;
    while (*tail)
        tail = &(*tail)->next;
    *tail = group;
    return group;
}

/** Creates a named group around the active tab. */
static void on_create_group(Ca_Button *button, void *data)
{
    (void)button;
    BrowserTabs *tabs = data;
    if (!tabs->active)
        return;
    const char *name = ca_input_text(tabs->group_name);
    while (isspace((unsigned char)*name))
        ++name;
    size_t length = strlen(name);
    while (length && isspace((unsigned char)name[length - 1]))
        --length;
    if (!length || length > GROUP_NAME_MAX) {
        ca_input_focus(tabs->group_name);
        return;
    }
    BrowserTabGroup *group = create_group(tabs, name, length);
    if (group)
        assign_group(tabs, group);
}

/** Writes a complete UTF-8 prefix of text to a fixed-size UI label. */
static void copy_label(char *label, size_t capacity, const char *text)
{
    size_t length = strlen(text);
    if (length >= capacity) {
        length = capacity - 1;
        while (length && ((unsigned char)text[length] & 0xc0) == 0x80)
            --length;
    }
    memcpy(label, text, length);
    label[length] = '\0';
}

/** Returns the page title, address, or label for a tab without a loaded document. */
static const char *tab_label(const BrowserTab *tab)
{
    const char *title = ca_webview_title(tab->view);
    const char *url = ca_webview_url(tab->view);
    return *title ? title : tab->start ? "Start Page" : *url ? url : "New Tab";
}

/** Tests whether a window-local point lies within a visible rail item. */
static bool contains_point(const Ca_Div *item, float x, float y)
{
    if (!item)
        return false;
    float left, top, width, height;
    ca_div_screen_rect(item, &left, &top, &width, &height);
    return x >= left && y >= top && x < left + width && y < top + height;
}

/** Moves a dragged tab into a folder or groups it with the tab it lands on. */
static void on_tab_drop(const Ca_DragEvent *event, void *data)
{
    BrowserTab *source = data;
    BrowserTabs *tabs = source->tabs;
    bool select = source->pointer_select;
    source->pointer_select = false;
    if (!select || !contains_point(tabs->list, event->x, event->y))
        return;
    if (fabsf(event->dx) + fabsf(event->dy) < 10.0f) {
        if (contains_point(source->rail_row, event->x, event->y))
            activate_tab_row(source);
        return;
    }

    BrowserTabGroup *group = NULL;
    for (BrowserTabGroup *target = tabs->groups; target; target = target->next) {
        if (contains_point(target->rail_header, event->x, event->y)) {
            group = target;
            break;
        }
    }
    if (!group) {
        for (BrowserTab *target = tabs->first; target; target = target->next) {
            if (target == source || !contains_point(target->rail_row, event->x, event->y))
                continue;
            group = target->group;
            if (!group) {
                char name[GROUP_NAME_MAX + 1];
                copy_label(name, sizeof(name), target->start ? "Tab group" : tab_label(target));
                group = create_group(tabs, name, strlen(name));
                if (group)
                    target->group = group;
            }
            break;
        }
    }
    if (group) {
        group->collapsed = false;
        source->group = group;
        tabs->list_dirty = true;
    }
}

/** Builds the compact tile or expanded row for a single tab. */
static void build_tab_row(BrowserTab *tab)
{
    BrowserTabs *tabs = tab->tabs;
    const char *title = tab_label(tab);

    ca_reconcile_key(tab->key);
    tab->rail_row = ca_div_begin(&(Ca_DivDesc){
        .direction = CA_HORIZONTAL,
        .style = !tabs->expanded ? "tab-tile-wrap"
            : tab == tabs->active ? "tab-row active" : "tab-row",
        .on_drag_end = on_tab_drop,
        .drag_data = tab,
    });
    if (!tabs->expanded) {
        const char *label = title;
        const char *scheme = strstr(label, "://");
        if (scheme && !*ca_webview_title(tab->view)) {
            label = scheme + 3;
            if (strncmp(label, "www.", 4) == 0) label += 4;
        }
        char letter[] = {(char)toupper((unsigned char)*label), '\0'};
        const char *icon = tab->start ? CA_ICON_NF_COD_COMPASS : isalnum((unsigned char)*label)
            ? letter : CA_ICON_NF_COD_GLOBE;
        rail_button(icon, tab == tabs->active ? "tab-tile active" : "tab-tile",
                    title, on_select_tab, tab, false);
        ca_div_end();
        return;
    }

    char label[CA_BUTTON_TEXT_MAX];
    const char *icon = tab->start ? CA_ICON_NF_COD_COMPASS : CA_ICON_NF_COD_GLOBE;
    size_t prefix = strlen(icon);
    memcpy(label, icon, prefix);
    memcpy(label + prefix, "  ", 2);
    copy_label(label + prefix + 2, sizeof(label) - prefix - 2, title);
    rail_button(label, "tab-label", title, on_select_tab, tab, false);
    rail_button(CA_ICON_NF_COD_CLOSE, "rail-button tab-close", "Close tab", on_close_tab, tab, false);
    ca_div_end();
}

/** Reconciles the grouped tab list or group assignment targets inside the rail. */
static void build_list(BrowserTabs *tabs)
{
    for (BrowserTab *tab = tabs->first; tab; tab = tab->next)
        tab->rail_row = NULL;
    for (BrowserTabGroup *group = tabs->groups; group; group = group->next)
        group->rail_header = NULL;
    ca_reconcile_begin(tabs->list);
    if (tabs->picking_group) {
        if (tabs->groups) {
            ca_text(&(Ca_TextDesc){.text = "Move active tab to", .style = "tab-group-caption"});
            rail_button("Ungrouped", "group-choice", NULL, on_ungroup, tabs, !tabs->active->group);
            for (BrowserTabGroup *group = tabs->groups; group; group = group->next) {
                ca_reconcile_key(group->key);
                rail_button(group->name, "group-choice", NULL, on_assign_group, group,
                            tabs->active->group == group);
            }
        }
    } else {
        bool show_ungrouped_caption = tabs->expanded && tabs->groups;
        for (BrowserTab *tab = tabs->first; tab; tab = tab->next) {
            if (!tab->group) {
                if (show_ungrouped_caption) {
                    ca_text(&(Ca_TextDesc){.text = "Ungrouped", .style = "tab-group-caption"});
                    show_ungrouped_caption = false;
                }
                build_tab_row(tab);
            }
        }
        for (BrowserTabGroup *group = tabs->groups; group; group = group->next) {
            ca_reconcile_key(group->key);
            ca_div_begin(&(Ca_DivDesc){.direction = CA_VERTICAL, .style = "tab-section"});
            if (tabs->expanded) {
                group->rail_header = ca_div_begin(&(Ca_DivDesc){
                    .direction = CA_HORIZONTAL, .style = "tab-group-head",
                });
                char label[CA_BUTTON_TEXT_MAX];
                snprintf(label, sizeof(label), "%s  %s", group->collapsed ? CA_ICON_NF_COD_CHEVRON_RIGHT
                         : CA_ICON_NF_COD_CHEVRON_DOWN, group->name);
                rail_button(label, "tab-group-label", group->name, on_toggle_group, group, false);
                rail_button(CA_ICON_NF_COD_ADD, "rail-button tab-close", "New tab in group",
                            on_add_to_group, group, false);
                rail_button(CA_ICON_NF_COD_CLOSE, "rail-button tab-close", "Remove group, keep tabs",
                            on_remove_group, group, false);
                ca_div_end();
            } else {
                bool active = tabs->active && tabs->active->group == group;
                group->rail_header = ca_div_begin(&(Ca_DivDesc){
                    .direction = CA_HORIZONTAL, .style = "tab-tile-wrap",
                });
                rail_button(group->collapsed ? CA_ICON_NF_COD_FOLDER : CA_ICON_NF_COD_FOLDER_OPENED,
                            active ? "tab-tile group-tile group-active" : "tab-tile group-tile",
                            group->name, on_toggle_group, group, false);
                ca_div_end();
            }
            if (!group->collapsed) {
                for (BrowserTab *tab = tabs->first; tab; tab = tab->next) {
                    if (tab->group == group)
                        build_tab_row(tab);
                }
            }
            ca_div_end();
        }
    }
    ca_div_end();
    tabs->list_dirty = false;
}

void browser_tabs_build_rail(BrowserTabs *tabs)
{
    tabs->rail_slot = ca_div_begin(&(Ca_DivDesc){.direction = CA_VERTICAL, .style = "tab-rail-slot"});
    tabs->rail = ca_div_begin(&(Ca_DivDesc){.direction = CA_VERTICAL, .style = "tab-rail"});
    ca_div_begin(&(Ca_DivDesc){.direction = CA_HORIZONTAL, .style = "tab-rail-head"});
    tabs->title = ca_text(&(Ca_TextDesc){.text = "Tabs", .style = "tab-rail-title", .hidden = true});
    tabs->groups_button = rail_button(CA_ICON_NF_COD_NEW_FOLDER, "rail-button", "Manage tab groups",
                                      on_group_picker, tabs, true);
    if (tabs->groups_button)
        ca_set_hidden(tabs->groups_button, true);
    tabs->add = rail_button(CA_ICON_NF_COD_ADD, "rail-button rail-add", "New tab", on_add, tabs, !tabs->supported);
    ca_div_end();

    tabs->editor = ca_div_begin(&(Ca_DivDesc){
        .direction = CA_HORIZONTAL, .style = "tab-group-editor", .hidden = true,
    });
    tabs->group_name = ca_input(&(Ca_InputDesc){
        .placeholder = "New group name", .style = "group-name", .max_length = GROUP_NAME_MAX,
    });
    rail_button("Create", "rail-button group-create", "Create group with active tab", on_create_group, tabs, false);
    ca_div_end();

    tabs->list = ca_div_begin(&(Ca_DivDesc){.direction = CA_VERTICAL, .style = "tab-list"});
    ca_div_end();

    ca_div_begin(&(Ca_DivDesc){.direction = CA_HORIZONTAL, .style = "tab-rail-footer"});
    tabs->toggle = rail_button(CA_ICON_NF_COD_CHEVRON_RIGHT, "rail-button rail-toggle",
                               "Expand or collapse tab rail", on_rail_toggle, tabs, false);
    ca_div_end();
    ca_div_end();
    ca_div_end();
}

void browser_tabs_build_pages(BrowserTabs *tabs, const char *initial_url)
{
    tabs->pages = ca_div_begin(&(Ca_DivDesc){
        .direction = CA_VERTICAL, .style = "tab-pages", .clip_content = true,
    });
    ca_div_end();
    if (tabs->supported)
        add_tab(tabs, NULL, initial_url);
    if (tabs->add) ca_set_disabled(tabs->add, !tabs->active);
    if (tabs->groups_button) ca_set_disabled(tabs->groups_button, !tabs->active);
    if (tabs->list_dirty)
        build_list(tabs);
}

void browser_tabs_tick(BrowserTabs *tabs)
{
    if (tabs->expanded)
        sync_layout(tabs, false);
    if (tabs->picking_group && ca_input_is_focused(tabs->group_name) &&
        (ca_input_key_pressed(tabs->group_name, CA_KEY_ENTER) ||
         ca_input_key_pressed(tabs->group_name, CA_KEY_KP_ENTER))) {
        on_create_group(NULL, tabs);
    }
    if (tabs->add_tab) {
        char url[sizeof(tabs->pending_url)];
        memcpy(url, tabs->pending_url, strlen(tabs->pending_url) + 1);
        tabs->pending_url[0] = '\0';
        tabs->add_tab = false;
        add_tab(tabs, tabs->add_group, *url ? url : NULL);
    }
    if (tabs->close_tab) {
        BrowserTab *tab = tabs->close_tab;
        tabs->close_tab = NULL;
        close_tab(tabs, tab);
    }
    if (tabs->remove_group) {
        BrowserTabGroup *group = tabs->remove_group;
        tabs->remove_group = NULL;
        remove_group(tabs, group);
    }
    if (tabs->list_dirty)
        build_list(tabs);
}

void browser_tabs_destroy(BrowserTabs *tabs)
{
    while (tabs->first) {
        BrowserTab *tab = tabs->first;
        tabs->first = tab->next;
        free(tab->error);
        free(tab);
    }
    while (tabs->groups) {
        BrowserTabGroup *group = tabs->groups;
        tabs->groups = group->next;
        free(group);
    }
}
