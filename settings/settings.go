package settings

import (
	"fmt"
	"gopkg.in/yaml.v2"
	"io/ioutil"
	"log"
	"path/filepath"
	"strings"
)

const (
	AppName = "TinyJPG"
)

var setting settingModel

var (
	SettingFile string
	Logger      *log.Logger

	Debug    bool
	General  = &setting.General
	Compress = &setting.Compress
)

//settingModel
type settingModel struct {
	General struct {
		Worker       int    `yaml:"worker"`
		WorkerBuffer int    `yaml:"worker_buffer"`
		EventBuffer  int    `yaml:"event_buffer"`
		Verbose      bool   `yaml:"verbose"`
		ErrorLog     string `yaml:"error_log"`
		InfoLog      string `yaml:"info_log"`
		PidFile      string `yaml:"pid_file"`
	} `yaml:"general"`

	Compress struct {
		Prefix  []string `yaml:"prefix"`
		Path    string   `yaml:"path"`
		Paths   []string `yaml:"paths"`
		Quality int      `yaml:"quality"`
	} `yaml:"compress"`
}

//Read and parse config file
func LoadSettings(configFile string) (error error) {
	error = nil
	filename, err := filepath.Abs(configFile)
	if err != nil {
		fmt.Println("Fail find settings")
		return err
	}
	yamlFile, err := ioutil.ReadFile(filename)
	if err != nil {
		fmt.Println("Fail open settings")
		return err
	}
	err = yaml.Unmarshal(yamlFile, &setting)
	if err != nil {
		log.Printf("[%s] error: %s parse from file %s\n", AppName, err, filename)
		return err
	}
	log.Println("load settings √")
	return error
}

//ReloadSettings
func ReloadSettings() error {
	filename, err := filepath.Abs(SettingFile)
	if err != nil {
		return fmt.Errorf("[%s] can not be reloaded, filepath Abs error: %s\n", AppName, err.Error())
	}
	yamlFile, err := ioutil.ReadFile(filename)
	if err != nil {
		return fmt.Errorf("[%s] can not be reloaded, can not read yaml-File: %s\n", AppName, err.Error())
	}
	err = yaml.Unmarshal(yamlFile, &setting)
	if err != nil {
		return fmt.Errorf("[%s] error: %s parse from file %s\n", AppName, err, filename)
	}
	log.Printf("[%s] - Setting file re-load: %s\n", AppName, filename)
	return nil
}

//getRegexp
func Regexp() string {
	if len(Compress.Prefix) > 0 && Compress.Prefix[0] != "*" {
		prefix := fmt.Sprintf(`(%s).*.(JPG|jpeg|JPEG|jpg|png|PNG)$`, strings.Join(Compress.Prefix, "|"))
		return prefix
	}
	return `^.*.(JPG|jpeg|JPEG|jpg|png|PNG)$`
}
