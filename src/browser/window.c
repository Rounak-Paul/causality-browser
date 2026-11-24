#include "window.h"
#include "address.h"

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

typedef struct BrowserWindow {
    Ca_Window *window;
    Ca_WebView *page;
    Ca_TextInput *address;
    Ca_Button *back;
    Ca_Button *forward;
    Ca_Button *reload;
    Ca_Label *status;
    bool loading;
    char pending_url[CA_INPUT_TEXT_MAX];
} BrowserWindow;

static const char browser_styles[] =
    ".browser { background: #171b22; }"
    ".toolbar { padding: 8px 10px; gap: 6px; align-items: center; }"
    ".tool { height: 34px; color: #e6ebf1; background: #282f3a; border-radius: 5px; }"
    ".tool:hover { background: #374150; }"
    ".address { height: 34px; flex-grow: 1; min-width: 40px; color: #edf1f6;"
    "           background: #101319; border-radius: 5px; padding: 0 10px; }"
    ".page { flex-grow: 1; }"
    ".status { color: #f2c18c; padding: 8px 10px; }"
    ".unavailable { flex-grow: 1; align-items: center; justify-content: center; }";

/** Sets the browser status line to message, or hides it for an empty message. */
static void show_status(BrowserWindow *browser, const char *message)
{
    ca_set_text(browser->status, message);
    ca_set_hidden(browser->status, !*message);
}

/** Permits HTTP(S) main-frame URLs and the browser's empty page. */
static bool allow_navigation(Ca_WebView *page, const char *url, void *data)
{
    (void)page;
    (void)data;
    if (!url)
        return false;
    if (strcmp(url, "about:blank") == 0)
        return true;

    char resolved[CA_INPUT_TEXT_MAX];
    return browser_address_resolve(url, resolved, sizeof(resolved)) &&
           strcmp(resolved, url) == 0;
}

/** Applies native page events to the browser chrome and queues requested links. */
static void on_page_event(Ca_WebView *page, const Ca_WebViewEvent *event, void *data)
{
    (void)page;
    BrowserWindow *browser = data;

    if (browser->loading != event->loading) {
        browser->loading = event->loading;
        ca_set_text(browser->reload, browser->loading ? "Stop" : "Reload");
    }

    switch (event->type) {
    case CA_WEBVIEW_EVENT_URL_CHANGED:
        if (!ca_input_is_focused(browser->address)) {
            const char *url = strcmp(event->url, "about:blank") == 0 ? "" : event->url;
            ca_set_text(browser->address, url);
        }
        break;
    case CA_WEBVIEW_EVENT_TITLE_CHANGED:
        ca_window_set_title(browser->window, *event->title ? event->title : BROWSER_NAME);
        break;
    case CA_WEBVIEW_EVENT_HISTORY_CHANGED:
        ca_set_disabled(browser->back, !event->can_go_back);
        ca_set_disabled(browser->forward, !event->can_go_forward);
        break;
    case CA_WEBVIEW_EVENT_LOAD_STARTED:
    case CA_WEBVIEW_EVENT_LOAD_FINISHED:
        show_status(browser, "");
        break;
    case CA_WEBVIEW_EVENT_LOAD_FAILED:
        show_status(browser, *event->detail ? event->detail : "The page could not load.");
        break;
    case CA_WEBVIEW_EVENT_PROCESS_TERMINATED:
        show_status(browser, "The page process exited. Reload to try again.");
        break;
    case CA_WEBVIEW_EVENT_NEW_WINDOW:
        if (allow_navigation(page, event->detail, data)) {
            memcpy(browser->pending_url, event->detail, strlen(event->detail) + 1);
            ca_instance_wake();
        }
        break;
    default:
        break;
    }
}

/** Opens the current address-bar input and transfers focus to the page. */
static void submit_address(BrowserWindow *browser)
{
    char url[CA_INPUT_TEXT_MAX];
    if (!browser_address_resolve(ca_input_text(browser->address), url, sizeof(url)) ||
        !ca_webview_load_url(browser->page, url)) {
        show_status(browser, "Enter a valid HTTP or HTTPS address.");
        return;
    }

    ca_set_text(browser->address, url);
    ca_window_clear_focus(browser->window);
    ca_webview_focus(browser->page);
}

/** Handles address-bar Enter and navigates popup links after the engine callback returns. */
static void on_frame(void *data)
{
    BrowserWindow *browser = data;
    if (ca_input_is_focused(browser->address) &&
        (ca_input_key_pressed(browser->address, CA_KEY_ENTER) ||
         ca_input_key_pressed(browser->address, CA_KEY_KP_ENTER))) {
        submit_address(browser);
    }

    if (browser->pending_url[0]) {
        char url[sizeof(browser->pending_url)];
        memcpy(url, browser->pending_url, strlen(browser->pending_url) + 1);
        browser->pending_url[0] = '\0';
        if (!ca_webview_load_url(browser->page, url))
            show_status(browser, "The link could not open.");
    }
}

/** Submits the address when Go is clicked. */
static void on_go(Ca_Button *button, void *data)
{
    (void)button;
    submit_address(data);
}

/** Traverses to the preceding page in this window's history. */
static void on_back(Ca_Button *button, void *data)
{
    (void)button;
    BrowserWindow *browser = data;
    ca_webview_go_back(browser->page);
}

/** Traverses to the following page in this window's history. */
static void on_forward(Ca_Button *button, void *data)
{
    (void)button;
    BrowserWindow *browser = data;
    ca_webview_go_forward(browser->page);
}

/** Stops the active load or reloads the current page. */
static void on_reload(Ca_Button *button, void *data)
{
    BrowserWindow *browser = data;
    if (browser->loading) {
        ca_webview_stop(browser->page);
        browser->loading = false;
        ca_set_text(button, "Reload");
    } else {
        ca_webview_reload(browser->page);
    }
}

/** Adds a toolbar button and binds its click callback to this browser window. */
static Ca_Button *toolbar_button(BrowserWindow *browser, const char *label,
                                 float width, Ca_ClickFn on_click, bool disabled)
{
    Ca_Button *button = ca_btn_begin(&(Ca_BtnDesc){
        .text = label,
        .style = "tool",
        .width = width,
        .disabled = disabled,
        .on_click = on_click,
        .click_data = browser,
    });
    ca_btn_end();
    return button;
}

/** Creates the browser chrome and a native page if the installed backend supports it. */
static void build_window(BrowserWindow *browser, const char *initial_url)
{
    ca_ui_begin(browser->window, &(Ca_DivDesc){.direction = CA_VERTICAL, .style = "browser"});
    bool supported = ca_webview_supported();
    if (supported) {
        ca_div_begin(&(Ca_DivDesc){.direction = CA_HORIZONTAL, .style = "toolbar"});
        browser->back = toolbar_button(browser, "Back", 54, on_back, true);
        browser->forward = toolbar_button(browser, "Forward", 70, on_forward, true);
        browser->reload = toolbar_button(browser, "Reload", 64, on_reload, false);
        browser->address = ca_input(&(Ca_InputDesc){
            .text = initial_url,
            .placeholder = "Enter a web address",
            .style = "address",
        });
        Ca_Button *go = toolbar_button(browser, "Go", 42, on_go, false);
        ca_div_end();

        browser->status = ca_text(&(Ca_TextDesc){.style = "status", .hidden = true, .wrap = true});
        if (browser->back && browser->forward && browser->reload && browser->address &&
            browser->status && go) {
            browser->page = ca_webview(&(Ca_WebViewDesc){
                .url = initial_url ? initial_url : "about:blank",
                .style = "page",
                .on_event = on_page_event,
                .on_navigate = allow_navigation,
                .user_data = browser,
            });
            if (browser->page)
                ca_window_set_on_frame(browser->window, on_frame, browser);
        }
        if (!browser->page) {
            if (browser->reload) ca_set_disabled(browser->reload, true);
            if (browser->address) ca_set_disabled(browser->address, true);
            if (go) ca_set_disabled(go, true);
        }
    }

    if (!browser->page) {
        ca_div_begin(&(Ca_DivDesc){.style = "unavailable"});
        ca_text(&(Ca_TextDesc){
            .text = supported
                ? "The native WebView could not start."
                : "Causality WebView is unavailable on this platform.",
            .style = "status",
            .wrap = true,
        });
        ca_div_end();
    }
    ca_ui_end();
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

    Ca_Instance *instance = ca_instance_create(&(Ca_InstanceDesc){.app_name = BROWSER_NAME});
    if (!instance) {
        fputs("Could not initialize Causality. A Vulkan 1.3-capable graphics device is required.\n", stderr);
        return EXIT_FAILURE;
    }

    Ca_Stylesheet *stylesheet = ca_css_parse(browser_styles);
    if (!stylesheet) {
        ca_instance_destroy(instance);
        fputs("Could not initialize the browser stylesheet.\n", stderr);
        return EXIT_FAILURE;
    }
    ca_instance_set_stylesheet(instance, stylesheet);

    BrowserWindow browser = {
        .window = ca_window_create(instance, &(Ca_WindowDesc){
            .title = BROWSER_NAME,
            .width = 1200,
            .height = 800,
            .native_frame = true,
            .resizable = true,
        }),
    };
    int result = EXIT_FAILURE;
    if (browser.window) {
        build_window(&browser, initial_address ? initial_url : NULL);
        while (ca_instance_tick(instance)) {}
        result = EXIT_SUCCESS;
    } else {
        fputs("Could not create the browser window.\n", stderr);
    }
    ca_instance_destroy(instance);
    ca_css_destroy(stylesheet);
    return result;
}
