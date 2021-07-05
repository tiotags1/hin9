FROM alpine:latest

RUN echo "http://dl-cdn.alpinelinux.org/alpine/edge/testing" >> /etc/apk/repositories

RUN apk update && apk upgrade
RUN apk add hinsightd
RUN apk add php8-cgi php8-mysqli

WORKDIR /app

# dev profile cache
#RUN apk update && apk upgrade && apk add nano bash sudo openssl curl coreutils xxd rsync ninja build-base cmake lua-dev linux-headers openssl-dev zlib-dev liburing-dev

# you need to run 'sh getsources.sh' first
ADD conf.tar.gz /app
ADD scripts /app/scripts
RUN sh /app/scripts/setup.sh

# build from source
#ADD src.tar.gz /app
#RUN sh /app/scripts/dev.sh

USER docker:docker

#exposes 8080 and 8081

CMD ["sh"]
#CMD ["sh", "/app/scripts/run.sh"]

