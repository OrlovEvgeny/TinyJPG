# TinyJPG Filesystem watcher and image compress

[![Join the chat at https://gitter.im/TinyJPG/Lobby](https://badges.gitter.im/TinyJPG/Lobby.svg)](https://gitter.im/TinyJPG/Lobby?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
[![Go Report Card](https://goreportcard.com/badge/github.com/OrlovEvgeny/TinyJPG?)](https://goreportcard.com/report/github.com/OrlovEvgeny/TinyJPG?)
[![Build Status](https://travis-ci.org/OrlovEvgeny/TinyJPG.svg?branch=master)](https://travis-ci.org/OrlovEvgeny/TinyJPG)
[![Maintainability](https://api.codeclimate.com/v1/badges/89f33892db95130c5b3a/maintainability)](https://codeclimate.com/github/OrlovEvgeny/TinyJPG/maintainability)

JPEG image compress watcher based Filesystem event notification [github.com/rjeczalik/notify](https://github.com/rjeczalik/notify)

![screenshot](doc/screen.png "compress example")


# Install binary

**Installation dependency**
```bash
~ $ apt install libmagickwand-dev imagemagick
```

for **amd64**
````bash
~ $ curl -L https://github.com/OrlovEvgeny/TinyJPG/releases/download/v0.0.7-beta/tinyjpg-amd64 --output /usr/local/bin/tinyjpg
````
 or **i386**
````bash
~ $ curl -L https://github.com/OrlovEvgeny/TinyJPG/releases/download/v0.0.7-beta/tinyjpg-386 --output /usr/local/bin/tinyjpg
````

**permission for execution**
````bash
~ $ chmod +x /usr/local/bin/tinyjpg
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
command=/usr/local/bin/tinyjpg -path=/home/www/example.com/images -verbose=true -worker=10
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
~ $ tinyjpg -path=/home/www/example.com/images -verbose=true -worker=10
```

**Args:**
* *path* - required. Path to watch new files, default /home/www
* *verbose* - optional. Verbose log out, default true
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
