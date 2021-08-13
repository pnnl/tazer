create_tazer_files:

This is a utility to create TAZeR metafiles for the given files.



Arguments List:
    -b, --blocksize <blocksize>        TAZeR blocksize [default: 1048576]
    -c, --compression <compression>    use compression [default: false]
    -e, --extension <extension>        add an extension to the new TAZeR file names [default: ]
    -f, --flat <flat>                  write TAZeR files to flat directory [default: false]
    -o, --outputpath <outputpath>      path to output directory [default: ./tazer]
    -p, --path <path>                  path to files
        --prefetch <prefetch>          use prefetching [default: false]
        --savelocal <savelocal>        use savelocal [default: false]
    -s, --server <server>...           server address and port seperated by a ':' <server address>:<port> [default:localhost:6023]
    -r, --serverroot <serverroot>      root path of files on server [default: ./]
    -t, --type <type>                  the tazer file type must be either 'input', 'output', or 'local' [default: input]
    -v, --version <version>            the current tazer version [default: TAZER0.1]



Examples:

Create a single metafile for a given file:
    "cargo run -- --path=files/file1 --version=TAZER0.1 --type=input --blocksize=1048576 --compression=false --prefetch=false --savelocal=false 
        --server=127.0.0.1:5001 --serverroot=/tmp/server_data/ --outputpath=/tmp/metafiles"

    Produces a metafile called /tmp/metafiles/file1, with the following contents:
    TAZER0.1
    type=input
    [server]
    host=127.0.0.1
    port=5001
    file=/tmp/server_data/files/file1
    block_size=1048576
    compress=false
    prefetch=false
    save_local=false

Create a single metafile with an added extension:
    "cargo run -- --path=files/file1 --version=TAZER0.1 --type=input --blocksize=1048576 --compression=false --prefetch=false --savelocal=false
        --server=127.0.0.1:5001 --serverroot=/tmp/server_data/ --outputpath=/tmp/metafiles --extension=.meta.in"

    Produces a metafile called /tmp/metafiles/file1.meta.in with the same contents as the previous example.


Recursively create metafiles for each file in a given path:
    "cargo run -- --path=/tmp/files/ --version=TAZER0.1 --type=input --blocksize=1048576 --compression=false --prefetch=false --savelocal=false 
        --server=127.0.0.1:5001 --serverroot=/tmp/server_data/ --outputpath=/tmp/metafiles"

    Produces the following metafiles:
    /tmp/metafiles/file1
    /tmp/metafiles/subdir/file3
    /tmp/metafiles/file2
    

Recursively create metafiles for each file in a given path with a flat output directory:
    "cargo run -- --path=/tmp/files/ --version=TAZER0.1 --type=input --blocksize=1048576 --compression=false --prefetch=false --savelocal=false 
        --server=127.0.0.1:5001 --serverroot=/tmp/server_data/ --outputpath=/tmp/metafiles --flat=true"

    Produces the following metafiles all in a single directory:
    /tmp/metafiles/file1
    /tmp/metafiles/file3
    /tmp/metafiles/file2