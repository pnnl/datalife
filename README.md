<!-- -*-Mode: markdown;-*- -->
<!-- $Id$ -->


DataLife
=============================================================================

**Home**:
  - https://github.com/pnnl/datalife
  
  - [Performance Lab for EXtreme Computing and daTa](https://github.com/perflab-exact)


**About (Technical)**: DataLife is measurement and analysis toolset for
distributed scientific workflows that use I/O and storage for task
composition. DataLife performs _data flow lifecycle_ (DFL) analysis to
guide decisions regarding coordinating tasks and data flows on
distributed resources. DataLife provides measurement, analysis,
visualization, and opportunity identification for data flow lifecycles
(DFLs). DataLife analysis involves a profiling and post-mortem
analysis phase. The profiling is distributed and scalable; and
currently focuses on POSIX and C I/O. The post-mortem analysis
provides several helpful analyses and visualizations to help identify
opportunities for improving task and data placement and resource
assignment.


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


**Contacts**: (_firstname_._lastname_@pnnl.gov)
  - Nathan R. Tallent ([www](https://hpc.pnnl.gov/people/tallent)), ([www](https://www.pnnl.gov/people/nathan-tallent))
  - Hyungro Lee ([www](https://www.pnnl.gov/science/staff/staff_info.asp?staff_num=10843)), ([www](https://lee212.github.io/))
  - Lenny Guo
  - Jesun Firoz

**Contributors**:
  - Hyungro Lee (PNNL) ([www](https://www.pnnl.gov/science/staff/staff_info.asp?staff_num=10843)), ([www](https://lee212.github.io/))
  - Lenny Guo (PNNL)
  - Jesun Firoz (PNNL)
  - Meng Tang (Illinois Institute of Technology)
  - Nathan R. Tallent (PNNL) ([www](https://hpc.pnnl.gov/people/tallent)), ([www](https://www.pnnl.gov/people/nathan-tallent))



References
-----------------------------------------------------------------------------

* H. Lee, L. Guo, M. Tang, J. S. Firoz, N. R. Tallent, A. Kougkas, and X.-H. Sun, "Data flow lifecycles for optimizing workflow coordination," in Proc. of the Intl. Conf. for High Performance Computing, Networking, Storage and Analysis (SuperComputing), November 2023



Acknowledgements
-----------------------------------------------------------------------------

This work was supported by the U.S. Department of Energy's Office of
Advanced Scientific Computing Research:

- Orchestration for Distributed & Data-Intensive Scientific Exploration



