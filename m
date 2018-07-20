#!/bin/ksh

# Copyright 2018 Krister Joas <krister@joas.jp>

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

if [[ -d "$1/_m" ]]
then
    topdir="$1"
    builddir="."
    shift
else
    topdir="."
    builddir="build"
fi

if [[ ! -f ${topdir}/_m ]]
then
    echo "Missing file: ${topdir}/_m"
    exit 1
fi

cat > build.ninja <<EOF
# This file is automatically generated.

# Copyright 2018 Krister Joas <krister@joas.jp>

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

builddir = ${builddir}
topdir = ${topdir}
EOF

cat >> build.ninja <<'EOF'
rule COMPILE.cc
 command = c++ $incs ${-D} ${-I} $ccflags -MMD -MF $out.d -c -o $out $in
 description = Compile $out

rule COMPILE.c
 command = cc $incs ${-I} $cflags -MMD -MF $out.d  -c -o $out $in
 description = Compile $out

rule ARCHIVE
 command = ar cr $out $in
 description = Archive $out

rule LINK.cc
 command = c++ $ldflags $in ${-L} ${-l} -o $out
 description = Link $out

EOF

typeset -A _lib_I
typeset -A _lib_L
typeset -A _lib_libs
typeset -A _subdirs
typeset -A _project_ccflags
typeset -A _project_ldflags

project() {
    _init
    _project="$1"
    _target="$1"
}

bin() {
    _generate
    _init
    _binary="$1"
    _target="$1"
}

lib() {
    _generate
    _init
    _library="$1"
    _target="$1"
    _lib_L["${_library}"]="-L\$builddir/lib"
}

url() {
    _url="$1"
    _hash="$2"
}

ccflags() {
    if [[ "${_project}" ]]
    then
        _project_ccflags[${#_project_ccflags[@]}]="$@"
    else
        _ccflags[${#_ccflags[@]}]="$@"
    fi
}

ldflags() {
    if [[ "${_project}" ]]
    then
        _project_ldflags[${#_project_ldflags[@]}]="$@"
    else
        _ldflags[${#_ldflags[@]}]="$@"
    fi
}

src() {
    _src="\$topdir/$1"
    if [[ "${_project}" ]]
    then
        _project_src="\$topdir/$1"
    fi
}

ext() {
    _ext="$1"
}

inc() {
    if [[ "$_library" ]]
    then
        case "$1" in
            /*)
                _lib_I["${_library}"]="-I$1"
                ;;
            *)
                _lib_I["${_library}"]="-I\$topdir/$1"
                ;;
        esac
        _i="${_lib_I[${_library}]}"
        _I["${_i}"]=1
    else
        _incs[${#_incs[@]}]="$1"
    fi
}

add() {
    case "$1" in
        src)
            _sources[${#_sources[@]}]="$2"
            if [[ "${_library}" ]]
            then
                _lib_libs[${_library}]=1
            fi
            ;;
        def)
            _D[$2]=1
            ;;
        lib)
            unset _lib
            case "$2" in
                boost::*)
                    _I["-I/usr/local/opt/boost/include"]=1
                    _L["-L/usr/local/opt/boost/lib"]=1
                    _lib=boost_"${2#boost::}"-mt
                    _libs[${#_libs[@]}]="-l${_lib}"
                    ;;
                boost)
                    _I["-I/usr/local/opt/boost/include"]=1
                    ;;
                icu::*)
                    _I["-I/usr/local/opt/icu4c/include"]=1
                    _L["-L/usr/local/opt/icu4c/lib"]=1
                    _lib="${2#icu::}"
                    _libs[${#_libs[@]}]="-l${_lib}"
                    ;;
                icu)
                    _I["-I/usr/local/opt/icu4c/include"]=1
                    ;;
                pugixml)
                    _I["-I/usr/local/opt/pugixml/include/pugixml-1.9"]=1
                    _L["-L/usr/local/opt/pugixml/lib/pugixml-1.9"]=1
                    _libs[${#_libs[@]}]="-lpugixml"
                    ;;
                unistring)
                    _I["-I/usr/local/opt/libunistring/include"]=1
                    _L["-L/usr/local/opt/libunistring/lib"]=1
                    _libs[${#_libs[@]}]="-lunistring"
                    ;;
                unittest-cpp)
                    _I["-I/usr/local/opt/unittest-cpp/include"]=1
                    _L["-L/usr/local/opt/unittest-cpp/lib"]=1
                    _libs[${#_libs[@]}]="-lUnitTest++"
                    ;;
                sys::*)
                    _lib="${2#sys::}"
                    _libs[${#_libs[@]}]="-l${_lib}"
                    ;;
                *)
                    _lib="$2"
                    if [[ "${_lib_I[${_lib}]}" ]]
                    then
                        _I["${_lib_I[${_lib}]}"]=1
                    fi
                    if [[ "${_lib_libs[${_lib}]}" ]]
                    then
                        _libs[${#_libs[@]}]="-l${_lib}"
                        _deps[${#_deps[@]}]="${_lib}"
                    fi
                    if [[ "${_lib_L[${_lib}]}" ]]
                    then
                        _L["${_lib_L[${_lib}]}"]=1
                    fi
                    ;;
            esac
            ;;
    esac
}

subdirs() {
    _subdirs["${#_subdirs[@]}"]="${topdir}/$1"
}

_init() {
    unset _library
    unset _binary
    unset _project
    unset _target
    unset _url
    unset _hash
    unset _sources
    unset _src
    unset _objects
    unset _build
    unset _libs
    unset _deps
    unset _ext
    unset _i
    unset _l
    unset _ccflags
    typeset -A _ccflags
    unset _ldflags
    typeset -A _ldflags
    unset _I
    typeset -A _I
    unset _D
    typeset -A _D
    unset _L
    typeset -A _L
    unset _incs
    typeset -A _incs
}

_fetch() {
    (
        if [[ ! -d "${topdir}/$1" ]]
        then
            echo "Cloning $1"
            git clone "$2" "$1"
        fi
        cd "$1"
        if git show --summary "$3" > /dev/null 2>&1
        then
            :
        else
            echo "Fetching $1"
            git fetch origin
        fi
        if [[ "$(git show --pretty=format:%H --no-patch)" != "$3" ]]
        then
            echo "Reset $1"
            git reset --hard "$3"
        fi
    ) >&2
}

_rule() {
    case "$1" in
        .c)
            echo COMPILE.c
            ;;
        .cc|.cxx|.cpp|.C)
            echo COMPILE.cc
            ;;
        *)
            echo "Unknown extension: ${ext}"
            exit 1
            ;;
    esac
}

_generate_project() {
    [[ "$_project_ccflags" ]] && echo "ccflags = ${_project_ccflags[@]}"
    [[ "$_project_ldflags" ]] && echo "ldflags = ${_project_ldflags[@]}"
    if [[ "${_incs}" ]]
    then
        print -n "incs ="
        for i in "${_incs[@]}"
        do
            print -n -- " -I$i"
        done
        print ""
    fi
}

_generate_build() {
    if [[ ${#_sources[@]} > 0 ]]
    then
        echo ""
        echo "# $1: ${_target}"

        _build="${_build:-\$builddir/obj/${_target}}"
        _ext="${_ext:-.cc}"
        if [[ "${_src}" ]]
        then
            :
        elif [[ "${_project_src}" ]]
        then
            _src="${_project_src}"
        else
            _src="\$topdir/${_srcdir}"
        fi
        __rule=$(_rule ${_ext})

        for i in ${_sources[@]}
        do
            o="${_build}/$(basename $i ${_ext}).o"
            _objects[${#_objects[@]}]="$o"
            echo "build $o: ${__rule} ${_src}/$i${_ext}"
            [[ "$_ccflags" ]] && echo " ccflags = ${_ccflags[@]}"
            if [[ ${#_I[@]} > 0 ]]
            then
                echo " -I = ${!_I[@]}"
            fi
            if [[ ${#_D[@]} > 0 ]]
            then
                echo " -D = ${!_D[@]}"
            fi
        done
    fi
}

_generate_library() {
    if [[ "$_url" ]]
    then
        _fetch "${topdir}/.externals/${_target}" "${_url}" "${_hash}"
    fi
    _generate_build lib

    if [[ "$_objects" ]]
    then
        echo "build lib${_target}.a: phony \$builddir/lib/lib${_target}.a"
        echo "build \$builddir/lib/lib${_target}.a: ARCHIVE ${_objects[@]}"
    fi
}

_generate_deps() {
    [[ "${_deps}" ]] && {
        __deps=" |"
        for d in "${_deps[@]}"
        do
            __deps="${__deps} \$builddir/lib/lib${d}.a"
        done
    }
}

_generate_link() {
    echo "build ${_target}: phony \$builddir/bin/${_target}"
    echo "build \$builddir/bin/${_target}: LINK.cc ${_objects[@]}${__deps}"
    [[ "$_libs" ]] && {
        echo " -l = ${_libs[@]}"
    }
    [[ "$_ldflags" ]] && echo " ldflags = ${_ldflags[@]}"
    if [[ ${#_L[@]} > 0 ]]
    then
        echo " -L = ${!_L[@]}"
    fi
}

_generate_binary() {
    _generate_build bin
    _generate_deps
    _generate_link
}

_generate_subdirs() {
    for s in "${_subdirs[@]}"
    do
        for m in $(find "$s" -name _m | sort)
        do
            . $m
            s="$(dirname $m)"
            _srcdir="${s#${topdir}/}"
            unset s
        done
    done
}

_generate() {
    if [[ "$_project" ]]
    then
        _generate_project
    elif [[ "$_library" ]]
    then
        _generate_library
    elif [[ "$_binary" ]]
    then
        _generate_binary
    fi
}

_finish() {
    _generate_subdirs
    _generate
}

{
    _srcdir="${topdir}"
    . ${topdir}/_m
    _finish
} >> build.ninja

export NINJA_STATUS='%p '
ninja "$@"
