
var Module;

if (typeof Module === 'undefined') Module = eval('(function() { try { return Module || {} } catch(e) { return {} } })()');

if (!Module.expectedDataFileDownloads) {
  Module.expectedDataFileDownloads = 0;
  Module.finishedDataFileDownloads = 0;
}
Module.expectedDataFileDownloads++;
(function() {
 var loadPackage = function(metadata) {

  var PACKAGE_PATH;
  if (typeof window === 'object') {
    PACKAGE_PATH = window['encodeURIComponent'](window.location.pathname.toString().substring(0, window.location.pathname.toString().lastIndexOf('/')) + '/');
  } else if (typeof location !== 'undefined') {
      // worker
      PACKAGE_PATH = encodeURIComponent(location.pathname.toString().substring(0, location.pathname.toString().lastIndexOf('/')) + '/');
    } else {
      throw 'using preloaded data can only be done on a web page or in a web worker';
    }
    var PACKAGE_NAME = 'game.data';
    var REMOTE_PACKAGE_BASE = 'game.data';
    if (typeof Module['locateFilePackage'] === 'function' && !Module['locateFile']) {
      Module['locateFile'] = Module['locateFilePackage'];
      Module.printErr('warning: you defined Module.locateFilePackage, that has been renamed to Module.locateFile (using your locateFilePackage for now)');
    }
    var REMOTE_PACKAGE_NAME = typeof Module['locateFile'] === 'function' ?
    Module['locateFile'](REMOTE_PACKAGE_BASE) :
    ((Module['filePackagePrefixURL'] || '') + REMOTE_PACKAGE_BASE);

    var REMOTE_PACKAGE_SIZE = metadata.remote_package_size;
    var PACKAGE_UUID = metadata.package_uuid;

    function fetchRemotePackage(packageName, packageSize, callback, errback) {
      var xhr = new XMLHttpRequest();
      xhr.open('GET', packageName, true);
      xhr.responseType = 'arraybuffer';
      xhr.onprogress = function(event) {
        var url = packageName;
        var size = packageSize;
        if (event.total) size = event.total;
        if (event.loaded) {
          if (!xhr.addedTotal) {
            xhr.addedTotal = true;
            if (!Module.dataFileDownloads) Module.dataFileDownloads = {};
            Module.dataFileDownloads[url] = {
              loaded: event.loaded,
              total: size
            };
          } else {
            Module.dataFileDownloads[url].loaded = event.loaded;
          }
          var total = 0;
          var loaded = 0;
          var num = 0;
          for (var download in Module.dataFileDownloads) {
            var data = Module.dataFileDownloads[download];
            total += data.total;
            loaded += data.loaded;
            num++;
          }
          total = Math.ceil(total * Module.expectedDataFileDownloads/num);
          if (Module['setStatus']) Module['setStatus']('Downloading data... (' + loaded + '/' + total + ')');
        } else if (!Module.dataFileDownloads) {
          if (Module['setStatus']) Module['setStatus']('Downloading data...');
        }
      };
      xhr.onerror = function(event) {
        throw new Error("NetworkError for: " + packageName);
      }
      xhr.onload = function(event) {
        if (xhr.status == 200 || xhr.status == 304 || xhr.status == 206 || (xhr.status == 0 && xhr.response)) { // file URLs can return 0
          var packageData = xhr.response;
          callback(packageData);
        } else {
          throw new Error(xhr.statusText + " : " + xhr.responseURL);
        }
      };
      xhr.send(null);
    };

    function handleError(error) {
      console.error('package error:', error);
    };

    function runWithFS() {

      function assert(check, msg) {
        if (!check) throw msg + new Error().stack;
      }
      Module['FS_createPath']('/', 'art', true, true);
      Module['FS_createPath']('/', 'son', true, true);

      function DataRequest(start, end, crunched, audio) {
        this.start = start;
        this.end = end;
        this.crunched = crunched;
        this.audio = audio;
      }
      DataRequest.prototype = {
        requests: {},
        open: function(mode, name) {
          this.name = name;
          this.requests[name] = this;
          Module['addRunDependency']('fp ' + this.name);
        },
        send: function() {},
        onload: function() {
          var byteArray = this.byteArray.subarray(this.start, this.end);

          this.finish(byteArray);

        },
        finish: function(byteArray) {
          var that = this;

        Module['FS_createDataFile'](this.name, null, byteArray, true, true, true); // canOwn this data in the filesystem, it is a slide into the heap that will never change
        Module['removeRunDependency']('fp ' + that.name);

        this.requests[this.name] = null;
      }
    };

    var files = metadata.files;
    for (i = 0; i < files.length; ++i) {
      new DataRequest(files[i].start, files[i].end, files[i].crunched, files[i].audio).open('GET', files[i].filename);
    }


    var indexedDB = window.indexedDB || window.mozIndexedDB || window.webkitIndexedDB || window.msIndexedDB;
    var IDB_RO = "readonly";
    var IDB_RW = "readwrite";
    var DB_NAME = "EM_PRELOAD_CACHE";
    var DB_VERSION = 1;
    var METADATA_STORE_NAME = 'METADATA';
    var PACKAGE_STORE_NAME = 'PACKAGES';
    function openDatabase(callback, errback) {
      try {
        var openRequest = indexedDB.open(DB_NAME, DB_VERSION);
      } catch (e) {
        return errback(e);
      }
      openRequest.onupgradeneeded = function(event) {
        var db = event.target.result;

        if(db.objectStoreNames.contains(PACKAGE_STORE_NAME)) {
          db.deleteObjectStore(PACKAGE_STORE_NAME);
        }
        var packages = db.createObjectStore(PACKAGE_STORE_NAME);

        if(db.objectStoreNames.contains(METADATA_STORE_NAME)) {
          db.deleteObjectStore(METADATA_STORE_NAME);
        }
        var metadata = db.createObjectStore(METADATA_STORE_NAME);
      };
      openRequest.onsuccess = function(event) {
        var db = event.target.result;
        callback(db);
      };
      openRequest.onerror = function(error) {
        errback(error);
      };
    };

    /* Check if there's a cached package, and if so whether it's the latest available */
    function checkCachedPackage(db, packageName, callback, errback) {
      var transaction = db.transaction([METADATA_STORE_NAME], IDB_RO);
      var metadata = transaction.objectStore(METADATA_STORE_NAME);

      var getRequest = metadata.get("metadata/" + packageName);
      getRequest.onsuccess = function(event) {
        var result = event.target.result;
        if (!result) {
          return callback(false);
        } else {
          return callback(PACKAGE_UUID === result.uuid);
        }
      };
      getRequest.onerror = function(error) {
        errback(error);
      };
    };

    function fetchCachedPackage(db, packageName, callback, errback) {
      var transaction = db.transaction([PACKAGE_STORE_NAME], IDB_RO);
      var packages = transaction.objectStore(PACKAGE_STORE_NAME);

      var getRequest = packages.get("package/" + packageName);
      getRequest.onsuccess = function(event) {
        var result = event.target.result;
        callback(result);
      };
      getRequest.onerror = function(error) {
        errback(error);
      };
    };

    function cacheRemotePackage(db, packageName, packageData, packageMeta, callback, errback) {
      var transaction_packages = db.transaction([PACKAGE_STORE_NAME], IDB_RW);
      var packages = transaction_packages.objectStore(PACKAGE_STORE_NAME);

      var putPackageRequest = packages.put(packageData, "package/" + packageName);
      putPackageRequest.onsuccess = function(event) {
        var transaction_metadata = db.transaction([METADATA_STORE_NAME], IDB_RW);
        var metadata = transaction_metadata.objectStore(METADATA_STORE_NAME);
        var putMetadataRequest = metadata.put(packageMeta, "metadata/" + packageName);
        putMetadataRequest.onsuccess = function(event) {
          callback(packageData);
        };
        putMetadataRequest.onerror = function(error) {
          errback(error);
        };
      };
      putPackageRequest.onerror = function(error) {
        errback(error);
      };
    };

    function processPackageData(arrayBuffer) {
      Module.finishedDataFileDownloads++;
      assert(arrayBuffer, 'Loading data file failed.');
      assert(arrayBuffer instanceof ArrayBuffer, 'bad input to processPackageData');
      var byteArray = new Uint8Array(arrayBuffer);
      var curr;

        // copy the entire loaded file into a spot in the heap. Files will refer to slices in that. They cannot be freed though
        // (we may be allocating before malloc is ready, during startup).
        if (Module['SPLIT_MEMORY']) Module.printErr('warning: you should run the file packager with --no-heap-copy when SPLIT_MEMORY is used, otherwise copying into the heap may fail due to the splitting');
        var ptr = Module['getMemory'](byteArray.length);
        Module['HEAPU8'].set(byteArray, ptr);
        DataRequest.prototype.byteArray = Module['HEAPU8'].subarray(ptr, ptr+byteArray.length);

        var files = metadata.files;
        for (i = 0; i < files.length; ++i) {
          DataRequest.prototype.requests[files[i].filename].onload();
        }
        Module['removeRunDependency']('datafile_game.data');

      };
      Module['addRunDependency']('datafile_game.data');

      if (!Module.preloadResults) Module.preloadResults = {};

      function preloadFallback(error) {
        console.error(error);
        console.error('falling back to default preload behavior');
        fetchRemotePackage(REMOTE_PACKAGE_NAME, REMOTE_PACKAGE_SIZE, processPackageData, handleError);
      };

      openDatabase(
        function(db) {
          checkCachedPackage(db, PACKAGE_PATH + PACKAGE_NAME,
            function(useCached) {
              Module.preloadResults[PACKAGE_NAME] = {fromCache: useCached};
              if (useCached) {
                console.info('loading ' + PACKAGE_NAME + ' from cache');
                fetchCachedPackage(db, PACKAGE_PATH + PACKAGE_NAME, processPackageData, preloadFallback);
              } else {
                console.info('loading ' + PACKAGE_NAME + ' from remote');
                fetchRemotePackage(REMOTE_PACKAGE_NAME, REMOTE_PACKAGE_SIZE,
                  function(packageData) {
                    cacheRemotePackage(db, PACKAGE_PATH + PACKAGE_NAME, packageData, {uuid:PACKAGE_UUID}, processPackageData,
                      function(error) {
                        console.error(error);
                        processPackageData(packageData);
                      });
                  }
                  , preloadFallback);
              }
            }
            , preloadFallback);
        }
        , preloadFallback);

      if (Module['setStatus']) Module['setStatus']('Downloading...');

    }
    if (Module['calledRun']) {
      runWithFS();
    } else {
      if (!Module['preRun']) Module['preRun'] = [];
      Module["preRun"].push(runWithFS); // FS is not initialized yet, wait for it
    }

  }
  loadPackage({"package_uuid":"3bd9d94c-cff0-4956-a4cd-c8ad3ce02e78","remote_package_size":769714,"files":[{"filename":"/art/bulletSam.png","crunched":0,"start":0,"end":182,"audio":false},{"filename":"/art/door_boss.png","crunched":0,"start":182,"end":1060,"audio":false},{"filename":"/art/door_closed.png","crunched":0,"start":1060,"end":1849,"audio":false},{"filename":"/art/door_open.png","crunched":0,"start":1849,"end":2245,"audio":false},{"filename":"/art/keyRogue1.png","crunched":0,"start":2245,"end":2370,"audio":false},{"filename":"/art/keyRogue2.png","crunched":0,"start":2370,"end":2496,"audio":false},{"filename":"/art/keyRogue3.png","crunched":0,"start":2496,"end":2621,"audio":false},{"filename":"/art/keyRogue4.png","crunched":0,"start":2621,"end":2746,"audio":false},{"filename":"/art/keyRogue5.png","crunched":0,"start":2746,"end":2871,"audio":false},{"filename":"/art/life.png","crunched":0,"start":2871,"end":2972,"audio":false},{"filename":"/art/mob1.png","crunched":0,"start":2972,"end":3277,"audio":false},{"filename":"/art/mob2.png","crunched":0,"start":3277,"end":3809,"audio":false},{"filename":"/art/mob3.png","crunched":0,"start":3809,"end":4148,"audio":false},{"filename":"/art/mobBoss.png","crunched":0,"start":4148,"end":4962,"audio":false},{"filename":"/art/room.png","crunched":0,"start":4962,"end":35822,"audio":false},{"filename":"/art/sam_1.png","crunched":0,"start":35822,"end":36162,"audio":false},{"filename":"/art/sam_2.png","crunched":0,"start":36162,"end":36499,"audio":false},{"filename":"/art/sam_3.png","crunched":0,"start":36499,"end":36834,"audio":false},{"filename":"/art/sam_4.png","crunched":0,"start":36834,"end":37151,"audio":false},{"filename":"/art/sam_5.png","crunched":0,"start":37151,"end":37489,"audio":false},{"filename":"/art/sam_6.png","crunched":0,"start":37489,"end":37826,"audio":false},{"filename":"/art/sam_7.png","crunched":0,"start":37826,"end":38168,"audio":false},{"filename":"/art/sam_8.png","crunched":0,"start":38168,"end":38485,"audio":false},{"filename":"/art/sam_tete_1.png","crunched":0,"start":38485,"end":38941,"audio":false},{"filename":"/art/sam_tete_2.png","crunched":0,"start":38941,"end":39381,"audio":false},{"filename":"/art/sam_tete_3.png","crunched":0,"start":39381,"end":39865,"audio":false},{"filename":"/art/sam_tete_4.png","crunched":0,"start":39865,"end":40308,"audio":false},{"filename":"/art/skull.png","crunched":0,"start":40308,"end":40709,"audio":false},{"filename":"/art/spider1.png","crunched":0,"start":40709,"end":41064,"audio":false},{"filename":"/art/spider2.png","crunched":0,"start":41064,"end":41435,"audio":false},{"filename":"/art/spider3.png","crunched":0,"start":41435,"end":41788,"audio":false},{"filename":"/art/spider4.png","crunched":0,"start":41788,"end":42144,"audio":false},{"filename":"/art/ui_map_bossroom.png","crunched":0,"start":42144,"end":44997,"audio":false},{"filename":"/art/ui_map_room.png","crunched":0,"start":44997,"end":45175,"audio":false},{"filename":"/main.lua","crunched":0,"start":45175,"end":67784,"audio":false},{"filename":"/son/boss.wav","crunched":0,"start":67784,"end":186898,"audio":true},{"filename":"/son/cle.wav","crunched":0,"start":186898,"end":216930,"audio":true},{"filename":"/son/degat.wav","crunched":0,"start":216930,"end":246962,"audio":true},{"filename":"/son/erreur.wav","crunched":0,"start":246962,"end":269938,"audio":true},{"filename":"/son/impact.wav","crunched":0,"start":269938,"end":276596,"audio":true},{"filename":"/son/menu.wav","crunched":0,"start":276596,"end":281490,"audio":true},{"filename":"/son/mort_ennemi.wav","crunched":0,"start":281490,"end":309758,"audio":true},{"filename":"/son/mort_joueur.wav","crunched":0,"start":309758,"end":398002,"audio":true},{"filename":"/son/pas.wav","crunched":0,"start":398002,"end":402456,"audio":true},{"filename":"/son/porte_fermee.wav","crunched":0,"start":402456,"end":420140,"audio":true},{"filename":"/son/porte_ouverte.wav","crunched":0,"start":420140,"end":468694,"audio":true},{"filename":"/son/ramassage.wav","crunched":0,"start":468694,"end":489906,"audio":true},{"filename":"/son/salle_nettoyee.wav","crunched":0,"start":489906,"end":544634,"audio":true},{"filename":"/son/splash.wav","crunched":0,"start":544634,"end":710930,"audio":true},{"filename":"/son/tir.wav","crunched":0,"start":710930,"end":718470,"audio":true},{"filename":"/son/valider.wav","crunched":0,"start":718470,"end":734390,"audio":true},{"filename":"/son/vie.wav","crunched":0,"start":734390,"end":769714,"audio":true}]});

})();
