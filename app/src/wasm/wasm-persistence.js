// Browser persistence for the Qt/C++ application filesystem.
// The desktop build never includes this file.
Module.preRun = Module.preRun || [];
Module.preRun.push(function () {
    const home = "/home/web_user";
    FS.mkdirTree(home);
    FS.mount(IDBFS, {}, home);

    // Load existing projects before QApplication and the projects hub start.
    addRunDependency("catchim-idbfs-initial-sync");
    FS.syncfs(true, function (error) {
        if (error) console.error("[Catchim] Could not restore browser data:", error);
        removeRunDependency("catchim-idbfs-initial-sync");
    });
});

// Called by ProjectStorage after an atomic save/delete. Keep writes serialized
// so a later sync cannot race an earlier IndexedDB transaction.
Module.catchimPersistNow = function () {
    if (Module.__catchimSyncing) {
        Module.__catchimSyncPending = true;
        return;
    }
    Module.__catchimSyncing = true;
    FS.syncfs(false, function (error) {
        Module.__catchimSyncing = false;
        if (error) console.error("[Catchim] Could not persist browser data:", error);
        if (Module.__catchimSyncPending) {
            Module.__catchimSyncPending = false;
            Module.catchimPersistNow();
        }
    });
};

// A best-effort final flush for edits made shortly before a tab is closed.
if (typeof window !== "undefined") {
    window.addEventListener("pagehide", function () {
        if (Module.catchimPersistNow) Module.catchimPersistNow();
    });
}
