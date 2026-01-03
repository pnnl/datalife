<!-- -*-Mode: markdown;-*- -->
<!-- $Id$ -->


DataLife
=============================================================================

**Home**:
  - https://github.com/pnnl/DataLife
  
  - [Performance Lab for EXtreme Computing and daTa](https://github.com/perflab-exact)

  - Related: 
  [DataFlowDrs](https://github.com/pnnl/DataFlowDrs)


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


**Contacts**: (_firstname_._lastname_@pnnl.gov)

  - Nathan R. Tallent ([www](https://nathantallent.github.io))
  - Lenny Guo ([www](https://www.pnnl.gov/people/luanzheng-guo))
  - Jesun Firoz ([www](https://www.pnnl.gov/people/jesun-firoz))
  - Meng Tang (Illinois Institute of Technology) ([www](https://scholar.google.com/citations?user=KXC9NesAAAAJ&hl=en))
  
  <!-- Hyungro Lee ([www](https://lee212.github.io/)) -->


**Contributors**:
  - Meng Tang (Illinois Institute of Technology) ([www](https://scholar.google.com/citations?user=KXC9NesAAAAJ&hl=en))
  - Lenny Guo ([www](https://www.pnnl.gov/people/luanzheng-guo))
  - Jesun Firoz ([www](https://www.pnnl.gov/people/jesun-firoz))
  - Zhen Peng ([www](https://johnpzh.github.io))
  - Nathan R. Tallent ([www](https://nathantallent.github.io))
  - Hyungro Lee ([www](https://lee212.github.io/))



References
-----------------------------------------------------------------------------

* H. Lee, L. Guo, M. Tang, J. Firoz, N. Tallent, A. Kougkas, and X.-H. Sun, “Data flow lifecycles for optimizing workflow coordination,” in Proc. of the Intl. Conf. for High Performance Computing, Networking, Storage and Analysis (SuperComputing), SC ’23, (New York, NY, USA), Association for Computing Machinery, November 2023. ([doi](https://doi.org/10.1145/3581784.3607104))

* M. Tang, J. Cernuda, J. Ye, L. Guo, N. R. Tallent, A. Kougkas, and X.-H. Sun, “DaYu: Optimizing distributed scientific workflows by decoding dataflow semantics and dynamics,” in Proc. of the 2024 IEEE Conf. on Cluster Computing, pp. 357–369, IEEE, September 2024. ([doi](https://doi.org/10.1109/CLUSTER59578.2024.00038))

* L. Guo, H. Lee, J. Firoz, M. Tang, and N. R. Tallent, “Improving I/O-aware workflow scheduling via data flow characterization and trade-off analysis,” in Seventh IEEE Intl. Workshop on Benchmarking, Performance Tuning and Optimization for Big Data Applications (Proc. of the IEEE Intl. Conf. on Big Data), IEEE Computer Society, December 2024. ([doi](https://doi.org/10.1109/BigData62323.2024.10825855))

* H. Lee, J. Firoz, N. R. Tallent, L. Guo, and M. Halappanavar, “FlowForecaster: Automatically inferring detailed & interpretable workflow scaling models for forecasts,” in Proc. of the 39th IEEE Intl. Parallel and Distributed Processing Symp., IEEE Computer Society, June 2025. ([doi](https://doi.org/10.1109/IPDPS64566.2025.00045))

* J. Firoz, H. Lee, L. Guo, M. Tang, N. R. Tallent, and Z. Peng, “FastFlow: Rapid workflow response by prioritizing critical data flows and their interactions,” in Proc. of the 37th Intl. Conf. on Scalable Scientific Data Management, ACM, June 2025. ([doi](https://doi.org/10.1145/3733723.3733735))

* M. Tang, Z. Zhu, L. Guo, J. G. Bandy, T. Carlson, S. Neuwirth, A. Kougkas, X.-H. Sun, and N. R. Tallent, “Quantifying AWS S3 I/O performance boundaries using the roofline model,” in Proc. of the SC ’25 Workshops of the Intl. Conf. for High Performance Computing, Networking, Storage and Analysis (10th Intl Parallel Data Systems Workshop), (New York, NY, USA), pp. 1415–1423, Association for Computing Machinery, 11 2025.

* M. Tang, L. Guo, A. Kougkas, X.-H. Sun, and N. R. Tallent, “Characterization and implications of dataflow in HPC workflows,” in Proc. of the 40th IEEE Intl. Parallel and Distributed Processing Symp., IEEE Computer Society, May 2026.

<!-- M. H. Rashid, J. Firoz, N. R. Tallent, L. Guo, M. Tang, and D. Dai, “QoSFlow: Ensuring QoS of distributed workflows using interpretable sensitivity models,” in Proc. of the 40th IEEE Intl. Parallel and Distributed Processing Symp., IEEE Computer Society, May 2026. -->


## Related
  
* C. Egersdoerfer, M. H. Rashid, D. Dai, B. Fang, and N. R. Tallent, “Understanding and predicting cross-application I/O interference in HPC storage systems,” in Proc. of the Workshops of the Intl. Conf. for High Performance Computing, Networking, Storage and Analysis (9th Intl. Parallel Data Systems Workshop), Nov. 2024. ([doi](https://doi.org/10.1109/SCW63240.2024.00174))

* M. H. Rashid, N. R. Tallent, F. S. Bao, and D. Dai, “CARAT: Client-side adaptive RPC and cache co-tuning for parallel file systems,” in Proc. of the 40th IEEE Intl. Parallel and Distributed Processing Symp., IEEE Computer Society, May 2026.



Acknowledgements
-----------------------------------------------------------------------------

This work was supported by the U.S. Department of Energy's Office of
Advanced Scientific Computing Research:

- Orchestration for Distributed & Data-Intensive Scientific Exploration



