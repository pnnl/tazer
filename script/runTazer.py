#!/usr/bin/python
import sys
import argparse
import tazerUtil as tu
import importlib
importlib.reload(tu)

# Check python version
def checkPython():
    if sys.version_info.major < 3:
        print("This script was tested with python version 3.7. Thus it requires python 3")
        ver = "python " + str(sys.version_info.major) + "." + str(sys.version_info.minor) + "." + str(sys.version_info.micro)
        print(ver)
        exit(1)

# Handles if a client command is passed on command line using quotation marks (i.e. "ls -alh")
def parseClientCommand(comList):
    res = []
    for one in comList:
        if ' ' in one:
            res.extend(one.split())
        else:
            res.append(one)
    return res

def parseArgs():
    parser = argparse.ArgumentParser()
    parser.add_argument('pathToBuild', action='store', type=str, help='path to the build directory of Tazer')
    parser.add_argument('-c', '--client', action='append', nargs='+', type=str, help='command for the client to run')
    parser.add_argument('-s', "--server", action='store', type=str, default=None, help='server IP address')
    parser.add_argument('-p', "--port", action='store', type=str, default=None, help='server port')
    parser.add_argument('--clientOnly', action='store_true', default=False, help='only runs the client')
    parser.add_argument('--serverOnly', action='store_true', default=False, help='only runs the sever')
    parser.add_argument('--closeServer', action='store_true', default=False, help='closes the server')
    parser.add_argument('--serverLog', action='store', type=str, default=None, help='outfile for server log')
    parser.add_argument('--clientLog', action='store', type=str, default=None, help='outfile for client log')
    parser.add_argument('--printEnv', action='store_true', default=False, help='prints Tazer environment variables')
    tazerEnvVars = tu.TazerEnv().getEnvVars()
    for envVar in tazerEnvVars:
        envVarArg = "--" + envVar
        parser.add_argument(envVarArg, action='store', default=None, help='Tazer environment variable')

    args = parser.parse_args()
    if sum([args.serverOnly, args.closeServer, args.clientOnly]) > 1:
        parser.error("Cannot set clientOnly serverOnly closeServer at the same time")
    if args.clientOnly and args.client == None:
        parser.error('Must set a client command to run')
    if args.clientOnly == False and args.serverOnly == False and args.closeServer == False and args.client == None:
        parser.error('Must set a client command to run')
    return args

# This takes the arguments from the command line and sets the Tazer environment variables
def getEnvFromArgs(args):
    tazerEnv = tu.TazerEnv()
    tazerEnv.setVar(args)
    tazerEnv.setEnv()
    if args.printEnv:
        tazerEnv.printEnv()

# This runs both the server and client
def runLocalCommand(path, server, port, comList, serverLog=None, clientLog=None):
    print("--Run Server and Client--")
    tazerServer = tu.TazerServer(path, serverIpAddr=server, port=port, outputFileName=serverLog)
    tazerServer.run()
    if tazerServer.poll():
        tazerServer.ping(serverIpAddr=server, port=port)
        tazerServer.pid()
        try:
            tazerClient = tu.TazerClient(path, outputFileName=clientLog)
            tazerClient.run(parseClientCommand(comList))
        except:
            print("Failed running client")
        finally:
            tazerServer.close(serverIpAddr=server, port=port)
            tazerServer.kill()
    else:
        print("Could not launch server.  Check to see if server:port is avaialble")

# Only runs the server. This leaves the server running.
def runServerCommand(path, server, port, serverLog=None):
    print("--Server Only--")
    tazerServer = tu.TazerServer(path, serverIpAddr=server, port=port, outputFileName=serverLog)
    tazerServer.run()
    if tazerServer.poll():
        tazerServer.ping(serverIpAddr=server, port=port)
        tazerServer.pid()
    else:
        print("Could not launch server.  Check to see if server:port is avaialble")

# Send a close command to a server
def closeServerCommand(path, server, port, serverLog=None):
    print("--Close server--")
    tu.TazerServer(path, outputFileName=serverLog).close(serverIpAddr=server, port=port)

# Runs a client command
def runClientCommand(path, comList, clientLog=None):
    print("--Client Only--")
    newComList = parseClientCommand(comList)
    tu.TazerClient(path, outputFileName=clientLog).run(newComList)

def main():
    checkPython()
    args = parseArgs()
    getEnvFromArgs(args)
    if args.closeServer:
        closeServerCommand(args.pathToBuild, args.server, args.port, serverLog=args.serverLog)
    elif args.serverOnly:
        runServerCommand(args.pathToBuild, args.server, args.port, serverLog=args.serverLog)
    elif args.clientOnly:
        runClientCommand(args.pathToBuild, *args.client, clientLog=args.clientLog)
    else:
        runLocalCommand(args.pathToBuild, args.server, args.port, *args.client, serverLog=args.serverLog, clientLog=args.clientLog)

if __name__ == "__main__":
    main()
