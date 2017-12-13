#!/bin/sh

libusb=libusb-1.0
CC=${CC:-cc}
CFLAGS=${CFLAGS:--g -Wall -Werror -pedantic -std=c99}

unset noudevrules
while [ $# -ne 0 ]; do
    case $1 in
    --prefix) shift; PREFIX=$1;;
    --bindir) shift; BINDIR=$1;;
    --datadir) shift; DATADIR=$1;;
    --mandir) shift; MANDIR=$1;;
    --udevrulesdir) shift; UDEVRULESDIR=$1;;
    --no-udevrules) noudevrules=1;;
    *) cat >&2 << 'EOT'
config.sh [ --prefix PREFIX ] [ --bindir BINDIR ] [ --datadir DATADIR ]
          [ --mandir MANDIR ] [ --udevrulesdir UDEVRULESDIR ]
          [ --no-udevrules ]
Generate config.h and Makefile
 --prefix       default prefix for the installation directories (/usr/local)
 --bindir       installation directory of the executables ($PREFIX/bin)
 --datadir      installation directory of the menu ROMs
                ($PREFIX/share/ems-flasher)
 --mandir       installation directory of the manual pages ($PREFIX/share/man)
 --udevrulesdir installation directory of the udev rules ensuring access to
                users to the USB device (/lib/udev/rules.d)
 --no-udevrules don't install the udev rules
EOT
       exit 1
    ;;
    esac
    shift
done

PREFIX=${PREFIX:-/usr/local}
BINDIR=${BINDIR:-$PREFIX/bin}
DATADIR=${DATADIR:-$PREFIX/share/ems-flasher}
MANDIR=${MANDIR:-$PREFIX/share/man}
if [ -z "$noudevrules" ]  && [ "$(uname -s)" = "Linux" ]; then
    UDEVRULESDIR=${UDEVRULESDIR:-/lib/udev/rules.d}
    if ! [ -d "$UDEVRULESDIR" ]; then
      echo "Can't find the udev rules directory." \
           "Specify it with --udevrulesdir or disable installation" \
           "of the udev rules with -no-udevrules" >&2
      exit 1
    fi
else
    unset UDEVRULESDIR
fi

cat <<EOT
CC=$CC
CFLAGS=$CFLAGS
BINDIR=$BINDIR
DATADIR=$DATADIR
MANDIR=$MANDIR
UDEVRULESDIR=$UDEVRULESDIR
EOT

if
  ! libusb_ldflags=`pkg-config --libs "$libusb"` ||
  ! libusb_cflags=`pkg-config --cflags "$libusb"`
then
    echo "Can't determine compile flags for $libusb. Are pkg-config and $libusb installed?" >&2
    exit 1
fi

tmpd=`mktemp -d 2>/dev/null || mktemp -d -t 'ems-flasher'` || exit
trap 'rm -r "$tmpd"' EXIT

cat > "$tmpd/conf" <<EOT
#ifndef EMS_CONFIG_H
#define EMS_CONFIG_H
EOT

cat <<EOT > "$tmpd/test_libpthread.c"
#include <stdio.h>
#include <libusb.h>
int main() {
	(void)libusb_init(NULL);
	return 0;
}
EOT

if
    ! $CC $CFLAGS $libusb_cflags "$tmpd/test_libpthread.c" $libusb_ldflags \
        -o "$tmpd/test_libpthread" 2>"$tmpd/err"
then
    cat "$tmpd/err" >&2
    exit 1
fi


# detect and use either ldd (linux) or otool (macOS)
if 
    command -v ldd >/dev/null 2>&1
then
    LDD=ldd
elif
    command -v otool >/dev/null 2>&1
then
    LDD="otool -L"
else
    echo "System doesn't have ldd or otool... exiting."
    exit 
fi

$LDD "$tmpd/test_libpthread" > "$tmpd/lddout" || exit

if
    grep -q libpthread "$tmpd/lddout"
then
    echo "$libusb seems to use libpthread"
    pthread_ldflags="-lpthread"
    echo "#define USE_PTHREAD" >> "$tmpd/conf"
else
    echo "$libusb doesn't seem to use libpthread. Fine."
fi

echo "#define MENUDIR \"$DATADIR\"" >> "$tmpd/conf"

echo '#endif' >> "$tmpd/conf"
if ! cmp -s "$tmp_conf" config.h; then
    mv "$tmpd/conf" config.h
fi

cat > Makefile << EOT
CC = $CC
CFLAGS = $CFLAGS
LIBUSB_CFLAGS = $libusb_cflags
LIBUSB_LDFLAGS = $libusb_ldflags
PTHREAD_LDFLAGS = $pthread_ldflags
BINDIR = $BINDIR
DATADIR = $DATADIR
MANDIR = $MANDIR
UDEVRULESDIR = $UDEVRULESDIR
EOT
cat Makefile.tmpl >>Makefile
