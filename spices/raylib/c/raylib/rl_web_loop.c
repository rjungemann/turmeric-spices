/* rl_web_loop.c -- the browser's frame-callback shim for raylib/web.
 *
 * WHY THIS FILE EXISTS
 *
 * A browser cannot be handed a Turmeric closure.  `emscripten_set_main_loop`
 * takes a bare `void (*)(void)` and calls it from the page's event loop, so
 * something has to hold the closure between registration and the first frame
 * and present a plain C function pointer to Emscripten.  That is all this
 * file does.
 *
 * WHAT IT DELIBERATELY DOES NOT DO
 *
 * It does not know Turmeric's closure layout.  The calling convention for a
 * function-typed parameter lives in the emitted preamble (`tur_poly_fn_t`),
 * and the c-integration guide is explicit that a spice-side copy of a calling
 * convention is "a silent miscompile waiting for the convention to move."  So
 * the inline-C body in `raylib/web.tur` -- which is compiled WITH the preamble
 * and can read the struct properly -- unpacks the closure and passes the two
 * resulting words here as opaque `void *`.  The only convention knowledge
 * below is "a zero-argument Turmeric function is called as fn(env)", which
 * `tests/fixtures/region-escape-via-main-loop` in the turmeric repo pins so it
 * cannot rot unnoticed.
 *
 * LIFETIME
 *
 * `emscripten_set_main_loop` with simulate_infinite_loop=1 unwinds out of
 * main() and lets the browser call back afterwards, so main's stack frame is
 * gone by the first frame.  Both of these words therefore outlive the bracket
 * that produced them, which is why the caller notes them for the region
 * allocator before calling in.  Static storage here is the C half of the same
 * requirement.
 */

#include <stdint.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

static void    *g_frame_env;
static int64_t (*g_frame_fn)(void *);

#ifdef __EMSCRIPTEN__
static void tur_rl_web_frame(void) {
    if (g_frame_fn) (void)g_frame_fn(g_frame_env);
}
#endif

/* fps == 0 asks Emscripten for requestAnimationFrame, which is what a browser
 * wants: it matches the display and stops entirely when the tab is hidden.
 * raylib's SetTargetFPS must NOT be used on the web -- it sleeps to hit a
 * rate, and sleeping is the one thing that must not happen on the thread that
 * also services the page. */
void tur_rl_web_run(void *env, int64_t (*fn)(void *), int fps) {
    g_frame_env = env;
    g_frame_fn  = fn;
#ifdef __EMSCRIPTEN__
    /* 1 == simulate an infinite loop: this call never returns.  On the web
     * that is correct rather than regrettable -- there is no window-close
     * event, and closing the tab reclaims the GL context, the textures and the
     * heap together, so the teardown it skips has nothing to do.  Passing 0
     * would not preserve main's frame either; both values abandon it. */
    emscripten_set_main_loop(tur_rl_web_frame, fps, 1);
#else
    /* Native: registration is meaningless, and silently doing nothing would
     * make a portable program look like it hung.  raylib/web's Turmeric side
     * never reaches here on a native build -- `run-main-loop` is web-only and
     * says so -- but the TU still has to compile and link for the native arm,
     * because `:c-sources` are compiled unconditionally. */
    (void)fps;
#endif
}

/* Stop the loop.  Only meaningful in callback mode, and only on the web. */
void tur_rl_web_cancel(void) {
#ifdef __EMSCRIPTEN__
    emscripten_cancel_main_loop();
#endif
}
