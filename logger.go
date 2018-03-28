package main

import (
	. "github.com/OrlovEvgeny/TinyJPG/config"
	"fmt"
	"log"
	"os"
)

//
type LogVerbose struct {
	ID         int
	ImagePath  string
	beforeSize int64
	afterSize  int64
}

//
type LoggerWorker struct {
	ErrorLog string
	InfoLog  string
}

// process info-log writer
func (lw *LoggerWorker) processInfo() {
	logfile, err := os.OpenFile(lw.InfoLog, os.O_WRONLY|os.O_CREATE, 0666)
	if err != nil {
		log.Fatal(err)
	}
	defer logfile.Close()

	for {
		textLog := <-LogInfoChannel
		if Config.General.Verbose {
			fmt.Println(textLog)
		} else {
			logfile.WriteString(textLog)
		}
	}
}

// process error-log writer
func (lw *LoggerWorker) processErr() {
	logfile, err := os.OpenFile(lw.ErrorLog, os.O_WRONLY|os.O_CREATE, 0666)
	if err != nil {
		log.Fatal(err)
	}
	defer logfile.Close()

	for {
		textLog := <-LogErrChannel
		if Config.General.Verbose {
			fmt.Println(textLog)
		} else {
			logfile.WriteString(textLog)
		}
	}
}

//
func (v *LogVerbose) push() {
	textLog := fmt.Sprintf("worker id: %d was compressed >> %s Before Size: %d, After size: %d \n", v.ID, v.ImagePath, v.beforeSize, v.afterSize)
	LogInfoChannel <- textLog
}
