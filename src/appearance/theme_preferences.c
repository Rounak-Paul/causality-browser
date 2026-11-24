#if defined(__linux__)
#define _POSIX_C_SOURCE 200809L
#endif

#include "theme.h"

#if defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>

#define THEME_DOMAIN CFSTR("com.causality.browser")
#define THEME_KEY CFSTR("Theme")

BrowserTheme browser_theme_load(void)
{
    int theme = BROWSER_THEME_DARK;
    CFPropertyListRef value = CFPreferencesCopyAppValue(THEME_KEY, THEME_DOMAIN);
    if (value) {
        if (CFGetTypeID(value) == CFNumberGetTypeID() &&
            !CFNumberGetValue(value, kCFNumberIntType, &theme))
            theme = BROWSER_THEME_DARK;
        CFRelease(value);
    }
    return (unsigned)theme < BROWSER_THEME_COUNT ? (BrowserTheme)theme : BROWSER_THEME_DARK;
}

bool browser_theme_save(BrowserTheme theme)
{
    int index = (int)theme;
    CFNumberRef value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &index);
    if (!value)
        return false;
    CFPreferencesSetAppValue(THEME_KEY, value, THEME_DOMAIN);
    CFRelease(value);
    return CFPreferencesAppSynchronize(THEME_DOMAIN);
}

#elif defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define THEME_REGISTRY_KEY L"Software\\Causality Browser"
#define THEME_VALUE L"Theme"

BrowserTheme browser_theme_load(void)
{
    DWORD theme;
    DWORD size = sizeof(theme);
    LSTATUS result = RegGetValueW(HKEY_CURRENT_USER, THEME_REGISTRY_KEY, THEME_VALUE,
                                  RRF_RT_REG_DWORD, NULL, &theme, &size);
    return result == ERROR_SUCCESS && theme < BROWSER_THEME_COUNT
        ? (BrowserTheme)theme : BROWSER_THEME_DARK;
}

bool browser_theme_save(BrowserTheme theme)
{
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, THEME_REGISTRY_KEY, 0, NULL, 0, KEY_SET_VALUE,
                        NULL, &key, NULL) != ERROR_SUCCESS)
        return false;
    DWORD value = theme;
    LSTATUS result = RegSetValueExW(key, THEME_VALUE, 0, REG_DWORD,
                                    (const BYTE *)&value, sizeof(value));
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

#else

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/** Writes the preference path to path, creating its config directory for writes. */
static bool theme_path(char path[PATH_MAX], bool for_write)
{
    char directory[PATH_MAX];
    const char *config = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    if ((!config || config[0] != '/') && (!home || home[0] != '/'))
        return false;

    int count = config && config[0] == '/'
        ? snprintf(directory, sizeof(directory), "%s", config)
        : snprintf(directory, sizeof(directory), "%s/.config", home);
    if (count < 0 || (size_t)count >= sizeof(directory))
        return false;
    if (for_write && mkdir(directory, 0700) != 0 && errno != EEXIST)
        return false;

    count = snprintf(path, PATH_MAX, "%s/causality-browser-theme", directory);
    return count >= 0 && count < PATH_MAX;
}

BrowserTheme browser_theme_load(void)
{
    char path[PATH_MAX];
    if (!theme_path(path, false))
        return BROWSER_THEME_DARK;

    FILE *file = fopen(path, "r");
    if (!file)
        return BROWSER_THEME_DARK;

    char name[32];
    bool read = fgets(name, sizeof(name), file) != NULL;
    fclose(file);
    if (read) {
        name[strcspn(name, "\r\n")] = '\0';
        for (BrowserTheme theme = 0; theme < BROWSER_THEME_COUNT; ++theme) {
            if (strcmp(name, browser_theme_name(theme)) == 0)
                return theme;
        }
    }
    return BROWSER_THEME_DARK;
}

bool browser_theme_save(BrowserTheme theme)
{
    char path[PATH_MAX];
    char temporary[PATH_MAX];
    if (!theme_path(path, true))
        return false;

    int count = snprintf(temporary, sizeof(temporary), "%s.XXXXXX", path);
    if (count < 0 || (size_t)count >= sizeof(temporary))
        return false;

    int fd = mkstemp(temporary);
    if (fd < 0)
        return false;

    FILE *file = fdopen(fd, "w");
    if (file) {
        bool written = fprintf(file, "%s\n", browser_theme_name(theme)) >= 0;
        if (fclose(file) == 0 && written && rename(temporary, path) == 0)
            return true;
    } else {
        close(fd);
    }
    unlink(temporary);
    return false;
}

#endif
