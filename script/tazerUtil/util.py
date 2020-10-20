import os
import subprocess as sp

#This is a global varible which toggles printing the command being launched
printOn = True

# This prints the command to be sent to subprocess
def printCommand(args):
    if printOn:
        sep=' '
        print(sep.join(args))

# This is just a function to collect the output in a file or to the screen
def openOutputFile(outFileName):
    if outFileName != None:
        try:
            outFile = open(outFileName, 'w')
        except:
            print ("Cannot open server output file ", outFileName)
            outFile = sp.PIPE
    else:
        outFile = sp.PIPE
    return outFile

class TazerEnv:
    #These are all the Tazer environment that can be set
    def __init__(self):
        self.envDict = {
            "TAZER_ENABLE_SHARED_MEMORY" : None,
            "TAZER_PRIVATE_MEM_CACHE_SIZE" : None,
            "TAZER_SHARED_MEM_CACHE" : None,
            "TAZER_SHARED_MEM_CACHE_SIZE" : None,
            "TAZER_BB_CACHE" : None,
            "TAZER_BB_CACHE_SIZE" : None,
            "TAZER_FILE_CACHE" : None,
            "TAZER_FILE_CACHE_SIZE" : None,
            "TAZER_BOUNDED_FILELOCK_CACHE" : None,
            "TAZER_BOUNDED_FILELOCK_CACHE_SIZE" : None,
            "TAZER_FILELOCK_CACHE" : None,
            "TAZER_NETWORK_CACHE" : None,
            "TAZER_LOCAL_FILE_CACHE" : None,
            "TAZER_NETWORK_CACHE" : None,
            "TAZER_SERVER_CACHE_SIZE" : None,
            "TAZER_SERVER_CONNECTIONS" : None,
            "TAZER_PREFETCH_EVICT" : None,
            "TAZER_PREFETCHER_TYPE" : None,
            "TAZER_PREFETCH_NUM_BLKS" : None,
            "TAZER_PREFETCH_DELTA" : None,
            "TAZER_PREFETCH_FILEDIR" : None,
            "TAZER_CURL_HANDLES" : None,
            "TAZER_URL_TIMEOUT" : None,
            "TAZER_DOWNLOAD_PATH" : None,
            "TAZER_DELETE_DOWNLOADS" : None,
            "TAZER_URL_TIMEOUT" : None,
            "TAZER_DOWNLOAD_FOR_SIZE" : None,
            "TAZER_CURL_ON_START" : None
        }

    #Sets a single Tazer environment variable
    def setVar(self, args):
        for key, value in args.__dict__.items():
            if key in self.envDict:
                if value != None:
                    if isinstance(value, int):
                        value = str(value)
                    self.envDict[key] = value

    #Sets the evironment based on the tazer dictionary
    def setEnv(self):
        for key in self.envDict.keys():
            if self.envDict[key] != None:
                os.environ[key] = self.envDict[key]

    #Prints the actual values of the environment
    def printEnv(self):
        for key in self.envDict.keys():
            if key in os.environ:
                print(key, " = ", os.environ[key])
            else:
                print(key, " = None")

    # Prints the available options to set in Tazer
    def printEnvOptions(self):
        for key in self.envDict.keys():
            print(key)

    # Returns the avaible options to set in Tazer
    def getEnvVars(self):
        return self.envDict.keys()
                