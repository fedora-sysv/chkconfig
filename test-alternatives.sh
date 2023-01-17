#!/bin/bash

# Include the BeakerLib environment
. /usr/share/beakerlib/beakerlib.sh

# Set the full test name
TEST="Test Alternatives"

# Package being tested
PACKAGE="chkconfig"

# We need to test both new "leader/follower" and legacy "master/slave" options
FOLLOWER_OR_SLAVE="follower"

function clean_dir {
    [ -n "${altdir}" ] && [ -d "${altdir}" ] && rm ${altdir}/* &> /dev/null
    [ -n "${admindir}" ] && [ -d "${admindir}" ] && rm ${admindir}/* &> /dev/null

    if [ -n "${testdir}" ] && [ -d "${testdir}" ] ; then
        for i in "${testdir}"/* ; do
            if [ -d "${i}" ] ; then
                rm "${i}"/*
                rmdir "${i}"
            else
                rm "${i}"
            fi
        done
    fi
}

function add_alternative {
    path="${testdir}/$1/main"
    prio=$2
    family=
    follower=follower
    mkdir -p "${testdir}/$1"
    touch $path

    [ -n "$3" ] && family="--family $3"

    [ -n "$4" ] && follower=${4}

    spath="${testdir}/$1/$follower"
    touch ${spath}

    rlRun "./alternatives --altdir ${altdir} --admindir ${admindir} --install ${link} ${name} ${path} ${prio} --${FOLLOWER_OR_SLAVE} ${slink} ${sname} ${spath} ${family}" 0 "NEW\tlink: $1\tPrio: $prio\tFamily: $3"
}

function remove_alternative {
    path="${testdir}/$1/main"
    rm ${testdir}/$1/*
    rmdir ${testdir}/$1

    rlRun "./alternatives --altdir ${altdir} --admindir ${admindir} --remove ${name} ${path}" 0 "REMOVE\tlink: $1"
}

function set_alternative {
    path="${testdir}/$1/main"

    rlRun "./alternatives --altdir ${altdir} --admindir ${admindir} --set ${name} ${path}" 0 "SET\tlink: $1"
}

function add_follower {
    path="${testdir}/$1/main"
    follower=follower
    touch $path

    [ -n "$2" ] && follower=${2}

    spath="${testdir}/$1/$follower"
    touch ${spath}
    rlRun "./alternatives --altdir ${altdir} --admindir ${admindir} --add-${FOLLOWER_OR_SLAVE} ${name} ${path} ${slink} ${sname} ${spath}" 0 "NEW_FOLLOWER\tlink: $spath"
}

function remove_follower {
    path="${testdir}/$1/main"
    follower=follower
    touch $path

    [ -n "$2" ] && follower=${2}
    rlRun "./alternatives --altdir ${altdir} --admindir ${admindir} --remove-${FOLLOWER_OR_SLAVE} ${name} ${path} ${sname}" 0 "NEW_FOLLOWER\tlink: $spath"
}

function check_alternative {
    path=$1
    shift
    state=$1
    shift
    follower=follower

    if [ "$state" = "manual" ] ; then
         best=${1}
         shift
    else
         best=${path}
    fi

    if [ "$1" = EMPTY ] ; then
        follower=
    elif [ -n "$1" ] ; then
        follower=$1
    fi

    cur_path=$(readlink ${altdir}/${name} | xargs dirname | xargs basename)
    cur_state=$(head -1 ${admindir}/${name})
    cur_best=$(LC_ALL=C ./alternatives --altdir ${altdir} --admindir ${admindir} --display TEST | grep best | cut -d " " -f5 | sed -e 's/\.$//' | xargs dirname | xargs basename)
    cur_spath=$(readlink ${altdir}/${sname} | xargs basename)
    echo $cur_spath
    rlAssertEquals "Mode:" "${state}" "${cur_state}"
    rlAssertEquals "Highest Priority:" "${best}" "${cur_best}"
    rlAssertEquals "Selected:" "${path}" "${cur_path}"
    rlAssertEquals "Follower:" "${cur_spath}" "${follower}"
}

rlJournalStart
    # Setup phase: Prepare test directory
    rlPhaseStartSetup
        rlAssertRpm $PACKAGE
        rlRun 'altdir=$(mktemp -d)' 0 'Creating tmp directory' # no-reboot
        rlRun 'admindir=$(mktemp -d)' 0 'Creating tmp directory' # no-reboot
        rlRun 'testdir=$(mktemp -d)' 0 'Creating tmp directory' # no-reboot

        rlRun 'name="TEST"'
        rlRun 'link="${testdir}/main_link"'

        rlRun 'sname="STEST"'
        rlRun 'slink="${testdir}/follower_link"'
    rlPhaseEnd

    for n in "follower" "slave"  ; do

    FOLLOWER_OR_SLAVE=$n
        # Test phase: Testing touch, ls and rm commands
        rlPhaseStart FAIL "Create Alternative"
            add_alternative link_a 10 ""
            check_alternative link_a auto
            clean_dir
        rlPhaseEnd
            
        rlPhaseStart FAIL "Set Manual"
            add_alternative link_a 10 ""
            set_alternative link_a
            check_alternative link_a manual link_a
            clean_dir
        rlPhaseEnd
            
        rlPhaseStart FAIL "Auto Priority Ascendant"
            add_alternative link_a 10 ""
            add_alternative link_b  20
            check_alternative link_b auto
            clean_dir
        rlPhaseEnd
            
        rlPhaseStart FAIL "Auto Priority Descendant"
            add_alternative link_a 20 ""
            add_alternative link_b 10 ""
            check_alternative link_a auto
            clean_dir
        rlPhaseEnd
            
        rlPhaseStart FAIL "Manual Overrides Best"
            add_alternative link_a 10 ""
            set_alternative link_a
            add_alternative link_b  20 ""
            check_alternative link_a manual link_b
            clean_dir
        rlPhaseEnd
            
        rlPhaseStart FAIL "Remove Manually Set"
            add_alternative link_a 10 ""
            set_alternative link_a
            add_alternative link_b  20 ""
            remove_alternative link_a
            check_alternative link_b auto
            clean_dir
        rlPhaseEnd
            
        rlPhaseStart FAIL "Follower"
            add_alternative link_a 10 "" follower_a
            add_alternative link_a 10 "" follower_b
            check_alternative link_a auto follower_b
            clean_dir
        rlPhaseEnd
            
        rlPhaseStart FAIL "Follower Manual"
            add_alternative link_a 10 "" follower_a
            set_alternative link_a
            add_alternative link_a 10 "" follower_b
            check_alternative link_a manual link_a follower_b
            clean_dir
        rlPhaseEnd
            
            ##########
            
        rlPhaseStart FAIL "Family Priority Ascendant"
            add_alternative link_a 10 family_a
            add_alternative link_b 20 family_a
            check_alternative link_b auto
            clean_dir
        rlPhaseEnd
            
        rlPhaseStart FAIL "Family Priority Descendant"
            add_alternative link_a 20 family_a
            add_alternative link_b 10 family_a
            check_alternative link_a auto
            clean_dir
        rlPhaseEnd
            
        rlPhaseStart FAIL "Families Priority Ascendant"
            add_alternative link_a 10 family_a
            add_alternative link_b 20 family_b
            check_alternative link_b auto
            clean_dir
        rlPhaseEnd
            
        rlPhaseStart FAIL "Families Priority Descendant"
            add_alternative link_a 20 family_a
            add_alternative link_b 10 family_b
            check_alternative link_a auto
            clean_dir
        rlPhaseEnd
            
        rlPhaseStart FAIL "Families Priority Ascendant Multiple"
            add_alternative link_a 10 family_a
            add_alternative link_b 20 family_a
            add_alternative link_c 30 family_b
            check_alternative link_c auto
            clean_dir
        rlPhaseEnd
            
        rlPhaseStart FAIL "Families Remove Manually Set"
            add_alternative link_a 10 family_a
            set_alternative link_a
            add_alternative link_c 30 family_b
            remove_alternative link_a
            check_alternative link_c auto
            clean_dir
        rlPhaseEnd
            
        rlPhaseStart FAIL "Families Remove Link After Manually Set Multiple"
            add_alternative link_a 10 family_a
            set_alternative link_a
            add_alternative link_b 20 family_a
            add_alternative link_c 30 family_b
            remove_alternative link_a
            check_alternative link_b manual link_c
            clean_dir
        rlPhaseEnd
            
        rlPhaseStart FAIL "Family After Remove Manually Set"
            add_alternative link_a 10 ""
            set_alternative link_a
            add_alternative link_b 20 ""
            add_alternative link_c 30 family_a
            remove_alternative link_a
            check_alternative link_c auto
            clean_dir
        rlPhaseEnd

            ##########

        rlPhaseStart FAIL "Dynamic Follower Add"
            add_alternative link_a 10 ""
            add_follower link_a follower_a
            check_alternative link_a auto follower_a
            clean_dir
        rlPhaseEnd

        rlPhaseStart FAIL "Dynamic Follower Add Auto"
            add_alternative link_a 10 ""
            add_alternative link_b 20 ""
            add_alternative link_c 5 ""
            add_follower link_a follower_a
            add_follower link_b follower_b
            check_alternative link_b auto follower_b
            clean_dir
        rlPhaseEnd

        rlPhaseStart FAIL "Dynamic Follower Add Manual"
            add_alternative link_a 10 ""
            add_alternative link_b 20 ""
            add_alternative link_c 5 ""
            add_follower link_a follower_a
            add_follower link_b follower_b
            set_alternative link_a
            check_alternative link_a manual link_b follower_a
            clean_dir
        rlPhaseEnd

        rlPhaseStart FAIL "Dynamic Follower Add And Remove Leader"
            add_alternative link_a 10 ""
            add_alternative link_b 20 ""
            add_alternative link_c 5 ""
            add_follower link_a follower_a
            add_follower link_b follower_b
            remove_alternative link_b
            check_alternative link_a auto follower_a
            clean_dir
        rlPhaseEnd

        rlPhaseStart FAIL "Dynamic Follower Remove"
            add_alternative link_a 10 ""
            add_follower link_a follower_a
            remove_follower link_a follower_a
            check_alternative link_a auto EMPTY
            clean_dir
        rlPhaseEnd

        rlPhase
    done

    # Cleanup phase: Remove test directory
    rlPhaseStartCleanup
        rlRun "rmdir $altdir $testdir $admindir"
    rlPhaseEnd
rlJournalEnd

# Print the test report
rlJournalPrintText

