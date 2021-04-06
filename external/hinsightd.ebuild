# Copyright 1999-2021 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=7

DESCRIPTION="hinsightd is a http/1.1 webserver"
HOMEPAGE="https://gitlab.com/tiotags/hin9"
SRC_URI="https://gitlab.com/tiotags/hin9/-/archive/master/hin9-master.tar.gz"
LICENSE=""
SLOT="0"
KEYWORDS="~amd64"
IUSE=""

S="${WORKDIR}/hin9-master"

# Run-time dependencies. Must be defined to whatever this depends on to run.
# Example:
#    ssl? ( >=dev-libs/openssl-1.0.2q:0= )
#    >=dev-lang/perl-5.24.3-r1
# It is advisable to use the >= syntax show above, to reflect what you
# had installed on your system when you tested the package.  Then
# other users hopefully won't be caught without the right version of
# a dependency.
#RDEPEND=""

# Build-time dependencies that need to be binary compatible with the system
# being built (CHOST). These include libraries that we link against.
# The below is valid if the same run-time depends are required to compile.
#DEPEND="${RDEPEND}"

# Build-time dependencies that are executed during the emerge process, and
# only need to be present in the native build system (CBUILD). Example:
BDEPEND="dev-util/ninja"

#src_configure() {
#}

src_compile() {
  set -e
  cd build
  ninja
}

src_install() {
  set -e
  newbin ${S}/build/hin9 hinsightd
  newinitd ${S}/external/initd.sh hinsightd

  insinto /etc/hinsightd
  newins ${S}/workdir/main.lua hinsightd.lua

  keepdir /var/www/localhost/htdocs
# simple index file
}


