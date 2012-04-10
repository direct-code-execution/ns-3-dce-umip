#!/bin/bash

cd `dirname $BASH_SOURCE`/../..

# build umip
git clone git://git.umip.org/umip.git
cd umip
autoreconf -i
CFLAGS="-fPIC -g" CXXFLAGS="-fPIC -g" LDFLAGS="-pie -g" ./configure --enable-vt --with-builtin-crypto
make
cp -f src/mip6d  ../ns-3-dce/build/bin_dce
cd ..

# mode ns-3-dev (FIXME)
cd ns-3-dev
sed "s/NS_ASSERT_MSG (nextHeaderPosition/\/\/NS_ASSERT_MSG (nextHeaderPosition/" src/internet/model/ipv6-l3-protocol.cc >a
mv a src/internet/model/ipv6-l3-protocol.cc
./waf
./waf install
cd ..

# mod ns-3-dce (FIXME)
#hg clone http://202.249.37.8/ical/ns-3-dce-patches/
cd ns-3-dce-patches
hg pull -u
cd ..
cd ns-3-dce
patch -p1 < ../ns-3-dce-patches/120410-dce-umip-support.patch
. ./utils/setenv.sh
./waf configure --prefix=`pwd`/../build --verbose --enable-kernel-stack=`pwd`/../ns-3-linux
./waf
./waf install
cd ..

# mod ns-3-linux (FIXME)
hg clone http://202.249.37.8/ical/ns-3-linux-patches/
cd ns-3-linux-patches
hg pull -u
cd ..
cd ns-3-linux
patch -p1 < ../ns-3-linux-patches/120410-linux-umip-support.patch
make clean
rm -f config
make config
make
cd ..

# build ns-3-dce-umip
cd ns-3-dce-umip
. ../ns-3-dce/utils/setenv.sh
cd ../ns-3-dce-umip
./waf configure --prefix=`pwd`/../build --verbose --enable-kernel-stack=`pwd`/../ns-3-linux
./waf
./waf install
echo Launch NS3UMIPTEST-DCE
./build/bin/ns3test-dce-umip --verbose


