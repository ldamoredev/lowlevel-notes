(function () {
  const root = document.body.dataset.root || ".";
  const localeRoot = document.body.dataset.localeRoot || ".";
  const lang = document.documentElement.lang || "en";
  const strings = {
    en: { noMatches: "No matches for", result: "result", results: "results", hint: "↑↓ navigate · Enter open" },
    es: { noMatches: "Sin coincidencias para", result: "resultado", results: "resultados", hint: "↑↓ navegar · Enter abrir" },
  };
  const s = strings[lang] || strings.en;
  const input = document.getElementById("search");
  const results = document.getElementById("search-results");
  const themeToggle = document.getElementById("theme-toggle");
  const sidebarToggle = document.getElementById("sidebar-toggle");
  const sidebar = document.querySelector(".sidebar");

  if (themeToggle) {
    const syncToggle = () => {
      themeToggle.setAttribute("aria-pressed", document.documentElement.getAttribute("data-theme") === "dark" ? "true" : "false");
    };
    syncToggle();
    themeToggle.addEventListener("click", () => {
      const next = document.documentElement.getAttribute("data-theme") === "dark" ? "light" : "dark";
      document.documentElement.setAttribute("data-theme", next);
      try { localStorage.setItem("theme", next); } catch (e) {}
      syncToggle();
    });
  }

  document.querySelectorAll(".lang-link").forEach((link) => {
    link.addEventListener("click", () => {
      try { localStorage.setItem("preferred-locale", link.getAttribute("hreflang") || "en"); } catch (e) {}
    });
  });

  if (sidebarToggle) {
    sidebarToggle.addEventListener("click", () => {
      document.body.classList.toggle("nav-open");
    });
  }

  document.querySelectorAll(".sidebar a").forEach((link) => {
    link.addEventListener("click", () => {
      if (window.matchMedia("(max-width: 780px)").matches) {
        document.body.classList.remove("nav-open");
      }
    });
  });

  const activeLink = document.querySelector(".sidebar .active");
  if (activeLink) {
    const rect = activeLink.getBoundingClientRect();
    if (rect.top < 80 || rect.bottom > window.innerHeight - 48) {
      activeLink.scrollIntoView({ block: "center" });
    }
  }

  let index = null;
  let currentHits = [];
  let activeIndex = -1;
  let debounce = null;

  async function loadIndex() {
    if (index) return index;
    const response = await fetch(localeRoot + "/search.json");
    index = await response.json();
    return index;
  }

  function score(entry, terms) {
    const haystack = [
      entry.title,
      entry.description,
      entry.branch,
      entry.group,
      entry.kind,
      (entry.tags || []).join(" "),
      (entry.keywords || []).join(" "),
      entry.text,
    ].join(" ").toLowerCase();
    let value = 0;
    for (const term of terms) {
      if (!term) continue;
      const title = String(entry.title || "").toLowerCase();
      const tags = String((entry.tags || []).join(" ")).toLowerCase();
      if (!haystack.includes(term)) return 0;
      if (title.includes(term)) value += 12;
      if (tags.includes(term)) value += 7;
      if (String(entry.branch || "").toLowerCase().includes(term)) value += 5;
      value += haystack.split(term).length - 1;
    }
    return value;
  }

  function runSearchSoon() {
    clearTimeout(debounce);
    debounce = setTimeout(runSearch, 100);
  }

  async function runSearch() {
    const query = input.value.trim().toLowerCase();
    if (!query) {
      results.hidden = true;
      results.innerHTML = "";
      currentHits = [];
      activeIndex = -1;
      return;
    }
    const terms = query.split(/\s+/).filter(Boolean);
    const idx = await loadIndex();
    currentHits = idx
      .map((entry) => ({ entry, score: score(entry, terms) }))
      .filter((hit) => hit.score > 0)
      .sort((a, b) => b.score - a.score)
      .slice(0, 18);
    activeIndex = currentHits.length ? 0 : -1;
    if (!currentHits.length) {
      results.innerHTML = '<div class="empty">' + s.noMatches + ' "' + escapeHtml(query) + '"</div>';
    } else {
      const label = currentHits.length === 1 ? s.result : s.results;
      results.innerHTML = '<div class="results-meta">' + currentHits.length + " " + label + " · " + s.hint + "</div>" +
        currentHits.map((hit, i) => {
          const entry = hit.entry;
          return '<a class="hit' + (i === 0 ? " active" : "") + '" href="' + localeRoot + "/" + entry.url + '">' +
            '<strong>' + highlight(entry.title, terms) + '</strong>' +
            '<span>' + escapeHtml(entry.branch) + ' · ' + escapeHtml(entry.kind) + '</span>' +
            '<p>' + highlight(entry.description || "", terms) + '</p>' +
          '</a>';
        }).join("");
    }
    results.hidden = false;
  }

  function updateActive() {
    const nodes = results.querySelectorAll(".hit");
    nodes.forEach((node, i) => node.classList.toggle("active", i === activeIndex));
    if (nodes[activeIndex]) nodes[activeIndex].scrollIntoView({ block: "nearest" });
  }

  function escapeHtml(value) {
    return String(value).replace(/[&<>"']/g, (char) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[char]));
  }

  function highlight(value, terms) {
    let safe = escapeHtml(value);
    const pattern = terms.map((term) => term.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")).join("|");
    return pattern ? safe.replace(new RegExp("(" + pattern + ")", "gi"), "<mark>$1</mark>") : safe;
  }

  if (!input || !results) return;

  input.addEventListener("focus", () => { loadIndex().catch(() => {}); });
  input.addEventListener("input", runSearchSoon);
  input.addEventListener("keydown", (event) => {
    if (results.hidden || !currentHits.length) return;
    if (event.key === "ArrowDown") {
      event.preventDefault();
      activeIndex = (activeIndex + 1) % currentHits.length;
      updateActive();
    } else if (event.key === "ArrowUp") {
      event.preventDefault();
      activeIndex = (activeIndex - 1 + currentHits.length) % currentHits.length;
      updateActive();
    } else if (event.key === "Enter" && activeIndex >= 0) {
      const nodes = results.querySelectorAll(".hit");
      if (nodes[activeIndex]) {
        event.preventDefault();
        window.location.href = nodes[activeIndex].href;
      }
    }
  });

  document.addEventListener("keydown", (event) => {
    const cmdK = (event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "k";
    if ((event.key === "/" || cmdK) && document.activeElement !== input) {
      event.preventDefault();
      input.focus();
    }
    if (event.key === "Escape") {
      results.hidden = true;
      input.blur();
      document.body.classList.remove("nav-open");
    }
  });

  document.addEventListener("click", (event) => {
    const toggleHit = sidebarToggle && sidebarToggle.contains(event.target);
    const sidebarHit = sidebar && sidebar.contains(event.target);
    if (document.body.classList.contains("nav-open") && !toggleHit && !sidebarHit) {
      document.body.classList.remove("nav-open");
    }
    if (event.target === input || results.contains(event.target) || toggleHit) return;
    results.hidden = true;
  });
})();
