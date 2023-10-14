<!-- -*-Mode: markdown;-*- -->
<!-- $Id$ -->


TAZeR (Transparent Asynchronous Zero-copy Remote I/O)
=============================================================================

**Home**:
  - <https://github.com/pnnl/tazer/>


**About**: TAZeR is a remote I/O framework that reduces effective data access latency. It was motivated by scientific workflow analytics. In these workloads inputs are large and read intensive, and include complex access patterns. Outputs are comparatively small and do not overwrite inputs, resulting in a simple data consistency model.

TAZeR combines state-of-the-art techniques to lower data access latencies and increase effective data movement bandwidth:
- TAZeR transparently converts POSIX I/O into operations that interleave application work with data transfer, using prefetching for reads and buffering for writes.
- TAZeR transfers data directly to/from application memory without synchronous intervention. We call this _soft zero-copy_ because any copying (for staging or buffering) occurs asynchronously after payload delivery, minimizing application blocking.
- TAZeR uses distributed bandwidth-aware staging to exploit reuse across application tasks and manage capacity constraints at each level of the memory/storage hierarchy.
- TAZeR is scalable: it introduces _no_ client-server bottlenecks: each client is ephemeral and connected to a task's process; all servers are associated with a persistent file system (and there can be multiple servers).


**Contacts**: (_firstname_._lastname_@pnnl.gov)
  - Ryan D. Friese
  - Joshua Suetterlein
  - Nathan R. Tallent


**Contributors**:
  - Ryan D. Friese (PNNL)
  - Joshua Suetterlein (PNNL)
  - Nathan R. Tallent (PNNL)



References
-----------------------------------------------------------------------------

* Ryan D. Friese, Burcu O. Mutlu, Nathan R. Tallent, Joshua Suetterlein, Jan Strube, "Effectively using remote I/O for work com- position in distributed workflows," in Proc. of the 2020 IEEE Intl. Conf. on Big Data, IEEE Computer Society, December 2020. <http://doi.org/10.1109/BigData50022.2020.9378352>

* Joshua Suetterlein, Ryan D. Friese, Nathan R. Tallent, and Malachi Schram, "TAZeR: Hiding the cost of remote I/O in distributed scientific workflows," in Proc. of the 2019 IEEE Intl. Conf. on Big Data, IEEE Computer Society, December 2019. <http://doi.org/10.1109/BigData47090.2019.9006418>

* Ryan D. Friese, Nathan R. Tallent, Malachi Schram, Mahantesh Halappanavar, and Kevin J. Barker, "Optimizing distributed data-intensive workflows," in Proc. of the 2018 IEEE Conf. on Cluster Computing, pp. 279–289, IEEE, September 2018. <http://doi.org/10.1109/CLUSTER.2018.00045>


Acknowledgements
-----------------------------------------------------------------------------

This work was supported by the U.S. Department of Energy's Office of
Advanced Scientific Computing Research:
- Integrated End-to-end Performance Prediction and Diagnosis

