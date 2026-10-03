#!/usr/bin/env python3
"""
tools/sitechrome.py -- the one answer to "what is on this site", for the
spices subdomain.

This is a port of the canonical chrome block in the turmeric repo's
`tools/genguides.py`, re-rooted onto the main host.  All three generators here
(`gendocs.py`, `genguides.py`, `genspices.py`) used to carry their own
hand-written copy of the topbar and the sidebar globals, which is how
spices.turmeric-lang.com ended up with a four-link topbar, no GitHub / Try it
buttons and a monospace sidebar while every other turmeric-lang.com surface had
moved on.  One module, three consumers, no copies.

Two rules keep this aligned with the main site:

  * NAV_LINKS and SIDEBAR_GROUPS mirror the lists of the same name in
    turmeric's `tools/genguides.py`.  Everything site-relative there is
    site-relative here too and gets re-rooted onto MAIN_SITE at render time,
    except the Spices entry, which is this site.
  * The markup `build_page_header` and `build_sidebar` emit is the markup
    turmeric's generators emit, class for class -- `nav.nav-links`, the
    `.nav-right` button cluster, `a.sidebar-back`, `.sidebar-uplinks`.  The
    shared stylesheet (web/styles/style.css) is keyed on those class names, so
    emitting anything else is what silently drops the styling.
"""

import html as _html
import re

# Where the rest of the site lives.  Every site-relative href below is rendered
# against this; the spices site is a subdomain, so "/" alone would point back
# into the spice tree.
MAIN_SITE = 'https://turmeric-lang.com'
SPICES_SITE = 'https://spices.turmeric-lang.com'

GITHUB_URL = 'https://github.com/turmeric-lang/turmeric-spices'

# Native `title` tooltips for the site chrome.  Keyed by site-relative href;
# `apply_link_titles` strips the MAIN_SITE prefix before the lookup so a link
# already written absolute resolves to the same entry.  Mirrors LINK_TITLES in
# turmeric's tools/genguides.py.
LINK_TITLES = {
    '/':                                  'Turmeric home',
    '/tour':                              'A guided tour of the language in fourteen stops',
    '/try':                               'Run Turmeric in your browser -- nothing to install',
    '/trowel':                            'Trowel -- the native Turmeric editor for macOS and Linux',
    '/docs/html/guides/':                 'Guides and tutorials, from quickstart to compiler internals',
    '/docs/html/api/':                    'Generated API reference for the standard library',
    '/roadmap':                           'Planned features, work in progress, and recent milestones',
    '/ci':                                'Build and test metrics from continuous integration',
    'https://c.turmeric-lang.com':        'A C interpreter running in your browser',
    SPICES_SITE:                          'Browse Spice packages -- the Turmeric package registry',
    GITHUB_URL:                           'Turmeric Spices source code on GitHub',
}

# Topbar links, in order -- the same six the main site shows, so the bar is the
# same width of content here as it is there.  `active` is matched by label.
NAV_LINKS = [
    ('/tour',          'Tour'),
    ('/try',           'Try It'),
    ('/docs/html/guides/', 'Guides'),
    ('/docs/html/api/',    'API Docs'),
    (SPICES_SITE,      'Spices'),
    ('/trowel',        'Trowel'),
]

# Sidebar link groups, in order.  The main site's three groups, plus a Spices
# group for the two places that only exist on this subdomain.
SIDEBAR_GROUPS = [
    ('Spices', [
        (SPICES_SITE + '/',        'Index'),
        (SPICES_SITE + '/guides/', 'Guides'),
    ]),
    ('Language', [
        ('/tour',   'Tour'),
        ('/trowel', 'Trowel'),
        ('/try',    'Try It'),
    ]),
    ('Ecosystem', [
        ('/docs/html/guides/',          'Guides'),
        ('/docs/html/api/',             'API Docs'),
        ('https://c.turmeric-lang.com', 'C Interpreter'),
    ]),
    ('Community', [
        (GITHUB_URL, 'GitHub'),
    ]),
]

LOCAL_TITLES = {
    SPICES_SITE + '/':        'Every first-party spice, with tier and C dependency',
    SPICES_SITE + '/guides/': 'Tutorials and how-tos for the spices',
}


def href(path: str) -> str:
    """Re-root a site-relative href onto the main host.

    Anything already absolute (the spices subdomain, GitHub, the C
    interpreter) is left exactly as written.
    """
    if not path.startswith('/'):
        return path
    return MAIN_SITE + path


_A_TAG_RE = re.compile(r'<a\s+([^>]*)href="([^"]+)"([^>]*)>')


def apply_link_titles(html: str, extra: dict | None = None) -> str:
    """Add a native `title` tooltip to every chrome link with a known href.

    Only used on the header/sidebar chrome, never on article bodies: a tooltip
    belongs on a navigation target, not on every prose link that happens to
    point at the same page.  Links already carrying a title, and hrefs absent
    from the table, are left alone.
    """
    table = {**LINK_TITLES, **LOCAL_TITLES, **(extra or {})}

    def repl(m):
        before, url, after = m.group(1), m.group(2), m.group(3)
        if 'title=' in before or 'title=' in after:
            return m.group(0)
        key = url.replace(MAIN_SITE, '') or '/'
        title = table.get(url) or table.get(key)
        if not title:
            return m.group(0)
        return f'<a {before}href="{url}"{after} title="{_html.escape(title, quote=True)}">'

    return _A_TAG_RE.sub(repl, html)


def build_page_header(active: str = '', search: str = '', indent: str = '  ') -> str:
    """Render the topbar -- identical on every page but for the `active` mark.

    `search` is the aria-label for the filter box on the pages that have one
    (the API indexes); pages without a filter simply omit it.
    """
    parts = []
    for path, label in NAV_LINKS:
        cls = ' class="active"' if label == active else ''
        parts.append(f'<a href="{href(path)}"{cls}>{label}</a>')
    links = ''.join(parts)
    search_html = (
        f'\n{indent}  <div class="search-wrap">'
        f'<input class="search-input" type="search" placeholder="Filter... (/)" '
        f'aria-label="{search}"></div>'
        if search else ''
    )
    return apply_link_titles(f'''\
{indent}<header class="site-header">
{indent}  <button class="hamburger" aria-label="Toggle navigation" aria-expanded="false">
{indent}    <span></span><span></span><span></span>
{indent}  </button>
{indent}  <a class="nav-logo" href="{href('/')}">
{indent}    <img src="/logo-icon.svg" width="28" height="28" alt="">
{indent}    <img src="/logo.svg" width="101" height="28" alt="Turmeric">
{indent}  </a>
{indent}  <nav class="nav-links">{links}</nav>{search_html}
{indent}  <div class="nav-right">
{indent}    <a href="{GITHUB_URL}" class="btn-ghost">GitHub</a>
{indent}    <a href="{href('/try')}" class="btn-gold">Try it</a>
{indent}  </div>
{indent}</header>''')


def build_sidebar_globals(indent: str = '      ') -> str:
    """Render SIDEBAR_GROUPS -- the block every sidebar ends with.

    Emitted after the page's own back link and table of contents, separated
    from them by the one `sidebar-divider` on the page.
    """
    out = [f'{indent}<hr class="sidebar-divider">']
    for heading, links in SIDEBAR_GROUPS:
        out.append(f'{indent}<h3>{heading}</h3>')
        out.append(f'{indent}<ul>')
        for path, label in links:
            out.append(f'{indent}  <li><a href="{href(path)}">{label}</a></li>')
        out.append(f'{indent}</ul>')
    return apply_link_titles('\n'.join(out))


def build_sidebar(toc: str = '', uplinks: list | None = None,
                  indent: str = '      ', extra_titles: dict | None = None) -> str:
    """Assemble a sidebar in the site's one shape.

    Home link, then the page's own "up" links (All Spices, API reference, ...),
    then its table of contents, then the divider and the global groups.  Every
    page on every turmeric surface reads top-to-bottom in that order, so the
    global links sit in the same place whether you arrived at a guide, a
    stdlib module, or a spice.
    """
    out = [f'{indent}<a class="sidebar-back" href="{href("/")}">Home</a>']
    if uplinks:
        items = ''.join(f'<a href="{u}">{label}</a>' for u, label in uplinks)
        out.append(f'{indent}<div class="sidebar-uplinks">{items}</div>')
    if toc:
        out.append(toc.rstrip())
    out.append(build_sidebar_globals(indent))
    return apply_link_titles('\n'.join(out), extra=extra_titles)


SIDEBAR_GLOBALS = build_sidebar_globals()
