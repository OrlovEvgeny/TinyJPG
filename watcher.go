package main

import (
	"github.com/rjeczalik/notify"
	"log"
)

//
type Watch struct {
	EC    chan notify.EventInfo
	Paths []string
}

//
func (w *Watch) watcherStart() {
	//
	for _, path := range w.Paths {
		go watcherInit(w.EC, path)
	}
}

//
func (w *Watch) watcherStop() {
	notify.Stop(w.EC)
}

//
func (w *Watch) watcherRestart() {
	w.watcherStop()
	w.watcherStart()
}

//
func watcherInit(ec chan notify.EventInfo, path string) {
	if err := notify.Watch(path+"/...", ec, notify.Create); err != nil {
		log.Fatal(err)
	}
}
