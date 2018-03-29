VERSION="$1"

PATH="$PATH:/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin"
TARGET_DIR=/usr/local/bin/tinyjpg
CONF_DIR=/etc/tinyjpg
PERM="chmod +x /usr/local/bin/tinyjpg"

if [ `getconf LONG_BIT` = "32" ]; then
    ARCH="386"
else
    ARCH="amd64"
fi

URL="https://github.com/OrlovEvgeny/TinyJPG/releases/download/$VERSION/tinyjpg-$ARCH"
CONF_URL="https://raw.githubusercontent.com/OrlovEvgeny/TinyJPG/master/config.yml"

if [ -n "`which curl`" ]; then
    download_cmd="curl -L $URL --output $TARGET_DIR"
    conf_download_cmd="curl -L $CONF_URL --output $CONF_DIR/config.yml"
else
    die "Failed to download okagent: curl not found, plz install curl"
fi

mkdir -p $CONF_DIR

echo -n "Fetching TinyJPG from $URL: "
$download_cmd || die "Error when downloading TinyJPG from $URL"
$conf_download_cmd || die "Error when downloading config file TinyJPG from $CONF_URL"
echo "done"

echo -n "Set permission execute TinyJPG: "
$PERM || die "Error permission execut TinyJPG from $TARGET_DIR"
echo "done"

echo "Finished"