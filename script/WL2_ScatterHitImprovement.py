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


if(len(sys.argv)< 3):
    print("please enter the png prefix and folder prefixes for each experiment set and baseline folder \n")
    sys.exit()

prefname = sys.argv[1]
exps = {}

base_key=sys.argv[-1]

for i in range(2, len(sys.argv)):
    folder = sys.argv[i]
    exps[folder] = {} #will create a separate chart for each exp 
    if(i == 2): #baseline experiments
        subfolder_list = glob.glob(sys.argv[i]+"*_[01]")
    else:
        subfolder_list = glob.glob(sys.argv[i]+"*_[01]")
    
    
    print(folder)
    # print(subfolder_list)
    # print("\n")
    for subfolder_name in subfolder_list: #_1, _2, _3 ..
        #num_subfolders+=1
        subfolder_id= subfolder_name.split("/")[-1][:-2] #assumes id is separated by '-'
        exps[folder][subfolder_id]={}
        client = subfolder_id
        exps[folder][client]["all_reads"]=0
        exps[folder][client]["private_hits"]=0
        exps[folder][client]["private_misses"]=0
        exps[folder][client]["shared_hits"]=-1
        exps[folder][client]["shared_misses"]=-1
        exps[folder][client]["filelock_hits"]=-1
        exps[folder][client]["filelock_misses"]=-1
        exps[folder][client]["network_hits"]=0

        file_name = glob.glob(subfolder_name+"/Workload*.out")
        #print(file_name)
        f1 = open(file_name[0], "r")

        for line in f1:
            if("[TAZER] base request hits" in line):
                if(exps[folder][client]["all_reads"] == 0):
                    dat = line.split(" ")
                    exps[folder][client]["all_reads"] = int(dat[5])
            elif("[TAZER] privatememory request hits" in line):
                private_name =  "Private"
                dat = line.split(" ")
                exps[folder][client]["private_hits"] = exps[folder][client]["private_hits"] + int(dat[5])
            elif("[TAZER] privatememory request misses" in line):
                dat = line.split(" ")
                exps[folder][client]["private_misses"] = exps[folder][client]["private_misses"] + int(dat[5])
            elif("[TAZER] privatememory request stalled" in line):
                dat = line.split(" ")
                exps[folder][client]["private_hits"] = exps[folder][client]["private_hits"] + int(dat[5])
                exps[folder][client]["private_misses"] = exps[folder][client]["private_misses"] - int(dat[5])
            elif("[TAZER] scalable request hits" in line):
                private_name =  "Scalable"
                dat = line.split(" ")
                exps[folder][client]["private_hits"] = exps[folder][client]["private_hits"] + int(dat[5])
            elif("[TAZER] scalable request misses" in line):
                dat = line.split(" ")
                exps[folder][client]["private_misses"] = exps[folder][client]["private_misses"] + int(dat[5])
            elif("[TAZER] scalable request stalled" in line):
                dat = line.split(" ")
                exps[folder][client]["private_hits"] = exps[folder][client]["private_hits"] + int(dat[5])
                exps[folder][client]["private_misses"] = exps[folder][client]["private_misses"] - int(dat[5])
            elif("[TAZER] sharedmemory request hits" in line):
                dat = line.split(" ")
                exps[folder][client]["shared_hits"] = exps[folder][client]["shared_hits"] + int(dat[5])
            elif("[TAZER] sharedmemory request misses" in line):
                dat = line.split(" ")
                exps[folder][client]["shared_misses"] = exps[folder][client]["shared_misses"] + int(dat[5])
            elif("[TAZER] sharedmemory request stalled" in line):
                dat = line.split(" ")
                exps[folder][client]["shared_hits"] = exps[folder][client]["shared_hits"] + int(dat[5])
                exps[folder][client]["shared_misses"] = exps[folder][client]["shared_misses"] - int(dat[5])
            elif("[TAZER] boundedfilelock request hits" in line):
                dat = line.split(" ")
                exps[folder][client]["filelock_hits"] = exps[folder][client]["filelock_hits"] + int(dat[5])
            elif("[TAZER] boundedfilelock request misses" in line):
                dat = line.split(" ")
                exps[folder][client]["filelock_misses"] = exps[folder][client]["filelock_misses"] + int(dat[5])
            elif("[TAZER] boundedfilelock request stalled" in line):
                dat = line.split(" ")
                exps[folder][client]["filelock_hits"] = exps[folder][client]["filelock_hits"] + int(dat[5])
                exps[folder][client]["filelock_misses"] = exps[folder][client]["filelock_misses"] - int(dat[5])
            elif("[TAZER] network request hits " in line):
                dat = line.split(" ")
                exps[folder][client]["network_hits"] = exps[folder][client]["network_hits"] + int(dat[5])
            elif("CloseServer" in line):
                break
 
#print(exps)

charts= exps[sys.argv[3]]


scats = {}
#Workload2_Hb-50_H-time_StealThreshold-0-NEWZ

for chart in charts:
    scats[chart]={}
    scats[chart]["base"]={}
    scats[chart]["default"]={}
    scats[chart]["best"]={}

    scats[chart]["base"]["private"]= exps[base_key][chart]["private_hits"]
    scats[chart]["base"]["network"]= exps[base_key][chart]["network_hits"]
    scats[chart]["default"]["private"]=exps["Workload2_Hb-50_H-time_StealThreshold-0-NEWZ/"][chart]["private_hits"]
    scats[chart]["default"]["network"]=exps["Workload2_Hb-50_H-time_StealThreshold-0-NEWZ/"][chart]["network_hits"]
    
    bestkey=""
    bestdata=-1
    for key in exps.keys():
        if(key != base_key):
            if(exps[key][chart]["network_hits"] < bestdata or bestdata < 0):
                bestkey = key
                bestdata = exps[key][chart]["network_hits"]
    
    scats[chart]["best"]["private"] = exps[bestkey][chart]["private_hits"]
    scats[chart]["best"]["network"] = exps[bestkey][chart]["network_hits"]





i=0

locs = np.array(range(8)) * 4
fig=plt.figure(figsize=(6,4)) #figsize=(15,10)
colors = ["pink", "blue", "red", "green", "violet",
            "lightgreen", "orange", "lightblue", "pink", "black", 
            "cyan", "tan", "lightblue", "firebrick", "gold", 
            "magenta", "forestgreen", "coral", "c", "moccasin",
            "pink", "blue", "red", "green", "violet",
            "lightgreen", "orange", "lightblue", "black", "pink"] 

wls = ['Workload2-A', 'Workload2-B', 'Workload2-C', 'Workload2-D', 'Workload2-E', 'Workload2-F', 'Workload2-G', 'Workload2-H']
print(wls)

for wl in wls:
    pos = [locs[i], locs[i]+1]
    vals_default = [scats[wl]["default"]["private"]/scats[wl]["base"]["private"]*100, ((2*scats[wl]["base"]["network"])-scats[wl]["default"]["network"])/scats[wl]["base"]["network"]*100]
    vals_best = [scats[wl]["best"]["private"]/scats[wl]["base"]["private"]*100, ((2*scats[wl]["base"]["network"])-scats[wl]["best"]["network"])/scats[wl]["base"]["network"]*100]
    
    plt.scatter(pos[0], vals_default[0], marker='.', color="orange")
    plt.scatter(pos[1], vals_default[1], marker='.', color="blue")
    plt.scatter(pos[0], vals_best[0], marker='o', facecolor='none',alpha=0.7, color="orange")
    plt.scatter(pos[1], vals_best[1], marker='o', facecolor='none',alpha=0.7, color="blue")

    i=i+1       

loc2 = locs+0.5

plt.xticks(loc2, [ "A", "B", "C", "D", "E", "F", "G", "H"])
plt.plot([locs[0]-2,locs[-1]+2],[100,100] , linestyle=":", color="black", label="Baseline")

legend_elements= [plt.Line2D([0], [0], marker='.', color='w', markerfacecolor='orange', label='Private', markersize=12),
                plt.Line2D([0], [0], marker='.', color='w', markerfacecolor='blue', label='Source', markersize=12),
                plt.Line2D([0], [0], linestyle=":", color="black", label="Baseline"),
                plt.scatter([-20], [0], marker='o', color='gray', label='Best Observed',facecolor='none')
                ]

plt.legend(handles=legend_elements)
#plt.title( "Workload 2 ")
plt.xlim(locs[0]-2,locs[-1]+2)
fig.savefig("BasicDataAnalytics-HitImprovement.pdf", bbox_inches="tight")

