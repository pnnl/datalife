<!-- -*-Mode: markdown;-*- -->
<!-- $Id$ -->


Prerequisites
=============================================================================

Environment
  - CMake (>= version 2.8)
  - C++14 compiler, GCC preferred
  - Python 3.7+



Building & Installing
=============================================================================

....

1. Common build options:

   - `CMAKE_INSTALL_PREFIX=<path>`: Install path

   - `-DENABLE_FlowMonitor=<ON|OFF>`: Enable build of FlowMonitor

   - `-DENABLE_FlowAnalysis=<ON|OFF>`: Enable build of FlowAnalysis
   
   - `-DCMAKE_CXX_COMPILER=<path>`: Path for C++ compiler

   - All options:
   ```sh
   cmake -LH <path-to-datalife-root>
   cmake -LA <path-to-datalife-root>
   ```

2. Build the datalife (monitor and analysis) within directory _build_:
   ```sh
   mkdir <build> && cd <build>
   cmake \
     -DCMAKE_INSTALL_PREFIX=<install-path> \
     <datalife-root-path>
   make install
   ```

   Individual packages can also be built, e.g.:
   ```sh
   cd flow-monitor
   mkdir <build> && cd <build>
   cmake ...
   make install
   ```

   <!-- mkdir BUILD && cd BUILD && cmake -DCMAKE_INSTALL_PREFIX=`pwd`/../INSTALL -DENABLE_FlowMonitor=OFF ..   -->

Using
=============================================================================

DataLife has two main steps.

1. Monitor...
   ```sh
    # Give your path to collect all datalife statistic files
    export DATALIFE_OUTPUT_PATH="./datalife_stats"
    # Give your list of target capture file regular expression patterns
    export DATALIFE_FILE_PATTERNS="*.gz, *.tar.gz, *.dcd, ior*.bin" 
    # Run your program with datalife with LD_PRELOAD
    LD_PRELOAD=/your_datalife_path/build/flow-monitor/src/libmonitor.so ./your_program
    # or Run your program with datalife-run
    export PATH="/your_datalife_path/build/bin:$PATH"
    datalife-run ./your_program
    ```

2. Analysis and diagnostics:

   ```sh
    datalife-analyze
    usage: datalife-analyze [-h] [-i INPUT] [-o OUTPUT]

    datalife-analyze produces data flow lifecycle (DFL) graph to guide
    decisions regarding coordinating tasks and data flows on distributed
    resources.

    optional arguments:
      -h, --help            show this help message and exit
      -i INPUT, --input INPUT
                            read I/O monitor stats from directory path
      -o OUTPUT, --output OUTPUT
                            write a graph output to a file
    ```

   <!-- export PYTHONPATH+="<install>/libexec/flow-analysis"  -->


Example: 1000 Genomes workflow
=============================================================================

1. Build the flow-monitor (CMake >= 3.24 for `--fresh`, Ninja, GCC):

   ```sh
   cmake -G Ninja -S <datalife-root> -B <build> \
     -DCMAKE_BUILD_TYPE=Release \
     -DCMAKE_INSTALL_PREFIX=<install> \
     -DENABLE_FlowMonitor=ON \
     -DENABLE_FlowAnalysis=OFF \
     -DTIMER_JSON=ON \
     -DINT_DOT=ON \
     --fresh
   cmake --build <build>
   cmake --install <build>
   ls <install>/lib/libmonitor.so
   ```

   `TIMER_JSON=ON` writes the per-process timer file
   `monitor_timer.<pid>-<host>.datalife.json` (I/O time, task time, file
   sizes).

2. Set the environment for the workflow:

   ```sh
   export MONITOR_LIB=<install>/lib/libmonitor.so
   export DATALIFE_OUTPUT_PATH=<trace-dir>            # all trace files go here
   export DATALIFE_FILE_PATTERNS='*.gz, *.tar.gz, *.vcf, sift*'   # files to trace (basename globs)
   export DATALIFE_JSON_OUTPUT=0   # 0: legacy *_r_stat/*_w_stat histograms; 1: *_blk_trace.json per file
   mkdir -p "$DATALIFE_OUTPUT_PATH"
   ```

3. Launch every task with the monitor preloaded and a task name. In a Slurm
   batch script each task is an `srun` that exports `LD_PRELOAD` and
   `DATALIFE_TASK_NAME`:

   ```sh
   export WF_SRUN_EXPORT="ALL,LD_PRELOAD=${MONITOR_LIB}"

   # stage 1: 300 individuals tasks (10 chromosomes x 30 chunks)
   srun --export="$WF_SRUN_EXPORT,DATALIFE_TASK_NAME=individuals" -n1 -N1 \
        individuals.py ALL.chr1.250000.vcf 1 1 201 6000 &
   # stage 2: individuals_merge (10) + sifting (10)
   srun --export="$WF_SRUN_EXPORT,DATALIFE_TASK_NAME=individuals_merge" -n1 -N1 \
        individuals_merge.py 1 chr1n-1-201.tar.gz ... chr1n-5801-6001.tar.gz &
   srun --export="$WF_SRUN_EXPORT,DATALIFE_TASK_NAME=sifting" -n1 -N1 \
        sifting.py ALL.chr1.phase3_shapeit2_mvncall_integrated_v5.20130502.sites.annotation.vcf 1 &
   # stage 3: mutation_overlap (70) + frequency (70), one per chromosome x population
   srun --export="$WF_SRUN_EXPORT,DATALIFE_TASK_NAME=mutation_overlap" -n1 -N1 \
        mutation_overlap.py -c 1 -pop SAS &
   srun --export="$WF_SRUN_EXPORT,DATALIFE_TASK_NAME=frequency" -n1 -N1 \
        frequency.py -c 1 -pop SAS &
   wait
   ```

   Submit with `sbatch`; the trace files appear in `DATALIFE_OUTPUT_PATH`
   as the tasks finish. Any task started without `LD_PRELOAD` simply runs
   uninstrumented.

   If the monitor was built with a compiler whose `libstdc++` is newer than
   the system one (a `gcc` module on an older OS, or a conda Python with its
   own `libstdc++`), preload that runtime first, otherwise tasks fail with
   `version GLIBCXX_... not found`:

   ```sh
   export LIBSTDCPP="$(g++ -print-file-name=libstdc++.so.6)"
   export WF_SRUN_EXPORT="ALL,LD_PRELOAD=${LIBSTDCPP}:${MONITOR_LIB}"
   ```

4. What is produced per traced process:

   ```sh
   monitor_timer.<pid>-<host>.datalife.json     # I/O time per call type, task wall/compute time, file sizes
   <file>_<pid>_r_stat / _w_stat                # block histograms (DATALIFE_JSON_OUTPUT=0)
   <file>.<pid>-<host>.r_blk_trace.json / .w_blk_trace.json   #per-file access record (DATALIFE_JSON_OUTPUT=1)
   ```

   A complete, runnable version of this setup for NERSC Perlmutter is in
   `datalife_histogram_loader/scripts/` (`build_datalife.sh`,
   `run_1kg_datalife_json_format.sh`, `profiled-baseline.v2.json_task_name.sbatch`),
   together with scripts that summarize the traces per task.
