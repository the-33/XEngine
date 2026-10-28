/* One light/dark preference for the XEngine portal and both Doxygen profiles.
   Doxygen owns the toggle UI; this adapter synchronizes its own cookies with
   the portal's persistent setting. It never changes the profiling palette. */
(function () {
  'use strict';
  const storageKey = 'xengine-doc-theme';
  const root = document.documentElement;

  function getSaved() {
    try {
      const value = localStorage.getItem(storageKey);
      return value === 'light' || value === 'dark' ? value : null;
    } catch (_) { return null; }
  }

  function remember(value) {
    try { localStorage.setItem(storageKey, value); } catch (_) { /* file:// privacy settings */ }
  }

  function current() { return root.classList.contains('dark-mode') ? 'dark' : 'light'; }

  function incoming() {
    const value = new URLSearchParams(window.location.search).get('xengine-doc-theme');
    return value === 'light' || value === 'dark' ? value : null;
  }

  function shareInUrl(value) {
    // file:// documents may not share localStorage in all browsers. Retain
    // the choice in the current address so refresh and subsequent links work.
    const url = new URL(window.location.href);
    if (url.searchParams.get('xengine-doc-theme') === value) return;
    url.searchParams.set('xengine-doc-theme', value);
    try { history.replaceState(history.state, '', url.href); } catch (_) { /* file:// policy */ }
  }

  function select(value) {
    if (value !== 'light' && value !== 'dark') return;
    if (typeof DarkModeToggle !== 'undefined') {
      // Go through Doxygen's public toggle setter so its native cookie and
      // toggle state agree with the shared setting on later page navigations.
      DarkModeToggle.userPreference = value === 'dark';
    } else {
      root.classList.toggle('dark-mode', value === 'dark');
      root.classList.toggle('light-mode', value === 'light');
    }
    remember(value);
    shareInUrl(value);
  }

  // This script is appended after Doxygen's darkmode_toggle.js. Applying the
  // shared selection *before* observing prevents an old Doxygen cookie from
  // overwriting a newer choice made in the documentation portal.
  const saved = incoming() || getSaved();
  if (saved) select(saved);
  else remember(current());

  const observer = new MutationObserver(function (changes) {
    if (changes.some(change => change.attributeName === 'class')) {
      const value = current();
      if (getSaved() !== value) remember(value);
      shareInUrl(value);
    }
  });
  observer.observe(root, {attributes: true, attributeFilter: ['class']});

  window.addEventListener('storage', function (event) {
    if (event.key === storageKey && (event.newValue === 'light' || event.newValue === 'dark')) {
      if (current() !== event.newValue) select(event.newValue);
    }
  });

  // Decorate internal HTML destinations at click time, including dynamically
  // inserted Doxygen tree/search links. This passes the theme even when the
  // destination file is a separate file:// storage scope. Crucially, Doxygen
  // reads fragment-only href attributes in its TOC click handler and passes
  // them to querySelector(). Rewriting `#member` into a full file:// URL
  // breaks navigation (and can abort its own JavaScript event handler).
  document.addEventListener('click', function (event) {
    const link = event.target instanceof Element && event.target.closest('a[href]');
    if (!link) return;
    const href = link.getAttribute('href');
    if (!href || href.startsWith('#')) return;
    const target = new URL(link.href, window.location.href);
    const rootUrl = new URL('../../', window.location.href);
    if (target.protocol !== rootUrl.protocol ||
        !target.pathname.startsWith(rootUrl.pathname) ||
        !target.pathname.endsWith('.html')) return;
    // Preserve relative hrefs and same-document anchors for Doxygen's native
    // tree/outline JS. Theme propagation is only needed between HTML pages.
    const currentPage = new URL(window.location.href);
    if (target.pathname === currentPage.pathname &&
        target.search === currentPage.search) return;
    target.searchParams.set('xengine-doc-theme', current());
    link.href = target.href;
  }, true);

  // A cached/missing navtreeindexN.js can leave Doxygen's left panel blank
  // even though all page HTML exists. The cache fingerprint normally prevents
  // this; retain a simple, accessible page-tree fallback if native navigation
  // still has not rendered after window load. Never replace a working tree.
  window.addEventListener('load', function () {
    setTimeout(function () {
      const container = document.getElementById('nav-tree-contents');
      if (!container || container.querySelector('.item') ||
          typeof NAVTREE === 'undefined' || !Array.isArray(NAVTREE)) return;

      const list = document.createElement('ul');
      list.className = 'xengine-fallback-tree';
      function render(entry, parent, depth) {
        if (!Array.isArray(entry) || depth > 6) return;
        const [title, href, children] = entry;
        const li = document.createElement('li');
        const label = document.createElement(href ? 'a' : 'span');
        label.textContent = title;
        if (href) label.setAttribute('href', href);
        if (Array.isArray(children) && children.length) {
          const details = document.createElement('details');
          details.open = depth < 2;
          const summary = document.createElement('summary');
          summary.appendChild(label);
          details.appendChild(summary);
          const nested = document.createElement('ul');
          children.forEach(child => render(child, nested, depth + 1));
          details.appendChild(nested);
          li.appendChild(details);
        } else {
          li.appendChild(label);
        }
        parent.appendChild(li);
      }
      NAVTREE.forEach(entry => render(entry, list, 0));
      container.appendChild(list);
      console.warn('XEngine: restored navigation from Doxygen NAVTREE after native tree initialization failed.');
    }, 1500);
  });
}());
