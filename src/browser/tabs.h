#pragma once

#include <ca_webview.h>
#include <causality_config.h>

typedef struct BrowserTabs BrowserTabs;
typedef struct BrowserTabGroup BrowserTabGroup;

#define BROWSER_START_URL "about:blank"
enum { BROWSER_DEFAULT_ZOOM = 4 };

/** Owns one tab's native page wrapper and state that survives selection changes. */
typedef struct BrowserTab {
    struct BrowserTab *next;
    BrowserTabs *tabs;
    BrowserTabGroup *group;
    Ca_Div *frame;
    Ca_Div *rail_row;
    Ca_WebView *view;
    char key[32];
    char *error;
    size_t zoom;
    bool start;
    bool loading;
    bool pointer_select;
} BrowserTab;

/** Owns a window's tab collection and the retained rail and page containers. */
struct BrowserTabs {
    Ca_Window *window;
    Ca_Div *rail_slot;
    Ca_Div *rail;
    Ca_Div *list;
    Ca_Div *editor;
    Ca_Div *pages;
    Ca_TextInput *group_name;
    Ca_Label *title;
    Ca_Button *groups_button;
    Ca_Button *toggle;
    Ca_Button *add;
    BrowserTab *first;
    BrowserTab *active;
    BrowserTab *close_tab;
    BrowserTabGroup *groups;
    BrowserTabGroup *add_group;
    BrowserTabGroup *remove_group;
    uint64_t next_key;
    bool expanded;
    bool full_width;
    bool picking_group;
    bool list_dirty;
    bool add_tab;
    bool supported;
    char pending_url[CA_INPUT_TEXT_MAX];
    void (*on_select)(void *data);
    void (*on_event)(const Ca_WebViewEvent *event, void *data);
    void (*on_error)(const char *message, void *data);
    Ca_WebViewNavigateFn allow_navigation;
    void *data;
};

/** Builds the rail into the active Causality container. */
void browser_tabs_build_rail(BrowserTabs *tabs);

/** Builds the page host and creates the initial tab when WebView is supported. */
void browser_tabs_build_pages(BrowserTabs *tabs, const char *initial_url);

/** Applies queued operations and updates the active build context. */
void browser_tabs_tick(BrowserTabs *tabs);

/** Updates visibility and the Start Page state of the active tab. */
void browser_tabs_show_start(BrowserTabs *tabs, bool start);

/** Collapses the rail if it currently takes up the entire workspace. */
void browser_tabs_reveal_page(BrowserTabs *tabs);

/** Queues an ungrouped blank tab. */
void browser_tabs_new(BrowserTabs *tabs);

/** Queues removal of the active tab. */
void browser_tabs_close_active(BrowserTabs *tabs);

/** Frees model state after the owning Causality instance has been destroyed. */
void browser_tabs_destroy(BrowserTabs *tabs);
