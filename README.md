<p align="center"><img src="https://raw.githubusercontent.com/OrlovEvgeny/TinyJPG/master/doc/logo.png" width="360"></p>
<p align="center">
  <a href="https://travis-ci.org/OrlovEvgeny/TinyJPG"><img src="https://travis-ci.org/OrlovEvgeny/TinyJPG.svg?branch=master" alt="Build Status"></img></a>
  <a href="https://codeclimate.com/github/OrlovEvgeny/TinyJPG/maintainability"><img src="https://api.codeclimate.com/v1/badges/89f33892db95130c5b3a/maintainability" alt="Maintainability"></a>
  <a href="https://goreportcard.com/report/github.com/OrlovEvgeny/TinyJPG?"><img src="https://goreportcard.com/badge/github.com/OrlovEvgeny/TinyJPG?" /></a>
  <a href="https://gitter.im/TinyJPG/Lobby?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge"><img src="https://badges.gitter.im/TinyJPG/Lobby.svg" /></a>
</p>

# TinyJPG Filesystem watcher and image compress

JPEG image compress watcher based Filesystem event notification [github.com/rjeczalik/notify](https://github.com/rjeczalik/notify)

**Example compress quality 82%**

original before size 1.47 MB (1,536,181 bytes)  | compressed after 277.56 KB (284,223 bytes)
------------- | -------------
![screenshot](https://raw.githubusercontent.com/OrlovEvgeny/TinyJPG/master/doc/meg-before.jpg "compress example")  | ![screenshot](https://raw.githubusercontent.com/OrlovEvgeny/TinyJPG/master/doc/meg-after.jpg "compress example")

# Install binary

**Installation dependency**
```bash
~ $ apt install libmagickwand-dev imagemagick
```

**Install TinyJPG for v0.0.8:**
````bash
curl -L https://raw.githubusercontent.com/OrlovEvgeny/TinyJPG/master/tinyjpg_install.sh | sh -s - v0.0.8
````

If you prefer **Ansible**:
````bash
tasks:
- name: TinyJPG installed
  sudo: yes
  shell: "curl -L https://raw.githubusercontent.com/OrlovEvgeny/TinyJPG/master/tinyjpg_install.sh | sh -s - v0.0.8"
````

**Edit config File**
````bash
~ $ vim /etc/tinyjpg/config.yml
````

````bash

##
# TinyJPG v0.0.8
#
# worker - maximum amount workers, Default value - 5
# verbose - verbose log, Default value - true
# worker_buffer - maximum buffer queue workers, Default value - 100
# event_buffer - maximum buffer an event reported by the underlying filesystem notification subsystem, Default value - 100
##
general:
  worker: 5
  worker_buffer: 100
  event_buffer: 300
  verbose: false
  error_log: '/var/log/tinyjpg/error.log'
  info_log: '/var/log/tinyjpg/info.log'

###
# Image compress settings
#
# paths - directories you need to track
# prefix - prefix of files to be processed, Default value all files - *
# example use
#
#   prefix:
#      - 'orig'
#      - 'medium'
#      - 'full'
#
# quality - This param image quality level in percentage.
# If the original image quality is lower than the quality of the parameter - quality
# the image will not be processed
###
compress:
  paths:
    - '/home/www/example.com/uploads'
    - '/home/www/site.org/uploads'
  prefix:
    - '*'
  quality: 82

````



**check that everything is fine**
````bash
~ $ tinyjpg -help

    Usage of build/tinyjpg:
      -config string
            config file path
      -event_buffer int
            buffer an event reported (default 300)
      -path string
            uploads folder path, default - /home/www (default "/home/www")
      -quality int
            image quality level in percentage (default 82)
      -worker int
            maximum amount workers (default 5)
      -worker_buffer int
            maximum buffer queue workers (default 500)

````

## Use
**I recommend using [supervisor](http://blog.questionable.services/article/running-go-applications-in-the-background/)**

*example config*
```bash
~ $ vim /etc/supervisor/conf.d/tinyjpg.conf
```
*write*
```bash
[program:tinyjpg]
command=/usr/local/bin/tinyjpg -config=/etc/tinyjpg/config.yml
environment=ENVIRONMENT=production
autorestart=true
user=root
redirect_stderr=true
stderr_logfile=/var/log/tinyjpg/log.err.log
stdout_logfile=/var/log/tinyjpg/log.out.log
```

```bash
~ $ mkdir -p /var/log/tinyjpg
```

```bash
~ $ service supervisor restart
```

or use **Tmux**

```bash
~ $ tinyjpg -config=/etc/tinyjpg/config.yml
```

# deprecated
or use **CLI** mode
````bash
    # deprecated
~ $ tinyjpg -path=/home/www/example.com/images -worker=10
````
**Args:**
* *path* - required. Path to watch new files, default /home/www
* *worker* - optional. Amount start workers process, default 5


# Build Source

**For compilation you need to install [Golang1.8](https://medium.com/@patdhlk/how-to-install-go-1-8-on-ubuntu-16-04-710967aa53c9)**

```bash
~ $ apt install libmagickwand-dev imagemagick
```

```bash
~ $ git clone https://github.com/OrlovEvgeny/TinyJPG && cd TinyJPG
```

```bash
~ $ go get -u github.com/rjeczalik/notify
```

```bash
~ $ mkdir build && go build -o ./build/tinyjpg *.go
```

```bash
~ $ mv /build/tinyjpg  /usr/local/bin/tinyjpg
```
**permission for execution**
````bash
~ $ chmod +x /usr/local/bin/tinyjpg
````

# License:

[MIT](LICENSE)
