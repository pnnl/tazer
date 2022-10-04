import os
import sys
import glob
import numpy as np
import matplotlib.pyplot as plt
import math
#import seaborn as sns

def set_box_color(bp, color):
    plt.setp(bp['boxes'], color=color)
    plt.setp(bp['whiskers'], color=color)
    plt.setp(bp['caps'], color=color)
    plt.setp(bp['medians'], color=color)


if(len(sys.argv)< 2):
    print("please enter the folder prefixes for each experiment set\n")
    sys.exit()


exps = {}
for i in range(1,len(sys.argv)):
    folder = sys.argv[i]
    #print(folder)
    exps[folder] = {}

    subfolder_list = glob.glob(sys.argv[i]+"*/")
    #print(subfolder_list)
    
    num_subfolders = 0
    for subfolder_name in subfolder_list: #_1, _2, _3 ..
        num_subfolders+=1
        subfolder_id= subfolder_name.split("-")[-1][:-1] #assumes id is separated by '-'
        exps[folder][subfolder_id] = {}
        file_list=glob.glob(subfolder_name+"Test*.txt")

        for file_name in file_list:
            client = (file_name.split("/")[1])
            exps[folder][subfolder_id][client] = {}
            f1 = open(file_name, "r")
            exps[folder][subfolder_id][client]["all_reads"]=-1
            exps[folder][subfolder_id][client]["private_hits"]=-1
            exps[folder][subfolder_id][client]["private_misses"]=-1
            exps[folder][subfolder_id][client]["shared_hits"]=-1
            exps[folder][subfolder_id][client]["shared_misses"]=-1
            exps[folder][subfolder_id][client]["filelock_hits"]=-1
            exps[folder][subfolder_id][client]["filelock_misses"]=-1
            exps[folder][subfolder_id][client]["network_hits"]=-1

            for line in f1:
                if("[TAZER] base request hits" in line):
                    dat = line.split(" ")
                    exps[folder][subfolder_id][client]["all_reads"] = int(dat[5])
                elif("[TAZER] privatememory request hits" in line):
                    private_name =  "Private"
                    dat = line.split(" ")
                    exps[folder][subfolder_id][client]["private_hits"] = exps[folder][subfolder_id][client]["private_hits"] + 1 + int(dat[5])
                elif("[TAZER] privatememory request misses" in line):
                    dat = line.split(" ")
                    exps[folder][subfolder_id][client]["private_misses"] = exps[folder][subfolder_id][client]["private_misses"] + 1 + int(dat[5])
                elif("[TAZER] privatememory request stalled" in line):
                    dat = line.split(" ")
                    exps[folder][subfolder_id][client]["private_hits"] = exps[folder][subfolder_id][client]["private_hits"] + int(dat[5])
                    exps[folder][subfolder_id][client]["private_misses"] = exps[folder][subfolder_id][client]["private_misses"] - int(dat[5])
                elif("[TAZER] scalable request hits" in line):
                    private_name =  "Scalable"
                    dat = line.split(" ")
                    exps[folder][subfolder_id][client]["private_hits"] = exps[folder][subfolder_id][client]["private_hits"] + 1 + int(dat[5])
                elif("[TAZER] scalable request misses" in line):
                    dat = line.split(" ")
                    exps[folder][subfolder_id][client]["private_misses"] = exps[folder][subfolder_id][client]["private_misses"] + 1 + int(dat[5])
                elif("[TAZER] scalable request stalled" in line):
                    dat = line.split(" ")
                    exps[folder][subfolder_id][client]["private_hits"] = exps[folder][subfolder_id][client]["private_hits"] + int(dat[5])
                    exps[folder][subfolder_id][client]["private_misses"] = exps[folder][subfolder_id][client]["private_misses"] - int(dat[5])
                elif("[TAZER] sharedmemory request hits" in line):
                    dat = line.split(" ")
                    exps[folder][subfolder_id][client]["shared_hits"] = exps[folder][subfolder_id][client]["shared_hits"] + 1 + int(dat[5])
                elif("[TAZER] sharedmemory request misses" in line):
                    dat = line.split(" ")
                    exps[folder][subfolder_id][client]["shared_misses"] = exps[folder][subfolder_id][client]["shared_misses"] + 1 + int(dat[5])
                elif("[TAZER] sharedmemory request stalled" in line):
                    dat = line.split(" ")
                    exps[folder][subfolder_id][client]["shared_hits"] = exps[folder][subfolder_id][client]["shared_hits"] + int(dat[5])
                    exps[folder][subfolder_id][client]["shared_misses"] = exps[folder][subfolder_id][client]["shared_misses"] - int(dat[5])
                elif("[TAZER] boundedfilelock request hits" in line):
                    dat = line.split(" ")
                    exps[folder][subfolder_id][client]["filelock_hits"] = exps[folder][subfolder_id][client]["filelock_hits"] + 1 + int(dat[5])
                elif("[TAZER] boundedfilelock request misses" in line):
                    dat = line.split(" ")
                    exps[folder][subfolder_id][client]["filelock_misses"] = exps[folder][subfolder_id][client]["filelock_misses"] +1 + int(dat[5])
                elif("[TAZER] boundedfilelock request stalled" in line):
                    dat = line.split(" ")
                    exps[folder][subfolder_id][client]["filelock_hits"] = exps[folder][subfolder_id][client]["filelock_hits"] + int(dat[5])
                    exps[folder][subfolder_id][client]["filelock_misses"] = exps[folder][subfolder_id][client]["filelock_misses"] - int(dat[5])
                elif("[TAZER] network request hits " in line):
                    dat = line.split(" ")
                    exps[folder][subfolder_id][client]["network_hits"] = exps[folder][subfolder_id][client]["network_hits"] + 1 + int(dat[5])
            #at this point we looked through one client file in one subfolder 
    #exps[folder]["num_subfolders"] = num_subfolders

    # for e in exps[folder].keys():
    #     if e != "num_subfolders":
    #         print(e)
    #     else:
    #         print("--" + str(exps[folder]["num_subfolders"]))

#we have all the numbers, get the hits for each folder (summed over all clients)

hits = {}
for exp_id in exps.keys():  #main folder
    hits[exp_id] = {}
    hits[exp_id]["private"] = []
    hits[exp_id]["shared"] = []
    hits[exp_id]["filelock"] = []
    hits[exp_id]["network"] = []

    for test in exps[exp_id].keys(): #subfolder
        private_hits=0
        shared_hits=0
        filelock_hits=0
        network_hits=0

        for client in exps[exp_id][test].keys(): #client
            private_hits += exps[exp_id][test][client]["private_hits"]
            shared_hits += exps[exp_id][test][client]["shared_hits"]
            filelock_hits += exps[exp_id][test][client]["filelock_hits"]
            network_hits += exps[exp_id][test][client]["network_hits"]

        hits[exp_id]["private"].append(private_hits if private_hits > 0 else 0)
        hits[exp_id]["shared"].append((shared_hits if shared_hits > 0 else 0))
        hits[exp_id]["filelock"].append(filelock_hits if filelock_hits> 0 else 0)
        hits[exp_id]["network"].append(network_hits)

print(hits)


#### plotting script below

numexp = len(hits.keys())
n = 0.5
width=0.4
i=0
dist = int(numexp/2 + 1.5)
colors = ["pink", "blue", "red", "green", "violet","lightgreen",   "orange", "lightblue", "black", "pink"] #['cyan', 'lightblue', 'lightgreen', 'tan', 'pink']
caches = ["Private", "SharedMemory", "SharedFilesytem", "DataSource"]

fig=plt.figure(figsize=(8,4)) #figsize=(15,10)
for key in hits.keys():
    i = i+1
    locs = np.array(range(4)) * dist + n*i
    #vals = [np.log(hits[key]["private"]),np.log(hits[key]["shared"]), np.log(hits[key]["filelock"]),np.log(hits[key]["network"]) ]
    vals = [hits[key]["private"],hits[key]["shared"], hits[key]["filelock"],hits[key]["network"] ]
    bpl = plt.boxplot(vals, positions=locs, sym='', widths=width)#, patch_artist=True)
    set_box_color(bpl, colors[i])
    plt.plot([], c=colors[i], label=key)

plt.legend()
plt.xticks(np.array(range(4))*dist+((i+1)/2*n), caches)
plt.ylim(bottom=0)
fig.savefig("D-experiment_results.png")
fig_width, fig_height = plt.gcf().get_size_inches()
print(fig_width, fig_height)

fig= plt.figure()
i=0
for key in hits.keys():
    i = i+1
    locs = [n*i]
    #vals = [np.log(hits[key]["private"]),np.log(hits[key]["shared"]), np.log(hits[key]["filelock"]),np.log(hits[key]["network"]) ]
    vals = [hits[key]["shared"]]
    bpl = plt.boxplot(vals,positions=locs, sym='', widths=width)#, patch_artist=True)
    set_box_color(bpl, colors[i])
    plt.plot([], c=colors[i], label=key)
plt.legend()
plt.xticks([(i+1)/2*n], ["SharedMemory"])
#plt.ylim(21500,24500)
fig.savefig("D-shared_results.png")

fig= plt.figure()
i=0
for key in hits.keys():
    i = i+1
    locs = [n*i]
    #vals = [np.log(hits[key]["private"]),np.log(hits[key]["shared"]), np.log(hits[key]["filelock"]),np.log(hits[key]["network"]) ]
    vals = [hits[key]["private"]]
    bpl = plt.boxplot(vals,positions=locs, sym='', widths=width)#, patch_artist=True)
    set_box_color(bpl, colors[i])
    plt.plot([], c=colors[i], label=key)
plt.legend()
plt.xticks([(i+1)/2*n], ["PrivateMemory"])
#plt.ylim(217700,218000)
fig.savefig("D-private_results.png")

fig= plt.figure()
i=0
for key in hits.keys():
    i = i+1
    locs = [n*i]
    #vals = [np.log(hits[key]["private"]),np.log(hits[key]["shared"]), np.log(hits[key]["filelock"]),np.log(hits[key]["network"]) ]
    vals = [hits[key]["network"]]
    bpl = plt.boxplot(vals,positions=locs, sym='', widths=width)#, patch_artist=True)
    set_box_color(bpl, colors[i])
    plt.plot([], c=colors[i], label=key)
plt.legend()
plt.xticks([(i+1)/2*n], ["DataSource"])
#plt.ylim(217700,218000)
fig.savefig("D-network_results.png")








