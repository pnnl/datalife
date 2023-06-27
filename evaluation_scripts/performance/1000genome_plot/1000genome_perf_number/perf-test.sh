#!/bin/bash

SCRIPT_DIR=/home/cc/datalife/evaluation_scripts/performance/1000genome_plot/1000genome_perf_number

SUMMARY_LOG=all_perf_test_sum.log
# rm -rf $OUTPUT_LOG


TEST_STORAGES=(SHM HDD SSD NVME)

RUN_ALL_TESTS() {
    # Run tests on all storages
    for TEST_STOR in "${TEST_STORAGES[@]}"; do
        OUTPUT_LOG="$TEST_STOR-PERF-TEST.log"
        bash "$SCRIPT_DIR/1kgenome_1node_parallel_tierd.sh" "$TEST_STOR" > "$OUTPUT_LOG"

        echo "$TEST_STOR performance :" | tee -a "$SUMMARY_LOG"
        cat "$OUTPUT_LOG" | grep "All done (msec)" | tee -a "$SUMMARY_LOG"
    done
}

RUN_ALL_TESTS

echo "** All tests done **"

# TEST_STOR="HDD"
# OUTPUT_LOG="$TEST_STOR-PERF-TEST.log"
# bash $SCRIPT_DIR/1kgenome_1node_parallel_tierd.sh $TEST_STOR $> $OUTPUT_LOG