#The following optional packages are autodetected:
#libtcmalloc_minimal from google-perftools
#bzip2
#lzma aka xz
scriptdir=`dirname $0`
. $scriptdir/../usegcc.sh
cp $scriptdir/../user-config.jam ~
xmtext=${1:-$HOME/c/xmt-externals/FC12}
icusubdir=${2:-icu-55.1}
cd ${3:-$scriptdir}
boostminor=${4:-58}
boostsub=boost_${boostver:-1_${boostminor}_0}
sharedinc=$xmtext/../Shared/cpp
boostinc=$sharedinc/$boostsub/include
boostlib=$xmtext/libraries/$boostsub/lib
neuralinc=$sharedinc/nplm-ken/include
neurallib=$xmtext/libraries/nplm-ken/lib
eigeninc=$sharedinc/eigen-3.2.0
maxorders="5 7"
links="static shared"
makekenlm() {
    local m=$1
    set -e
    [[ -d $boostinc ]]
    [[ -d $boostlib ]]
    if ! [[ -f lm/binary_format.hh ]] ; then
        echo run from kenlm source root
        exit 1
    fi
    rm -rf lib include
    find . -name bin -exec rm -rf {} \;
    icu=$xmtext/libraries/$icusubdir
    toolset=gcc
    if [[ $lwarch = Apple ]] ; then
        icu=/usr/local/Cellar/icu4c/55.1
        toolset=Darwin
    fi
    withicu=--with-icu=$icu
    if [[ `uname` = Linux ]] ; then
        memcpyarg="cxxflags=-include cxxflags=util/glibc_memcpy.hh"
    fi
    marg="-s max-order=$m --max-kenlm-order=$m"
    nplmarg="--with-nplm -s with-nplm=1"
    boostarg="cxxflags=-I$boostinc cxxflags=-I$neuralinc cxxflags=-I$eigeninc linkflags=-L$boostlib linkflags=-L$neurallib"
    warnarg="cxxflags=-Wno-unused-variable cxxflags=-Wno-deprecated-declarations"
    #  ldflags=
    set -x
    for link in $links; do
        LD_LIBRARY_PATH=$neurallib:$LD_LIBRARY_PATH ./bjam -a -q -d+1 $nplmarg $marg link=$link variant=release debug-symbols=off $warnarg $memcpyarg $boostarg $withicu --with-toolset=$toolset -j10
    done
    set +x
}
installkenlm() {
    local m=$1
    set -e
    headers=$xmtext/../Shared/cpp/KenLM/include/KenLM
    mkdir -p $headers
    cp -a include/* $headers
    from=
    libfrom=$scriptdir/lib
    binfrom=$scriptdir/lm/bin/gcc-5/release/link-static/threading-multi/
    rm -f $binfrom/*.o
    mgram=$xmtext/libraries/KenLM/$m-gram
    libs=$mgram/lib
    bins=$mgram/bin
    mkdir -p $libs
    mkdir -p $bins
    set -x
    for s in so a; do
        from=$libfrom/libkenlm.$s
        cp $from $libs/libKenLM.$s &&
        ln -sf libKenLM.$s $libs/libKenLM_debug.$s || echo no $from
    done
    cp $binfrom/* $bins
    set +x
}
makekenlm 5 && installkenlm 5 && makekenlm 7 && installkenlm 7 && echo ok && exit || exit 1
