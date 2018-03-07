# TinyJPG Filesystem watcher and image compress
[![Go Report Card](https://goreportcard.com/badge/github.com/OrlovEvgeny/TinyJPG?)](https://goreportcard.com/report/github.com/OrlovEvgeny/TinyJPG?)


JPEG image compress watcher based Filesystem event notification [github.com/rjeczalik/notify](https://github.com/rjeczalik/notify)

![screenshot](doc/screen.png "compress example")


## Build

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
~ $ mkdir build && go build -o ./build/tinyjpg-watcher *.go
```


## Start
**I recommend using [supervisor](http://blog.questionable.services/article/running-go-applications-in-the-background/)**

*example config*
```bash
~ $ vim /etc/supervisor/conf.d/tinyjpg.conf
```
*write*
```bash
[program:tinyjpg]
command=/home/TinyJPG/build/tinyjpg-watcher -path=/home/www/example.com/images -verbose=true -worker=10
directory=/home/TinyJPG/build
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
~ $ ./build/tinyjpg-watcher -path=/home/www/example.com/images -verbose=true -worker=10
```

**Args:**
* *path* - required. Path to watch new files, default /home/www
* *verbose* - optional. Verbose log out, default true
* *worker* - optional. Amount start workers process, default 5
