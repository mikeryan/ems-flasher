#!/bin/sh

libusb=libusb-1.0
CC=${CC:-cc}
CFLAGS=${CFLAGS:--g -Wall -Werror -pedantic -std=c99}

while [ $# -ne 0 ]; do
    case $1 in
    --prefix) shift; PREFIX=$1;;
    --bindir) shift; BINDIR=$1;;
    --datadir) shift; DATADIR=$1;;
    --mandir) shift; MANDIR=$1;;
    *) cat >&2 << 'EOT'
config.sh [ --prefix PREFIX ] [ --bindir BINDIR ] [ --datadir DATADIR ]
          [ --mandir MANDIR ]
Generate config.h and Makefile
 --prefix  default prefix for the installation directories (/usr/local)
 --bindir  installation directory of the executables ($PREFIX/bin)
 --datadir installation directory of the menu ROMs ($PREFIX/share/ems-flasher)
 --mandir  installation directory of the manual pages ($PREFIX/share/man)
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

cat <<EOT
CC=$CC
CFLAGS=$CFLAGS
BINDIR=$BINDIR
DATADIR=$DATADIR
MANDIR=$MANDIR
EOT

if
  ! libusb_ldflags=`pkg-config --libs "$libusb"` ||
  ! libusb_cflags=`pkg-config --cflags "$libusb"`
then
    echo "Can't determine compile flags for $libusb. Are pkg-config and $libusb installed?" >&2
    exit 1
fi

#tmp_conf=`mktemp` || exit
tmp_conf=`mktemp 2>/dev/null || mktemp -t 'emsflasher'` || exit

cat > "$tmp_conf" <<EOT
#ifndef EMS_CONFIG_H
#define EMS_CONFIG_H
EOT

#tmp_c=`mktemp` || exit
tmp_c=`mktemp 2>/dev/null || mktemp -t 'emsflasher'` || exit
cat <<EOT > "$tmp_c.c"
#include <stdio.h>
#include <libusb.h>
int main() {
	(void)libusb_init(NULL);
	return 0;
}
EOT
$CC $CFLAGS $libusb_cflags "$tmp_c.c" $libusb_ldflags -o "$tmp_c" || exit
if
    ldd "$tmp_c" | grep -q libpthread
then
    echo "$libusb seems to use libpthread"
    pthread_ldflags="-lpthread"
    echo "#define USE_PTHREAD" >> "$tmp_conf"
else
    echo "$libusb doesn't seem to use libpthread. Fine."
fi

echo "#define MENUDIR \"$DATADIR\"" >> "$tmp_conf"

echo '#endif' >> "$tmp_conf"
if ! cmp -s "$tmp_conf" config.h; then
    mv "$tmp_conf" config.h
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
EOT
cat Makefile.tmpl >>Makefile
