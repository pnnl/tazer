seed_value= 0

# 1. Set the `PYTHONHASHSEED` environment variable at a fixed value
import os
os.environ['PYTHONHASHSEED']=str(seed_value)

# 2. Set the `python` built-in pseudo-random generator at a fixed value
import random
random.seed(seed_value)

# 3. Set the `numpy` pseudo-random generator at a fixed value
import numpy as np
np.random.seed(seed_value)

# 4. Set the `tensorflow` pseudo-random generator at a fixed value
import tensorflow as tf
tf.random.set_seed(seed_value)
from block_sel import block
import time
import math
import statistics
import sys
from sklearn.preprocessing import MinMaxScaler, StandardScaler

count_whole = 0

file_ = open("speed.txt", "r")
speed_val = file_.read()
file_.close()
speed_val = int(speed_val)

time_ = block(50000, "../runner-test/integration/client/") #120000
#time_ = block(200000, "../runner-test/integration/client/") #with this line it worked :)
data_ = time_.formatdata()
while len(data_.keys()) == 0:
    #time_ = block(200000, "../runner-test/integration/client/")
    data_ = time_.formatdata()
    print(data_.keys())
    time.sleep(1)
while (count_whole == 0 or (count_whole == 1 and speed_val == 1)):
    #time.sleep(1)
    data_ = time_.formatdata()
    #zero = 0
    #for i in list(data_.keys()):
    #    if len(data_[i]) > 0:
    #        zero += 1 
    #while zero < len(list(data_.keys())):
    #    data_ = time_.formatdata()
    #    zero = 0
    #    for i in list(data_.keys()):
    #        if len(data_[i]) > 0:
    #            zero += 1
    blocks_sel = {}
    block_acc = {}
    kk = list(data_.keys())
    for name in list(data_.keys()):
        print(name)
        #data = data_[name][-15000:,2:-1]
        data = data_[name][:,2:-1]

        #data = data[::2]   
 
        training = []
        target = []

        M=int(sys.argv[1])#17#14#8#4#1
        N=int(sys.argv[2])#14#15#10
        O=1

        scaler_y = MinMaxScaler()
        data = scaler_y.fit_transform(data)

        for i in range(0, len(data)-(M+N+O)):
            training.append(data[i:i+M])
            target.append(data[i+M+N:i+M+N+O])

        training = np.array(training)
        target = np.array(target)

        train = []
        targ = []
        for i in range(len(training)):
            train.append(training[i].transpose())
            targ.append(target[i].transpose())

        train = np.array(train)
        print(train.shape)
        targ = np.array(targ)
        print(targ.shape)

        q = train
        p = targ
        model = time_.train(q, p, int(round(len(train)*0.8)))
        prediction = np.array(time_.predict(q[int(round(len(train)*0.8)):]))

        prediction = prediction.reshape((prediction.shape[0],prediction.shape[1]))
        targ1 = p[int(round(len(train)*0.8)):]
        targ1 = targ1.reshape((targ1.shape[0], targ1.shape[1])) 
        prediction = scaler_y.inverse_transform(prediction)
        targ1 = scaler_y.inverse_transform(targ1)

        print(prediction)#[:-(M+N+O)])
        print("---------------------")
        print(targ1)

        #cacheID = time.cacheID
        #action = time.action

        tt = []

        for i in range(len(prediction)):
            if int(round(prediction[i,-1])) < 0:
                prediction[i,-1] = -int(round(prediction[i,-1])) 
            if int(round(prediction[i,-2])) < 0:
                prediction[i,-2] = -int(round(prediction[i,-2]))
            #if int(round(prediction[i,-1])) > 100000:
            #    prediction[i,-1] = 0
            #if int(round(prediction[i,-1])) < 100000 and int(round(prediction[i,-1])) > 0:
            #    if prediction[i,-1] > 1000:
            #        tt.append(str(int(round(prediction[i,-2]))) + "_" + str(int(round(prediction[i,-1]/1000))))
            #    else:
            if  True: #prediction[i,-2] not in time_.last_added:
                tt.append(str(int(round(prediction[i,-2]))) + "_" + str(int(round(prediction[i,-1]))))

        #pr_bl = np.unique(tt)
        pr_bl = []
        for i in tt:
            if i not in pr_bl:
                pr_bl.append(i)
        print(pr_bl)
        #pr_fl = np.unique(prediction[:,-1])
    
        timestep_acc = {}
        for i in range(len(prediction) - 10*(N+O), len(prediction)): #1000*(N+O), len(prediction)):
            #if prediction[i,0] not in timestep_acc.keys():
            if i not in timestep_acc.keys():
                timestep_acc[i] = {}
            if int(round(prediction[i,-1])) < 0:
                prediction[i,-1] = -prediction[i,-1]
            if int(round(prediction[i,-2])) < 0:
                prediction[i,-2] = -int(round(prediction[i,-2]))
            #if prediction[i,-1] > 1000:
            #    temp = str(int(round(prediction[i,-2]))) + "_" + str(int(round(prediction[i,-1]/1000)))
            #else:
            if True: #prediction[i,-2] not in time_.last_added:
                temp = str(int(round(prediction[i,-2]))) + "_" + str(int(round(prediction[i,-1])))
                for j in pr_bl:
                    if j == temp:
                        timestep_acc[i][j] = 1
                    elif temp not in timestep_acc[i].keys():
                        timestep_acc[i][j] = 0
        acc = []
        keys1 = list(timestep_acc.keys())
        #print(tt)
        for i in timestep_acc.keys():
            temp_acc = []
            for x in pr_bl:#tt:
                if x not in timestep_acc[i].keys():
                    temp_acc.append(0)
                else:
                    temp_acc.append(timestep_acc[i][x])
            acc.append(temp_acc)
        blocks_sel[name] = pr_bl
        block_acc[name] = np.array(acc)
        #blocks_sel[kk[0]] = pr_bl
        #block_acc[kk[0]] = np.array(acc)

   
    keys = list(blocks_sel.keys())
    res = []
    acc = []
    for n in range(len(keys)):
        print(block_acc[keys[n]].shape)
        print(len(blocks_sel[keys[n]]))
        for a_ind in range(len(blocks_sel[keys[n]])):
            a = blocks_sel[keys[n]][a_ind]
            #NF = True
            #for c in range(len(keys)):
            #    #if c != n:
            if a not in res:
                res.append(a)
                acc.append(block_acc[keys[n]][:,a_ind])
            #elif sum(block_acc[keys[n]][:,a_ind]) > 0:
            #    acc[res.index(a)] = block_acc[keys[n]][:,a_ind]

    """
                if c != n:
                    if a not in blocks_sel[keys[c]]:
                        NF = False
            if NF:
                res.append(a)
                acc.append(block_acc[keys[n]][:,a_ind])
    """        

    #while True:
    #    succ = True
    #    if os.path.isdir('lock_file'):
    #        time.sleep(1)
    #    else:
    #        try:
    #            os.mkdir("lock_file")
    #        except OSError as error:
    #            succ = False
    #            print("Directory can not be created")
    #        if succ: #os.path.isdir('lock_file'):
    #            break
    #while os.path.isdir('lock_file'):
    #    time.sleep(1)
    file_ = open("blocks_to_move.txt", "w")
    acc = np.array(acc)
    acc = acc.transpose()
    print(res)
    t = ""
    if len(res) > 0:
        if acc.shape[1] > 5:
            acc = acc[::-1]
            count = 0
            for i in range(acc.shape[1]):
                if sum(acc[:,i]) == 0:
                    t += str(res[i]) + ","
                    count += 1
                    if count == 5:
                        break
                    #i = acc.shape[1]
        else:
            for i in range(acc.shape[1]):
                if sum(acc[:,i]) == 0:
                    t += str(res[i]) + "," #-1 was initially i
        file_.write(t[:-1])
    file_.close()
    #if os.path.isdir('lock_file'):
    #    os.rmdir("lock_file")
    

    if count_whole == 0 and len(res) > 0:
        count_whole += 1
        file_ = open("speed.txt", "w")
        file_.write("1")
        file_.close()

    file_ = open("speed.txt", "r")
    speed_val = file_.read()
    file_.close()
    speed_val = int(speed_val)
    #time.sleep(500)    
