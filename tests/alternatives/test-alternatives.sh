#!/bin/bash

# Include the BeakerLib environment
. /usr/share/beakerlib/beakerlib.sh

# Set the full test name
TEST="Test Alternatives"

# Package being tested
PACKAGE="alternatives"
if [ "${MERGED_SBIN}" = "1" ] ; then
    TEST_BIN="${TEST_PATH}alternatives-merged"
else
    TEST_BIN="${TEST_PATH}${PACKAGE}"
fi

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

    rlRun "${TEST_BIN} --altdir ${altdir} --admindir ${admindir} --install ${link} ${name} ${path} ${prio} --${FOLLOWER_OR_SLAVE} ${slink} ${sname} ${spath} ${family}" 0 "NEW\tlink: $1\tPrio: $prio\tFamily: $3"
}

function remove_alternative {
    path="${testdir}/$1/main"
    rm ${testdir}/$1/*
    rmdir ${testdir}/$1

    rlRun "${TEST_BIN} --altdir ${altdir} --admindir ${admindir} --remove ${name} ${path}" 0 "REMOVE\tlink: $1"
}

function set_alternative {
    path="${testdir}/$1/main"

    rlRun "${TEST_BIN} --altdir ${altdir} --admindir ${admindir} --set ${name} ${path}" 0 "SET\tlink: $1"
}

function add_follower {
    path="${testdir}/$1/main"
    follower=follower
    touch $path

    [ -n "$2" ] && follower=${2}

    spath="${testdir}/$1/$follower"
    touch ${spath}
    rlRun "${TEST_BIN} --altdir ${altdir} --admindir ${admindir} --add-${FOLLOWER_OR_SLAVE} ${name} ${path} ${slink} ${sname} ${spath}" 0 "NEW_FOLLOWER\tlink: $spath"
}

function remove_follower {
    path="${testdir}/$1/main"
    follower=follower
    touch $path

    [ -n "$2" ] && follower=${2}
    rlRun "${TEST_BIN} --altdir ${altdir} --admindir ${admindir} --remove-${FOLLOWER_OR_SLAVE} ${name} ${path} ${sname}" 0 "NEW_FOLLOWER\tlink: $spath"
}

function write_config {
    local config_name=$1
    local mode=$2
    local link_title=$3
    local follower_title=$4
    local follower_link=$5
    shift 5

    local config="${admindir}/${config_name}"
    echo "${mode}" > "${config}"
    echo "${link_title}" >> "${config}"
    if [ -n "${follower_title}" ] ; then
        echo "${follower_title}" >> "${config}"
        echo "${follower_link}" >> "${config}"
    fi
    echo "" >> "${config}"

    while [ $# -ge 2 ] ; do
        local target=$1
        local prio=$2
        shift 2
        echo "${target}" >> "${config}"
        echo "${prio}" >> "${config}"
        if [ -n "${follower_title}" ] ; then
            local ftarget=$1
            shift
            echo "${ftarget}" >> "${config}"
        fi
    done
}

function count_alts_in_config {
    local config="${admindir}/$1"
    local count=0
    local in_alts=0
    while IFS= read -r line; do
        if [ ${in_alts} -eq 0 ] ; then
            [ -z "${line}" ] && in_alts=1
            continue
        fi
        if [[ "${line}" =~ ^[0-9] ]] || [[ "${line}" =~ ^@ ]] ; then
            count=$((count + 1))
        fi
    done < "${config}"
    echo ${count}
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
    cur_best=$(LC_ALL=C ${TEST_BIN} --altdir "${altdir}" --admindir "${admindir}" --display TEST | grep best | cut -d " " -f5 | sed -e 's/\.$//' | xargs dirname | xargs basename)
    cur_spath=""
    # basename fails if the path is empty
    [ ! "$1" = EMPTY ] && cur_spath=$(readlink "${altdir}/${sname}" | xargs basename)
    echo $cur_spath
    rlAssertEquals "Mode:" "${state}" "${cur_state}"
    rlAssertEquals "Highest Priority:" "${best}" "${cur_best}"
    rlAssertEquals "Selected:" "${path}" "${cur_path}"
    rlAssertEquals "Follower:" "${cur_spath}" "${follower}"
}

rlJournalStart
    # Setup phase: Prepare test directory
    rlPhaseStartSetup
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
    done

    if [ "${MERGED_SBIN}" = "1" ] ; then

        rlPhaseStart FAIL "Dedup bin/sbin leader"
            # Create two alternatives that differ only in /usr/bin vs /usr/sbin
            mkdir -p "${testdir}/bin_link" "${testdir}/sbin_link"
            touch "${testdir}/bin_link/main" "${testdir}/bin_link/follower"
            touch "${testdir}/sbin_link/main" "${testdir}/sbin_link/follower"

            write_config "${name}" auto "${link}" "${sname}" "${slink}" \
                "${testdir}/bin_link/main" 10 "${testdir}/bin_link/follower" \
                "${testdir}/sbin_link/main" 10 "${testdir}/sbin_link/follower"

            # Also need the altdir symlink to exist
            ln -sf "${testdir}/bin_link/main" "${altdir}/${name}"
            ln -sf "${testdir}/bin_link/follower" "${altdir}/${sname}"

            # Display triggers readConfig which should NOT dedup (paths don't start with /usr/bin or /usr/sbin)
            # These paths are in testdir, so no dedup expected
            rlRun "${TEST_BIN} --altdir ${altdir} --admindir ${admindir} --display ${name}" 0 "Display before dedup"
            count=$(count_alts_in_config "${name}")
            rlAssertEquals "Both alternatives still present (no bin/sbin paths)" "${count}" "2"

            clean_dir
        rlPhaseEnd

        rlPhaseStart FAIL "Dedup real bin/sbin paths"
            # Write a config with /usr/bin/ and /usr/sbin/ paths that should be deduped
            mkdir -p "${testdir}/realbin" "${testdir}/realsbin"
            touch "${testdir}/realbin/main" "${testdir}/realsbin/main"

            write_config "${name}" auto "${link}" "" "" \
                "/usr/bin/testprog" 10 \
                "/usr/sbin/testprog" 10

            ln -sf "/usr/bin/testprog" "${altdir}/${name}"

            rlRun "${TEST_BIN} --altdir ${altdir} --admindir ${admindir} --display ${name}" 0 "Display triggers dedup"
            count=$(count_alts_in_config "${name}")
            rlAssertEquals "Duplicate removed, only one alternative remains" "${count}" "1"
            remaining=$(grep "^/usr/" "${admindir}/${name}")
            rlAssertEquals "Kept /usr/bin/ path" "${remaining}" "/usr/bin/testprog"

            clean_dir
        rlPhaseEnd

        rlPhaseStart FAIL "Dedup keeps bin over sbin"
            write_config "${name}" auto "${link}" "" "" \
                "/usr/sbin/myprog" 20 \
                "/usr/bin/myprog" 10

            ln -sf "/usr/sbin/myprog" "${altdir}/${name}"

            rlRun "${TEST_BIN} --altdir ${altdir} --admindir ${admindir} --display ${name}" 0 "Display triggers dedup (sbin first)"
            count=$(count_alts_in_config "${name}")
            rlAssertEquals "Duplicate removed" "${count}" "1"
            remaining=$(grep "^/usr/" "${admindir}/${name}")
            rlAssertEquals "Kept /usr/bin/ over /usr/sbin/" "${remaining}" "/usr/bin/myprog"

            clean_dir
        rlPhaseEnd

        rlPhaseStart FAIL "Dedup skips different follower count"
            mkdir -p "${testdir}/fc_a" "${testdir}/fc_b"
            touch "${testdir}/fc_a/main" "${testdir}/fc_a/follower"
            touch "${testdir}/fc_b/main"

            # alt A has a follower, alt B does not — they should NOT be deduped
            {
                echo "auto"
                echo "${link}"
                echo "${sname}"
                echo "${slink}"
                echo ""
                echo "/usr/bin/fcprog"
                echo "10"
                echo "${testdir}/fc_a/follower"
                echo "/usr/sbin/fcprog"
                echo "10"
            } > "${admindir}/${name}"

            ln -sf "/usr/bin/fcprog" "${altdir}/${name}"
            ln -sf "${testdir}/fc_a/follower" "${altdir}/${sname}"

            rlRun "${TEST_BIN} --altdir ${altdir} --admindir ${admindir} --display ${name}" 0 "Display with different follower count"
            count=$(count_alts_in_config "${name}")
            rlAssertEquals "Both alternatives kept (different follower count)" "${count}" "2"

            clean_dir
        rlPhaseEnd

        rlPhaseStart FAIL "Dedup with three alternatives"
            write_config "${name}" auto "${link}" "" "" \
                "/usr/bin/triprog" 10 \
                "/usr/sbin/triprog" 20 \
                "/usr/bin/otherprog" 30

            ln -sf "/usr/bin/otherprog" "${altdir}/${name}"

            rlRun "${TEST_BIN} --altdir ${altdir} --admindir ${admindir} --display ${name}" 0 "Display with three alts"
            count=$(count_alts_in_config "${name}")
            rlAssertEquals "One duplicate removed, two remain" "${count}" "2"
            # triprog bin/sbin deduped, otherprog kept
            rlRun "grep -q '/usr/bin/triprog' ${admindir}/${name}" 0 "bin/triprog kept"
            rlRun "grep -q '/usr/sbin/triprog' ${admindir}/${name}" 1 "sbin/triprog removed"
            rlRun "grep -q '/usr/bin/otherprog' ${admindir}/${name}" 0 "otherprog kept"

            clean_dir
        rlPhaseEnd

    fi

    # Cleanup phase: Remove test directory
    rlPhaseStartCleanup
        rlRun "rmdir $altdir $testdir $admindir"
    rlPhaseEnd
rlJournalEnd

# Print the test report
rlJournalPrintText
rlGetTestState
