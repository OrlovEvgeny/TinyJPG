package config

import (
	"gopkg.in/yaml.v2"
	"io/ioutil"
	"log"
	"os"
	"path/filepath"
)

var Config = struct {
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
}{}

func LoadConfig(fail bool, configPath string) {

	filename, _ := filepath.Abs(configPath)
	yamlFile, err := ioutil.ReadFile(filename)
	if err != nil {
		log.Println("open config: ", err)
		if fail {
			os.Exit(1)
		}
	}

	if err = yaml.Unmarshal(yamlFile, &Config); err != nil {
		log.Println("parse config: ", err)
		if fail {
			os.Exit(1)
		}
	}
}
