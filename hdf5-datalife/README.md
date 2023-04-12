# DataLife
Currently contains code to generate the data lifecycle sankey graph for both the [sim_emulator.py and aggregate.py programs](https://gitlab.pnnl.gov/perf-lab/workflows/deepdrivemd/-/tree/main/examples/co-scheduling).

## Files in Folders analysis_scripts
#### io_vol_vfd.ipynb
Inputs:
- VOL and VFD logs are stored as plaintext files, one file for a single python program.
Analysis:
- The script maps the VOL and VFD logs with common parameters if exist, and with observed patterns knowledge.
- It converts I/O traces from VOL-VFD.log to dataframes.
- Dataframe are stored in csv format. (For better performance, store in parquet format)
#### detailed_VOLVFD_Graph_SimAgg.ipynb
From stored dataframe (.csv or .parquet files), this cript build NetworkX and Sankey Diagram. \
Each vertex represetns a task (red), a single vol-object data access (orange), a single posix-address I/O operation (light blue), and a file (dark blue). \
Since the graph can become large with the recorded I/O, a parameter `nshow` is used in the beginning to control roughly how many data-access and I/O nodes are shown. \
A parameter `DATASET` can be used to select the dataset you want to generate graph for.
#### grouped_VOLVFD_Graph_SimAgg.ipynb
From stored dataframe (.csv or .parquet files), this cript build NetworkX and Sankey Diagram. \
Each vertex represetns a task (red), a group of vol-object data accesses (orange), a group of posix-address I/O operation that corresponds to the vol-object accesses (light blue), and a file (dark blue). \
A parameter `group_cnt` is used in the beginning to control how many groups of vol-object the graph would display.

#### old_scripts
Archive of old example scripts.


## Folders sankey_duiagram
Have a more detailed README.md describing the graph components.
## Folders sankey_duiagram/detailed_graph
3 Images of NetworkX graph:
- Simulation
- Aggregation Read
- Aggregation Write
3 Images of Sankey graph:
- Simulation
- Aggregation
- Simulation and Aggregation.
## Folders sankey_duiagram/group_graph
5 Images of NetworkX graph:
- Simulation
- Aggregation Read
- Aggregation Write
- Simulation and Aggregation Read
- Simulation and Aggregation (both Read and Write)
3 Images of Sankey graph:
- Simulation
- Aggregation
- Simulation and aggregation.
