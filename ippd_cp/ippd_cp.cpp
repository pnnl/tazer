// -*-Mode: C++;-*-

//*BeginLicense**************************************************************
//
//---------------------------------------------------------------------------
// TAZeR (github.com/pnnl/tazer/)
//---------------------------------------------------------------------------
//
// Copyright ((c)) 2019, Battelle Memorial Institute
//
// 1. Battelle Memorial Institute (hereinafter Battelle) hereby grants
//    permission to any person or entity lawfully obtaining a copy of
//    this software and associated documentation files (hereinafter "the
//    Software") to redistribute and use the Software in source and
//    binary forms, with or without modification.  Such person or entity
//    may use, copy, modify, merge, publish, distribute, sublicense,
//    and/or sell copies of the Software, and may permit others to do
//    so, subject to the following conditions:
//    
//    * Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimers.
//
//    * Redistributions in binary form must reproduce the above
//      copyright notice, this list of conditions and the following
//      disclaimer in the documentation and/or other materials provided
//      with the distribution.
//
//    * Other than as used herein, neither the name Battelle Memorial
//      Institute or Battelle may be used in any form whatsoever without
//      the express written consent of Battelle.
//
// 2. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
//    CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
//    INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
//    MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//    DISCLAIMED. IN NO EVENT SHALL BATTELLE OR CONTRIBUTORS BE LIABLE
//    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
//    OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
//    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//    LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
//    USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
//    DAMAGE.
//
// ***
//
// This material was prepared as an account of work sponsored by an
// agency of the United States Government.  Neither the United States
// Government nor the United States Department of Energy, nor Battelle,
// nor any of their employees, nor any jurisdiction or organization that
// has cooperated in the development of these materials, makes any
// warranty, express or implied, or assumes any legal liability or
// responsibility for the accuracy, completeness, or usefulness or any
// information, apparatus, product, software, or process disclosed, or
// represents that its use would not infringe privately owned rights.
//
// Reference herein to any specific commercial product, process, or
// service by trade name, trademark, manufacturer, or otherwise does not
// necessarily constitute or imply its endorsement, recommendation, or
// favoring by the United States Government or any agency thereof, or
// Battelle Memorial Institute. The views and opinions of authors
// expressed herein do not necessarily state or reflect those of the
// United States Government or any agency thereof.
//
//                PACIFIC NORTHWEST NATIONAL LABORATORY
//                             operated by
//                               BATTELLE
//                               for the
//                  UNITED STATES DEPARTMENT OF ENERGY
//                   under Contract DE-AC05-76RL01830
// 
//*EndLicense****************************************************************

//g++ --std=c++14 -g -O3 ippd_cp.cpp -fopenmp -o ippd_cp

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <omp.h>
#include <unistd.h>

using namespace std;

int main(int argc, char *argv[]) {
    stringstream sFileName;
    stringstream oFileName;
    //sFileName << "_virtualremote_";
    int64_t readSize = 0;
    bool doWrite = true;
    if (argc > 3) {
        doWrite = false;
    }
    if (argc > 4) {
        readSize = atol(argv[4]);
    }
    if (argc > 1) {
        sFileName << argv[1]; //<< ".meta.in";
        cout << sFileName.str() << endl;
        int ifp = open(sFileName.str().c_str(), O_RDONLY);
        oFileName << argv[2]; //<< ".meta.out";
        //FILE * ofp = fopen(oFileName.str().c_str(), "w");
        int ofp = open(oFileName.str().c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
        //int ofd = fileno(ofp);
        cout << "in: " << sFileName.str() << " out: " << oFileName.str() << endl;

        int64_t fileSize = lseek(ifp, 0L, SEEK_END);
        if (fileSize > 0) {
            cout << "fileSize: " << fileSize << endl;
            lseek(ifp, 0L, SEEK_SET);
            //char * buf = new char[fileSize];
            int64_t size = fileSize;
            int64_t cnt = 1;
            while (size > 256 * 1024) { //100000000) { //arbitrary value? better method to calculate write to disk size?
                //cnt++;
                size = size / 2;
                //cout << "size: " << size << " cnt: " << cnt << endl;
            }

            if (readSize == 0) {
                readSize = size;
            }
            else {
                size = readSize;
            }
            cnt = (fileSize / size) + 1;
            std::cout << "READSIZE: " << readSize << std::endl;
            double t1 = 0;
            double t2 = 0;
            char *buf = new char[readSize];
            int64_t numread = 0;
            for (int i = 0; i < cnt; i++) {
                if (i == cnt - 1) {
                    size = fileSize % size;
                }
                //cerr << "SIZE: " << size << " " << readSize << std::endl;
                double t = omp_get_wtime();
                //size_t ret = reaid(ifp, buf + readSize * i, size);
                int64_t temp_size = size;
                char *temp_buf = buf;
                while (temp_size) {
                    size_t ret = read(ifp, temp_buf, temp_size);
                    if (ret >= 0) {
                        temp_buf += ret;
                        temp_size -= ret;
                    }
                }
                t1 += omp_get_wtime() - t;
                //if ((int64_t)ret != size) {
                //	cerr << "file expected size: " << size << " actual: " << ret << endl;
                //}
                t = omp_get_wtime();
                if (doWrite) {
                    temp_size = size;
                    temp_buf = buf;
                    //ret = fwrite(buf , sizeof(char), size, ofp);
                    while (temp_size) {
                        size_t ret = write(ofp, buf, temp_size);
                        fsync(ofp);
                        temp_buf += ret;
                        temp_size -= ret;
                    }
                }
                t2 += omp_get_wtime() - t;
                //if ((int64_t)ret != size) {
                //	cerr << "write whole file expected size: " << size << " actual: " << ret << endl;
                //}
                numread += size;
                //cerr << "numread: " << numread << " of: " << fileSize << std::endl;
                //sleep(1);
            }
            cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!read time: " << t1 << " write: " << t2 << endl;
            delete[] buf;
        }
        double t = omp_get_wtime();
        close(ifp);
        close(ofp);
        cout << "###### Close time = " << omp_get_wtime() - t << std::endl;
    }
}
