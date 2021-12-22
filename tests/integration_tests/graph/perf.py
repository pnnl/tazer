import glob
import numpy as np

list_of_dir = glob.glob("../backup/*")
temp = []
for i in list_of_dir:
    if "speed" in i:# or "lru_long" in i: #"speed" in i or "lru" in i:
        client_ID = glob.glob(i+"/*")
        for j in client_ID:
            int_file = glob.glob(j+"/*")
            for x in int_file:
                if "block_stats.txt" in x or "access_new.txt" in x:
                    temp.append(x)

#print(temp) 
for i in temp:
    print (i)
    perf = 0
    if "access_new.txt" in i:
        f = open(i, "r")
        data = f.read()
        f.close()
        data = data.split("\n")
        data = data[:-1]
        tt = []
        perf = []
        for j in range(len(data)):
            tt.append(data[j].split(" "))
            perf.append(float(tt[-1][-1]))
        data = np.array(tt)  
        perf_ = sum(perf)/len(perf)
        print(perf_)
        print(len(perf))       
    
    miss = 0
    if "block_stats.txt" in i:
        f = open(i, "r")
        data2 = f.read()
        f.close()
        data2 = data2.split("\n")
        data2 = data2[:-1]
        tt = []
        for j in range(len(data2)):
            tt_temp = data2[j].split(" ")
            new_arr = []
            for x in tt_temp:
                if len(x) > 0:
                    new_arr.append(x)
            tt.append(new_arr)
            
            if "[BLOCK_EVICTED]" in new_arr[2]:#if "[BLOCK_READ_MISS_CLIENT]" in new_arr[2]:
                miss += 1 
        data2 = np.array(tt)
        print(miss)
        print(len(data2))
