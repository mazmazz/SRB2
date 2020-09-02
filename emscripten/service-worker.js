// https://googlechrome.github.io/samples/service-worker/basic/

/*
 Copyright 2016 Google Inc. All Rights Reserved.
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
 http://www.apache.org/licenses/LICENSE-2.0
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

// Names of the two caches used in this version of the service worker.
// Change to v2, etc. when you update any of the local resources, which will
// in turn trigger the install event again.

// Templated by emscripten-package.py
const SHELL_VERSION = '{{{ SHELL_VERSION }}}';
const PACKAGE_VERSIONS = [ {{{ PACKAGE_VERSIONS }}} ];

const PRECACHE = `_SHELL-${SHELL_VERSION}`;

// A list of local resources we always want to be cached.
const PRECACHE_URLS = [
  'index.html',
  './', // Alias for index.html
  './srb2.webmanifest',
  './version-shell.txt',
  './version-package.txt',
  './assets/background.jpg',
  './assets/srb2logo.png',
  './assets/srb2.png',
  './assets/srb2-144.png',
  './libs/jszip@3.3.0/jszip.min.js',
  './libs/Sortable@1.10.1/Sortable.min.js',
  './libs/idb-keyval@mazmazz_idb-version_c088f87/dist/idb-keyval-iife.min.js',
  './libs/FileSaver@2.0.2/dist/FileSaver.min.js',
  './libs/es6-promise-pool@2.5.0/es6-promise-pool.min.js'
];

// The install handler takes care of precaching the resources we always need.
self.addEventListener('install', event => {
  event.waitUntil(
    caches.open(PRECACHE)
      .then(cache => cache.addAll(PRECACHE_URLS))
      .then(self.skipWaiting())
  );
});

// The activate handler takes care of cleaning up old caches.
self.addEventListener('activate', event => {
  let currentCaches = [PRECACHE, ...PACKAGE_VERSIONS];
  event.waitUntil(
    caches.keys().then(cacheNames => {
      return cacheNames.filter(cacheName => !currentCaches.includes(cacheName));
    }).then(cachesToDelete => {
      return Promise.all(cachesToDelete.map(cacheToDelete => {
        return caches.delete(cacheToDelete);
      }));
    }).then(() => self.clients.claim())
  );
});

// The fetch handler serves responses for same-origin resources from a cache.
// If no response is found, it populates the runtime cache with the response
// from the network before returning it to the page.
self.addEventListener('fetch', async (event) => {
  const url = new URL(event.request.url);

  if (url.origin == location.origin) {
    // TODO: Enable cache-first for speed increase, except for MD5
    event.respondWith(networkOrCache(event.request));
  }
});

function networkOrCache(request) {
  return fetch(request).then(function (response) {
    if (response.ok) {
      if (request.url.includes('data/')) {
        //* If retrieving program JS, store in version cache.
        //* We do this here because program JS is retrieved from a script tag href, not from code.
        if(request.url.includes('srb2.js'))
          storeVersionCache(request, response.clone());

        //* If retrieving MD5, store in version cache.
        if (request.url.includes('.md5'))
          storeVersionCache(request, response.clone());
      }

      // Server-first response
      return response;
    }
    else
      // Cache-second response
      return fromCache(request)
        .catch(function () {
          // if cache does not exist, pass-thru fetch's original response
          return response;
        });
  })
  .catch(function () {
    // Cache-second response (from fetch error)
    return fromCache(request)
      .catch(function () {
        // Return our own response, since fetch errored
        return useFallback();
      });
  });
}

function fromCache(request) {
  return caches.match(request).then(function (matching) {
    return matching || Promise.reject('request-not-in-cache');
  });
}

function useFallback() {
  return Promise.resolve(new Response(null, { 
    'status': 404
  }));
}

function storeVersionCache(request, response) {
  // Get version string from URL ("landing/data/{VERSION}/srb2.js")
  var loc = request.url;
  var path = loc.substring(0, loc.lastIndexOf("/"));
  var cacheName = path.substring(path.lastIndexOf("/")+1);

  caches.open(cacheName).then(function (cache) {
    cache.put(request, response);
  });
}
