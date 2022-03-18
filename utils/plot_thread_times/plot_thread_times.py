import numpy as np
import scipy.stats as stats
import matplotlib.pyplot as plt
import re
import sys
import matplotlib
import os
import argparse

matplotlib.rcParams.update({'font.size': 22})  

"""StartTime,N,ExpName,TaskType,Host,IOType,Slot,StartSetup,InputDataSet,StartInputTx,StopInputTx,StartExp,
StopExp,sys_input_time,sys_input_accesses,sys_input_amount,sys_output_time,sys_output_accesses,sys_output_amount,
local_input_time,local_input_accesses,local_input_amount,local_output_time,local_output_accesses,local_output_amount,
tazer_input_time,tazer_input_accesses,tazer_input_amount,tazer_output_time,tazer_output_accesses,tazer_output_amount,
cache_accesses,cache_time,cache_amount,base_cache_ovh,
privatememory_read_time,privatememorycache_hits,privatememorycache_hit_time,privatememorycache_hit_amount,
privatememorycache_stalls_1,privatememorycache_stall_time_1,privatememorycache_stalls_2,privatememorycache_stall_time_2,
privatememorycache_stall_amount,privatememorycache_misses,privatememorycache_miss_time,
privatememorycache_prefetches,privatememorycache_lock_time,privatememorycache_ovh_time,
sharedmemory_read_time,sharedmemorycache_hits,sharedmemorycache_hit_time,sharedmemorycache_hit_amount,
sharedmemorycache_stalls_1,sharedmemorycache_stall_time_1,sharedmemorycache_stalls_2,sharedmemorycache_stall_time_2,
sharedmemorycache_stall_amount,sharedmemorycache_misses,sharedmemorycache_miss_time,
sharedmemorycache_prefetches,sharedmemorycache_lock_time,sharedmemorycache_ovh_time,
boundedfilelock_read_time,boundedfilelock_hits,boundedfilelock_hit_time,boundedfilelock_hit_amount,
boundedfilelock_stalls_1,boundedfilelock_stall_time_1,boundedfilelock_stalls_2,boundedfilelock_stall_time_2,
boundedfilelock_stall_amount,boundedfilelock_misses,boundedfilelock_miss_time,
boundedfilelock_prefetches,boundedfilelock_lock_time,boundedfilelock_ovh_time,
network_accesses,network_time,network_total_amount,network_used_amount,network_ovh_time"""

"""
StartTime,N,ExpName,TaskType,Host,IOType,Slot,StartSetup,InputDataSet,StartInputTx,StopInputTx,StartExp,StopExp,
sys_input_time,sys_input_accesses,sys_input_amount,sys_output_time,sys_output_accesses,sys_output_amount,sys_destruction_time,
local_input_time,local_input_accesses,local_input_amount,local_output_time,local_output_accesses,local_output_amount,local_destruction_time,
tazer_input_time,tazer_input_accesses,tazer_input_amount,tazer_output_time,tazer_output_accesses,tazer_output_amount,tazer_destruction_time,
cache_accesses,cache_time,cache_amount,base_cache_ovh,base_destruction,
base_request_hits,base_request_hit_time,base_request_hit_amount,base_request_misses,base_request_miss_time,base_request_prefetches,
base_request_stalls,base_request_stall_time,base_request_ovh_time,base_request_read_time,base_request_read_amt,base_request_destruction_time,
privatememory_request_hits,privatememory_request_hit_time,privatememory_request_hit_amount,privatememory_request_misses,privatememory_request_miss_time,privatememory_request_prefetches,
privatememory_request_stalls,privatememory_request_stall_time,privatememory_request_ovh_time,privatememory_request_read_time,privatememory_request_read_amt,privatememory_request_destruction_time,
sharedmemory_request_hits,sharedmemory_request_hit_time,sharedmemory_request_hit_amount,sharedmemory_request_misses,sharedmemory_request_miss_time,sharedmemory_request_prefetches,
sharedmemory_request_stalls,sharedmemory_request_stall_time,sharedmemory_request_ovh_time,sharedmemory_request_read_time,sharedmemory_request_read_amt,sharedmemory_request_destruction_time,
boundedfilelock_request_hits,boundedfilelock_request_hit_time,boundedfilelock_request_hit_amount,boundedfilelock_request_misses,boundedfilelock_request_miss_time,boundedfilelock_request_prefetches,
boundedfilelock_request_stalls,boundedfilelock_request_stall_time,boundedfilelock_request_ovh_time,boundedfilelock_request_read_time,boundedfilelock_request_read_amt,boundedfilelock_request_destruction_time,
network_request_hits,network_request_hit_time,network_request_hit_amount,network_request_misses,network_request_miss_time,network_request_prefetches,
network_request_stalls,network_request_stall_time,network_request_ovh_time,network_request_read_time,network_request_read_amt,network_request_destruction_time
"""
 
def updateMachine(jobid,num_threads,machines,slot,vals,labels,host,exp_name,server):    
    if host not in machines:
        machines[host]={}
    if slot not in machines[host]:
        machines[host][slot] = {}

    for t in range(1,num_threads+1):
        thread = str(t)
        if thread not in machines[host][slot]:
            machines[host][slot][thread]={"total":[],"jobids":[],"tazer_in":[],"tazer_out":[],"tazer_destruct":[],"local_in":[],"local_out":[],"sys_in":[],"sys_out":[],"setup":[],
                                "net_in":[],"net_out":[],"net_stall":[],"net_ovh":[],"net_destruct":[],"net_construct":[],
                                "disk_in":[],"disk_out":[],"disk_stall":[],"disk_ovh":[],"disk_destruct":[],"disk_construct":[],
                                "bb_in":[],"bb_stall":[],"bb_ovh":[],"bb_destruct":[],"bb_construct":[],
                                "mem_in":[],"mem_stall":[],"mem_ovh":[],"mem_destruct":[],"mem_construct":[],
                                "io_total":[],"exp_time":[],"start_time":[],"cpu_time":[],"tazer_amt":[],"network_amt":[],"bw":[],"ovh":[],"stall":[]}   
            
        machines[host][slot][thread]["total"].append(float(vals[labels.index("FinishedTime")])-float(vals[labels.index("StartTime")]))
        machines[host][slot][thread]["jobids"].append(jobid)
        machines[host][slot][thread]["tazer_in"].append(float(vals[labels.index("tazer_input_time_"+thread)]))
        machines[host][slot][thread]["tazer_out"].append(float(vals[labels.index("tazer_output_time_"+thread)]))
        machines[host][slot][thread]["tazer_destruct"].append(float(vals[labels.index("tazer_destruction_time_"+thread)]))
        machines[host][slot][thread]["local_in"].append(float(vals[labels.index("local_input_time_"+thread)]))
        machines[host][slot][thread]["local_out"].append(float(vals[labels.index("local_output_time_"+thread)]))
        machines[host][slot][thread]["sys_in"].append(float(vals[labels.index("sys_input_time_"+thread)]))
        machines[host][slot][thread]["sys_out"].append(float(vals[labels.index("sys_output_time_"+thread)]))
        machines[host][slot][thread]["setup"].append((float(vals[labels.index("StartInputTx")])-float(vals[labels.index("StartTime")]))+(float(vals[labels.index("FinishedTime")])-float(vals[labels.index("StopExp")])))
        machines[host][slot][thread]["tazer_amt"].append(float(vals[labels.index("tazer_input_amount_"+thread)]))
        machines[host][slot][thread]["network_amt"].append(float(vals[labels.index("network_request_read_amt_"+thread)]))
        if machines[host][slot][thread]["tazer_in"][-1] > 0: #if not probably we are plotting an "optimal" experiment
            machines[host][slot][thread]["bw"].append(machines[host][slot][thread]["tazer_amt"][-1]/machines[host][slot][thread]["tazer_in"][-1])
        else:
            machines[host][slot][thread]["bw"].append(0)
        machines[host][slot][thread]["net_in"].append(float(vals[labels.index("network_request_hit_time_"+thread)]))
        machines[host][slot][thread]["net_ovh"].append(float(vals[labels.index("network_request_ovh_time_"+thread)]))
        machines[host][slot][thread]["net_construct"].append(float(vals[labels.index("network_request_construction_time_"+thread)]))
        machines[host][slot][thread]["net_destruct"].append(float(vals[labels.index("network_request_destruction_time_"+thread)])+float(vals[labels.index("tazer_destruction_time_"+thread)])) #tazer distruction time is mostly closeing the connections....
        machines[host][slot][thread]["net_stall"].append(float(vals[labels.index("network_request_stall_time_"+thread)]))
        machines[host][slot][thread]["net_out"].append(float(vals[labels.index("tazer_output_time_"+thread)]) + float(vals[labels.index("servefile_send_time_"+thread)]))

        machines[host][slot][thread]["mem_in"].append(float(vals[labels.index("privatememory_request_hit_time_"+thread)])+float(vals[labels.index("sharedmemory_request_hit_time_"+thread)]))
        machines[host][slot][thread]["mem_ovh"].append(float(vals[labels.index("privatememory_request_ovh_time_"+thread)])+float(vals[labels.index("sharedmemory_request_ovh_time_"+thread)]))
        machines[host][slot][thread]["mem_construct"].append(float(vals[labels.index("privatememory_request_construction_time_"+thread)])+float(vals[labels.index("sharedmemory_request_construction_time_"+thread)]))
        machines[host][slot][thread]["mem_destruct"].append(float(vals[labels.index("privatememory_request_destruction_time_"+thread)])+float(vals[labels.index("sharedmemory_request_destruction_time_"+thread)]))
        machines[host][slot][thread]["mem_stall"].append(float(vals[labels.index("privatememory_request_stall_time_"+thread)])+float(vals[labels.index("sharedmemory_request_stall_time_"+thread)]))

        machines[host][slot][thread]["disk_in"].append(float(vals[labels.index("local_input_time_"+thread)])+float(vals[labels.index("boundedfilelock_request_hit_time_"+thread)])+float(vals[labels.index("localfilecache_request_hit_time_"+thread)]))
        machines[host][slot][thread]["disk_ovh"].append(float(vals[labels.index("boundedfilelock_request_ovh_time_"+thread)])+float(vals[labels.index("localfilecache_request_ovh_time_"+thread)]))
        machines[host][slot][thread]["disk_construct"].append(float(vals[labels.index("boundedfilelock_request_construction_time_"+thread)])+float(vals[labels.index("localfilecache_request_construction_time_"+thread)]))
        machines[host][slot][thread]["disk_destruct"].append(float(vals[labels.index("boundedfilelock_request_destruction_time_"+thread)])+float(vals[labels.index("localfilecache_request_destruction_time_"+thread)]))
        machines[host][slot][thread]["disk_stall"].append(float(vals[labels.index("boundedfilelock_request_stall_time_"+thread)])+float(vals[labels.index("localfilecache_request_stall_time_"+thread)]))
        machines[host][slot][thread]["disk_out"].append(float(vals[labels.index("local_output_time_"+thread)]))


        machines[host][slot][thread]["bb_in"].append(float(vals[labels.index("burstbuffer_request_hit_time_"+thread)]))
        machines[host][slot][thread]["bb_ovh"].append(float(vals[labels.index("burstbuffer_request_ovh_time_"+thread)]))
        machines[host][slot][thread]["bb_construct"].append(float(vals[labels.index("burstbuffer_request_construction_time_"+thread)]))
        machines[host][slot][thread]["bb_destruct"].append(float(vals[labels.index("burstbuffer_request_destruction_time_"+thread)]))
        machines[host][slot][thread]["bb_stall"].append(float(vals[labels.index("burstbuffer_request_stall_time_"+thread)]))


        machines[host][slot][thread]["ovh"].append(float(vals[labels.index("base_request_ovh_time_"+thread)]))
        machines[host][slot][thread]["exp_time"].append(float(vals[labels.index("StopExp")])-float(vals[labels.index("StartExp")]))
        machines[host][slot][thread]["start_time"].append(float(vals[labels.index("StartTime")]))

        # dest=(machines[host][slot][thread]["net_destruct"][-1]+machines[host][slot][thread]["bb_destruct"][-1]+machines[host][slot][thread]["disk_destruct"][-1]+machines[host][slot][thread]["mem_destruct"][-1])
        # machines[host][slot][thread]["net_destruct"][-1]=float(vals[labels.index("tazer_destruction_time")])-dest

        destruct_time=machines[host][slot][thread]["mem_destruct"][-1]+machines[host][slot][thread]["disk_destruct"][-1]+machines[host][slot][thread]["bb_destruct"][-1]+machines[host][slot][thread]["net_destruct"][-1]
        constuct_time=machines[host][slot][thread]["mem_construct"][-1]+machines[host][slot][thread]["disk_construct"][-1]+machines[host][slot][thread]["bb_construct"][-1]+machines[host][slot][thread]["net_construct"][-1]

        if machines[host][slot][thread]["tazer_in"][-1] > 0:
            machines[host][slot][thread]["tazer_in"][-1] -= constuct_time

        temp_net = (machines[host][slot][thread]["net_in"][-1]+machines[host][slot][thread]["net_ovh"][-1]+ machines[host][slot][thread]["net_destruct"][-1]+machines[host][slot][thread]["net_construct"][-1] + machines[host][slot][thread]["net_stall"][-1]) + machines[host][slot][thread]["net_out"][-1]
        temp_mem = (machines[host][slot][thread]["mem_in"][-1]+machines[host][slot][thread]["mem_ovh"][-1]+ machines[host][slot][thread]["mem_destruct"][-1]+machines[host][slot][thread]["mem_construct"][-1]+ machines[host][slot][thread]["mem_stall"][-1])
        temp_disk = (machines[host][slot][thread]["disk_in"][-1]+machines[host][slot][thread]["disk_ovh"][-1] + machines[host][slot][thread]["disk_destruct"][-1]+machines[host][slot][thread]["disk_construct"][-1]+ machines[host][slot][thread]["disk_stall"][-1]) + machines[host][slot][thread]["disk_out"][-1]
        temp_bb = (machines[host][slot][thread]["bb_in"][-1]+machines[host][slot][thread]["bb_ovh"][-1]+ machines[host][slot][thread]["bb_destruct"][-1]+machines[host][slot][thread]["bb_construct"][-1]+ machines[host][slot][thread]["bb_stall"][-1])

        if server:
            total_io = temp_net + temp_mem + temp_disk+ temp_bb
        else:
            total_io = (machines[host][slot][thread]["tazer_in"][-1]+machines[host][slot][thread]["tazer_out"][-1]+destruct_time + constuct_time
                    +machines[host][slot][thread]["local_in"][-1]+machines[host][slot][thread]["local_out"][-1]
                    +machines[host][slot][thread]["sys_in"][-1]+machines[host][slot][thread]["sys_out"][-1])
        machines[host][slot][thread]["io_total"].append(total_io)
        tx_io = float(vals[labels.index("StopInputTx")])-float(vals[labels.index("StartInputTx")])
        cpu = machines[host][slot][thread]["exp_time"][-1]-(total_io-tx_io)
        machines[host][slot][thread]["cpu_time"].append(cpu)

        extra = (machines[host][slot][thread]["total"][-1]-machines[host][slot][thread]["setup"][-1])-machines[host][slot][thread]["exp_time"][-1] #extra time most likely due to loading libraries which we cant explicitly capture...
        #machines[host][slot][thread]["sys_in"][-1]+=extra

        

        if total_io - (temp_net+temp_mem+temp_disk+temp_bb) > 1:
            print(jobid,total_io - (temp_net+temp_mem+temp_disk+temp_bb), total_io,(machines[host][slot][thread]["tazer_in"][-1]+machines[host][slot][thread]["tazer_out"][-1]),
            (machines[host][slot][thread]["local_in"][-1]+machines[host][slot][thread]["local_out"][-1]),
            (machines[host][slot][thread]["sys_in"][-1]+machines[host][slot][thread]["sys_out"][-1]),
            temp_net,temp_mem,temp_disk,temp_bb,temp_net+temp_mem+temp_disk+temp_bb,machines[host][slot][thread]["ovh"][-1])

        global global_cnt
        global_cnt+=1
        print(machines[host][slot][thread])

    
    
def plotMachine(machines,s_i,mint):
    names=[]
    totals=[]
    stimes=[]
    times=[]
    t_ins=[]
    t_outs=[]
    l_ins=[]
    l_outs=[]
    s_ins=[]
    s_outs=[]
    s_ups=[]
    io_tots=[]
    mem_ins=[]
    bb_ins=[]
    cpus=[]
    ovh=[]
    exps=[]
    tots=[]
    t_amt=[]
    n_amt=[]
    bws=[]

    cnts=[]
    jobids=[]


    for mach in sorted(machines):
        for slot in sorted(machines[mach]):
            for thread in sorted(machines[mach][slot]):
                cnts.append(len(machines[mach][slot][thread]["total"]))
                totals.append((sum(machines[mach][slot][thread]["total"])))
                stimes.append(0*(min(machines[mach][slot][thread]["start_time"])-mint))
                t_ins.append(((sum(machines[mach][slot][thread]["net_in"])+sum(machines[mach][slot][thread]["net_ovh"])+sum(machines[mach][slot][thread]["net_destruct"])+sum(machines[mach][slot][thread]["net_construct"])+sum(machines[mach][slot][thread]["net_stall"]))))
                t_outs.append((sum(machines[mach][slot][thread]["net_out"])))#/float(len(machines[mach])))
                l_ins.append(((sum(machines[mach][slot][thread]["disk_in"])+sum(machines[mach][slot][thread]["disk_ovh"])+sum(machines[mach][slot][thread]["disk_destruct"])+sum(machines[mach][slot][thread]["disk_construct"])+sum(machines[mach][slot][thread]["disk_stall"]))))#/float(len(machines[mach])))
                l_outs.append((sum(machines[mach][slot][thread]["disk_out"])))#/float(len(machines[mach])))
                s_ins.append((sum(machines[mach][slot][thread]["sys_in"])))#/float(len(machines[mach])))
                s_outs.append((sum(machines[mach][slot][thread]["sys_out"])))#/float(len(machines[mach])))
                mem_ins.append(((sum(machines[mach][slot][thread]["mem_in"])+sum(machines[mach][slot][thread]["mem_ovh"])+sum(machines[mach][slot][thread]["mem_destruct"])+sum(machines[mach][slot][thread]["mem_construct"])+sum(machines[mach][slot][thread]["mem_stall"]))))
                bb_ins.append(((sum(machines[mach][slot][thread]["bb_in"])+sum(machines[mach][slot][thread]["bb_ovh"])+sum(machines[mach][slot][thread]["bb_destruct"])+sum(machines[mach][slot][thread]["bb_construct"])+sum(machines[mach][slot][thread]["bb_stall"]))))

                s_ups.append((sum(machines[mach][slot][thread]["setup"])))#/float(len(machines[mach])))
                cpus.append((sum(machines[mach][slot][thread]["cpu_time"])))
                t_amt.append((sum(machines[mach][slot][thread]["tazer_amt"])))
                n_amt.append((sum(machines[mach][slot][thread]["network_amt"])))
                ovh.append((sum(machines[mach][slot][thread]["ovh"])))
                times.append(t_ins[-1]+t_outs[-1]+l_ins[-1]+l_outs[-1]+s_ins[-1]+s_outs[-1]+s_ups[-1]+cpus[-1]+ovh[-1])
                io_tots.append(sum(machines[mach][slot][thread]["io_total"]))
                exps += machines[mach][slot][thread]["cpu_time"]
                tots += machines[mach][slot][thread]["total"]
                bws+=machines[mach][slot][thread]["bw"]
                names.append(str(mach)+"_"+str(thread))
                jobids.append(machines[mach][slot][thread]["jobids"])
                if len(machines[mach][slot][thread]["total"]) == 4:
                    print(mach,slot,thread,machines[mach][slot][thread]["jobids"])
        

    ssind=np.argsort(np.array(totals))
    snames = np.array(names)[ssind]
    sjobids= np.array(jobids)[ssind]

    print(np.sum(tots),np.sum(io_tots),np.sum(exps),np.sum(cpus),"bws: ",np.mean(bws),np.max(bws),np.min(bws))
    print(tots,io_tots,exps,cpus)

    global tot_sum
    tot_sum+=np.sum(tots)
    global io_sum
    io_sum+=np.sum(io_tots)
    global nio_sum
    nio_sum+=np.sum(t_ins)
    global cpu_sum
    cpu_sum+=np.sum(exps)
    global t_amt_sum
    t_amt_sum = np.sum(t_amt)
    global n_amt_sum
    n_amt_sum = np.sum(n_amt)

    
    sind = ssind
    print (np.array(cnts)[sind])
    ind=range(s_i,s_i+len(sind))
       
    bars = [(np.array(times),"b","CPU"),
            (np.array(t_ins),"r","network read (data)"),
            (np.array(t_outs),"m","network write (data)"),
            (np.array(l_ins),"y","disk read (data)"),
            (np.array(l_outs),"xkcd:camo green","disk write (data)"),
            (np.array(s_ins),"xkcd:sky","disk read (config)"),
            (np.array(s_outs),"xkcd:bright purple","disk write (config)"),
            (np.array(mem_ins),"c","mem read (data)"),
            (np.array(bb_ins),"xkcd:ocean blue","burst buf read(data)"),
            (np.array(cpus),"b","CPU"),
            (np.array(s_ups),"xkcd:dark purple","setup"),
            (np.array(ovh),"k","ovh")]

    ec="none"

    #bmutlu
    barsums = [(np.sum(times),"b","CPU"),
            (np.sum(t_ins),"r","network read (data)"),
            (np.sum(t_outs),"m","network write (data)"),
            (np.sum(l_ins),"y","disk read (data)"),
            (np.sum(l_outs),"xkcd:camo green","disk write (data)"),
            (np.sum(s_ins),"xkcd:sky","disk read (config)"),
            (np.sum(s_outs),"xkcd:bright purple","disk write (config)"),
            (np.sum(mem_ins),"c","mem read (data)"),
            (np.sum(bb_ins),"xkcd:ocean blue","burst buf read(data)"),
            (np.sum(cpus),"b","CPU"),
            (np.sum(s_ups),"xkcd:dark purple","setup"),
            (np.sum(ovh),"k","ovh")]

    sumvals = [i[0] for i in barsums]
    exp_dir = os.path.basename(os.getcwd())
    # exp_dir = "125MBs_io_8_tpf"
    # 125MBs_io_8_tpf
    io=exp_dir.split("MBs")[0]
    tpf=exp_dir.split("_")[2]
    startline=io+","+tpf
    sumvals.insert(0,exp_dir)

    with open("../all_exp_results.txt","a") as outfile:
        outfile.write(startline)
        for element in sumvals:
            outfile.write(",")
            outfile.write(str(element))
        outfile.write("\n")
    
    #end bmutlu
    
    

    
    
    if s_i == 0:
        # plt.bar(ind,np.array(totals)[sind],color="w",edgecolor="k",width=1,alpha=0.5)
        bottoms = np.array(stimes)[sind]
        print(bars)
        for i in range(1,len(bars)):
            #print("i="+str(i))
            #print(bars[i][0][3])
            bottoms+=bars[i][0][sind] 
        print (np.where(bottoms > np.array(totals)[sind]))
        print (sjobids[np.where(bottoms > np.array(totals)[sind])])
        #print (bottoms[3])
        # print(sjobids[136],sjobids[137])
        # print(sjobids[178],sjobids[179],sjobids[180])

        for i in reversed(range(1,len(bars))):
            bottoms-=bars[i][0][sind]
            plt.bar(ind,bars[i][0][sind],bottom=bottoms,color=bars[i][1],edgecolor=ec,width=1,label=bars[i][2])
        plt.bar(ind,np.array(totals)[sind],color="w",edgecolor="k",width=1,alpha=0.5)
    else: 
        plt.bar(ind,bars[0][0][sind],bottom=np.array(stimes)[sind],color=bars[0][1],edgecolor=ec,width=1)
        bottoms = np.array(stimes)[sind]
        for i in range(1,len(bars)):
            plt.bar(ind,bars[i][0][sind],bottom=bottoms,color=bars[i][1],edgecolor=ec,width=1)
            bottoms+=bars[i][0][sind]
    return  s_i+len(sind)+3

tot_sum=0
io_sum=0  
nio_sum=0
cpu_sum=0
t_amt_sum=0
n_amt_sum=0
def plotData(path,title,fname,leg_loc,server): 
    global tot_sum
    tot_sum=0
    global io_sum
    io_sum=0
    global nio_sum
    nio_sum=0
    global cpu_sum
    cpu_sum = 0
    global t_amt_sum
    t_amt_sum = 0
    global n_amt_sum
    n_amt_sum = 0
    data={}
    
    with open(path) as rdata:
        for l in rdata:
            temp=l.split(";")
            if temp[0] not in data:
                data[temp[0]]=[[],[]]
            names=[x.strip().strip("\"")  for x in temp[1].split(",")]
            vals=[x.strip().strip("\"")  for x in temp[2].split(",")]
            data[temp[0]][0]+=names
            data[temp[0]][1]+=vals

    machines={}
    ioCnt=0
    nioCnt=0
    maxt=0
    mint=99999999999999999
    temp=0

    accesses=[]
    for jobid in data:
        labels=data[jobid][0]
        vals=data[jobid][1]
        num_threads=int(vals[labels.index("threads")])
        print("threads="+str(num_threads))
        host=vals[labels.index("Host")]
        slot=int(vals[labels.index("Slot")])
        
        if mint > int(vals[labels.index("StartTime")]):
            mint = int(vals[labels.index("StartTime")])
        if maxt < int(vals[labels.index("FinishedTime")]):
            maxt = int(vals[labels.index("FinishedTime")])
        if temp < int(vals[labels.index("FinishedTime")])-int(vals[labels.index("StartTime")]):
            temp = int(vals[labels.index("FinishedTime")])-int(vals[labels.index("StartTime")])
        updateMachine(jobid,num_threads,machines,slot,vals,labels,host,title,server)
   

    # print (len(bluesky),len(ivy),len(amd),len(haswell_1),len(haswell_2),len(seapearl))
    print (len(machines))
    cores=[]
    # hosts =  [bluesky,ivy,amd,haswell_1,haswell_2,seapearl]
    # for mach in machines:
    #     cores.append(0);
    for node in machines:
        cores.append(0)
        print(node)
        cores[-1] += len(machines[node])
    print(cores,sum(cores))
    s_i=0
    #fig=plt.figure()
    fig=plt.figure(figsize=(17,7))
    # for mach in hosts:
    #     if len(mach) > 0:
    s_i=plotMachine(machines,s_i,mint)
            
    if io_sum == 0:
        print("io_sum = 0")
    else:
        print ("tot:",tot_sum,"io:",io_sum,"cpu:",cpu_sum,"tazer amt:",t_amt_sum/1000000.0,n_amt_sum/1000000.0, "BW: ",(t_amt_sum/io_sum)/1000000.0,(n_amt_sum/io_sum)/1000000.0)
    print(title)
    print()
    ax =plt.gca()

    # props = dict(boxstyle='round', facecolor='white', alpha=0.75)
    # ax.text(0.28, 0.98, "Node Type 1\n(Cluster A)", transform=ax.transAxes,
    #     verticalalignment='top', bbox=props)
    # ax.text(0.71, 0.98, "Node Type 2\n(Cluster B)", transform=ax.transAxes,
    #     verticalalignment='top', bbox=props)
    # ax.text(0.902, 0.98, "3", transform=ax.transAxes,
    #     verticalalignment='top', bbox=props)
    # ax.text(0.926, 0.98, "4", transform=ax.transAxes,
    #     verticalalignment='top', bbox=props)
    # ax.text(0.951, 0.98, "5", transform=ax.transAxes,
    #     verticalalignment='top', bbox=props)

    # if "Unbounded" in title or "Local" in title:
    #     plt.ylim((0,100))

    plt.xticks([])
    # plt.grid(True)
    plt.legend(loc=leg_loc,prop={'size': 12})
    #plt.title(title)
    #plt.ylabel("Exec Time (Minutes)")
    plt.ylabel("Exec Time (Seconds)")
    plt.xlabel("Threads")
    plt.tight_layout()
    fig.savefig(fname+".pdf")
    plt.show()
    return tot_sum,io_sum,cpu_sum
    
if __name__ == "__main__":
    global_cnt = 0
    res=[]
    dpath = "./logs/data.txt"
    output_path = "thread_times"

    parser = argparse.ArgumentParser()
    parser.add_argument("-s", "--server", help="count all threads, use for parsing server output", action='store_true', default=False)
    parser.add_argument("-i", "--input", help="input file to be parsed", type=str, default="./logs/data.txt")
    parser.add_argument("-o", "--output", help="name of outputfile",  type=str, default= "thread_times")
    args = parser.parse_args(sys.argv[1:])

    
    dpath = args.input
    output_path = args.output

    res.append(plotData(dpath,"tazer",output_path,"best",args.server))
    print ("globalcnt:",global_cnt)

    print (res)


