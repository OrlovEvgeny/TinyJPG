# TinyJPG Filesystem watcher and image compress

JPEG image compress watcher based Filesystem event notification fsnotify



*Installation*

```
~ $ apt install libmagickwand-dev imagemagick
```

```
~ $ git clone https://github.com/OrlovEvgeny/TinyJPG && cd TinyJPG
```

```
~ $ go get -u github.com/rjeczalik/notify
```

```
~ $ mkdir build && go build -o ./build/tinyjpg-watcher *.go
```


*Start*
I recommend using supervisor http://blog.questionable.services/article/running-go-applications-in-the-background/

or Tmux

```
~ $ ./build/tinyjpg-watcher -path=/Users/oj/Desktop/PP -verbose=true -worker=10
```

Params:
* path - required
* verbose - optional
* worker - optional
