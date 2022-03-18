convert_tazer_metafile:

Converts TAZeR metafiles from the old format to the new format.

Example old format:
    127.0.0.1:5001:1:0:0:16777216:/home/user/data/test_file|

Example new format:
    TAZER0.1
    type=input
    [server]
    host=127.0.0.1
    port=5001
    file=/home/user/data/test_file
    compress=true
    prefetch=false
    save_local=false
    block_size=16777216



Arguments List:
    <input path> <output path>
    -r, --recursive    Convert all metafiles of the input directory path to the new format.
    -e, --extension <extension>    Add an extension to the end of the new metafile names and remove the old extension.

    

Examples:

Convert a single metafile to the new format:
    "cargo run -- /tmp/old_metafiles/test_file.meta.in /tmp/new_metafiles/test_metafile"


Convert all metafiles in a given path to the new format:
    "cargo run -- --recursive /tmp/old_metafiles/ /tmp/new_metafiles/"

    Produces output directory:
        new_metafiles/test_file
        new_metafiles/test_file2
        new_metafiles/test_file3
        new_metafiles/subdir/test_file4

    From input directory:
        old_metafiles/test_file.meta.in
        old_metafiles/test_file2.meta.out
        old_metafiles/test_file3.meta.local
        old_metafiles/subdir/test_file4.meta.in


Convert all files and add an extension to the new metafile names:
    cargo run -- --recursive --extension=.example /tmp/old_metafiles/ /tmp/new_metafiles/

    Produces output directory:
        new_metafiles/test_file.example
        new_metafiles/test_file2.example
        new_metafiles/test_file3.example
        new_metafiles/subdir/test_file4.example

    From input directory:
        old_metafiles/test_file.meta.in
        old_metafiles/test_file2.meta.out
        old_metafiles/test_file3.meta.local
        old_metafiles/subdir/test_file4.meta.in