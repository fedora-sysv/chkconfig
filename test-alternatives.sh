#!/bin/bash

[ -x ./alternatives ] || exit 1

altdir=$(mktemp -d)
admindir=$(mktemp -d)
testdir=$(mktemp -d)

rc=0
buf=

name="TEST"
link="${testdir}/main_link"

sname="STEST"
slink="${testdir}/slave_link"

function clean_dir {
    [ -n ${altdir} -a -d ${altdir} ] && rm ${altdir}/* &> /dev/null
    [ -n ${admindir} -a -d ${admindir} ] && rm ${admindir}/* &> /dev/null

    if [ -n ${testdir} -a -d ${testdir} ] ; then
        for i in ${testdir}/* ; do
            [ -f ${i} -o -L ${i} ] &&  rm ${i}
            [ -d ${i} ] && ( rm ${i}/* ; rmdir ${i} )
        done
    fi
}

function add_alternative {
    path="${testdir}/$1/main"
    prio=$2
    family=
    slave=slave
    mkdir -p "${testdir}/$1"
    touch $path

    [ -n "$3" ] && family="--family $3"

    [ -n "$4" ] && slave=${4}

    spath="${testdir}/$1/$slave"
    touch ${spath}

    ./alternatives --altdir ${altdir} --admindir ${admindir} --install ${link} ${name} ${path} ${prio} --slave ${slink} ${sname} ${spath} ${family}
    echo -e "NEW\tlink: $1\tPrio: $prio\tFamily: $3"
}

function remove_alternative {
    path="${testdir}/$1/main"
    rm ${testdir}/$1/*
    rmdir ${testdir}/$1

    ./alternatives --altdir ${altdir} --admindir ${admindir} --remove ${name} ${path}
    echo -e "REMOVE\tlink: $1"
}

function set_alternative {
    path="${testdir}/$1/main"

    ./alternatives --altdir ${altdir} --admindir ${admindir} --set ${name} ${path}
    echo -e "SET\tlink: $1"
}

function check_alternative {
    path=$1
    shift
    state=$1
    shift
    slave=slave

    if [ "$state" = "manual" ] ; then
         best=${1}
         shift
    else
         best=${path}
    fi

    [ -n "$1" ] && slave=$1

    cur_path=$(readlink ${altdir}/${name} | xargs dirname | xargs basename)
    cur_state=$(head -1 ${admindir}/${name})
    cur_best=$(LC_ALL=C ./alternatives --altdir ${altdir} --admindir ${admindir} --display TEST | grep best | cut -d " " -f5 | sed -e 's/\.$//' | xargs dirname | xargs basename)
    cur_spath=$(readlink ${altdir}/${sname} | xargs basename)
    echo $cur_spath
    echo
    echo -n "Path:  ${cur_path}"
    echo -e "\t(expected: ${path})"
    echo -n "State: ${cur_state}"
    echo -e "\t(expected: ${state})"
    echo -n "Best: ${cur_best}"
    echo -e "\t(expected: ${best})"
    echo -n "Slave: ${cur_spath}"
    echo -e "\t(expected: ${slave})"
    if [ "${state}" = "${cur_state}" ] && [ "${best}" = "${cur_best}" ] &&  [ "${path}" = "${cur_path}" ] && [ "${cur_spath}" = "${slave}" ] ; then
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

add_alternative link_a 10 ""
check_alternative link_a auto
clean_dir

add_alternative link_a 10 ""
set_alternative link_a
check_alternative link_a manual link_a
clean_dir

add_alternative link_a 10 ""
add_alternative link_b  20
check_alternative link_b auto
clean_dir

add_alternative link_a 20 ""
add_alternative link_b 10 ""
check_alternative link_a auto
clean_dir

add_alternative link_a 10 ""
set_alternative link_a
add_alternative link_b  20 ""
check_alternative link_a manual link_b
clean_dir

add_alternative link_a 10 ""
set_alternative link_a
add_alternative link_b  20 ""
remove_alternative link_a
check_alternative link_b auto
clean_dir

add_alternative link_a 10 "" slave_a
add_alternative link_a 10 "" slave_b
check_alternative link_a auto slave_b
clean_dir

add_alternative link_a 10 "" slave_a
set_alternative link_a
add_alternative link_a 10 "" slave_b
check_alternative link_a manual link_a slave_b
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

add_alternative link_a 10 ""
set_alternative link_a
add_alternative link_b 20 ""
add_alternative link_c 30 family_a
remove_alternative link_a
check_alternative link_c auto
clean_dir


echo "$rc fails"

rmdir $altdir $testdir $admindir

exit $rc
