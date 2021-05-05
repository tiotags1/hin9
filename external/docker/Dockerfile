FROM alpine:latest

RUN apk update && apk upgrade
RUN apk add ninja
RUN apk add build-base cmake lua-dev linux-headers openssl-dev zlib-dev liburing-dev
RUN apk add php8 php8-fpm php8-opcache php8-mysqli php8-gd
RUN apk add php8-cgi
RUN ln -s /usr/bin/php-cgi8 /usr/bin/php-cgi

WORKDIR /app

RUN addgroup -g 1000 -S docker
RUN adduser -S --disabled-password -G docker -u 1000 -s /bin/sh -h /app docker
USER docker:docker

# you need to run 'sh getsources.sh' first
ADD src.tar.gz /app
ADD init.sh .
RUN sh /app/init.sh
ADD htdocs /app/htdocs

EXPOSE 8080
CMD ["/app/build/hin9"]
