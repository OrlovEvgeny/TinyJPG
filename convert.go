package main

import (
	"fmt"
	"log"
	"os"
	"os/exec"
	"regexp"
	"strconv"
)

//worker unique identificator
type Worker struct {
	id int
}

//
func (w *Worker) process(c chan string) {
	jpg, err := regexp.Compile(`^.*.(JPG|jpeg|JPEG|jpg)$`)
	if err != nil {
		msg := fmt.Sprintf("Error: There is a problem with your regexp\n")
		LogErrChannel <- msg
		log.Println(msg)
		os.Exit(1)
	}

	for {
		imagePath := <-c
		fi, _ := os.Stat(imagePath)
		beforeSize := fi.Size()

		quality := 82
		if qualityCheck(quality, imagePath) != true {
			LogInfoChannel <- fmt.Sprintf("File %s already compressed\n", imagePath)
			continue
		}

		interlace := "PNG"
		if jpg.MatchString(imagePath) == true {
			interlace = "JPEG"
		}
		cmd := fmt.Sprintf("convert %s -sampling-factor 4:2:0 -strip -quality %d -interlace %s -colorspace sRGB %s",
			imagePath,
			quality,
			interlace,
			imagePath)

		out, err := exec.Command("bash", "-c", cmd).Output()
		if err != nil {
			log.Println(out)
		}

		fl, _ := os.Stat(imagePath)
		afterSize := fl.Size()

		verbose := LogVerbose{
			ID:         w.id,
			ImagePath:  imagePath,
			beforeSize: beforeSize,
			afterSize:  afterSize,
		}

		go verbose.push()
	}
}

//check quality
func qualityCheck(quality int, file string) bool {
	cmd := fmt.Sprintf("identify -format %s %s", "'%Q'", file)
	out, err := exec.Command("bash", "-c", cmd).Output()
	if err != nil {
		log.Println(string(out))
		log.Printf("Incorect file name %s", file)
	}
	qualityNum, _ := strconv.ParseInt(string(out), 10, 0)
	if int64(quality) >= qualityNum {
		return false
	}
	return true
}
