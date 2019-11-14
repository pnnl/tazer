-*-Mode: markdown;-*-
=============================================================================

Requirements:
  1. C++11 compiler, GCC preferred
  2. Cmake (>= version 2.6)

To build within directory <build>:
  ```sh
  mkdir <build> && cd <build>
  cmake \
    -DCMAKE_INSTALL_PREFIX=<install> \
    <path-to-tazer-root>
  make install
  ```

You may also wish to use:
- `-DCMAKE_C_COMPILER=...`
- `-DCMAKE_CXX_COMPILER=...`

=============================================================================
Server
=============================================================================

Usage: `server -p <port-to-listen-on> -l (optional) <path-to-store-load-logs>`
  `./server -p 5001 -l loadLogs/`

Functionality: Listen for incoming file requests. Requests can either be to 
serve a file, or to save a file. Serving a file corresponds to the client 
reading an input file. Saving a file corresponds to the client writing to an 
output file. A single client can simultaneously request multiple files (both 
serve and save files). Each file request opens a new connection between the 
client and the server

"Load" logging currently the server captures and stores the number of active 
connections, network transfers, and disk accesses.

Format:
  Connection-id Client-name Timestamp Current-load

Two versions of loads are calculated.
1. The first is the total (raw) loads.
   -calculates loads with respect to individual files
2. The second is normalized loads.
   -normalizes loads with respect to individual clients

Debug logging:
To ease debugging, the stdout/stderr associated with each connection
can be saved into individual files setting the environment variable
`IPPD_LOG=1` enables this feature. Setting `IPPD_LOG_PATH` sets where
these debug logs should be saved.
Default: `IPPD_LOG_PATH=./`


=============================================================================
Client_lib
=============================================================================

Usage: `LD_PRELOAD=path-to-lib.so <application> <app-args>`

lib.so intercepts common i/o system calls (e.g. read, write, open, close) made 
within application code. lib.so is designed to operate on special ".meta" files,
if a regular file is detected, it is passed through to the normal i/o call. 
There are two types of meta files, meta.in (representing input files (to be 
transfered FROM the server)), and meta.out (to be transfered TO the server). The
meta files contain the information necessary for contacting a server and 
transferring a specific file.

meta file format:
    `server-ip-address:server-port:compression:unused:unused:block_size:file-location-on-server|`
ex:
    `127.0.0.1:5001:1:0:0:16777216:/home/user/data/test_file|`

For input files the file location is where on the server the needed file is located.
For output files the file location is where it will be saved to on the file.

After a meta file has been loaded, a connection with the server will be 
initiated, and data will begin to be transfered.

Debug logging:
To ease debugging, the stdout/stderr associated with each connection
can be saved into individual files setting the environment variable
`IPPD_LOG=1` enables this feature. Setting `IPPD_LOG_PATH` sets where
these debug logs should be saved.
Default: `IPPD_LOG_PATH=./`


=============================================================================
Tazer_cp
=============================================================================

`ippd_cp` is a simple utility used to emulate the unix "cp/scp" utilities.

usage: `ippd_cp src dst`
This will copy file at "src" to "dst"

examples:
* Copy local file to local destination
  `./ippd_cp client_test_file.txt client_test_file.txt.temp`
  (in `ippd_cp` dir) `client_test_file.txt.temp` should contain "client test file"*

* Copy remote file to local destination
  `LD_PRELOAD=../client_lib/lib.so ./ippd_cp server_test_file.txt.meta.in server_test_file.txt`
  (in `ippd_cp` dir) `server_test_file.txt` should contain "server test file"*

* Copy local file to remote destination
  `LD_PRELOAD=../client_lib/lib.so ./ippd_cp client_test_file.txt client_test_file.txt.meta.out`
  (in server dir) `client_test_file.txt.temp` should contain "client test file"*

* Copy remote file to remote destination
  `LD_PRELOAD=../client_lib/lib.so ./ippd_cp server_test_file.txt.meta.in client_test_file.txt.meta.out`
  (in server dir) `client_test_file.txt.temp` should contain "server test file"*


