import numpy as np
import matplotlib.pyplot as plt
import sys
import argparse

def genAccess(fsize,fname, rsize, rcofv, rprob, fileStarts, segmentStarts, bsize, bIdx, fIdx, accesses, of):
    # reset indice to start from starting index
    fileInd = segmentStarts[bIdx]+fileStarts[fIdx]
    while fileInd < segmentStarts[bIdx]+fileStarts[fIdx]+bsize and fileInd < fsize:
        actual_read_size = abs(int(np.random.normal(rsize, rsize*rcofv)))
        while actual_read_size == 0:
            abs(int(np.random.normal(rsize, rsize*rcofv)))
        if rprob > np.random.rand():
            accesses.append((fileInd, actual_read_size, fIdx))
            # of.write(fname + " " + str(fileInd) +
            #          " " + str(actual_read_size)+" 0 0\n")
        fileInd += actual_read_size


def genAccessPattern(dsize, fname, physicalFileSize, nfiles, rsize, rcofv, rprob, segmentSize, nreps, osize, wtype, outName, random):
    fsize = int(dsize/nfiles)
    ssize = segmentSize if segmentSize > 0 else fsize
    nsegs = int(fsize/ssize) + (1 if ssize % fsize > 0 else 0)
    fileStarts = np.array(range(nfiles))*fsize
    segmentStarts = np.array(range(nsegs))*ssize

    wsize = int(osize/nsegs)

    accesses = []
    outInd = 0
    with open(outName, 'w') as of:
        for f in range(nfiles):
            for b in range(nsegs):
                for _ in range(nreps):
                    tmp_accesses=[]
                    genAccess(fsize, fname, rsize, rcofv, rprob, fileStarts,
                              segmentStarts, ssize, b, f, tmp_accesses, of)
                    if random:
                        np.random.shuffle(tmp_accesses)
                    for (fileInd, actual_read_size, fIdx) in tmp_accesses:
                        of.write(fname + " " + str(fileInd) + " " + str(actual_read_size)+" 0 0\n")
                    
                    accesses += tmp_accesses
                if wtype == "strided":
                    of.write("tazer_output.dat "+str(outInd) +
                                " "+str(wsize) + " 0 0\n")
                    outInd += wsize
        if wtype == "batched":
            of.write("tazer_output.dat "+str(outInd)+" "+str(osize) + " 0 0\n")

    return np.array(accesses)


if __name__ == "__main__":

    parser = argparse.ArgumentParser()
    # experiment/architecture parameters
    parser.add_argument("-i", "--ioRate", type=float,
                        help="experiment wide io rate (MB/s)", default=125)
    parser.add_argument("-t", "--tasksPerCore", type=int,
                        help="number of tasks", default=1)
    parser.add_argument("-C", "--numCores", type=int,
                        help="number of cores in experiment", default=1)
    parser.add_argument("-T", "--execTime", type=int,
                        help="desired cpu time (seconds)", default=300)
    
    

    # file access parameters
    parser.add_argument("-f", "--numFiles", type=int,
                        help="number of files to simulate", default=1)
    parser.add_argument("-c", "--numCycles", type=int,
                        help="number of cycles through a file", default=1)
    parser.add_argument("-s", "--segmentSize", type=int,
                        help="size of a segment (0 = fileSize)", default=0)
    parser.add_argument("-R","--random",action='store_true',help= "enable random acccess pattern")
    

    # individual read parameters
    parser.add_argument("-r", "--readSize", type=int,
                        help="average read size (B)", default=2**14)
    parser.add_argument("-V", "--readCofV", type=float,
                        help="read size coefficient of variation", default=.1)
    parser.add_argument("-p", "--readProbability", type=float,
                        help="(0.0-1.0) probability of a read occuring", default=1.0)
    parser.add_argument("-o", "--outputSize", type=int,
                    help="average output size write", default=2**23)
    parser.add_argument("-w", "--outputPattern", choices=["strided","batched"],
                        help="strided: data written after a segment has been read, batch: all data is written at the end", default="strided")

    # miscellaneous generator parameters

    parser.add_argument("-S", "--maxFileSize", type=int,
                        help="max pyhsical file size", default=8 * 2**30)
    parser.add_argument("-n", "--inputFileName", type=str,
                        help="name/path of file to generate accesses for", default="tazer8GB.dat")
    parser.add_argument("-F", "--outputFileName", 
                        help="access pattern trace output name", default="accesses.txt")
    parser.add_argument("-v", "--verbose", action='store_true',
                        help="enable verbose output")
    parser.add_argument("-P", "--plot", type=str,
                        help="filename to save plot of resulting access pattern", default="accesses.png")
    

    args = parser.parse_args()
    if args.verbose:
        print(args)

    ioRate = args.ioRate * 10**6
    # data to read, not necessarily file size...
    totalSize = (ioRate*args.execTime*args.tasksPerCore) / \
        args.readProbability
    taskSize = float(totalSize)/float(args.numCores*args.tasksPerCore)
    maxFileSize = taskSize/args.numCycles

    if maxFileSize <= args.maxFileSize:
        accesses = genAccessPattern(maxFileSize, args.inputFileName, args.maxFileSize, args.numFiles, args.readSize, args.readCofV,
                                    args.readProbability, args.segmentSize, args.numCycles, args.outputSize, args.outputPattern, args.outputFileName, args.random)
    else:
        print("ERROR: Physical File Size is smaller than required given paramemters")
        print("required file size > ",int(maxFileSize),"bytes" )
        sys.exit(1)

    if args.verbose:

        maxUniqueSize = maxFileSize * args.numCores
        baseLineRate = 125*10**6  # 110 MB/s
        print("total experiment data read:", totalSize/10.0**9,
              "GB, tx time @ 1gbps =", totalSize/baseLineRate, "seconds")
        print("total task file size:", taskSize/10.0**9,
              "GB, tx time @ 1gbps =", taskSize/baseLineRate, "seconds")
        print("actual data read:", np.sum(accesses, axis=0)[1]/10.0**9, "GB")

    if args.plot:
        if args.verbose:
            print(np.unique(accesses.T[2]))
        for f in np.unique(accesses.T[2]):

            fileAccess = np.argwhere(accesses.T[2] == f)
            plt.plot(fileAccess, accesses.T[0][fileAccess]/(1024*1024), "o")
        plt.tight_layout()
        plt.xlabel("access")
        plt.ylabel("file offset (MB)")
        plt.savefig(args.plot)

    #print per task iorate
    if args.verbose:
        print ("Per task I/O rate")
    print (args.ioRate/args.numCores)
