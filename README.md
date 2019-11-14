-*-Mode: markdown;-*-

TAZeR (Transparent Asynchronous Zero-copy Remote I/O)
============================================================

* URL:
  - https://github.com/pnnl/tazer/

* People
  - Joshua Suetterlein
  - Ryan D. Friese
  - Nathan R. Tallent

* Contacting:
  <firstname>.<lastname>@pnnl.gov

Description:
------------

TAZeR is a remote I/O framework that reduces effective data access latency. It was motivated by scientific workflow analytics. In these workloads inputs are large and read intensive, and include complex access patterns. Outputs are comparatively small and do not overwrite inputs, resulting in a simple data consistency model.

TAZeR combines state-of-the-art techniques to lower data access latencies and increase effective data movement bandwidth:
- TAZeR transparently converts POSIX I/O into operations that interleave application work with data transfer, using prefetching for reads and buffering for writes.
- TAZeR transfers data directly to/from application memory without synchronous intervention. We call this _soft zero-copy_ because any copying (for staging or buffering) occurs asynchronously after payload delivery, minimizing application blocking.
- TAZeR uses distributed bandwidth-aware staging to exploit reuse across application tasks and manage capacity constraints at each level of the memory/storage hierarchy.
- TAZeR is scalable: it introduces _no_ client-server bottlenecks: each client is ephemeral and connected to a task's process; all servers are associated with a persistent file system (and there can be multiple servers).


References:
------------

* Joshua Suetterlein, Ryan D. Friese, Nathan R. Tallent, and Malachi Schram, "TAZeR: Hiding the cost of remote I/O in distributed scientific workflows," in Proc. of the 2019 IEEE Intl. Conf. on Big Data, IEEE Computer Society, December 2019.
