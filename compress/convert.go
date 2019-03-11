package compress

import (
	"context"
	"fmt"
	"github.com/OrlovEvgeny/TinyJPG/core"
	"github.com/OrlovEvgeny/TinyJPG/settings"
	"log"
	"os"
	"os/exec"
	"regexp"
	"strconv"
)

const (
	PNG  = "PNG"
	JPEG = "JPEG"
)

//Imagemagic
type Imagemagic struct {
	ctx     context.Context
	Log     *log.Logger
	Quality int
}

//NewImagemagic cmd wrapper
func NewImagemagic(ctx context.Context) *Imagemagic {
	return &Imagemagic{
		Log:     ctx.Value(core.Log).(*log.Logger),
		ctx:     ctx,
		Quality: settings.Compress.Quality,
	}
}

func (im *Imagemagic) Run(c chan string) {
	for i := 0; i > settings.General.Worker; i++ {
		go im.process(c)
	}
}

//process
func (im *Imagemagic) process(c chan string) {
	jpg, err := regexp.Compile(`^.*.(JPG|jpeg|JPEG|jpg)$`)
	if err != nil {
		im.Log.Printf("Error: There is a problem with your regexp\n")
		os.Exit(1)
	}

	for {
		select {
		case imagePath := <-c:
			fi, _ := os.Stat(imagePath)
			beforeSize := fi.Size()

			if !im.qualityCheck(82, imagePath) {
				im.Log.Printf("File %s already compressed\n", imagePath)
				continue
			}

			interlace := PNG
			if jpg.MatchString(imagePath) == true {
				interlace = JPEG
			}

			cmd := im.buildArgs(im.Quality, imagePath, interlace)

			if _, err := exec.Command("bash", "-c", cmd).Output(); err != nil {
				im.Log.Printf("Commpress imagemagic error: %s\n", err.Error())
				continue
			}

			fl, _ := os.Stat(imagePath)
			afterSize := fl.Size()
			im.Log.Printf("Compress file %s is done, filesize before %d, after %d\n", fi.Name(), beforeSize, afterSize)

		case <-im.ctx.Done():
			return
		}
	}
}

//check quality
func (im *Imagemagic) qualityCheck(quality int, file string) bool {
	cmd := fmt.Sprintf("identify -format %s %s", "'%Q'", file)
	out, err := exec.Command("bash", "-c", cmd).Output()
	if err != nil {
		im.Log.Printf("Incorect file name %s", file)
	}
	qualityNum, _ := strconv.ParseInt(string(out), 10, 0)
	if int64(quality) >= qualityNum {
		return false
	}
	return true
}

//buildArgs
func (im *Imagemagic) buildArgs(quality int, imagePath, interlace string) string {
	return fmt.Sprintf("convert %s -sampling-factor 4:2:0 -strip -quality %d -interlace %s -colorspace sRGB %s",
		imagePath,
		quality,
		interlace,
		imagePath)
}
