
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
      Module['FS_createPath']('/art', 'city', true, true);
      Module['FS_createPath']('/art', 'forest', true, true);
      Module['FS_createPath']('/art', 'ninja', true, true);
      Module['FS_createPath']('/art', 'ranger', true, true);
      Module['FS_createPath']('/art', 'samurai', true, true);
      Module['FS_createPath']('/art', 'valkyrie', true, true);
      Module['FS_createPath']('/', 'musique', true, true);
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
  loadPackage({"package_uuid":"f91a5d01-45e6-4245-909f-e355cf779a03","remote_package_size":9937797,"files":[{"filename":"/art/city/back.png","crunched":0,"start":0,"end":1353,"audio":false},{"filename":"/art/city/front.png","crunched":0,"start":1353,"end":3071,"audio":false},{"filename":"/art/city/ground.png","crunched":0,"start":3071,"end":9411,"audio":false},{"filename":"/art/city/middle.png","crunched":0,"start":9411,"end":10438,"audio":false},{"filename":"/art/forest/back.png","crunched":0,"start":10438,"end":14025,"audio":false},{"filename":"/art/forest/front.png","crunched":0,"start":14025,"end":17014,"audio":false},{"filename":"/art/forest/lights.png","crunched":0,"start":17014,"end":18786,"audio":false},{"filename":"/art/forest/middle.png","crunched":0,"start":18786,"end":21365,"audio":false},{"filename":"/art/ninja/attack_1.png","crunched":0,"start":21365,"end":21836,"audio":false},{"filename":"/art/ninja/attack_2.png","crunched":0,"start":21836,"end":22319,"audio":false},{"filename":"/art/ninja/attack_3.png","crunched":0,"start":22319,"end":22789,"audio":false},{"filename":"/art/ninja/hurt_1.png","crunched":0,"start":22789,"end":23237,"audio":false},{"filename":"/art/ninja/hurt_2.png","crunched":0,"start":23237,"end":23721,"audio":false},{"filename":"/art/ninja/hurt_3.png","crunched":0,"start":23721,"end":24192,"audio":false},{"filename":"/art/ninja/hurt_4.png","crunched":0,"start":24192,"end":24641,"audio":false},{"filename":"/art/ninja/idle_1.png","crunched":0,"start":24641,"end":25122,"audio":false},{"filename":"/art/ninja/idle_2.png","crunched":0,"start":25122,"end":25609,"audio":false},{"filename":"/art/ninja/idle_3.png","crunched":0,"start":25609,"end":26081,"audio":false},{"filename":"/art/ninja/idle_4.png","crunched":0,"start":26081,"end":26563,"audio":false},{"filename":"/art/ninja/jump_1.png","crunched":0,"start":26563,"end":27052,"audio":false},{"filename":"/art/ninja/jump_2.png","crunched":0,"start":27052,"end":27534,"audio":false},{"filename":"/art/ninja/jump_3.png","crunched":0,"start":27534,"end":28008,"audio":false},{"filename":"/art/ninja/jump_4.png","crunched":0,"start":28008,"end":28495,"audio":false},{"filename":"/art/ninja/run_1.png","crunched":0,"start":28495,"end":28967,"audio":false},{"filename":"/art/ninja/run_2.png","crunched":0,"start":28967,"end":29426,"audio":false},{"filename":"/art/ninja/run_3.png","crunched":0,"start":29426,"end":29891,"audio":false},{"filename":"/art/ninja/run_4.png","crunched":0,"start":29891,"end":30371,"audio":false},{"filename":"/art/ninja/run_5.png","crunched":0,"start":30371,"end":30849,"audio":false},{"filename":"/art/ninja/run_6.png","crunched":0,"start":30849,"end":31324,"audio":false},{"filename":"/art/ranger/attack_1.png","crunched":0,"start":31324,"end":31828,"audio":false},{"filename":"/art/ranger/attack_2.png","crunched":0,"start":31828,"end":32342,"audio":false},{"filename":"/art/ranger/attack_3.png","crunched":0,"start":32342,"end":32863,"audio":false},{"filename":"/art/ranger/attack_4.png","crunched":0,"start":32863,"end":33371,"audio":false},{"filename":"/art/ranger/hurt_1.png","crunched":0,"start":33371,"end":33815,"audio":false},{"filename":"/art/ranger/hurt_2.png","crunched":0,"start":33815,"end":34302,"audio":false},{"filename":"/art/ranger/hurt_3.png","crunched":0,"start":34302,"end":34814,"audio":false},{"filename":"/art/ranger/hurt_4.png","crunched":0,"start":34814,"end":35285,"audio":false},{"filename":"/art/ranger/idle_1.png","crunched":0,"start":35285,"end":35783,"audio":false},{"filename":"/art/ranger/idle_2.png","crunched":0,"start":35783,"end":36282,"audio":false},{"filename":"/art/ranger/idle_3.png","crunched":0,"start":36282,"end":36780,"audio":false},{"filename":"/art/ranger/idle_4.png","crunched":0,"start":36780,"end":37278,"audio":false},{"filename":"/art/ranger/jump_1.png","crunched":0,"start":37278,"end":37781,"audio":false},{"filename":"/art/ranger/jump_2.png","crunched":0,"start":37781,"end":38279,"audio":false},{"filename":"/art/ranger/jump_3.png","crunched":0,"start":38279,"end":38761,"audio":false},{"filename":"/art/ranger/jump_4.png","crunched":0,"start":38761,"end":39266,"audio":false},{"filename":"/art/ranger/run_1.png","crunched":0,"start":39266,"end":39760,"audio":false},{"filename":"/art/ranger/run_2.png","crunched":0,"start":39760,"end":40248,"audio":false},{"filename":"/art/ranger/run_3.png","crunched":0,"start":40248,"end":40743,"audio":false},{"filename":"/art/ranger/run_4.png","crunched":0,"start":40743,"end":41240,"audio":false},{"filename":"/art/ranger/run_5.png","crunched":0,"start":41240,"end":41731,"audio":false},{"filename":"/art/ranger/run_6.png","crunched":0,"start":41731,"end":42217,"audio":false},{"filename":"/art/samurai/attack_1.png","crunched":0,"start":42217,"end":42756,"audio":false},{"filename":"/art/samurai/attack_2.png","crunched":0,"start":42756,"end":43286,"audio":false},{"filename":"/art/samurai/attack_3.png","crunched":0,"start":43286,"end":43803,"audio":false},{"filename":"/art/samurai/attack_4.png","crunched":0,"start":43803,"end":44333,"audio":false},{"filename":"/art/samurai/attack_5.png","crunched":0,"start":44333,"end":44868,"audio":false},{"filename":"/art/samurai/attack_6.png","crunched":0,"start":44868,"end":45411,"audio":false},{"filename":"/art/samurai/attack_7.png","crunched":0,"start":45411,"end":45959,"audio":false},{"filename":"/art/samurai/attack_8.png","crunched":0,"start":45959,"end":46508,"audio":false},{"filename":"/art/samurai/hurt_1.png","crunched":0,"start":46508,"end":46952,"audio":false},{"filename":"/art/samurai/hurt_2.png","crunched":0,"start":46952,"end":47492,"audio":false},{"filename":"/art/samurai/hurt_3.png","crunched":0,"start":47492,"end":48033,"audio":false},{"filename":"/art/samurai/hurt_4.png","crunched":0,"start":48033,"end":48539,"audio":false},{"filename":"/art/samurai/idle_1.png","crunched":0,"start":48539,"end":49087,"audio":false},{"filename":"/art/samurai/idle_2.png","crunched":0,"start":49087,"end":49633,"audio":false},{"filename":"/art/samurai/idle_3.png","crunched":0,"start":49633,"end":50180,"audio":false},{"filename":"/art/samurai/idle_4.png","crunched":0,"start":50180,"end":50728,"audio":false},{"filename":"/art/samurai/jump_1.png","crunched":0,"start":50728,"end":51262,"audio":false},{"filename":"/art/samurai/jump_2.png","crunched":0,"start":51262,"end":51793,"audio":false},{"filename":"/art/samurai/jump_3.png","crunched":0,"start":51793,"end":52327,"audio":false},{"filename":"/art/samurai/jump_4.png","crunched":0,"start":52327,"end":52863,"audio":false},{"filename":"/art/samurai/run_1.png","crunched":0,"start":52863,"end":53397,"audio":false},{"filename":"/art/samurai/run_2.png","crunched":0,"start":53397,"end":53925,"audio":false},{"filename":"/art/samurai/run_3.png","crunched":0,"start":53925,"end":54457,"audio":false},{"filename":"/art/samurai/run_4.png","crunched":0,"start":54457,"end":54984,"audio":false},{"filename":"/art/samurai/run_5.png","crunched":0,"start":54984,"end":55508,"audio":false},{"filename":"/art/samurai/run_6.png","crunched":0,"start":55508,"end":56033,"audio":false},{"filename":"/art/valkyrie/attack_1.png","crunched":0,"start":56033,"end":56565,"audio":false},{"filename":"/art/valkyrie/attack_2.png","crunched":0,"start":56565,"end":57109,"audio":false},{"filename":"/art/valkyrie/attack_3.png","crunched":0,"start":57109,"end":57639,"audio":false},{"filename":"/art/valkyrie/hurt_1.png","crunched":0,"start":57639,"end":58083,"audio":false},{"filename":"/art/valkyrie/hurt_2.png","crunched":0,"start":58083,"end":58641,"audio":false},{"filename":"/art/valkyrie/hurt_3.png","crunched":0,"start":58641,"end":59181,"audio":false},{"filename":"/art/valkyrie/hurt_4.png","crunched":0,"start":59181,"end":59685,"audio":false},{"filename":"/art/valkyrie/idle_1.png","crunched":0,"start":59685,"end":60208,"audio":false},{"filename":"/art/valkyrie/idle_2.png","crunched":0,"start":60208,"end":60743,"audio":false},{"filename":"/art/valkyrie/idle_3.png","crunched":0,"start":60743,"end":61279,"audio":false},{"filename":"/art/valkyrie/idle_4.png","crunched":0,"start":61279,"end":61804,"audio":false},{"filename":"/art/valkyrie/jump_1.png","crunched":0,"start":61804,"end":62338,"audio":false},{"filename":"/art/valkyrie/jump_2.png","crunched":0,"start":62338,"end":62888,"audio":false},{"filename":"/art/valkyrie/jump_3.png","crunched":0,"start":62888,"end":63434,"audio":false},{"filename":"/art/valkyrie/jump_4.png","crunched":0,"start":63434,"end":63988,"audio":false},{"filename":"/art/valkyrie/jump_5.png","crunched":0,"start":63988,"end":64493,"audio":false},{"filename":"/art/valkyrie/jump_6.png","crunched":0,"start":64493,"end":65050,"audio":false},{"filename":"/art/valkyrie/run_1.png","crunched":0,"start":65050,"end":65572,"audio":false},{"filename":"/art/valkyrie/run_2.png","crunched":0,"start":65572,"end":66098,"audio":false},{"filename":"/art/valkyrie/run_3.png","crunched":0,"start":66098,"end":66626,"audio":false},{"filename":"/art/valkyrie/run_4.png","crunched":0,"start":66626,"end":67149,"audio":false},{"filename":"/art/valkyrie/run_5.png","crunched":0,"start":67149,"end":67680,"audio":false},{"filename":"/art/valkyrie/run_6.png","crunched":0,"start":67680,"end":68200,"audio":false},{"filename":"/main.lua","crunched":0,"start":68200,"end":88952,"audio":false},{"filename":"/musique/fight1.ogg","crunched":0,"start":88952,"end":2614481,"audio":true},{"filename":"/musique/fight2.ogg","crunched":0,"start":2614481,"end":5839047,"audio":true},{"filename":"/musique/menu.ogg","crunched":0,"start":5839047,"end":9360019,"audio":true},{"filename":"/musique/victory.ogg","crunched":0,"start":9360019,"end":9571195,"audio":true},{"filename":"/son/atterrissage.wav","crunched":0,"start":9571195,"end":9581823,"audio":true},{"filename":"/son/choix.wav","crunched":0,"start":9581823,"end":9601271,"audio":true},{"filename":"/son/cloche.wav","crunched":0,"start":9601271,"end":9680695,"audio":true},{"filename":"/son/curseur.wav","crunched":0,"start":9680695,"end":9685149,"audio":true},{"filename":"/son/fleche.wav","crunched":0,"start":9685149,"end":9696659,"audio":true},{"filename":"/son/garde.wav","crunched":0,"start":9696659,"end":9716107,"audio":true},{"filename":"/son/ko.wav","crunched":0,"start":9716107,"end":9795531,"audio":true},{"filename":"/son/saut.wav","crunched":0,"start":9795531,"end":9807923,"audio":true},{"filename":"/son/seconde.wav","crunched":0,"start":9807923,"end":9812377,"audio":true},{"filename":"/son/touche.wav","crunched":0,"start":9812377,"end":9821241,"audio":true},{"filename":"/son/touche_fort.wav","crunched":0,"start":9821241,"end":9844217,"audio":true},{"filename":"/son/vent.wav","crunched":0,"start":9844217,"end":9853963,"audio":true},{"filename":"/son/victoire.wav","crunched":0,"start":9853963,"end":9937797,"audio":true}]});

})();
