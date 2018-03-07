package main

import (
	"flag"
	"fmt"
	"github.com/rjeczalik/notify"
	"log"
	"os"
	"regexp"
)

var rootPath = flag.String("path", "/home/www", "uploads folder path, default - /home/www")
var verbose = flag.Bool("verbose", true, "verbose log")
var maxWorker = flag.Int("worker", 5, "maximum amount workers")

func main() {
	flag.Parse()

	re, err := regexp.Compile(`^.*.(JPG|jpeg|JPEG|jpg)$`)
	if err != nil {
		fmt.Printf("There is a problem with your regexp.\n")
		os.Exit(1)
	}

	c := make(chan string, 500)
	ec := make(chan notify.EventInfo, 1)
	done := make(chan bool)

	for i := 1; i <= *maxWorker; i++ {
		worker := &Worker{id: i}
		go worker.process(c)
	}

	if err := notify.Watch(*rootPath+"/...", ec, notify.Create); err != nil {
		log.Fatal(err)
	}
	defer notify.Stop(ec)

	// Process events
	go func() {
		for {
			select {
			case ev := <-ec:
				if re.MatchString(ev.Path()) == true {
					c <- ev.Path()
				}
			}
		}
	}()

	<-done
}
