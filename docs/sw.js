// Service worker del Logger BLE — hace la app instalable y usable sin red.
// Estrategia: red primero (siempre fresca cuando hay internet) y, si la red
// falla, lo último que se vio (la app sigue abriendo sin conexión).
const CACHE = 'logger-ble-v1';
const BASE = [
  './',
  './manifest.webmanifest',
  './icons/icono-192.png',
  './icons/icono-512.png',
];

self.addEventListener('install', e => {
  e.waitUntil(
    caches.open(CACHE).then(c => c.addAll(BASE)).then(() => self.skipWaiting())
  );
});

self.addEventListener('activate', e => {
  e.waitUntil(
    caches.keys()
      .then(ks => Promise.all(ks.filter(k => k !== CACHE).map(k => caches.delete(k))))
      .then(() => self.clients.claim())
  );
});

self.addEventListener('fetch', e => {
  if (e.request.method !== 'GET') return;
  e.respondWith(
    fetch(e.request)
      .then(r => {
        const copia = r.clone();
        caches.open(CACHE).then(c => c.put(e.request, copia));
        return r;
      })
      .catch(() => caches.match(e.request, { ignoreSearch: true }))
  );
});
