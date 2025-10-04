/*
 * obs-browser-hotkey-forwarder.c
 *
 * OBS Browser Source: Hotkey forwarding with configurable modifier filtering.
 *
 * This module allows registering hotkeys for browser sources and forwarding those keystrokes
 * to the embedded browser. Users can choose to:
 *   - Pass the hotkey exactly as pressed (including modifiers),
 *   - Strip only the first modifier,
 *   - Strip all modifiers (send only the base key).
 *
 * This enables workflows such as sending the same keystroke (e.g., 'A') to different browser
 * sources by using different modifiers (e.g., CTRL+A for Browser Source 1, ALT+A for Browser
 * Source 2), while the browser itself only receives the intended key event.
 *
 * Three hotkey options are automatically registered for each browser source:
 *   - Send Key to Browser Source (Exact)
 *   - Send Key to Browser Source (Strip First Modifier)
 *   - Send Key to Browser Source (Strip All Modifiers)
 *
 * The user can assign any key or key+modifier combo to these actions. When triggered, the
 * keystroke is forwarded based on the selected filtering mode.
 *
 * Author: Paul van Brouwershaven (@vanbroup)
 * License: GPL-2.0-or-later
 */

#include <obs-module.h>
#include <stdlib.h>
#include <string.h>

/* Modifier filtering strategy */
typedef enum {
    FILTER_NONE,
    FILTER_FIRST,
    FILTER_ALL
} modifier_filter_mode_t;

/* Callback context for each registered hotkey */
struct hotkey_cb_context {
    obs_source_t *src;
    modifier_filter_mode_t mode;
};

/* Utility: filter the modifiers based on the selected mode */
static uint32_t filter_modifiers(uint32_t modifiers, modifier_filter_mode_t mode)
{
    if (mode == FILTER_NONE)
        return modifiers;
    if (mode == FILTER_ALL)
        return 0;
    if (mode == FILTER_FIRST) {
        if (!modifiers) return 0;
        uint32_t first = modifiers & -modifiers; // Isolate lowest bit
        return modifiers & ~first;
    }
    return modifiers;
}

/* The unified hotkey callback */
static void browser_source_hotkey_forwarder(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
    if (!pressed || !data || !hotkey) return;
    struct hotkey_cb_context *ctx = (struct hotkey_cb_context *)data;
    obs_source_t *src = ctx->src;
    if (!src) return;

    struct obs_key_event ev = {
        .key = hotkey->key,
        .modifiers = filter_modifiers(hotkey->modifiers, ctx->mode),
        .native_vkey = hotkey->native_vkey,
        .native_modifiers = filter_modifiers(hotkey->native_modifiers, ctx->mode),
        .text = {0}
    };
    obs_source_send_key_click(src, &ev, false); // Key down
    obs_source_send_key_click(src, &ev, true);  // Key up
}

/* Registration helper: creates and registers all filter variants for a browser source.
 * Returns an array of context pointers that must be freed on source destruction. */
struct hotkey_cb_context **register_browser_source_hotkey_forwarders(obs_source_t *src)
{
    static const struct {
        const char *id;
        const char *desc;
        modifier_filter_mode_t mode;
    } registrations[] = {
        { "browser.send_key_exact", "Send Key to Browser Source (Exact)", FILTER_NONE },
        { "browser.send_key_strip_first", "Send Key to Browser Source (Strip First Modifier)", FILTER_FIRST },
        { "browser.send_key_strip_all", "Send Key to Browser Source (Strip All Modifiers)", FILTER_ALL }
    };

    const size_t count = sizeof(registrations)/sizeof(registrations[0]);
    struct hotkey_cb_context **contexts = bzalloc(sizeof(*contexts) * (count + 1));

    for (size_t i = 0; i < count; ++i) {
        struct hotkey_cb_context *ctx = bzalloc(sizeof(*ctx));
        ctx->src = src;
        ctx->mode = registrations[i].mode;
        obs_hotkey_register_source(src, registrations[i].id, registrations[i].desc,
                                   browser_source_hotkey_forwarder, ctx);
        contexts[i] = ctx;
    }
    contexts[count] = NULL; // NULL-terminated array

    return contexts;
}

/* Cleanup helper: frees all context pointers allocated during registration */
void cleanup_browser_source_hotkey_forwarders(struct hotkey_cb_context **contexts)
{
    if (!contexts)
        return;

    for (size_t i = 0; contexts[i] != NULL; ++i) {
        bfree(contexts[i]);
    }
    bfree(contexts);
}
