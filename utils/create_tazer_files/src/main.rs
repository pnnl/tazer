//This utility creates Tazer metafiles for the given files
use clap::{Arg, App};
use std::fs;
use std::io;
use std::io::Write;
use std::path::Path;
use std::vec::Vec;

struct MetaInfo {
    extension: String,
    tazer_version: String,
    file_type: String,
    hosts: Vec<String>,
    ports: Vec<String>,
    server_root: String,
    compress: String,
    prefetch: String,
    save_local: String,
    block_size: String,
}

fn main() {
    let args = App::new("Create TAZeR Files")
    .arg(
        Arg::with_name("server").short("s").long("server").takes_value(true).multiple(true).default_value("localhost:6023")
        .help("server address and port seperated by a ':' <server address>:<port>")
    )
    .arg(
        Arg::with_name("blocksize").short("b").long("blocksize").takes_value(true).default_value("1048576")
        .help("TAZeR blocksize")
    )
    .arg(
        Arg::with_name("path").short("p").long("path").takes_value(true).required(true)
        .help("path to files")
    )
    .arg(
        Arg::with_name("flat").short("f").long("flat").takes_value(true).default_value("false")
        .help("write TAZeR files to flat directory")
    )
    .arg(
        Arg::with_name("serverroot").short("r").long("serverroot").takes_value(true).default_value("./")
        .help("root path of files on server")
    )
    .arg(
        Arg::with_name("outputpath").short("o").long("outputpath").takes_value(true).default_value("./tazer")
        .help("path to output directory")
    )
    .arg(
        Arg::with_name("type").short("t").long("type").takes_value(true).default_value("input")
        .help("the tazer file type must be either 'input', 'output', or 'local'")
    )
    .arg(
        Arg::with_name("compression").short("c").long("compression").takes_value(true).default_value("false")
        .help("use compression")
    )
    .arg(
        Arg::with_name("prefetch").long("prefetch").takes_value(true).default_value("false")
        .help("use prefetching")
    )
    .arg(
        Arg::with_name("savelocal").long("savelocal").takes_value(true).default_value("false")
        .help("use savelocal")
    )
    .arg(
        Arg::with_name("version").long("version").short("v").takes_value(true).default_value("TAZER0.1")
        .help("the current tazer version")
    )
    .arg(
        Arg::with_name("extension").long("extension").short("e").takes_value(true).default_value("")
        .help("add an extension to the new TAZeR file names")
    )
    .get_matches();

    let mut meta_info = MetaInfo {
        extension: args.value_of("extension").unwrap().to_string(),
        tazer_version: args.value_of("version").unwrap().to_string(),
        file_type: String::from("type="),
        hosts: Vec::new(),
        ports: Vec::new(),
        server_root: String::from("file="),
        compress: String::from("compress="),
        prefetch: String::from("prefetch="),
        save_local: String::from("save_local="),
        block_size: String::from("block_size="),
    };
    meta_info.tazer_version.push('\n');
    meta_info.file_type.push_str(args.value_of("type").unwrap());
    meta_info.file_type.push('\n');
    meta_info.server_root.push_str(args.value_of("serverroot").unwrap());
    meta_info.server_root.push('\n');
    meta_info.compress.push_str(args.value_of("compression").unwrap());
    meta_info.compress.push('\n');
    meta_info.prefetch.push_str(args.value_of("prefetch").unwrap());
    meta_info.prefetch.push('\n');
    meta_info.save_local.push_str(args.value_of("savelocal").unwrap());
    meta_info.save_local.push('\n');
    meta_info.block_size.push_str(args.value_of("blocksize").unwrap());
    meta_info.block_size.push('\n');

    let server_args: Vec<&str> = args.values_of("server").unwrap().collect();
    for s in  server_args {
        let split: Vec<&str> = s.split(":").collect();

        let mut host_addr = String::from("host=");
        host_addr.push_str(split[0]);
        host_addr.push('\n');

        let mut port = String::from("port=");
        port.push_str(split[1]);
        port.push('\n');

        meta_info.hosts.push(host_addr);
        meta_info.ports.push(port);
    }

    let output_path = Path::new(args.value_of("outputpath").unwrap());
    let input_path = Path::new(args.value_of("path").unwrap());
    
    let flat: bool;
    if args.value_of("flat").unwrap().to_string() == "true" {
        println!("flat: true");
        flat = true;
    }
    else {
        flat = false;
    }

    let md = fs::metadata(&input_path).expect("failed to access file or directory in main()");
    if md.is_dir() {
        find_files(&input_path, &output_path, &meta_info, flat).expect("find_files() failed in main()");
    }
    else if md.is_file() {
        fs::create_dir_all(output_path).expect("could not create output path");
        let new_file_path = output_path.join(input_path.file_name().unwrap().to_str().unwrap());
        create_tazer_file(&new_file_path, &meta_info).expect("create_tazer_file() failed in main()");
    }
}

fn find_files(input_path: &Path, output_path: &Path, meta_info: &MetaInfo, flat: bool) -> Result<(), io::Error> {
    fs::create_dir_all(output_path).expect("could not create output path");

    let entries = fs::read_dir(input_path).expect("error reading from directory");
    for entry in entries {
        let path = entry.unwrap().path();
        let md = fs::metadata(&path)?;

        if md.is_dir() {
            if flat {
                let _ = find_files(&path, &output_path, &meta_info, flat);
            }
            else {
                let new_output_path = output_path.join(path.file_name().unwrap().to_str().unwrap());
                let _ = find_files(&path, &new_output_path, &meta_info, flat);
            }
        }
        else if md.is_file() {
            let new_file_path = output_path.join(path.file_name().unwrap().to_str().unwrap());
            let _ = create_tazer_file(&new_file_path, &meta_info)?;
        }
    }

    Ok(())
}

fn create_tazer_file(new_file_path: &Path, meta_info: &MetaInfo) -> Result<(), io::Error>{

    let mut temp = String::from(new_file_path.to_str().unwrap());
    temp.push_str(meta_info.extension.as_str());
    let file_path_with_extension = Path::new(temp.as_str());
    println!("creating tazer file: {}", file_path_with_extension.to_str().unwrap());
    let mut file = fs::File::create(file_path_with_extension).expect("error creating new file");

    file.write_all(meta_info.tazer_version.as_bytes())?;
    file.write_all(meta_info.file_type.as_bytes())?;
    let mut i: usize = 0;
    while i < meta_info.hosts.len() {
        file.write_all("[server]\n".as_bytes())?;
        file.write_all(meta_info.hosts[i].as_bytes())?;
        file.write_all(meta_info.ports[i].as_bytes())?;
        file.write_all(meta_info.server_root.as_bytes())?;
        file.write_all(meta_info.block_size.as_bytes())?;
        file.write_all(meta_info.compress.as_bytes())?;
        file.write_all(meta_info.prefetch.as_bytes())?;
        file.write_all(meta_info.save_local.as_bytes())?;
        
        i += 1;
    }

    Ok(())
}
