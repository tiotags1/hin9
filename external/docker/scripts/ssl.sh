#!/bin/sh

if [ -z $SITE_HOSTNAME ]; then
  echo "ssl.sh can't find $SITE_HOSTNAME in environment";
  exit 1
fi

if [ -z $SITE_EMAIL ]; then
  EMAIL="-e $SITE_EMAIL"
fi

bash /app/scripts/bacme $EMAIL -w /app/htdocs/ $SITE_HOSTNAME

NAME=$(echo $SITE_HOSTNAME | awk '{print $1}')
INDIR=/app/$NAME
OUTDIR=/app/workdir/ssl
mkdir $OUTDIR
openssl x509 -in $INDIR/$NAME.crt -out $OUTDIR/cert.pem -outform PEM
openssl rsa -in $INDIR/$NAME.key -out $OUTDIR/key.pem -outform PEM

killall -USR1 hin9

