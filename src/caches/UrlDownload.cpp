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

std::mutex        CurlHandle::_lock;
ReaderWriterLock  CurlHandle::_initLock;
std::queue<CURL*> CurlHandle::_handles;
bool              CurlHandle::_started = false;

CurlHandle::CurlHandle(std::string url) {
    _handle = NULL;
    while(!_handle) {
        std::unique_lock<std::mutex> lock(_lock);
        if(!_handles.empty()) {
            _handle = (CURL*)_handles.front();
            _handles.pop();
        }
        lock.unlock();
    }
    reset(url);
}

CurlHandle::~CurlHandle() {
    std::unique_lock<std::mutex> lock(_lock);
    _handles.push((void*)_handle);
    lock.unlock();
}

CURL * CurlHandle::reset(std::string url) {
    // These are the base options
    curl_easy_reset(_handle);
    curl_easy_setopt(_handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(_handle, CURLOPT_USERAGENT, "tazer");
    curl_easy_setopt(_handle, CURLOPT_FOLLOWLOCATION, true);
    curl_easy_setopt(_handle, CURLOPT_TIMEOUT, Config::UrlTimeOut);
    return _handle;
}

template<class optType, class argType>
void CurlHandle::setOption(optType opt, argType arg) {
    curl_easy_setopt(_handle, opt, arg);
}

bool CurlHandle::run() {
    auto errornum = curl_easy_perform(_handle);
    if(errornum != CURLE_OK) {
        fprintf(stdout, "CURL ERROR %s\n", curl_easy_strerror(errornum));
        return false;
    }
    return true;
}

bool CurlHandle::startCurl() {
    bool ret = false;
    if(_initLock.cowardlyTryWriterLock()) {
        if(!_started) {
            DPRINTF("Num Handles: %u Timeout: %u DownloadForSize: %u\n", Config::numCurlHandles, Config::UrlTimeOut, Config::downloadForSize);
            std::unique_lock<std::mutex> lock(_lock);
            if(_handles.empty())
            {
                CURL * temp = curl_easy_init();
                _handles.push((void*)temp);
                for(unsigned int i=0; i<Config::numCurlHandles-1; i++) {
                    _handles.push((void*)curl_easy_duphandle(temp));
                }
            }
            lock.unlock();
            _started = true;
            ret = true;
        }
        _initLock.writerUnlock();
    }
    if(!Config::curlOnStartup)
        _initLock.readerLock(); //+1
    return ret;
}

bool CurlHandle::endCurl(bool end) {
    bool ret = false;
    if(!Config::curlOnStartup && !end) {
        _initLock.readerUnlock();
        ret = _initLock.cowardlyTryWriterLock();
    }
    else if(end) {
        _initLock.writerLock();
        ret = true;
    }

    if(ret) {
        if(_started) {
            std::unique_lock<std::mutex> lock(_lock);
            while(!_handles.empty()) {
                CURL * temp = _handles.front();
                _handles.pop();
                curl_easy_reset(temp);
                curl_easy_cleanup(temp);
            }
            lock.unlock();
            _started = false;
        }
        _initLock.writerUnlock();
    }
    return ret;
}

void CurlHandle:: initCurl() { 
    curl_global_init(CURL_GLOBAL_ALL); 
}

void CurlHandle:: destroyCurl() { 
    curl_global_cleanup();
}

UrlDownload::UrlDownload(std::string url, int size):
_url(url),
_filepath(url),
_supported(false),
_exists(false),
_ranges(false),
_size(0) {
    replace(_filepath.begin(), _filepath.end(), '/', '_');
    replace(_filepath.begin(), _filepath.end(), '.', '_');
    replace(_filepath.begin(), _filepath.end(), ':', '_');
    _filepath = Config::DownloadPath + "/" + _filepath;

    if(supportedType(url)) {
        _supported = true;
        CurlHandle::startCurl();

        if(size > -1) {
            _exists = true;
            _size = (unsigned int) size;
        }
        else {
            char * header = getHeader();
            _exists = !checkHeaderForNotFound(header);
            if(_exists) {
                _ranges = checkHeaderForRanges(header);
                _size = checkHeaderForContentLength(header);
            }
        }
        DPRINTF("%s Supported: %u Exists: %u Ranges: %u Size: %u\n", _filepath.c_str(), _supported, _exists, _ranges, _size);
    }
}

UrlDownload::~UrlDownload() {
    if(_supported)
        CurlHandle::endCurl();
}

/********************************CurlCallbackFunctions********************************/

//Used in the CURL write callback
struct MemoryChunk {
    char * memory;
    size_t size;
    unsigned int start;
    unsigned int end;
    unsigned int current;

    MemoryChunk() : 
        memory(NULL), 
        size(1),
        start(0),
        end(0),
        current(0) {
            memory = (char*) malloc(1);
            memory[0] = '\n';
            size = 1;
        }

    ~MemoryChunk() {
        if(memory)
            free(memory);
    }

    void * get() {
        void * ret = memory;
        memory = NULL;
        return ret;
    }
};

//This is a CURL writeback used to store memory locally
size_t CountMemoryCallback(void * contents, size_t size, size_t nmemb, void * userp) {
    size_t packetSize = size * nmemb;
    size_t * totalSize = (size_t*) userp;
    (*totalSize)+=packetSize;
    DPRINTF("PACKETSIZE: %lu\n", packetSize);
    return packetSize;
}

//This is a CURL writeback used to store memory locally
template<class T=void*>
size_t WriteMemoryCallback(T contents, size_t size, size_t nmemb, void * userp) {
    size_t realsize = size * nmemb;
    MemoryChunk * mem = (MemoryChunk *) userp;
    char * ptr = (char*) realloc(mem->memory, mem->size + realsize + 1);
    if(ptr == NULL) {
        printf("Out-of-memory\n");
        return 0;
    }
    DPRINTF("RealSize: %lu\n", realsize);
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), (void*)contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = '\0';
    return realsize;
}

//This is a CURL writeback used to store memory locally
template<class T=void*>
size_t WriteMemoryRangeCallback(T contents, size_t size, size_t nmemb, void * userp) {
    size_t realsize = size * nmemb;
    if(nmemb) {
        MemoryChunk * mem = (MemoryChunk *) userp;
        unsigned int start = mem->current;
        unsigned int end = start + nmemb - 1;
        unsigned int copyStart;
        unsigned int copySize;
        DPRINTF("%u - %u | %u - %u | size: %u elem: %u\n", start, end, mem->start, mem->end, size, nmemb);
        if(end < mem->start || start > mem->end) { //Outside the boundary
            DPRINTF("Outside\n");
            copyStart = 0;
            copySize = 0;
        }
        else if(mem->start <= start && start <= mem->end && mem->start <= end && end <= mem->end) { //Completely inside
            DPRINTF("Inside\n");
            copyStart = 0;
            copySize = nmemb;
        }
        else if(start <= mem->start && mem->end <= end) {
            DPRINTF("Over\n");
            copyStart = mem->start - mem->current;
            copySize = mem->end - mem->start + 1;
        }
        else if(start < mem->start && end <= mem->end) { //Shifted left
            DPRINTF("Left\n");
            copyStart = mem->start - mem->current;
            copySize = nmemb - copyStart;
        }
        else if(start <= mem->end && mem->end < end) { //Shifted right
            DPRINTF("Right\n");
            copyStart = 0;
            copySize = mem->end - mem->current + 1;
        }
        else { //Something is wrong
            std::cout << "This is wrong" << std::endl;
            copyStart = 0;
            copySize = 0;
        }

        copySize*=size;
        DPRINTF("CopyStart: %u CopySize: %u\n", copyStart, copySize);
        if(copySize) {
            char * ptr = (char*) realloc(mem->memory, mem->size + copySize + 1);
            if(ptr == NULL) {
                printf("Out-of-memory\n");
                return 0;
            }
            mem->memory = ptr;
            memcpy(&(mem->memory[mem->size - 1]), (void*)&contents[copyStart], copySize);
            mem->size += copySize;
            mem->memory[mem->size] = '\0';
        }
        mem->current+=nmemb;
    }
    return realsize;
}

/********************************HeaderFunctions********************************/

//This will only download the header.
char * UrlDownload::getHeader() {
    CurlHandle handle(_url);
    MemoryChunk chunk;
    handle.setOption(CURLOPT_HEADERDATA, (void *)&chunk);
    handle.setOption(CURLOPT_HEADERFUNCTION, WriteMemoryCallback<char*>);
    handle.setOption(CURLOPT_NOBODY, true);
    if(handle.run()){
        free(chunk.memory);
        chunk.memory = NULL;
    }
    DPRINTF("Header Size: %u\n", chunk.size);
    return chunk.memory;
}

//Header parsing function - Looks for content length and returns it
unsigned int UrlDownload::checkHeaderForContentLength(char * data) {
    unsigned int ret = 0;
    if(data) {
        char * pos = strstr(data, "Content-Length: ");
        if(pos) {
            char * start;
            for(start = pos; start && *start != ' '; start++);
            
            char * end;
            for(end = start; end && *end != '\n'; end++);
            *end = '\0';
            ret = atoi(start);
        }
    }
    return ret;
}

//Header parsing function - Checks for 404 Not Found
bool UrlDownload::checkHeaderForNotFound(char * data) {
    if(data) {
        char * pos = strstr(data, "HTTP/1.1 404 Not Found");
        // if(pos) printf("%s\n", pos);
        return !pos;
    }
    return false;
}

//Header parsing function - Checks for 404 Not Found
bool UrlDownload::checkHeaderForRanges(char * data) {
    if(data) {
        char * pos = strstr(data, "Accept-Ranges: bytes");
        // if(pos) printf("%s\n", pos);
        return !pos;
    }
    return false;
}

/********************************PublicFunctions********************************/
//This downloads remote file to file pointer
bool UrlDownload::download() {
    bool ret = false;
    if(_supported && _exists) {
        CurlHandle handle(_url);
        DPRINTF("download filepath: %s\n", _filepath.c_str());
        FILE * fp = fopen(_filepath.c_str(), "wb");
        if(fp) {
            handle.setOption(CURLOPT_WRITEDATA, fp);
            handle.setOption(CURLOPT_WRITEFUNCTION, NULL);
            handle.run();
            fclose(fp);
            ret = true;
        }
    }
    return ret;
}

void * UrlDownload::downloadRange(unsigned int start, unsigned int end) {
    void * ret = NULL;
    if(_supported && _exists) {
        CurlHandle handle(_url);
        MemoryChunk chunk;
        chunk.start = start;
        chunk.end = end;
        
        if(_ranges) {
            std::string range = std::to_string(start) + "-" + std::to_string(end);
            handle.setOption(CURLOPT_RANGE, range);
            handle.setOption(CURLOPT_WRITEFUNCTION, WriteMemoryCallback<char*>);
        }
        else
            handle.setOption(CURLOPT_WRITEFUNCTION, WriteMemoryRangeCallback<char*>);
        handle.setOption(CURLOPT_WRITEDATA, (void *)&chunk);
        if(handle.run())
            ret = chunk.get();
    }
    return ret;
}

//Returns the size of a file to download
unsigned int UrlDownload::size() {
    if(_supported && _exists) {
        if(!_size && Config::downloadForSize) {
            size_t ret = 0;
            CurlHandle handle(_url);
            handle.setOption(CURLOPT_WRITEDATA, (void *)&ret);
            handle.setOption(CURLOPT_WRITEFUNCTION, CountMemoryCallback);
            handle.run();
            _size = ret;
        }
        return _size;
    }
    return -1;
}

std::string UrlDownload::name() {
    return _filepath;
}

bool UrlDownload::exists() {
    return _exists;
}

bool UrlDownload::ranges() {
    return _ranges;
}

/********************************StaticFunctions********************************/

bool UrlDownload::supportedType(std::string path) {
    if(strstr(path.c_str(), "http://") || strstr(path.c_str(), "https://"))
        return true;
    return false;
}

//Returns if the file can be downloaded by checking headers
bool UrlDownload::checkDownloadable(std::string path) {
    DPRINTF("checkDownloadable\n");
    UrlDownload url(path);
    return url.exists();
}

//https does not always set content-length... we need to figure a way around...
int UrlDownload::sizeUrl(std::string path) {
    DPRINTF("sizeUrl\n");
    UrlDownload url(path);
    return url.size();
}

std::string UrlDownload::downloadUrl(std::string path) {
    DPRINTF("downloadUrl\n");
    UrlDownload url(path);
    url.download();
    return url.name();
}

void * UrlDownload::downloadUrlRange(std::string path, unsigned int start, unsigned int end) {
    DPRINTF("downloadUrlRange\n");
    void * ret = NULL;
    if(end > start) {
        UrlDownload url(path);
        ret = url.downloadRange(start, end-1);
    }
    return ret;
}
