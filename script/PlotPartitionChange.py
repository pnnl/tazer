import os
import sys
import matplotlib.pyplot as plt
import numpy as np

if(len(sys.argv) < 3):
    print("Usage: PlotPartitionChange.py [input name] [output figure name]\n")
    sys.exit()

input_trace = sys.argv[1]
output_figure = sys.argv[2]
filename1 = input_trace
# ASKING FOR NEW BLOCK:2:6:1646955999292158343
# PARTITION INFO:2:6:1646955999292988646
# MARGINALBENEFIT:2:0.00000000000000000000:1646955999292988646
# PARTITION INFO:1:1:1646955999292988646
# MARGINALBENEFIT:1:0.00000000000000000000:1646955999292988646

partitionTimes=[]
partitions = {}
marginalBenefits={}
blockRequests={}
blockReqTimes={}

#partitionTimes.append(-1)

f1 = open(filename1, "r")

for line in f1:
    p = line.split(':')
    if(len(p) > 1):
        f_id = p[1]
        if p[0] == "PARTITION INFO":
            if p[1] not in partitions.keys():
                partitions[f_id] = []
            partitions[f_id].append(int(p[2]))
            if len(partitionTimes) == 0 or int(p[3]) != partitionTimes[-1]:
                partitionTimes.append(int(p[3]))
            
        elif p[0] == "MARGINALBENEFIT":
            if p[1] not in marginalBenefits.keys():
                marginalBenefits[p[1]] = []
            marginalBenefits[p[1]].append(float(p[2]))
        elif p[0] == "ASKING FOR NEW BLOCK":
            if p[1] not in blockRequests.keys():
                blockRequests[p[1]] = []
                blockReqTimes[p[1]] = []
            blockRequests[p[1]].append(int(p[2]))
            blockReqTimes[p[1]].append(int(p[3]))


f1.close()
print("File closed")



#adjusting time arrays to start from 0 
minBlock=sys.maxsize
for key in blockReqTimes.keys():
    if blockReqTimes[key][0] < minBlock:
         minBlock = blockReqTimes[key][0]

minTime = partitionTimes[0] if partitionTimes[0] < minBlock else minBlock

partitionTimes = [x - minTime for x in partitionTimes]

for key in blockReqTimes.keys():
    blockReqTimes[key] = [x - minTime for x in blockReqTimes[key]]



print(len(partitionTimes))

plt.figure(figsize=(27,8))
colorDict = {}

for key in partitions.keys():
    name = "File " + key
    p=plt.plot(partitionTimes, partitions[key], "-", label=name)
    plt.scatter(blockReqTimes[key], blockRequests[key], c=p[-1].get_color(), alpha=0.3, label=name + " NewBlocks")
    colorDict[key] = p[-1].get_color()

plt.xlabel("Time")
plt.ylabel("Number of Blocks")
plt.legend(loc="upper left")
ax1 = plt.gca()
ax2 = ax1.twinx()
ax2.set_ylabel("Unit Marginal Benefit")

for key in marginalBenefits:
    name = "File " + key + " UMB"
    ax2.plot(partitionTimes, marginalBenefits[key], linestyle='--', color=colorDict[key], label=name)

plt.legend(loc="upper right")

# h1, l1 = ax1.get_legend_handles_labels()
# h2, l2 = ax2.get_legend_handles_labels()
# ax1.legend(h1+h2, l1+l2, loc=2)
# plt.show()

plt.savefig(output_figure)


