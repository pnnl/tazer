//This is a utility to convert Tazer meta files from the old format to the new format
use clap::{Arg, App};
use std::fs::File;
use std::path::Path;
use std::io;
use std::io::Read;
use std::io::Write;
use std::fs::read_dir;
use std::fs::read_link;
use std::fs::create_dir_all;
use std::fs::metadata;
use std::fs::symlink_metadata;

struct MetaInfo {
    tazer_version: String,
    file_type: String,
    host_addr: String,
    port: String,
    filepath: String,
    compress: String,
    prefetch: String,
    save_local: String,
    block_size: String,
}

fn main() -> Result<(), io::Error> {
    let mut extension: String = String::new();

    let args = App::new("Convert Metafiles")
    .arg(
        Arg::with_name("recursive")
        .short("r")
        .long("recursive")
        .takes_value(false)
        .help("Convert all metafiles of the input directory path to the new format.")
    )
    .arg(
        Arg::with_name("extension")
        .short("e")
        .long("extension")
        .takes_value(true)
        .help("Add an extension to the end of the new metafile names and remove the old extension.")
    )
    .arg(
        Arg::with_name("input path")
        .required(true)
        .takes_value(true)
        .help("The path to the file that will be converted, can be a directory if -r option is used.")
    )
    .arg(
        Arg::with_name("output path")
        .required(true)
        .takes_value(true)
        .help("The path to the new file that will be created, must be a directory if -r option is used.")
    )
    .get_matches();

    let input_path = Path::new(args.value_of("input path").unwrap());
    let output_path = Path::new(args.value_of("output path").unwrap());

    if args.is_present("extension") {
        extension = args.value_of("extension").unwrap().to_string();
    }

    if args.is_present("recursive") {
        create_dir_all(output_path).expect("Error creating directory");
        recursive(input_path, output_path, &extension)?;
    }
    else {
        let meta_info = read_meta_info(input_path);
        let meta_info = match meta_info {
            Some(meta_info) => meta_info,
            None => panic!("Error reading from metafile"),
        };
        create_new_metafile(output_path, meta_info);
    }

    Ok(())
}

fn read_meta_info(path: &Path) -> Option<MetaInfo> {

    let mut meta_info : MetaInfo = MetaInfo {
        tazer_version: "TAZER0.1\n".to_string(),
        file_type: "".to_string(),
        host_addr: "".to_string(),
        port: "".to_string(),
        filepath: "".to_string(),
        compress: "compress=false\n".to_string(),
        prefetch: "prefetch=false\n".to_string(),
        save_local: "save_local=false\n".to_string(),
        block_size: "block_size=1\n".to_string(),
    };

    let path_str;
    let meta_data = match symlink_metadata(path) {
        Ok(meta_data) => meta_data,
        Err(_) => return None,
    };
    if meta_data.file_type().is_symlink() {
        //if path is symlink, get the real path so we can check the .meta extension
        let real_path  = read_link(path).unwrap();
        path_str = String::from(real_path.file_name().unwrap().to_str().unwrap());
    }
    else {
        path_str = String::from(path.file_name().unwrap().to_str().unwrap());
    }

    if path_str.contains(".meta.in") {
        meta_info.file_type = "type=input\n".to_string();
    }
    else if path_str.contains(".meta.out") {
        meta_info.file_type = "type=output\n".to_string();
    }
    else if path_str.contains(".meta.local") {
        meta_info.file_type = "type=local\n".to_string();
    }
    else {
        return None
    }

    let file = File::open(&path);
    let mut file = match file {
        Ok(file) => file,
        Err(_) => return None,
    };

    let mut buffer = String::new();
    let read = file.read_to_string(&mut buffer);
    let mut _read = match read {
        Ok(read) => read,
        Err(_) => return None,
    };

    let split = buffer.split(":");
    let sections: Vec<&str> = split.collect();

    if sections.len() < 7 {
        return None
    }

    meta_info.host_addr = "host=".to_string();
    meta_info.host_addr.push_str(sections[0]);
    meta_info.host_addr.push('\n');

    meta_info.port = "port=".to_string();
    meta_info.port.push_str(sections[1]);
    meta_info.port.push('\n');

    if sections[2].to_string() == "1" {
        meta_info.compress = "compress=true\n".to_string();
    }

    if sections[3].to_string() == "1" {
        meta_info.compress = "prefetch=true\n".to_string();
    }

    if sections[4].to_string() == "1" {
        meta_info.compress = "save_local=true\n".to_string();
    }

    meta_info.block_size = "block_size=".to_string();
    meta_info.block_size.push_str(sections[5]);
    meta_info.block_size.push('\n');

    meta_info.filepath = "file=".to_string();
    meta_info.filepath.push_str(sections[6]);
    meta_info.filepath = meta_info.filepath.replace("|", "");
    if meta_info.filepath.chars().last() != Some('\n') {
        meta_info.filepath.push('\n');
    }

    Some(meta_info)
}

fn create_new_metafile(path: &Path, meta_info: MetaInfo) {
    let file = File::create(path);
    let mut file = match file {
        Ok(file) => file,
        Err(error) => panic!("Failed to create file {:?}", error),
    };

    file.write_all(meta_info.tazer_version.as_bytes()).expect("write failed");
    file.write_all(meta_info.file_type.as_bytes()).expect("write failed");
    file.write_all("[server]\n".as_bytes()).expect("write failed");
    file.write_all(meta_info.host_addr.as_bytes()).expect("write failed");
    file.write_all(meta_info.port.as_bytes()).expect("write failed");
    file.write_all(meta_info.filepath.as_bytes()).expect("write failed");
    file.write_all(meta_info.compress.as_bytes()).expect("write failed");
    file.write_all(meta_info.prefetch.as_bytes()).expect("write failed");
    file.write_all(meta_info.save_local.as_bytes()).expect("write failed");
    file.write_all(meta_info.block_size.as_bytes()).expect("write failed");
}

fn recursive(input_path: &Path, output_path: &Path, extension: &String) -> Result<(), io::Error> {
    let paths = read_dir(input_path)?;
    for entry in paths {
        let path = entry.unwrap().path();
        let md = metadata(&path)?;

        if md.is_dir() {
            let new_output_path = output_path.join(path.file_name().unwrap().to_str().unwrap());
            create_dir_all(&new_output_path).expect("Error creating directory");
            let _ = recursive(path.as_path(), &new_output_path, &extension);
        }
        else if md.is_file() {
            let meta_info = read_meta_info(path.as_path());
            
            if meta_info.is_some() {
                let mut new_file_name = String::from(path.file_name().unwrap().to_str().unwrap());

                if new_file_name.contains(".meta.in") {
                    new_file_name = new_file_name.replace(".meta.in", extension.as_str());
                }
                else if new_file_name.contains(".meta.out") {
                    new_file_name = new_file_name.replace(".meta.out", extension.as_str());
                }
                else if new_file_name.contains(".meta.local") {
                    new_file_name = new_file_name.replace(".meta.local", extension.as_str());
                }
    
                let new_file_path = output_path.join(new_file_name);
    
                //println!("new filepath: {}", new_file_path.as_path().display());
                create_new_metafile(new_file_path.as_path(), meta_info.unwrap());
            }
        }
    }

    Ok(())
}