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

#include "UrlDownload.h"
#include "Config.h"
#include <algorithm>

// #define DPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define DPRINTF(...)

//This is afile to test
//https://farm4.static.flickr.com/3202/2960028736_74d31b947d.jpg

std::mutex        UrlDownload::_lock;
ReaderWriterLock  UrlDownload::_initLock;
std::queue<CURL*> UrlDownload::_handles;

UrlDownload::UrlDownload(std::string url):
_url(url),
_filepath(url) {
    if(_initLock.cowardlyTryWriterLock()) {
        std::unique_lock<std::mutex> lock(_lock);
        if(_handles.empty())
        {
            CURL * temp = curl_easy_init();
            _handles.push(temp);
            for(unsigned int i=0; i<Config::numCurlHandles-1; i++) {
                _handles.push(curl_easy_duphandle(temp));
            }
        }
        lock.unlock();
        _initLock.writerUnlock();
    }
    
    _initLock.readerLock();

    replace(_filepath.begin(), _filepath.end(), '/', '_');
    replace(_filepath.begin(), _filepath.end(), '.', '_');
    replace(_filepath.begin(), _filepath.end(), ':', '_');
    _filepath = Config::DownloadPath + "/" + _filepath;
}

UrlDownload::~UrlDownload() {
    _initLock.readerUnlock();
    
    if(_initLock.cowardlyTryWriterLock()) {
        std::unique_lock<std::mutex> lock(_lock);
        while(!_handles.empty()) {
            CURL * temp = _handles.front();
            _handles.pop();
            curl_easy_cleanup(temp);
        }
        lock.unlock();
        _initLock.writerUnlock();
    }
    
}

CURL * UrlDownload::getHandle() {
    CURL * handle = NULL;
    while(!handle) {
        std::unique_lock<std::mutex> lock(_lock);
        if(!_handles.empty()) {
            handle = _handles.front();
            _handles.pop();
        }
        lock.unlock();
    }
    return handle;
}

void UrlDownload::retHandle(CURL * handle) {
    std::unique_lock<std::mutex> lock(_lock);
    _handles.push(handle);
    lock.unlock();
}

bool UrlDownload::download(){
    bool ret = false;
    CURL * handle = getHandle();
    DPRINTF("download filepath: %s\n", _filepath.c_str());
    FILE * fp = fopen(_filepath.c_str(), "wb");
    if(fp) {
        curl_easy_reset(handle);
        curl_easy_setopt(handle, CURLOPT_USERAGENT, "tazer");
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, NULL);
        curl_easy_setopt(handle, CURLOPT_URL, _url.c_str());
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, fp);
        curl_easy_perform(handle);
        fclose(fp);
        ret = true;
    }
    retHandle(handle);
    return ret;
}

//Used in the CURL write callback
struct MemoryStruct {
    char * memory;
    size_t size;
};
 
 //This is a CURL writeback used to store memory locally
size_t WriteMemoryCallback(void * contents, size_t size, size_t nmemb, void * userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct * mem = (struct MemoryStruct *) userp;
    char * ptr = (char*) realloc(mem->memory, mem->size + realsize + 1);
    if(ptr == NULL) {
        printf("Out-of-memory\n");
        return 0;
    }
    DPRINTF("RealSize: %lu\n", realsize);
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = '\0';
    return realsize;
}

//Returns the size of a file to download
unsigned int UrlDownload::size() {
    CURL * handle = getHandle();
    
    struct MemoryStruct chunk;
    chunk.memory = (char*) malloc(1);
    chunk.memory[0] = '\0';
    chunk.size = 0;
    
    curl_easy_reset(handle);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "tazer");
    curl_easy_setopt(handle, CURLOPT_NOBODY, true);
    curl_easy_setopt(handle, CURLOPT_HEADER, true);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, true);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, Config::UrlTimeOut);
    curl_easy_setopt(handle, CURLOPT_URL, _url.c_str());
    CURLcode errornum = curl_easy_perform(handle);
    DPRINTF("%s\n", curl_easy_strerror(errornum));
    
    unsigned int ret = 0;
    // printf("%s", chunk.memory);
    char * pos = strstr(chunk.memory, "Content-Length: ");
    if(pos) {
        char * start;
        for(start = pos; start && *start != ' '; start++);
        
        char * end;
        for(end = start; end && *end != '\n'; end++);
        *end = '\0';
        ret = atoi(start);
    }
    else {
        std::cout << "No content-length" << std::endl;
    }
    free(chunk.memory);
    return ret;
}

std::string UrlDownload::name() {
    return _filepath;
}

std::string UrlDownload::downloadUrl(std::string path) {
    if(strstr(path.c_str(), "http://") || strstr(path.c_str(), "https://")) {
        UrlDownload url(path);
        url.download();
        return url.name();
    }
    return path;
}

//https does not always set content-length... we need to figure a way around...
int UrlDownload::sizeUrl(std::string path) {
    int ret = -1;
    if(strstr(path.c_str(), "http://") || strstr(path.c_str(), "https://")) {
        UrlDownload url(path);
        std::cout << "getting size..." << std::endl;
        ret = url.size();
    }
    return ret;
}