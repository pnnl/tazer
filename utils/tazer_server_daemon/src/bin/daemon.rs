//listens for requests to start/close tazer servers
use std::net::{TcpListener, TcpStream};
use std::io::{Read, Write};
use std::str::from_utf8;
use std::error::Error;
use std::process::{Command, Stdio};
use std::fs::File;
use clap::{Arg, App};
use regex::Regex;
use daemonize::Daemonize;

struct TazerServer {
    host:String,
    port:String,
    env_vars:Vec<String>,
}

fn send_response(mut stream:&TcpStream, message:&str) {
    let bytes:usize = message.len();
    let mut full_message = bytes.to_string();
    full_message.push(':');
    full_message.push_str(message);
    stream.write(full_message.as_bytes()).unwrap();
}

fn close_tazer_server(message:String) -> Result<(String, String, String), Box<dyn Error>> {
    let split:Vec<&str> = message.split(":").collect();
    let host = split[2].to_string();
    let port = split[3].to_string();

    let child_process = Command::new("src/close_tazer_server.sh")
    .arg(host.as_str())
    .arg(port.as_str())
    .output()
    .expect("Failed to close tazer server");

    if child_process.status.success() {
        let server_output:String = String::from_utf8_lossy(&child_process.stdout).to_string();
        Ok((host, port, server_output))
    }
    else {
        Err("CloseServer failed".into())
    }
}

fn add_tazer_server(message:String) -> Result<TazerServer, Box<dyn Error>> {
    //parse message for info (port and cache environment variables) before launching the tazer server
    let mut tazer_server = TazerServer {
        host:"".to_string(),
        port:"".to_string(),
        env_vars:Vec::new(),
    };

    let split:Vec<&str> = message.split(":").collect();
    tazer_server.port = split[2].to_string();
    for i in 3..split.len() {
        tazer_server.env_vars.push(split[i].to_string());
    }

    //attempt to launch tazer server node
    let child_process = Command::new("src/launch_tazer_server.sh")
    .arg(&tazer_server.port)
    .args(&tazer_server.env_vars)
    .stdout(Stdio::piped())
    .spawn()
    .expect("Failed to start tazer server");

    let output = child_process.wait_with_output()?;

    //parse output to determine host address
    println!("output: {}", from_utf8(&output.stdout)?);
    let re = Regex::new(r"host=.*").unwrap();
    let captures = re.captures(from_utf8(&output.stdout)?).unwrap();
    tazer_server.host = captures.get(0).map_or("", |m| m.as_str()).to_string();
    tazer_server.host.replace_range(0..5, "");
    //println!("tazer_server.host = {}", tazer_server.host);
    //if host is "" return error
    let re2 = Regex::new(r"failed to ping").unwrap();
    if re2.is_match(from_utf8(&output.stdout)?) {
        return Err("failed to start tazer server".into())
    }

    Ok(tazer_server)
}

fn message_to_string(mut stream: &TcpStream) -> Result<String, Box<dyn Error>> {
    //let mut incoming_data = [0 as u8; 2048]; 
    let mut message = String::new();
    let mut incoming_data = [0 as u8; 1024]; 
    let mut total_bytes:usize = stream.read(&mut incoming_data)?;
    message.push_str(from_utf8(&incoming_data[0..total_bytes])?);
    let split:Vec<&str> = message.split(":").collect();
    let incoming_bytes:usize = split[0].parse::<usize>().unwrap() + split[0].len() + 1;

    while total_bytes < incoming_bytes {
        let bytes = stream.read(&mut incoming_data)?;
        total_bytes += bytes;
        message.push_str(from_utf8(&incoming_data[0..bytes])?);
        //println!("total bytes = {}, incoming = {}", total_bytes, incoming_bytes);
        //println!("received {} bytes: {}", bytes ,message);
    }

    let message = message.trim_matches(char::from(0)).to_string();
    println!("received message: {}", message);
    Ok(message)
}

fn main() {
    let args = App::new("TazerServerStarter")
    .arg(
        Arg::with_name("host").short("h").long("host").takes_value(true).default_value("localhost")
        .help("server address to run on")
    )
    .arg(
        Arg::with_name("port").short("p").long("port").takes_value(true).default_value("5003")
        .help("port to listen on")
    )
    .get_matches();

    let host:&str = args.value_of("host").unwrap();
    let port:&str = args.value_of("port").unwrap();
    let listener = TcpListener::bind(format!("{}{}{}", host, ":", port)).unwrap();
    //let active_tazer_servers:u32 = 0;
    let mut active_tazer_servers:Vec<TazerServer> = Vec::new();

    println!("Starting Server {}:{}", host, port);

    let stdout = File::create("./daemon.out").unwrap();
    let stderr = File::create("./daemon.err").unwrap();

    let daemonize = Daemonize::new()
    .pid_file("./daemon.pid")
    .chown_pid_file(true)
    .working_directory("./")
    .stdout(stdout)
    .stderr(stderr);

    match daemonize.start() {
        Ok(_) => println!("Success, daemonized"),
        Err(e) => eprintln!("Error, {}", e),
    }

    for stream in listener.incoming() {
        match stream {
            Ok(stream) => {
                match message_to_string(&stream) {
                    Ok(message) => {
                        if message.contains("AddServer") {
                            println!("Adding Tazer Server");
                            match add_tazer_server(message) {
                                Ok(new_server) => {
                                    let mut resp = String::from("Success:");
                                    resp.push_str(&new_server.host);
                                    resp.push(':');
                                    resp.push_str(&new_server.port);
                                    send_response(&stream, &resp);
                                    active_tazer_servers.push(new_server);
                                }
                                Err(e) => {
                                    println!("Error adding tazer server: {}", e);
                                    send_response(&stream, "Error adding tazer server");
                                }
                            }
                        }
                        else if message.contains("CloseServer") {
                            println!("Closing Tazer Server");
                            match close_tazer_server(message) {
                                Ok(old_server) => {
                                    let mut resp = String::from("Successfully closed tazer server:\n");
                                    resp.push_str(&old_server.2);
                                    send_response(&stream, &resp);
                                    if let Some(pos) = active_tazer_servers.iter().position(|x| *x.host == old_server.0 && *x.port == old_server.1) {
                                        active_tazer_servers.remove(pos);
                                    }
                                }
                                Err(e) => {
                                    println!("Error closing tazer server: {}", e);
                                    send_response(&stream, "Error closing tazer server");
                                }
                            }
                        }
                        else if message.contains("Exit") {
                            println!("Exiting");
                            //first close all tazer servers
                            for server in active_tazer_servers {
                                let mut close_message = "0:CloseServer:".to_string();
                                close_message.push_str(&server.host);
                                close_message.push(':');
                                close_message.push_str(&server.port);
                                let _ = close_tazer_server(close_message);
                            }
                            send_response(&stream, "Shutting down daemon");
                            break;
                        }
                        else {
                            println!("Recieved unknown message: {}", message);
                        }
                    }
                    Err(e) => {
                        println!("Error reading message: {}", e);
                    }
                }
            }
            Err(e) => {
                println!("Stream Error: {}", e);
            }
        }
    }
    drop(listener);
}
