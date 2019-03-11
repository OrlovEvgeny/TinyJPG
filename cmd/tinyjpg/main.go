package main

import (
	"context"
	"flag"
	"fmt"
	"github.com/OrlovEvgeny/TinyJPG/compress"
	"github.com/OrlovEvgeny/TinyJPG/core"
	"github.com/OrlovEvgeny/TinyJPG/fswatch"
	"github.com/OrlovEvgeny/TinyJPG/settings"
	"github.com/OrlovEvgeny/logger"
	"github.com/rjeczalik/notify"
	"log"
	"os"
	"regexp"
)

//ldflags override
var (
	version string
	build   string
	commit  string
	docs    string
)

//init
func init() {
	//print helps if not require args
	if len(os.Args) < 2 {
		fmt.Printf("Usage: TinyJPG -options=param\n\n")
		flag.PrintDefaults()
		os.Exit(0)
	}

	if len(os.Args) == 2 && (os.Args[1] == "--version" || os.Args[1] == "-v" || os.Args[1] == "ver") {
		printVersion()
		os.Exit(0)
	}

	flag.BoolVar(&settings.Debug, "debug", false, "example --debug=true")
	flag.StringVar(&settings.SettingFile, "c", "/etc/tinyjpg/config.yml", "example --c=config.yml")
	flag.Parse()

	if err := settings.LoadSettings(settings.SettingFile); err != nil {
		log.Fatal(err)
	}

	//Initial global logger
	settings.Logger = logger.New(&logger.Config{
		AppName: settings.AppName,
		Debug:   settings.Debug,
		LogFile: settings.General.InfoLog,
	})
}

//main
func main() {
	ctx := context.Background()
	ctx = context.WithValue(ctx, core.Log, settings.Logger)

	c := make(chan string, settings.General.WorkerBuffer)
	fchan := make(chan notify.EventInfo, settings.General.EventBuffer)
	done := make(chan struct{}, 1)

	compress.NewImagemagic(ctx).Run(c)

	FSWatcher := &fswatch.FSWatcher{
		FChan: fchan,
		Paths: settings.Compress.Paths,
	}

	regexpTeml := settings.Regexp()
	re, err := regexp.Compile(regexpTeml)
	if err != nil {
		settings.Logger.Printf("Error: There is a problem with your regexp: %s\n", regexpTeml)
		os.Exit(1)
	}

	FSWatcher.FSWatcherStart()
	defer notify.Stop(fchan)

	// Process events
	go func() {
		for {
			select {
			case ev := <-fchan:
				if re.MatchString(ev.Path()) == true {
					c <- ev.Path()
				}
			case <-ctx.Done():
				return
			}
		}
	}()

	<-done
	fmt.Println("exit.")

}

//printVersion program build data
func printVersion() {
	fmt.Print(`
 ________  ______  __    __  __      __           _____  _______    ______  
/        |/      |/  \  /  |/  \    /  |         /     |/       \  /      \ 
$$$$$$$$/ $$$$$$/ $$  \ $$ |$$  \  /$$/          $$$$$ |$$$$$$$  |/$$$$$$  |
   $$ |     $$ |  $$$  \$$ | $$  \/$$/              $$ |$$ |__$$ |$$ | _$$/ 
   $$ |     $$ |  $$$$  $$ |  $$  $$/          __   $$ |$$    $$/ $$ |/    |
   $$ |     $$ |  $$ $$ $$ |   $$$$/          /  |  $$ |$$$$$$$/  $$ |$$$$ |
   $$ |    _$$ |_ $$ |$$$$ |    $$ |          $$ \__$$ |$$ |      $$ \__$$ |
   $$ |   / $$   |$$ | $$$ |    $$ |          $$    $$/ $$ |      $$    $$/ 
   $$/    $$$$$$/ $$/   $$/     $$/            $$$$$$/  $$/        $$$$$$/

`)
	fmt.Printf("Version: %s\nBuild Time: %s\nGit Commit Hash: %s\nDocs: %s\n\n\n", version, build, commit, docs)
}
