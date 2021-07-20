daemon.rs:
    The daemon runs as a background process listening for requests from a client.

    There are three requests the daemon can recieve:
    1. start a new tazer server with a specified port and environment variables for the server
    2. close a tazer server with a specified host and port
    3. 'exit' which first closes all active tazer servers, then stops the daemon process

    To start the daemon with default host address and port:
    "cargo run --bin=daemon"

    To start the daemon with specified host address and port:
    "cargo run --bin=daemon -- --host=`hostname` --port=5001"

    The daemon then creates 3 files in the current working directory:
    1. daemon.out (stdout)
    2. daemon.err (stderr)
    3. daemon.pid (the pid of the daemon)

    The process can either be stopped with "kill -9 `cat daemon.pid`" or through an "exit" message from the client.



client.rs:
    The client sends 1 of 3 possible messages to the daemon, and waits for a response.
    The --connection argument is always required in order to connnect to the daemon.

    Examples:

    Request to start server with a specified port and environment variables:
    "cargo run --bin=client -- --connection=bluesky.pnl.gov:5001 --start=5123:TAZER_SERVER_CACHE_SIZE=$((128*1024*1024)):TAZER_PRIVATE_MEM_CACHE_SIZE=$((128*1024*1024))"
    example daemon response: "Success:node28:5123"

    Request to close the tazer server that was started in the previous example:
    "cargo run --bin=client -- --connection=bluesky.pnl.gov:5001 --stop=node28:5123"
    example daemon response: "Successfully closed tazer server"

    Request to shutdown the daemon (also closes active tazer servers):
    "cargo run --bin=client -- --connection=bluesky.pnl.gov:5001 --exit"
    example daemon response: "Shutting down daemon"
