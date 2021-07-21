//client to request a new tazer server
use std::net::{TcpStream};
use std::io::{Read, Write};
use std::str::from_utf8;
use clap::{Arg, App};

fn send_message(connection:&str, message:&str) {
    let bytes:usize = message.len();
    let mut full_message = bytes.to_string();
    full_message.push(':');
    full_message.push_str(message);
    println!("sending: {}", full_message);

    match TcpStream::connect(connection) {
        Ok(mut stream) => {
            println!("Connected to server");
            stream.write(full_message.as_bytes()).unwrap();

            //read server response
            let mut resp = String::new();
            let mut incoming_data = [0 as u8; 1024]; 
            let mut total_bytes:usize = stream.read(&mut incoming_data).unwrap();
            resp.push_str(from_utf8(&incoming_data[0..total_bytes]).unwrap());
            let split:Vec<&str> = resp.split(":").collect();
            let incoming_bytes:usize = split[0].parse::<usize>().unwrap() + split[0].len() + 1;

            while total_bytes < incoming_bytes {
                let bytes = stream.read(&mut incoming_data).unwrap();
                total_bytes += bytes;
                resp.push_str(from_utf8(&incoming_data[0..bytes]).unwrap());
                //println!("total bytes = {}, incoming = {}", total_bytes, incoming_bytes);
                //println!("received {} bytes: {}", bytes ,resp);
            }
            println!("received message: {}", resp);
        }
        Err(e) => {
            println!("Failed to connect to server: {}", e);
        }
    }
}

fn main() {
    let args = App::new("Client")
    .arg(
        Arg::with_name("connection").short("c").long("connection").takes_value(true).default_value("localhost:5003")
        .help("server address and port of daemon: <server address>:<port>")
    )
    .arg(
        Arg::with_name("start").long("start").takes_value(true)
        .help("start a new tazer server: <port>:<environment_var1>:<environment_var2>:<environment_var3> . . .")
    )
    .arg(
        Arg::with_name("stop").long("stop").takes_value(true)
        .help("stop a tazer server: <server address>:<port>")   
    )
    .arg(
        Arg::with_name("exit").short("e").long("exit").takes_value(false)
        .help("stop the daemon currently listening for requests")
    )
    .get_matches();

    let connection = args.value_of("connection").unwrap();

    if args.is_present("start") {
        let mut message = String::from("AddServer:");
        message.push_str(args.value_of("start").unwrap());
        send_message(connection, message.as_str());
    }
    if args.is_present("stop") {
        let mut message = String::from("CloseServer:");
        message.push_str(args.value_of("stop").unwrap());
        send_message(connection, message.as_str());
    }
    if args.is_present("exit") {
        send_message(connection, "Exit");
    }

    // send_message(connection, "AddServer:5001:TAZER_SERVER_CACHE_SIZE=$((128*1024*1024)):TAZER_PRIVATE_MEM_CACHE_SIZE=$((128*1024*1024))");
    // send_message(connection, "CloseServer:hostname:5001");
    // send_message(connection, "Exit");
}
