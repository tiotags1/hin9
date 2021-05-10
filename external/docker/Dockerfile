FROM alpine:latest

RUN apk update && apk upgrade
RUN apk add ninja build-base cmake lua-dev linux-headers openssl-dev zlib-dev liburing-dev
RUN apk add php8 php8-fpm php8-opcache php8-mysqli php8-gd
RUN apk add php8-cgi

WORKDIR /app

# you need to run 'sh getsources.sh' first
ADD src.tar.gz /app
ADD init.sh .
RUN sh /app/init.sh
ADD htdocs /app/htdocs

USER docker:docker

EXPOSE 8080
CMD ["/app/build/hin9"]
