const CACHE_NAME = "exact-chinesechess-pwa-v8";
const APP_SHELL = [
  "./",
  "index.html",
  "manifest.webmanifest",
  "icons/icon.svg",
  "icons/icon-192.png",
  "icons/icon-512.png",
  "assets/xiangqi/board.png",
  "assets/xiangqi/b_a.png",
  "assets/xiangqi/b_b.png",
  "assets/xiangqi/b_c.png",
  "assets/xiangqi/b_k.png",
  "assets/xiangqi/b_n.png",
  "assets/xiangqi/b_p.png",
  "assets/xiangqi/b_r.png",
  "assets/xiangqi/r_a.png",
  "assets/xiangqi/r_b.png",
  "assets/xiangqi/r_c.png",
  "assets/xiangqi/r_k.png",
  "assets/xiangqi/r_n.png",
  "assets/xiangqi/r_p.png",
  "assets/xiangqi/r_r.png",
  "engine/fairy-stockfish/engine-worker.js",
  "engine/fairy-stockfish/stockfish.js",
  "engine/fairy-stockfish/stockfish.wasm",
  "engine/fairy-stockfish/stockfish.worker.js",
  "licenses/fairy-stockfish-GPL-3.0.txt"
];

self.addEventListener("install", (event) => {
  event.waitUntil(caches.open(CACHE_NAME).then((cache) => cache.addAll(APP_SHELL)));
  self.skipWaiting();
});

self.addEventListener("activate", (event) => {
  event.waitUntil(
    caches.keys().then((keys) =>
      Promise.all(keys.filter((key) => key !== CACHE_NAME).map((key) => caches.delete(key)))
    )
  );
  self.clients.claim();
});

self.addEventListener("fetch", (event) => {
  if (event.request.method !== "GET") return;
  event.respondWith(
    caches.match(event.request).then((cached) => {
      if (cached) return withIsolationHeaders(cached);
      return fetch(event.request).then((response) => {
        const isolated = withIsolationHeaders(response);
        const copy = isolated.clone();
        caches.open(CACHE_NAME).then((cache) => cache.put(event.request, copy));
        return isolated;
      });
    })
  );
});

function withIsolationHeaders(response) {
  if (!response || response.type === "opaque") return response;
  const headers = new Headers(response.headers);
  headers.set("Cross-Origin-Opener-Policy", "same-origin");
  headers.set("Cross-Origin-Embedder-Policy", "require-corp");
  headers.set("Cross-Origin-Resource-Policy", "same-origin");
  return new Response(response.body, {
    status: response.status,
    statusText: response.statusText,
    headers
  });
}
