import numpy as np
import sys

f = open(sys.argv[1], "r")
data = f.read()
f.close()

data = data.split("\n")

data = data[:-1]
nd = []
for i in range(len(data)):
    t = data[i].split(",")
    temp = t[:3]
    xx = 0
    for j in t[3:]:
        r = j.split(" +- ")
        xx += float(r[0]) + float(r[1])
    temp.append(xx)
    nd.append(temp)
data = np.array(nd)
error = data[:,-1]
min_ = min(error)
ind_ = list(error).index(min_)
print(data[ind_])
