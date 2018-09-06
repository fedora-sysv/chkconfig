#!/bin/bash

# Include the BeakerLib environment
. /usr/share/beakerlib/beakerlib.sh

# Set the full test name
TEST="Test Alternatives"

# Package being tested
PACKAGE="chkconfig"

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

    rlRun "./alternatives --altdir ${altdir} --admindir ${admindir} --install ${link} ${name} ${path} ${prio} --slave ${slink} ${sname} ${spath} ${family}" 0 "NEW\tlink: $1\tPrio: $prio\tFamily: $3"
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

function add_slave {
    path="${testdir}/$1/main"
    slave=slave
    touch $path

    [ -n "$2" ] && slave=${2}

    spath="${testdir}/$1/$slave"
    touch ${spath}
    rlRun "./alternatives --altdir ${altdir} --admindir ${admindir} --add-slave ${name} ${path} ${slink} ${sname} ${spath}" 0 "NEW_SLAVE\tlink: $spath"
}

function remove_slave {
    path="${testdir}/$1/main"
    slave=slave
    touch $path

    [ -n "$2" ] && slave=${2}
    rlRun "./alternatives --altdir ${altdir} --admindir ${admindir} --remove-slave ${name} ${path} ${sname}" 0 "NEW_SLAVE\tlink: $spath"
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

    if [ "$1" = EMPTY ] ; then
        slave=
    elif [ -n "$1" ] ; then
        slave=$1
    fi

    cur_path=$(readlink ${altdir}/${name} | xargs dirname | xargs basename)
    cur_state=$(head -1 ${admindir}/${name})
    cur_best=$(LC_ALL=C ./alternatives --altdir ${altdir} --admindir ${admindir} --display TEST | grep best | cut -d " " -f5 | sed -e 's/\.$//' | xargs dirname | xargs basename)
    cur_spath=$(readlink ${altdir}/${sname} | xargs basename)
    echo $cur_spath
    rlAssertEquals "Mode:" "${state}" "${cur_state}"
    rlAssertEquals "Highest Priority:" "${best}" "${cur_best}"
    rlAssertEquals "Selected:" "${path}" "${cur_path}"
    rlAssertEquals "Slave:" "${cur_spath}" "${slave}"
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
        rlRun 'slink="${testdir}/slave_link"'
    rlPhaseEnd

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
        
    rlPhaseStart FAIL "Slave"
        add_alternative link_a 10 "" slave_a
        add_alternative link_a 10 "" slave_b
        check_alternative link_a auto slave_b
        clean_dir
    rlPhaseEnd
        
    rlPhaseStart FAIL "Slave Manual"
        add_alternative link_a 10 "" slave_a
        set_alternative link_a
        add_alternative link_a 10 "" slave_b
        check_alternative link_a manual link_a slave_b
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

    rlPhaseStart FAIL "Dynamic Slave Add"
        add_alternative link_a 10 ""
        add_slave link_a slave_a
        check_alternative link_a auto slave_a
        clean_dir
    rlPhaseEnd

    rlPhaseStart FAIL "Dynamic Slave Add Auto"
        add_alternative link_a 10 ""
        add_alternative link_b 20 ""
        add_alternative link_c 5 ""
        add_slave link_a slave_a
        add_slave link_b slave_b
        check_alternative link_b auto slave_b
        clean_dir
    rlPhaseEnd

    rlPhaseStart FAIL "Dynamic Slave Add Manual"
        add_alternative link_a 10 ""
        add_alternative link_b 20 ""
        add_alternative link_c 5 ""
        add_slave link_a slave_a
        add_slave link_b slave_b
        set_alternative link_a
        check_alternative link_a manual link_b slave_a
        clean_dir
    rlPhaseEnd

    rlPhaseStart FAIL "Dynamic Slave Add And Remove Master"
        add_alternative link_a 10 ""
        add_alternative link_b 20 ""
        add_alternative link_c 5 ""
        add_slave link_a slave_a
        add_slave link_b slave_b
        remove_alternative link_b
        check_alternative link_a auto slave_a
        clean_dir
    rlPhaseEnd

    rlPhaseStart FAIL "Dynamic Slave Remove"
        add_alternative link_a 10 ""
        add_slave link_a slave_a
        remove_slave link_a slave_a
        check_alternative link_a auto EMPTY
        clean_dir
    rlPhaseEnd

    rlPhase

    # Cleanup phase: Remove test directory
    rlPhaseStartCleanup
        rlRun "rmdir $altdir $testdir $admindir"
    rlPhaseEnd
rlJournalEnd

# Print the test report
rlJournalPrintText

