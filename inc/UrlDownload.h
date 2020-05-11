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

#ifndef URLDOWNLOAD_H
#define URLDOWNLOAD_H

#ifdef USE_CURL

#include <iostream>
#include <string.h>
#include <string>
#include <mutex>
#include <queue>
#include <curl/curl.h>
#include "ReaderWriterLock.h"

class CurlHandle {
private:
    CURL * _handle;
    
    static std::mutex _lock;
    static ReaderWriterLock _initLock;
    static std::queue<void*> _handles;
    static bool _started;

public:
    CurlHandle(std::string url);
    ~CurlHandle();
    template<class optType, class argType>
    void setOption(optType opt, argType arg);
    bool run();
    CURL * reset(std::string url);
    static bool startCurl();
    static bool endCurl(bool end=false);
    static void initCurl();
    static void destroyCurl();
};

/* Building a class that takes a url and a filepath and then downloads the file to the filepath.*/
class UrlDownload {
private:
    std::string _url;
    std::string _filepath;

    bool _supported;
    bool _exists;
    bool _ranges;
    unsigned int _size;

    char * getHeader();
    unsigned int checkHeaderForContentLength(char * data);
    bool checkHeaderForNotFound(char * data);
    bool checkHeaderForRanges(char * data);

public:
    UrlDownload(std::string url, int size = -1);
    ~UrlDownload();
    
    bool download();
    void * downloadRange(unsigned int start, unsigned int end);
    unsigned int size();
    std::string name();
    bool exists();
    bool ranges();

    
    static bool supportedType(std::string path);
    static std::string downloadUrl(std::string path);
    static int sizeUrl(std::string path);
    static bool checkDownloadable(std::string path);
    static void * downloadUrlRange(std::string path, unsigned int start, unsigned int end);
};

#define supportedUrlType(path) UrlDownload::supportedType(path)
#define checkUrlPath(path) UrlDownload::checkDownloadable(path)
#define sizeUrlPath(path) UrlDownload::sizeUrl(path)
#define downloadUrlPath(path) UrlDownload::downloadUrl(path)
#define downloadUrlRange(path, start, end) UrlDownload::downloadUrlRange(path, start, end)
#define curlEnd(x) (x) ? CurlHandle::endCurl(true) : false
#define curlInit CurlHandle::initCurl()
#define curlDestroy CurlHandle::destroyCurl()

#else

#define supportedUrlType(path) false
#define checkUrlPath(path) false
#define sizeUrlPath(path) -1
#define downloadUrlPath(path) path
#define downloadUrlRange(path, start, end) NULL
#define curlEnd(x) false 
#define curlInit
#define curlDestroy


#endif
#endif /* URLDOWNLOAD_H */

