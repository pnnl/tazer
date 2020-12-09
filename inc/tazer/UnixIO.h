// -*-Mode: C++;-*- // technically C99

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

#ifndef UNIXIO_H_
#define UNIXIO_H_
#include <dlfcn.h>
#include <stdio.h>

typedef int (*unixopen_t)(const char *path, int flags, ...);
typedef int (*unixclose_t)(int fd);

typedef ssize_t (*unixread_t)(int fd, void *buf, size_t count);
typedef ssize_t (*unixwrite_t)(int fd, const void *buf, size_t count);

typedef ssize_t (*unixlseek_t)(int fd, off_t offset, int whence);
typedef off64_t (*unixlseek64_t)(int fd, off64_t offset, int whence);

typedef int (*unixxstat_t)(int version, const char *filename, struct stat *buf);
typedef int (*unixxstat64_t)(int version, const char *filename, struct stat64 *buf);

typedef int (*unixfsync_t)(int fd);
typedef int (*unixfdatasync_t)(int fd);

typedef FILE *(*unixfopen_t)(__const char *__restrict __filename, __const char *__restrict __modes);
typedef int (*unixfclose_t)(FILE *fp);

typedef size_t (*unixfread_t)(void *ptr, size_t size, size_t nmemb, FILE *stream);
typedef size_t (*unixfwrite_t)(const void *ptr, size_t size, size_t nmemb, FILE *stream);

typedef long int (*unixftell_t)(FILE *__stream);
typedef int (*unixfseek_t)(FILE *__stream, long int __off, int __whence);
typedef void (*unixrewind_t)(FILE *__stream);

typedef int (*unixfgetc_t)(FILE *fp);
typedef char *(*unixfgets_t)(char *__restrict s, int n, FILE *__restrict fp);
typedef int (*unixfputc_t)(int __c, FILE *__stream);
typedef int (*unixfputs_t)(const char *__restrict __s, FILE *__restrict __stream);

typedef int (*unixfileno_t)(FILE *fp);
typedef int (*unixflockfile_t)(FILE *fp);
typedef int (*unixftrylockfile_t)(FILE *fp);
typedef int (*unixfunlockfile_t)(FILE *fp);

typedef int (*unixfflush_t)(FILE *fp);
typedef int (*unixfeof_t)(FILE *fp);

typedef ssize_t (*unixreadv_t)(int fd, const struct iovec *iov, int iovcnt);
typedef ssize_t (*unixwritev_t)(int fd, const struct iovec *iov, int iovcnt);
#endif /* UNIXIO_H_ */