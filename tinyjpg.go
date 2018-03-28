package main

import (
	. "github.com/OrlovEvgeny/TinyJPG/config"
	"flag"
	"fmt"
	"github.com/rjeczalik/notify"
	"log"
	"os"
	"regexp"
	"strings"
)

var (
	version string
	build   string
	commit  string
	docs    string

	LogErrChannel  = make(chan string, 300)
	LogInfoChannel = make(chan string, 300)

	configPath = flag.String("config", "", "config file path")
)

//
func InitProcess() {

	c := make(chan string, Config.General.WorkerBuffer)
	ec := make(chan notify.EventInfo, Config.General.EventBuffer)

	workerWatch := &Watch{
		EC:    ec,
		Paths: Config.Compress.Paths,
	}

	workerLog := &LoggerWorker{
		ErrorLog: Config.General.ErrorLog,
		InfoLog:  Config.General.InfoLog,
	}

	go workerLog.processErr()
	go workerLog.processInfo()

	re, err := regexp.Compile(getRegxp())
	if err != nil {
		msg := fmt.Sprintf("Error: There is a problem with your regexp: %s\n", getRegxp())
		LogErrChannel <- msg
		log.Println(msg)

		os.Exit(1)
	}

	for i := 1; i <= Config.General.Worker; i++ {
		worker := &Worker{id: i}
		go worker.process(c)
	}

	workerWatch.watcherStart()
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

}

//
func getRegxp() string {
	if len(Config.Compress.Prefix) > 0 && Config.Compress.Prefix[0] != "*" {
		prefix := fmt.Sprintf(`(%s).*.(JPG|jpeg|JPEG|jpg|png|PNG)$`, strings.Join(Config.Compress.Prefix, "|"))
		return prefix
	}
	return `^.*.(JPG|jpeg|JPEG|jpg|png|PNG)$`
}

//
func main() {

	if len(os.Args) == 2 && (os.Args[1] == "--version" || os.Args[1] == "-v" || os.Args[1] == "ver") {
		PrintVersion()
		os.Exit(0)
	}

	flag.StringVar(&Config.Compress.Path, "path", "/home/www", "uploads folder path, default - /home/www")
	flag.IntVar(&Config.General.Worker, "worker", 5, "maximum amount workers")
	flag.IntVar(&Config.General.WorkerBuffer, "worker_buffer", 500, "maximum buffer queue workers")
	flag.IntVar(&Config.Compress.Quality, "quality", 82, "image quality level in percentage")
	flag.IntVar(&Config.General.EventBuffer, "event_buffer", 300, "buffer an event reported")

	flag.Parse()

	//print helps if not require args
	if len(os.Args) < 2 {
		fmt.Printf("Usage: TinyJPG -options=param\n\n")
		flag.PrintDefaults()
		os.Exit(0)
	}

	PrintVersion()

	//Config file load
	if *configPath != "" {
		LoadConfig(true, *configPath)
	} else {
		Config.Compress.Paths = append(Config.Compress.Paths, Config.Compress.Path)
	}

	InitProcess()

	done := make(chan bool, 1)
	<-done

	LogInfoChannel <- fmt.Sprintf("tinyjpg-watcher stoped")
	fmt.Println("tinyjpg-watcher stoped")
	os.Exit(0)
}
