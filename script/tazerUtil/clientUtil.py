import os
import subprocess as sp
from . import util

# This is a Tazer client class.  It is here to help launch programs using
# the Tazer interposer.  The Commands listed in the arguments are the
# equivalent commands (and arguments) as if run from the build tree.
class TazerClient:
    # The path is the path to the build directory
    def __init__(self, path, outputFileName=None):
        self.path = path + "/src/client/libclient.so"
        self.outputFileName = outputFileName
        self.outFile = util.openOutputFile(outputFileName)

    # This function will run a given command using LD_PRELOAD to load 
    # the tazer interposing library.  Any options required by the 
    # tazer client should be set using the evironment variables which 
    # can be found in config.h or util.py.
    # Command:
    #   env LD_PRELOAD /pathToBuild/src/client/libclient.so; app argv
    # Args:
    #   args - this is a list of the space seperated (string) arguments provided to the app
    #       (i.e. ["file1.txt", "file2.txt"])
    def run(self, args):
        # We will store the previous value of LD_PRELOAD and restore it after the run
        try:
            oldLdPreload = os.environ["LD_PRELOAD"]
        except:
            oldLdPreload = None
            newLdPreload = self.path
        else:
            newLdPreload = self.path + ":" + oldLdPreload
        os.environ["LD_PRELOAD"] = newLdPreload
        util.printCommand(args)
        if self.outputFileName == None:
            for progress in self.execute(args):
                print(progress, end="")
        else:
            process = sp.Popen(args, stdout=self.outFile, stderr=self.outFile, universal_newlines=True)
            process.wait()
        # Here we do the LD_PRELOAD restoration
        if oldLdPreload == None:
            del os.environ["LD_PRELOAD"]
        else:
            os.environ["LD_PRELOAD"] = oldLdPreload

    #This is a trick to print the progress as we go
    def execute(self, cmd):
        # print("SHOULD SEE OUTPUT")
        process = sp.Popen(cmd, stdout=sp.PIPE, stderr=sp.PIPE, universal_newlines=True)
        for outLine in iter(process.stdout.readline, ""):
            yield outLine 
        process.stdout.close()
        process.wait()
        print(process.stderr.read())