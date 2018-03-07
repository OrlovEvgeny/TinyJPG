package main

import (
	"fmt"
	"os"
	"os/exec"
)

type Worker struct {
	id int
}

func (w *Worker) process(c chan string) {
	for {
		imagePath := <-c
		fi, _ := os.Stat(imagePath)
		beforeSize := fi.Size()

		cmd := fmt.Sprintf("convert %s -sampling-factor 4:2:0 -strip -quality 85 -interlace JPEG -set comment 'pp-compressed' -colorspace sRGB %s",
			imagePath,
			imagePath)
		out, err := exec.Command("bash", "-c", cmd).Output()
		if err != nil {
			fmt.Println(out)
		}

		fii, _ := os.Stat(imagePath)
		afterSize := fii.Size()
		if *verbose == true {
			fmt.Printf("worker id: %d was compressed >> %s Before Size: %d, After size: %d \n", w.id, imagePath, beforeSize, afterSize)
		}
	}
}
