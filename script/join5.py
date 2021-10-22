import os
import sys

def changeLine(line, num):
    if num > 0:
        temp = line.split(" ")
        temp[0] = "tazer" + str(num+1) + ".dat"
        return " ".join(temp)
    return line

outfile = sys.argv[1]
filename = sys.argv[2]
num = int(sys.argv[3])

def myIter():
    while True:
        for i in range(num):
            yield i

with open(filename, "r") as f:
    random = f.readlines()

newLines = []
for line, i in zip(random, myIter()):
    newLines.append(changeLine(line,i))

with open(outfile, "w") as f:
    for line in newLines:
        f.write(line)
