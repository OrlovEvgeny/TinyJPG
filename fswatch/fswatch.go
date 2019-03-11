package fswatch

import (
	"github.com/rjeczalik/notify"
	"log"
)

//Watch
type FSWatcher struct {
	FChan chan notify.EventInfo
	Paths []string
}

//watcherStart
func (w *FSWatcher) FSWatcherStart() {
	//
	for _, path := range w.Paths {
		go watcherInit(w.FChan, path)
	}
}

//watcherStop
func (w *FSWatcher) FSWatcherStop() {
	notify.Stop(w.FChan)
}

//watcherRestart
func (w *FSWatcher) FSWatcherRestart() {
	w.FSWatcherStop()
	w.FSWatcherStart()
}

//watcherInit
func watcherInit(ec chan notify.EventInfo, path string) {
	if err := notify.Watch(path+"/...", ec, notify.Create); err != nil {
		log.Fatal(err)
	}
}
