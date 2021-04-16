# Copyright 2021 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=7

LUA_COMPAT=( lua5-{1..4} )

inherit git-r3 fcaps user

DESCRIPTION="hinsightd a http/1.1 webserver with (hopefully) minimal goals"
HOMEPAGE="https://gitlab.com/tiotags/hin9"
EGIT_REPO_URI="https://gitlab.com/tiotags/hin9.git"
LICENSE="BSD"
SLOT="0"
KEYWORDS=""
IUSE="" #+openssl

RDEPEND="
sys-libs/liburing
dev-lang/lua
sys-libs/zlib
dev-libs/openssl
"

DEPEND="${RDEPEND}"

BDEPEND="dev-util/ninja"

FILESDIR1=${S}/external/ebuild/files/

PATCHES=(
"${FILESDIR1}/gentoo.patch"
)

pkg_preinst() {
  enewgroup hinsightd
  enewuser hinsightd -1 -1 /dev/null hinsightd
}

#src_configure() {
#}

src_compile() {
  cd build
  ninja
}

src_install() {
  newbin ${S}/build/hin9 hinsightd
  newinitd ${FILESDIR1}/init.d.sh hinsightd

  insinto /etc/hinsightd
  newins ${S}/workdir/main.lua hinsightd.lua

  keepdir /var/www/localhost/htdocs
  keepdir /var/log/hinsightd
  keepdir /var/tmp/hinsightd
}

pkg_postinst() {
  fcaps CAP_NET_BIND_SERVICE /usr/bin/hinsightd
}


