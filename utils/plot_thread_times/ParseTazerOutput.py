import sys
import re
import argparse

def getThreads(data, count_cache_threads):
    pattern = re.compile("^\[TAZER\].* thread [0-9]+")
    id_pattern = re.compile("[0-9]+")
    cache_names = ["base", "privatememory", "sharedmemory", "burstbuffer", "network", "localfilecache", "boundedfilelock"]
    is_cache = False
    thread_count = 0
    threads = {}
    threads = set()

    if count_cache_threads:
        for line in data:
            if pattern.match(line):
                threads.add(id_pattern.search(line).group(0))          
    else:
        for line in data:
            if pattern.match(line):
                #print(line)
                for cache in cache_names:
                    if cache in line:
                        is_cache = True

                if is_cache == False:    
                    threads.add(id_pattern.search(line).group(0))
            is_cache = False
    #print("threads: "+str(threads))
    #print(len(threads))
    return threads



def getVals(t, data, thread, aggregated):
    input_time = 0.0
    input_accesses = 0.0
    input_amount = 0.0
    output_time = 0.0
    output_accesses = 0.0
    output_amount = 0.0
    destruction_time = 0.0
    in_thread = False
    pattern = re.compile("^\[TAZER\].* thread [0-9]+")
    #print("parsing thread "+str(thread))
    if aggregated:
        for line in data:
            if pattern.match(line):
                in_thread = True
            elif len(line.strip()) == 0:
                in_thread = False

            #if not in_thread:
                #print(line)

            if "[TAZER] "+t in line:
                if not in_thread:
                    #print(line)
                    vals = line.split(" ")
                    if vals[2] in ["open", "access", "stat", "seek", "in_open", "in_close", "in_fopen", "in_fclose"]:
                        input_time += float(vals[3])
                    elif vals[2] in ["read", "fread"]:
                        input_time += float(vals[3])
                        input_accesses += float(vals[4])
                        input_amount += float(vals[5])
                    elif vals[2] in ["close", "fsync", "out_open", "out_close"]:
                        output_time += float(vals[3])
                    elif vals[2] in ["write", "fwrite"]:
                        output_time += float(vals[3])
                        output_accesses += float(vals[4])
                        output_amount += float(vals[5])
                    elif vals[2] in ["destructor"]:
                        destruction_time += float(vals[3])
    else:
        for line in data:
            if pattern.match(line):
                if "thread "+str(thread) in line:
                    in_thread = True
                else:
                    in_thread = False
            if len(line.strip()) == 0:
                in_thread = False
            # if in_thread:
            #     print(line)
            if "[TAZER] "+t in line:
                if in_thread:
                    # print(line)
                    vals = line.split(" ")
                    if vals[2] in ["open", "access", "stat", "seek", "in_open", "in_close", "in_fopen", "in_fclose"]:
                        input_time += float(vals[3])
                    elif vals[2] in ["read", "fread"]:
                        input_time += float(vals[3])
                        input_accesses += float(vals[4])
                        input_amount += float(vals[5])
                    elif vals[2] in ["close", "fsync", "out_open", "out_close"]:
                        output_time += float(vals[3])
                    elif vals[2] in ["write", "fwrite"]:
                        output_time += float(vals[3])
                        output_accesses += float(vals[4])
                        output_amount += float(vals[5])
                    elif vals[2] in ["destructor"]:
                        destruction_time += float(vals[3])
    # print(input_time, input_accesses, input_amount, output_time,
    #       output_accesses, output_amount, destruction_time)
    return input_time, input_accesses, input_amount, output_time, output_accesses, output_amount, destruction_time


def getCacheData(type, name, data, thread, aggregated):
    hits = 0
    hit_time = 0
    hit_amount = 0
    misses = 0
    miss_time = 0.0
    prefetches = 0
    stalls = 0
    stall_time = 0.0
    stall_amount = 0
    ovh_time = 0.0
    reads = 0
    read_time = 0.0
    read_amt = 0
    destruction_time = 0.0
    construction_time = 0.0
    in_thread = False
    pattern = re.compile("^\[TAZER\].* thread [0-9]+")

    if aggregated:
        for line in data:
            if pattern.match(line):
                in_thread = True
            elif len(line.strip()) == 0:
                in_thread = False

            #if not in_thread:
                #print(line)

            if name+" "+type in line:
                if not in_thread:
                    #print(line)
                    vals = line.split(" ")
                    if vals[3] == "hits":
                        hit_time += float(vals[4])
                        hits += int(vals[5])
                        hit_amount += int(vals[6])
                    elif vals[3] == "misses":
                        miss_time += float(vals[4])
                        misses += int(vals[5])
                    elif vals[3] == "prefetches":
                        prefetches += int(vals[5])
                    elif vals[3] == "stalls":
                        stall_time += float(vals[4])
                        stalls += int(vals[5])
                        stall_amount += int(vals[6])
                    elif vals[3] == "ovh":
                        ovh_time += float(vals[4])
                    elif vals[3] == "read":
                        read_time += float(vals[4])
                        reads += int(vals[5])
                        read_amt += int(vals[6])
                    elif vals[3] == "destructor":
                        destruction_time += float(vals[4])
                    elif vals[3] == "constructor":
                        construction_time += float(vals[4])
    else:
        for line in data:
            if pattern.match(line):
                if "thread "+str(thread) in line:
                    in_thread = True
                else:
                    in_thread = False
            if len(line.strip()) == 0:
                in_thread = False
            if name+" "+type in line:
                if in_thread:
                    vals = line.split(" ")
                    if vals[3] == "hits":
                        hit_time += float(vals[4])
                        hits += int(vals[5])
                        hit_amount += int(vals[6])
                    elif vals[3] == "misses":
                        miss_time += float(vals[4])
                        misses += int(vals[5])
                    elif vals[3] == "prefetches":
                        prefetches += int(vals[5])
                    elif vals[3] == "stalls":
                        stall_time += float(vals[4])
                        stalls += int(vals[5])
                        stall_amount += int(vals[6])
                    elif vals[3] == "ovh":
                        ovh_time += float(vals[4])
                    elif vals[3] == "read":
                        read_time += float(vals[4])
                        reads += int(vals[5])
                        read_amt += int(vals[6])
                    elif vals[3] == "destructor":
                        destruction_time += float(vals[4])
                    elif vals[3] == "constructor":
                        construction_time += float(vals[4])

    return hits, hit_time, hit_amount, misses, miss_time, prefetches, stalls, stall_time, stall_amount, ovh_time, reads, read_time, read_amt, destruction_time, construction_time


def getConnectionData(data):
    cons = []
    acceses = []
    amounts = []
    times = []
    for line in data:
        if "connection:" in line:
            vals = line.split(" ")
            if vals[2] in cons:
                i = cons.index(vals[2])
                acceses[i] += int(vals[4])
                amounts[i] += float(vals[6])
                times[i] += float(vals[9])
            else:
                cons.append(vals[2])
                acceses.append(int(vals[4]))
                amounts.append(float(vals[6]))
                times.append(float(vals[9]))
    return cons, acceses, amounts, times


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("infile", help="input file to be parsed")
    parser.add_argument("-s", "--server", help="count all threads, use for parsing server output", action='store_true', default=False)
    parser.add_argument("-a", "--aggregated", help="only count aggregated statistics", action='store_true', default=False)
    args = parser.parse_args(sys.argv[1:])

    tazer_input_total = 0
    tazer_input_mem = 0
    tazer_input_shmem = 0
    tazer_input_global = 0
    tazer_input_network = 0
    tazer_output = 0
    tazer_destructor = 0.0
    local_input = 0
    local_output = 0
    sys_input = 0
    sys_output = 0

    data = []
    with open(args.infile) as file:
        for line in file:
            data.append(line)

    if args.aggregated:
        threads = [0]
    else:
        threads = getThreads(data, args.server)

    types = ["sys", "local", "tazer"]
    names = ["input_time", "input_accesses", "input_amount", "output_time",
             "output_accesses", "output_amount", "destruction_time"]
    vals = []
    labels = []
    labels.append("threads")
    vals.append(len(threads))
    thread_num = 1
    for thread_id in threads:
        for t in types:
            vs = getVals(t, data, thread_id, args.aggregated)
            vals += vs
            for n in names:
                labels.append(t+"_"+n+"_"+str(thread_num))
        thread_num += 1

    # hits,hit_time,hit_amount,misses,miss_time,prefetches,stalls,stall_time,stall_amount,ovh_time,reads,read_time,read_amt,destruction_time

    # vs = getCacheData("request", "base", data)
    # labels += ["cache_accesses", "cache_time", "cache_amount",
    #            "base_cache_ovh", "base_destruction"]
    # vals += [vs[0], vs[1], vs[10], vs[8], vs[11]]

    caches = ["base","privatememory","sharedmemory","burstbuffer", "boundedfilelock","network"]
    types = ["request", "prefetch"]
    names = ["hits", "hit_time", "hit_amount", "misses", "miss_time", "prefetches",
             "stalls", "stall_time", "stall_amt", "ovh_time", "reads", "read_time", "read_amt", "destruction_time", "construction_time"]

    thread_num = 1
    for thread_id in threads:
        for t in types:
            for c in caches:
                vs = getCacheData(t, c, data, thread_id, args.aggregated)
                vals += vs
                for n in names:
                    labels.append(c+"_"+t+"_"+n+"_"+str(thread_num))
        thread_num += 1
    # for t in types:
    #     vs = getCacheData(t, "network", data)
    #     labels += ["network_accesses", "network_time",
    #                "network_total_amount", "network_used_amount", "network_ovh_time"]
    #     vals += vs[0:3]
    #     vals.append(vs[10])
    #     vals.append(vs[8])

    cons, acceses, amounts, times = getConnectionData(data)
    names = ["_accesses", "_amount", "_time"]
    for i in range(len(cons)):
        vals += [acceses[i], amounts[i], times[i]]
        for n in names:
            labels.append("con_"+cons[i]+n)

    label_str = "labels:"
    vals_str = "vals:"
    for i in range(len(labels)):
        label_str += labels[i]+","
        vals_str += str(vals[i])+","
        # print (labels[i],str(vals[i]))

    print(label_str[:-1])
    print(vals_str[:-1])