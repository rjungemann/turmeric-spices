/* watch/platform.h -- the platform-specific headers behind one #include.
 *
 * This exists because tur's `__tur_include__` hoist sorts its payloads into
 * two buckets and emits every #include ahead of every other directive. A
 * guarded block written as
 *
 *     __tur_include__: #if defined(__linux__)
 *     __tur_include__: #include <sys/inotify.h>
 *     __tur_include__: #endif
 *     __tur_include__: #if defined(__APPLE__)
 *     __tur_include__: #include <sys/event.h>
 *     __tur_include__: #endif
 *
 * therefore reaches the generated C as all five includes first, followed by
 * two empty #if/#endif pairs -- so <sys/event.h> was included on Linux and
 * every build of this spice died with "sys/event.h: No such file or
 * directory". Putting the guards inside a real header keeps them attached to
 * what they guard, and backend.tur hoists this single include instead.
 *
 * struct kevent and the inotify constants are needed at file scope: several
 * functions in backend.tur use them, so these cannot move into one body.
 */
#ifndef TUR_WATCH_PLATFORM_H
#define TUR_WATCH_PLATFORM_H

#if defined(__linux__)
#  include <poll.h>
#  include <sys/inotify.h>
#elif defined(__APPLE__)
#  include <sys/event.h>
#  include <sys/time.h>
#endif

#endif /* TUR_WATCH_PLATFORM_H */
