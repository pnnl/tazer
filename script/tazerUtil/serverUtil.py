import subprocess as sp
from . import util

# This is a Tazer Server class.  It is here to help wrap several functions that
# exist across the Tazer source.  The Commands listed in the arguments are the
# equivalent commands (and arguments) as if run from the build tree.
class TazerServer:
    # The path is the path to the build directory!!!
    def __init__(self, path, serverIpAddr=None, port=None, outputFileName=None):
        self.serverProc = None
        self.path = path
        self.serverIpAddr = serverIpAddr
        self.port = port
        self.printResult = True
        self.outputFileName = outputFileName
        self.outFile = util.openOutputFile(outputFileName)
        self.pid

    # This functions runs a Tazer server locally
    # Command:
    #   pathToBuild/src/server/server port
    # Args:
    #   (optional) port = Port to listen on. Default is 6023 (Config.h: serverPort)
    def run(self, port=None):
        port = port if port != None else self.port 
        serverPath = self.path + "/src/server/server"
        args = [serverPath]
        if port:
            args.append(port)
        util.printCommand(args)
        self.serverProc = sp.Popen(args, stdout=self.outFile, stderr=self.outFile)
        self.childPid = self.serverProc.pid

    # This funtions pings a Tazer server.
    # Command:
    #   pathToBuild/test/PingServer serverIpAddr port attempts sleepTime
    # Args:
    #   (optional) serverIpAddr = Ip address of the server. Default is 127.0.0.1
    #   (optional) port = Port server is listening on. Default is 6023 (Config.h: serverPort)
    #   (optional) attempts = Number of times to attempt a ping. Default is 10
    #   (optional) sleepTime = Time in seconds to sleep between attempts. Default is 10
    def ping(self, serverIpAddr=None, port=None, attempts=None, sleepTime=None):
        serverIpAddr = serverIpAddr if serverIpAddr != None else self.serverIpAddr
        port = port if port != None else self.port 
        pingPath = self.path + "/test/PingServer"
        args = [pingPath]
        if serverIpAddr:
            args.append(serverIpAddr)
            if port:
                args.append(port)
                if attempts:
                    args.append(attempts)
                    if sleepTime:
                        args.append(sleepTime)
        util.printCommand(args)
        process = sp.Popen(args, stdout=self.outFile, stderr=self.outFile, universal_newlines=True)
        process.wait()
        if self.printResult and self.outputFileName == None:
            print(process.stdout.read())

    # This funtion closes a Tazer server. Can close a local or remote server.
    # Command:
    #   pathToBuild/test/CloseServer serverIpAddr port
    # Args:
    #   serverIpAddr = Ip address of the server.
    #   (optional) port = Port server is listening on. Default is 6023 (Config.h: serverPort)
    def close(self, serverIpAddr=None, port=None):
        serverIpAddr = serverIpAddr if serverIpAddr != None else self.serverIpAddr
        port = port if port != None else self.port
        closePath = self.path + "/test/CloseServer"
        args = [closePath]
        if serverIpAddr == None:
            serverIpAddr = "127.0.0.1"
        args.append(serverIpAddr)
        if port:
            args.append(port)
        util.printCommand(args)
        process = sp.Popen(args, stdout=self.outFile, stderr=self.outFile, universal_newlines=True)
        process.wait()
        if self.printResult and self.outputFileName == None:
            print(process.stdout.read())

    # Will block until the process that launched server finishes
    def wait(self):
        if self.serverProc != None:
            self.serverProc.wait()

    # Returns true if the process that launched server is still running
    def poll(self):
        if self.serverProc != None:
            return None == self.serverProc.poll()
        return False

    # Kills the processes that is running the server
    def kill(self):
        if self.serverProc != None:
                self.serverProc.kill()

    # Get the PID of the child process
    def pid(self):
        if self.serverProc != None:
            print("PID: ", self.childPid)