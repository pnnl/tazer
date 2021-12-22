import os
import sys

filename1 = sys.argv[1]
filename2 = sys.argv[2]
outfile = sys.argv[3]

with open(filename1, "r") as f:
    random = f.readlines()

with open(filename2, "r") as f:
    linear = f.readlines()

newLines = []
for a, b in zip(random, linear):
    newLines.append(a)
    parts = b.split(" ")
    #parts[0] = "tazer2.dat"
    b = " ".join(parts)
    newLines.append(b)

with open(outfile, "w") as f:
    for line in newLines:
        #if "tazer_output.dat" in line:
        #    f.write(sys.argv[4] + line)
        #else:
        f.write(line)
