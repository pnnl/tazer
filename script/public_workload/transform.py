import glob
import numpy as np
import sys

#exp_num = 216
exp_num = sys.argv[1]

files = "data/logs-experiment.pattern.matching.4gb.to-"+str(exp_num)+"/*/*"

list_of_files = glob.glob(files)
data_dict = {}
fileIDs_dict = {}
fileIDs_act_dict = {}
offset = {}
count = 0
for i in list_of_files:
    file_temp = open(i, "r")
    temp = file_temp.read()
    file_temp.close()
    temp = temp.split("\n")
    temp = temp[:-1]
    t = {}
    for j in temp:
        tt = j.split("\t")
        out = []
        if tt[1] not in fileIDs_dict.keys():
            fileIDs_dict[tt[1]] = "tazer"+str(count)
            count += 1
        if tt[2] in "R":
            out.append(fileIDs_dict[tt[1]]+"_output.dat")
        else:
            out.append(fileIDs_dict[tt[1]]+".dat")
        if out[0] not in fileIDs_act_dict.keys():
            fileIDs_act_dict[out[0]] = tt[1]
        out.append(tt[-2])
        if out[0] not in offset.keys():
            offset[out[0]] = []
        offset[out[0]].append(float(tt[-2]))
        out.append(tt[-1])
        #t.append(out) 
        if tt[0] not in data_dict.keys():
            data_dict[tt[0]] = []
        data_dict[tt[0]].append(out)


y = []
times = list(data_dict.keys())
times = sorted(times)
times = times[::-1]
for i in times:
    y.append(data_dict[i][0])

y = np.array(y)

ss_out = ""
for i in y:
    ss = str(i[0])
    for j in i[1:]:
        ss += " " + str(j)
    ss_out += ss 
    ss_out += "\n"

file_out = open("Test3_exp"+str(exp_num)+".txt", "w")
file_out.write(ss_out)
file_out.close()



param_file = open("data/output/experiment.pattern.matching.4gb.to-"+str(exp_num)+".out", "r")
temp_param = param_file.read()
param_file.close()
temp_param = temp_param.split("\n")
temp_param = temp_param[:-1]
f = {}
keys = []
for i in temp_param:
    if "Completed MPI-IO" in i:
        temp = i.split(" ")
        temp = temp[-1][:-1]
        if temp not in f.keys():
            keys.append(temp)
            f[temp] = []

count = -1
for i in temp_param:
    if "Effective" in i:
        count += 1
    if "Bandwidth" in i:
        temp = i[4:-1].split(":")
        temp[1] = temp[1][1:].split(" ")
        e = temp[0].split("(")
        temp[0] = e[0][:-1]
        temp[1] = temp[1][2] +" "+ temp[1][-1]
        f[keys[count]].append(temp)

ss_out2 = "output label = write action\nno output label = read action\nTranslated ID: File hash: Max offset\n"
for i in fileIDs_act_dict.keys():
    ss_out2 += i +": "+ fileIDs_act_dict[i] +": " +str(max(offset[i]))+ "\n"

ss_out2 += "Bandwidth:\n"
for i in f.keys():
    for j in f[i]:
        ss_out2 += i + " : " + j[0] + " : " +j[1]+ "\n"

file_out = open("FileIDs_exp"+str(exp_num)+".txt", "w")
file_out.write(ss_out2)
file_out.close()
