import numpy as np
import glob
import matplotlib.pyplot as plt
import math


exp_ = "../*UMB_res/*"
list_of_dir = glob.glob(exp_)
hits_dir = {}
caches = []

exp = []

for i in list_of_dir:
    d = i.split("/")
    d = d[1]
    if d not in exp:
        exp.append(d)
    temp = glob.glob(i+"/Test*.txt")
    t = i.split("/")
    t = t[-1].split("Results_")
    h = t[-1][2:]
    t = t[-1][0]
    if d not in hits_dir.keys():
        hits_dir[d] = {}
    if t not in hits_dir[d].keys():
        hits_dir[d][t] = {}
    if h not in hits_dir[d][t].keys():
        hits_dir[d][t][h] = {}
    clk = []
    for x in temp:
        tt = x.split("Results_")
        tt = tt[1].split("/")
        #print(tt[4])
        if tt[-1] not in clk:
             clk.append(tt[-1])
        node = 0#tt[0]
        client = clk.index(tt[-1])#tt[4]
        if node not in hits_dir[d][t][h].keys():
            hits_dir[d][t][h][node] = {}
        f = open(x, "r")
        data = f.read()
        f.close()
        data = data.split("\n")
        data = np.array(data)
        if client not in hits_dir[d][t][h][node].keys():
            hits_dir[d][t][h][node][client] = {}
        for xx in data:
            stall = 0
            if "base request stalled" in xx:
                sx = xx.split("request stalled")
                st = sx[-1].split(" ")
                stall = int(st[2])
            if "request hits" in xx and "base" not in xx: # and "network" not in xx:
                tx = xx.split("request hits")
                cache_type = tx[0].split(" ")
                hit_amount = tx[-1].split(" ")
                print(hit_amount[2])
                if int(hit_amount[2]) + stall > 0:
                    hits_dir[d][t][h][node][client][cache_type[1]] = math.log2(int(hit_amount[2]) + stall)
                else:
                    hits_dir[d][t][h][node][client][cache_type[1]] = 0
                if cache_type[1] not in caches:
                    caches.append(cache_type[1])

caches = [['privatememory', 'scalable'],  'sharedmemory', 'boundedfilelock', 'network']

print( hits_dir.keys())
to_graph = {}
for ex in hits_dir.keys():
    for i in hits_dir[ex].keys():
        for j in hits_dir[ex][i].keys():
            clientIDs = {}
            for x in hits_dir[ex][i][j].keys():
                for f in hits_dir[ex][i][j][x].keys():
                    clientIDs[float(f)] = hits_dir[ex][i][j][x][f]
            IDs = list(clientIDs.keys())
            IDs = sorted(IDs)
            print(IDs)
            for xx in IDs:
                if i not in to_graph.keys():
                    to_graph[i] = {}
                if xx not in to_graph[i].keys():
                    to_graph[i][xx] = [[],[]]
                temp = []
                cc = []
                for c in caches[0]:
                    if c in clientIDs[xx].keys():
                        temp.append(clientIDs[xx][c]) 
                        cc.append(c)
                for c in caches[1:]:
                    if c in clientIDs[xx].keys():
                        temp.append(clientIDs[xx][c])
                    else:
                        temp.append(0)
                    cc.append(c)
                cc = ["private", "node-wide", "cluster-wide", "data file"]
                #to_graph[i][xx][0].append(temp)
                leg = j.split("_")
                leg_ = ""
                if int(leg[0]) == 1:
                    leg_ = "umb"
                else:
                    leg_ = "lru*"
                if int(leg[1]) == 1:
                    to_graph[i][xx][0].append(temp)
                    #title = ex.split("/")
                    title = ex.split("_")
                    title = title[0]
                    for h in range(1, len(leg)):
                        w = "umb"
                        if "noUMB" in title:
                            w = "lru*"
                        if int(leg[h]) == 1:
                            leg_ += "/"+w
                        else:
                            leg_ += "/XX"
                    to_graph[i][xx][1].append(leg_)
                    to_graph[i][xx].append(cc)


for i in to_graph.keys():
    for j in to_graph[i].keys():
        t = {}
        count = 0
        for x in to_graph[i][j][1]:
            tt = x.split("/")
            if "XX" in x:
                tt = tt[:-1]
            if "w" not in t.keys():
                t["w"] = [{},{}]
            if len(tt) not in t["w"][1].keys():
                t["w"][1][len(tt)] = []
            if len(tt) not in t["w"][0].keys():
                t["w"][0][len(tt)] = []
            if x not in t["w"][1][len(tt)]:
                t["w"][1][len(tt)].append(x)
                t["w"][0][len(tt)].append(to_graph[i][j][0][count])
            count += 1
        e = [[],[]] 
        for x in t.keys():
            for x2 in t[x][0].keys():
                for w in t[x][0][x2]:
                    e[0].append(w)
                for w in t[x][1][x2]:
                    e[1].append(w)   
        e.append(to_graph[i][j][2])
        to_graph[i][j] = e

for i in to_graph.keys():
    for j in to_graph[i].keys():
        X = np.arange(len(to_graph[i][j][0][0]))
        #fig = plt.figure()
        #ax = fig.add_axes([0,0,1,1])
        #print(j)
        plt.subplots(figsize =(12, 8))
        for z in range(len(to_graph[i][j][0])):
            plt.bar(X + z*0.1, to_graph[i][j][0][z], width = 0.1)
            #print(to_graph[i][j][0][z])
        plt.ylabel('Number of hits')
        plt.xticks(np.arange(0,len(to_graph[i][j][2]), 1), to_graph[i][j][2])
        plt.title('Hits for client num' + str(int(j)) + " in test "+ str(i))
        plt.legend(to_graph[i][j][1])
        plt.tight_layout()
        plt.savefig(str(i)+"_"+str(int(j))+'.png')
#print(to_graph)
print("-------------------------------")

avg_hits  = {}
for i in to_graph.keys():
    if i not in avg_hits.keys():
        avg_hits[i] = {} 
    for j in to_graph[i].keys():
        #if to_graph[i][j][1][0] not in avg_hits[i].keys():
        #    avg_hits[i][to_graph[i][j][1][0]] = to_graph[i][j][0][0]
        for y in range(0,len(to_graph[i][j][0])):
            count = 0
            if to_graph[i][j][1][y] not in avg_hits[i].keys():   
                avg_hits[i][to_graph[i][j][1][y]] = to_graph[i][j][0][y]         
            for x in to_graph[i][j][0][y]:  
                avg_hits[i][to_graph[i][j][1][y]][count] += x #hits_dir[i][j][x][z][f]
                count += 1

#for i in avg_hits.keys():
#    for j in avg_hits[i].keys():
#        avg_hits[i][j] = avg_hits[i][j][0] #/avg_hits[i][j][x][1]

print(avg_hits)

cc = ["private", "node-wide", "cluster-wide", "data file"]
for i in avg_hits.keys():
    X = np.arange(len(cc))
    plt.subplots(figsize =(12, 8))
    z = 0
    for j in avg_hits[i].keys():
        plt.bar(X + z*0.1, avg_hits[i][j], width = 0.1, label=j)
        z += 1
    plt.ylabel('Number of hits')
    plt.xticks(np.arange(0,len(cc), 1), cc)
    plt.title("Hits over all clients for workload "+ str(i))
    plt.legend()
    plt.tight_layout()
    plt.savefig("SUM_"+str(i)+'.png')
    #print("===========================")

