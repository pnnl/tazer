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
# ADDFILE:2:tazer2.dat
# ASKING FOR NEW BLOCK:2:6:1646955999292158343
# PARTITION INFO:2:5:1648674463499588098
# UNITBENEFIT:2:1919.65498699479621791397:1648674463499588098
# UNITMARGINALBENEFIT:2:-1370.05550726276669593062:1648674463499588098
# PARTITION INFO:3:7:1648674463499588098
# UNITBENEFIT:3:1088.08479651970583290677:1648674463499588098
# UNITMARGINALBENEFIT:3:-169.83532897442478315497:1648674463499588098

filenames={}
partitionTimes=[]
partitions = {}
marginalBenefits={}
blockRequests={}
blockReqTimes={}
unitBenefits={}
demandMetric={}

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
            
        elif p[0] == "UNITMARGINALBENEFIT":
            if p[1] not in marginalBenefits.keys():
                marginalBenefits[p[1]] = []
            marginalBenefits[p[1]].append(float(p[2]))

        elif p[0] == "UNITBENEFIT":
            if p[1] not in unitBenefits.keys():
                unitBenefits[p[1]] = []
            unitBenefits[p[1]].append(float(p[2]))
        
        elif p[0] == "UPPERLEVELMETRIC":
            if p[1] not in demandMetric.keys():
                demandMetric[p[1]] = []
            demandMetric[p[1]].append(float(p[2]))

        elif p[0] == "ASKING FOR NEW BLOCK":
            if p[1] not in blockRequests.keys():
                blockRequests[p[1]] = []
                blockReqTimes[p[1]] = []
            blockRequests[p[1]].append(int(p[2]))
            blockReqTimes[p[1]].append(int(p[3]))

        elif p[0] == "ADDFILE":
            if p[1] not in filenames.keys():
                filenames[p[1]] = p[2]


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

#for key in partitions.keys():
for key in sorted(partitions):
    #name = "File " + key
    p=plt.plot(partitionTimes, partitions[key], "-", label=filenames[key])
    plt.scatter(blockReqTimes[key], blockRequests[key], c=p[-1].get_color(), alpha=0.3, label=filenames[key] + " NewBlocks")
    colorDict[key] = p[-1].get_color()

plt.xlabel("Time")
plt.ylabel("Number of Blocks")
plt.legend(loc="upper left")
ax1 = plt.gca()
ax2 = ax1.twinx()
ax2.set_ylabel("Unit Marginal Benefit")

for key in marginalBenefits:
    name = "File " + key + " UMB"
    ax2.plot(partitionTimes, marginalBenefits[key], linestyle='--', color=colorDict[key], label=filenames[key] + " UMB")

plt.legend(loc="upper right")

# h1, l1 = ax1.get_legend_handles_labels()
# h2, l2 = ax2.get_legend_handles_labels()
# ax1.legend(h1+h2, l1+l2, loc=2)
# plt.show()

plt.savefig("UMB_"+output_figure)

########################


plt.figure(figsize=(27,8))
colorDict = {}

#for key in partitions.keys():
for key in sorted(partitions):
    #name = "File " + key
    p=plt.plot(partitionTimes, partitions[key], "-", label=filenames[key])
    plt.scatter(blockReqTimes[key], blockRequests[key], c=p[-1].get_color(), alpha=0.3, label=filenames[key] + " NewBlocks")
    colorDict[key] = p[-1].get_color()

plt.xlabel("Time")
plt.ylabel("Number of Blocks")
plt.legend(loc="upper left")
ax1 = plt.gca()
ax2 = ax1.twinx()
ax2.set_ylabel("Unit Benefit")

for key in unitBenefits:
    name = "File " + key + " UB"
    ax2.plot(partitionTimes, unitBenefits[key], linestyle='--', color=colorDict[key], label=filenames[key] + " UB")

plt.legend(loc="upper right")

# h1, l1 = ax1.get_legend_handles_labels()
# h2, l2 = ax2.get_legend_handles_labels()
# ax1.legend(h1+h2, l1+l2, loc=2)
# plt.show()

plt.savefig("UB_"+output_figure)

#################


plt.figure(figsize=(27,8))
colorDict = {}

#for key in partitions.keys():
for key in sorted(partitions):
    #name = "File " + key
    p=plt.plot(partitionTimes, partitions[key], "-", label=filenames[key])
    plt.scatter(blockReqTimes[key], blockRequests[key], c=p[-1].get_color(), alpha=0.3, label=filenames[key] + " NewBlocks")
    colorDict[key] = p[-1].get_color()

plt.xlabel("Time")
plt.ylabel("Number of Blocks")
plt.legend(loc="upper left")
ax1 = plt.gca()
ax2 = ax1.twinx()
ax2.set_ylabel("Demand")

for key in demandMetric:
    name = "File " + key + " Demand"
    ax2.plot(partitionTimes, demandMetric[key], linestyle='--', color=colorDict[key], label=filenames[key] + " Demand")

plt.legend(loc="upper right")

# h1, l1 = ax1.get_legend_handles_labels()
# h2, l2 = ax2.get_legend_handles_labels()
# ax1.legend(h1+h2, l1+l2, loc=2)
# plt.show()

plt.savefig("Demand_"+output_figure)

#############
plt.figure(figsize=(27,8))
colorDict = {}

#for key in partitions.keys():
for key in sorted(partitions):
    #name = "File " + key
    p=plt.plot(partitionTimes, marginalBenefits[key], "--", label=filenames[key])
    


plt.xlabel("Time")
plt.ylabel("Unit Marginal Benefit")
plt.legend(loc="upper left")
ax1 = plt.gca()


plt.legend(loc="upper right")

plt.ylim(-0.01,0.01)
plt.savefig("UMBonly_"+output_figure)

#################