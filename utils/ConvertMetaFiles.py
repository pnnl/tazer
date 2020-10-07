#//*BeginLicense**************************************************************
#//
#//---------------------------------------------------------------------------
#// TAZeR (github.com/pnnl/tazer/)
#//---------------------------------------------------------------------------
#//
#// Copyright ((c)) 2019, Battelle Memorial Institute
#//
#// 1. Battelle Memorial Institute (hereinafter Battelle) hereby grants
#//    permission to any person or entity lawfully obtaining a copy of
#//    this software and associated documentation files (hereinafter "the
#//    Software") to redistribute and use the Software in source and
#//    binary forms, with or without modification.  Such person or entity
#//    may use, copy, modify, merge, publish, distribute, sublicense,
#//    and/or sell copies of the Software, and may permit others to do
#//    so, subject to the following conditions:
#//    
#//    * Redistributions of source code must retain the above copyright
#//      notice, this list of conditions and the following disclaimers.
#//
#//    * Redistributions in binary form must reproduce the above
#//      copyright notice, this list of conditions and the following
#//      disclaimer in the documentation and/or other materials provided
#//      with the distribution.
#//
#//    * Other than as used herein, neither the name Battelle Memorial
#//      Institute or Battelle may be used in any form whatsoever without
#//      the express written consent of Battelle.
#//
#// 2. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
#//    CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
#//    INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
#//    MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#//    DISCLAIMED. IN NO EVENT SHALL BATTELLE OR CONTRIBUTORS BE LIABLE
#//    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#//    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
#//    OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
#//    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
#//    LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#//    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
#//    USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
#//    DAMAGE.
#//
#// ***
#//
#// This material was prepared as an account of work sponsored by an
#// agency of the United States Government.  Neither the United States
#// Government nor the United States Department of Energy, nor Battelle,
#// nor any of their employees, nor any jurisdiction or organization that
#// has cooperated in the development of these materials, makes any
#// warranty, express or implied, or assumes any legal liability or
#// responsibility for the accuracy, completeness, or usefulness or any
#// information, apparatus, product, software, or process disclosed, or
#// represents that its use would not infringe privately owned rights.
#//
#// Reference herein to any specific commercial product, process, or
#// service by trade name, trademark, manufacturer, or otherwise does not
#// necessarily constitute or imply its endorsement, recommendation, or
#// favoring by the United States Government or any agency thereof, or
#// Battelle Memorial Institute. The views and opinions of authors
#// expressed herein do not necessarily state or reflect those of the
#// United States Government or any agency thereof.
#//
#//                PACIFIC NORTHWEST NATIONAL LABORATORY
#//                             operated by
#//                               BATTELLE
#//                               for the
#//                  UNITED STATES DEPARTMENT OF ENERGY
#//                   under Contract DE-AC05-76RL01830
#// 
#//*EndLicense****************************************************************

import argparse
import os
import sys
from distutils.dir_util import copy_tree
from distutils.file_util import copy_file, move_file

def convertFile(in_path,out_path,trim):
    output_string="TAZER0.1\n"
    index = in_path.find(".meta.")
    if index == -1:
        print("error unrecognized file: ",in_file," -- skipping")
        return
    
    ftype = in_path[index+6:]
    if ftype == "in":
        output_string+="type=input\n"
    elif ftype == "out":
        output_string+="type=output\n" 
    elif ftype == "local":
        output_string+="type=local\n"
    else:
        print("unknown meta file type:",ftype," -- skipping")

    with open(in_path, "r") as in_file:
        text = in_file.read()
        servers = text.split("|")
        for server in servers:
            if server:
                output_string+="[server]\n"
                params = server.split(":")
                # print(params)
                output_string+="host="+params[0]+"\n"
                output_string+="port="+params[1]+"\n"
                output_string+="file="+params[6]+"\n"
                if int(params[2]) > 0:
                    output_string+="compress="+params[2]+"\n"
                if int(params[3]) > 0:
                    output_string+="prefetch="+params[3]+"\n"
                if int(params[5]) > 0:
                    output_string+="blocksize="+params[5]+"\n"

    with open(out_path,"w") as out_file:
        out_file.write(output_string)
    
    if trim:
        index = out_path.find(".meta.")
        move_file(out_path,out_path[:index])


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-p", "--path",type=str, help="path to files",required=True)
    parser.add_argument("-t", "--trim", help="trim '.meta.*' extension from file name",action='store_true')
    parser.add_argument("-i", "--inplace",type=bool, help="inplace update of metafiles (disabled if output path specified)",default=True)
    parser.add_argument("-o", "--outputPath",type=str, help="path to store new files",default="")
    args = parser.parse_args(sys.argv[1:])

    in_path = args.path
    trim = args.trim
    
    in_place = args.inplace
    output_root=args.outputPath
    if output_root != "":
        in_place=False
    if not in_place:
        if output_root == "":
            output_root="./"
        output_root.rstrip("/") +"/"
        old_root=output_root+"old_files/"
        try:
            os.mkdir(output_root)
        except:
            pass
        try:
            os.mkdir(old_root)
        except:
            pass
    

    if os.path.isfile(in_path):
        copy_file(in_path,old_root)
        if in_place:            
            out_path=in_path
        else:
            out_path=output_root+os.path.basename(in_path)
        convertFile(in_path,out_path,trim)
    else:
        copy_tree(in_path,old_root)
        if in_place:            
            for (dirpath, dirnames, filenames) in os.walk(in_path):
                for file in filenames:
                    convertFile(dirpath+"/"+in_file,dirpath+"/"+in_file,trim)
        else:
            for (dirpath, dirnames, filenames) in os.walk(in_path):
                dirpath = dirpath.rstrip("/") +"/"
                out_path=dirpath.strip("../")
                out_path=out_path.strip("./")
                try:
                    os.mkdir(output_root+out_path)
                except:
                    pass
                for in_file in filenames:
                    convertFile(dirpath+"/"+in_file,output_root+out_path+"/"+in_file,trim)
    
        
