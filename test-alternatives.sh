#!/bin/bash

[ -x ./alternatives ] || exit 1

altdir=$(mktemp -d)
admindir=$(mktemp -d)
testdir=$(mktemp -d)

rc=0
buf=

name="TEST"
link="${testdir}/main_link"


function clean_dir {
    [ -n ${altdir} -a -d ${altdir} ] && rm ${altdir}/* &> /dev/null
    [ -n ${admindir} -a -d ${admindir} ] && rm ${admindir}/* &> /dev/null
    [ -n ${testdir} -a -d ${testdir} ] && rm ${testdir}/* &> /dev/null
}


function add_alternative {
    path="${testdir}/$1"
    prio=$2
    family=
    [ -n "$3" ] && family="--family $3"
    touch ${path}

    ./alternatives --altdir ${altdir} --admindir ${admindir} --install ${link} ${name} ${path} ${prio} ${family}
    echo -e "NEW\tlink: $1\tPrio: $prio\tFamily: $3"
}

function remove_alternative {
    path="${testdir}/$1"
    rm ${path}

    ./alternatives --altdir ${altdir} --admindir ${admindir} --remove ${name} ${path}
    echo -e "REMOVE\tlink: $1"
}

function set_alternative {
    path="${testdir}/$1"

    ./alternatives --altdir ${altdir} --admindir ${admindir} --set ${name} ${path}
    echo -e "SET\tlink: $1"
}

function check_alternative {
    path=$1
    state=$2
    best=${3:-$1}

    cur_path=$(readlink ${altdir}/${name} | xargs basename)
    cur_state=$(head -1 ${admindir}/${name})
    cur_best=$(LC_ALL=C ./alternatives --altdir ${altdir} --admindir ${admindir} --display TEST | grep best | cut -d " " -f5 | sed -e 's/\.$//' | xargs basename)
    echo
    echo -n "Path:  ${cur_path}"
    echo -e "\t(expected: ${path})"
    echo -n "State: ${cur_state}"
    echo -e "\t(expected: ${state})"
    echo -n "Best: ${cur_best}"
    echo -e "\t(expected: ${best})"
    if [ "${state}" = "${cur_state}" ] && [ "${best}" = "${cur_best}" ] &&  [ "${path}" = "${cur_path}" ]; then
        echo -en '\e[0;32m'
        echo -n "SUCCESS"
        echo -e '\e[0m'
    else
        echo -en '\e[0;31m'
        echo -n "FAIL"
        echo -e '\e[0m'
        ((rc++))
    fi
    echo
    echo "-----------------------------------"
    echo
}

add_alternative link_a 10
check_alternative link_a auto
clean_dir

add_alternative link_a 10
set_alternative link_a
check_alternative link_a manual
clean_dir

add_alternative link_a 10
add_alternative link_b  20
check_alternative link_b auto
clean_dir

add_alternative link_a 20
add_alternative link_b 10
check_alternative link_a auto
clean_dir

add_alternative link_a 10
set_alternative link_a
add_alternative link_b  20
check_alternative link_a manual link_b
clean_dir

add_alternative link_a 10
set_alternative link_a
add_alternative link_b  20
remove_alternative link_a
check_alternative link_b auto
clean_dir

##########

add_alternative link_a 10 family_a
add_alternative link_b 20 family_a
check_alternative link_b auto
clean_dir

add_alternative link_a 20 family_a
add_alternative link_b 10 family_a
check_alternative link_a auto
clean_dir

add_alternative link_a 10 family_a
add_alternative link_b 20 family_b
check_alternative link_b auto
clean_dir

add_alternative link_a 20 family_a
add_alternative link_b 10 family_b
check_alternative link_a auto
clean_dir

add_alternative link_a 10 family_a
add_alternative link_b 20 family_a
add_alternative link_c 30 family_b
check_alternative link_c auto
clean_dir

add_alternative link_a 10 family_a
set_alternative link_a
add_alternative link_c 30 family_b
remove_alternative link_a
check_alternative link_c auto
clean_dir

add_alternative link_a 10 family_a
set_alternative link_a
add_alternative link_b 20 family_a
add_alternative link_c 30 family_b
remove_alternative link_a
check_alternative link_b manual link_c
clean_dir


echo "$rc fails"

rmdir $altdir $testdir $admindir

exit $rc
