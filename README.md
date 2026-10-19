<!-- -*-Mode: markdown;-*- -->
<!-- $Id$ -->


DataLife
=============================================================================

**Home**:
  - [DataLife](https://github.com/pnnl/DataLife), part of [DataFlowDrs](https://github.com/pnnl/DataFlowDrs)
  
  - [Performance Lab for EXtreme Computing and daTa](https://github.com/PerfLab-EXaCT)


**About (Technical)**: 
DataLife is a measurement and analysis toolset for distributed
scientific workflows that use I/O and storage for task
composition. DataLife performs _data flow lifecycle_ (DFL) analysis to
guide decisions regarding coordinating tasks and data flows on
distributed resources. DataLife provides measurement, analysis,
visualization, and opportunity identification for data flow lifecycles
(DFLs). With the aid of the DaYu module, it analyzes semantic
relationships between logical datasets and file addresses, how dataset
operations translate into I/O, and combinations of the two across
entire workflows.

DataLife analysis involves a profiling and post-mortem analysis
phase. The profiling is distributed and scalable; and supports I/O
through HDF5, POSIX, C I/O.  The post-mortem analysis provides several
helpful analyses and visualizations in order to extract workflow data
patterns, develop insights into the behavior of data flows, and
identify opportunities for both users and I/O libraries to optimize
improving task placement and data placement and resource assignment.


**About (General)**: 

New materials have the potential for improving solar generation,
creating new batteries, developing new health care treatments, and
enabling new techniques in computing. The problem is that new
materials with just the right properties are extremely hard to
find. The key to accelerating this discovery is automation of the
complex theory-experiment cycle that consists of guidance and
explanation from theory and experimental measurement and validation of
experimentalists. In other words, new computational techniques are
needed for the workflows that coordination large computational models,
hypothesis generation, instrument control, and experimental
interpretation and feedback.

Distributed scientific workflows pass information -- often large
volumes -- along chains of different computational tasks [in the
experiment-instrument-theory cycle], causing data flow bottlenecks in
storage and networks. We have developed DataLife, a measurement and
analysis toolset for these workflows. DataLife performs data flow
lifecycle (DFL) analysis to guide decisions regarding coordinating
task and data flows on distributed resources. DataLife provides tools
for measuring, analyzing, visualizing, and estimating the severity of
flow bottlenecks. DataLife's measurement introduces techniques that
deliver high precision while also imposing minimal overhead. The
bottleneck estimator provides several analyses and visualizations to
identify and rank opportunities for improving task and data placement
and resource assignment.

------------------------------------------------------------------------------

# Getting Started

See: [README-Install.txt](/README-Install.txt)


------------------------------------------------------------------------------

## Contacts

**Contacts**: (_firstname_._lastname_@pnnl.gov)
  - Nathan R. Tallent ([www](https://nathantallent.github.io))
  - Lenny Guo ([www](https://www.pnnl.gov/people/luanzheng-guo))
  - Jesun Firoz ([www](https://www.pnnl.gov/people/jesun-firoz))
  - Md Hasanur Rashid ([www](https://www.linkedin.com/in/hasanurrashid95/)) 
    <!-- https://www.linkedin.com/in/hasanurrashid95/ https://scholar.google.com.ec/citations?user=bxJd9ukAAAAJ&hl=fil -->
  - Meng Tang (Illinois Institute of Technology) ([www](https://scholar.google.com/citations?user=KXC9NesAAAAJ&hl=en))

  <!-- Hyungro Lee ([www](https://lee212.github.io/)) -->


**Contributors**:
  - Meng Tang (Illinois Institute of Technology) ([www](https://scholar.google.com/citations?user=KXC9NesAAAAJ&hl=en))
  - Lenny Guo ([www](https://www.pnnl.gov/people/luanzheng-guo))
  - Jesun Firoz ([www](https://www.pnnl.gov/people/jesun-firoz))
  - Zhen Peng ([www](https://johnpzh.github.io))
  - Md Hasanur Rashid ([www](https://www.linkedin.com/in/hasanurrashid95/))
  - Nathan R. Tallent ([www](https://nathantallent.github.io))
  - Hyungro Lee ([www](https://lee212.github.io/))


References
-----------------------------------------------------------------------------
- **Overview**: Nathan R. Tallent, Meng Tang, Zhen Peng, Jesun Firoz, Luanzheng Guo, Anthony Kougkas, and Xian-He Sun. "DataFlowDrs: Automating Performance Optimization of Data Flow Within HPC Workflows" IEEE Transactions on Parallel and Distributed Systems, pp. 1-18, September 2026 ([doi: 10.1109/TPDS.2026.3722592](https://doi.org/10.1109/IPDPS65963.2026.00112))

* **Specific**:  Hyungro Lee, Luanzheng Guo, Meng Tang, Jesun Firoz, Nathan Tallent, Anthony Kougkas, and Xian-He Sun. "Data Flow Lifecycles for Optimizing Workflow Coordination." Proc. of the Intl. Conf. for High Performance Computing, Networking, Storage and Analysis (SuperComputing), SC '23, Association for Computing Machinery, November 2023. ([doi: 10.1145/3581784.3607104](https://doi.org/10.1145/3581784.3607104))

- For all related references, see [DataFlowDrs](https://github.com/pnnl/DataFlowDrs)


## License

BSD 2-clause license: [README-License.txt](/README-License.txt)


Acknowledgements
-----------------------------------------------------------------------------

This work was supported by the U.S. Department of Energy's Office of
Advanced Scientific Computing Research:

- Orchestration for Distributed & Data-Intensive Scientific Exploration

