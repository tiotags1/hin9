FROM alpine:latest

RUN apk update && apk upgrade
RUN apk add ninja build-base cmake lua-dev linux-headers openssl-dev zlib-dev liburing-dev
#RUN apk add php8 php8-fpm php8-opcache php8-mysqli php8-gd
RUN apk add php8 php8-cgi php8-mysqli php8-gd
RUN apk add nano bash openssl curl coreutils xxd rsync

WORKDIR /app

# used for ssl certificate retrieval,
#set in docker run --env SITE_HOSTNAME="example.com"
#ENV SITE_HOSTNAME="example.com www.example.com"
#ENV SITE_EMAIL="example@example.com"

# you need to run 'sh getsources.sh' first
ADD src.tar.gz /app
ADD scripts /app/scripts
ADD htdocs /app/htdocs
RUN sh /app/scripts/setup.sh

USER docker:docker

EXPOSE 8080
EXPOSE 8081
CMD ["/app/build/hin9"]

