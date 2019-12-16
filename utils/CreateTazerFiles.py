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

def createTazerFile(output_path,dirpath,file,servers,compression,blocksize,serverRoot):
    print (output_root,dirpath,file)
    with open(output_path+"/"+file+".meta.in","w") as f:
        for server in servers:
            for s in server:
                serverInfo= ""
                serverInfo+=s+":"
                serverInfo+=str(compression)+":"
                serverInfo+="0:0:" #unused currently
                serverInfo+=str(blocksize)+":"
                serverInfo+=str(serverRoot)+"/"+dirpath+file+"|"
                f.write(serverInfo)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-s", "--server", help="server address and port <server address>:<port>", action='append', nargs='*',default=[])
    parser.add_argument("-b", "--blocksize",type=int,help="TAZeR blocksize",default=1048576)
    parser.add_argument("-p", "--path",type=str, help="path to files",required=True)
    parser.add_argument("-f", "--flat",action='store_true', help="write TAZeR files to flat directory",default=False)
    parser.add_argument("-r", "--serverroot",type=str, help="root path of files on server",default="./")
    parser.add_argument("-o", "--outputpath",type=str, help="path to output folder",default="./tazer")
    args = parser.parse_args(sys.argv[1:])


    #-------------Parse args-------------------
    servers = args.server
    if len(servers) == 0:
        servers.append("localhost:6023") #default server (set in inc/Config.h)
    
    compression = 0 #currently unused
    blocksize = args.blocksize
    serverRoot = args.serverroot.rstrip("/")

    in_path = args.path
    # if in_path.startswith("./"):
    #     in_path = in_path.strip("./")
    output_root=args.outputpath.rstrip("/") +"/"
    os.mkdir(output_root)

    flat = args.flat

    #-------------------------------------------
     
    #----------create tazer meta files----------

    if os.path.isfile(in_path):
        createTazerFile(output_root,"",in_path,servers,compression,blocksize,serverRoot)

    else:
        for (dirpath, dirnames, filenames) in os.walk(in_path):
            dirpath = dirpath.rstrip("/") +"/"
            out_path=dirpath.strip("../")
            out_path=out_path.strip("./")
            print(output_root,out_path)
            if not flat:
                os.mkdir(output_root+out_path)
            for file in filenames:
                if flat:
                    createTazerFile(output_root,dirpath,file,servers,compression,blocksize,serverRoot)
                else:
                    createTazerFile(output_root+out_path,dirpath,file,servers,compression,blocksize,serverRoot)

    #------------------------------------------
