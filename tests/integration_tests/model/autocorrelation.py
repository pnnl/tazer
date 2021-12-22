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

os.environ["CUDA_DEVICE_ORDER"] = "PCI_BUS_ID"
os.environ["CUDA_VISIBLE_DEVICES"] = "0"
import tensorflow.keras
import time
import math
import statistics
import matplotlib.pyplot as plt
from sklearn.preprocessing import MinMaxScaler, StandardScaler
from statsmodels.graphics.tsaplots import plot_pacf
import sys
class block:
    def __init__(self, n):
        xx = [x[0] for x in os.walk(sys.argv[1])]#"../runner-test/integration/client/")]
        self.dir_ = []
        self.n = n
        self.model = None
        for i in xx:
            t = i.split("/")
            if "local" in t[-1]:
                self.dir_.append(i)
        #print(self.dir_)
        self.index = 0
        self.data = {}
        self.size = 0
        self.FID = []

        self.cacheID = {}
        self.action = {}
        self.Fsize = {}
        self.Fsize2 = {}
        self.countcache = 0
        self.countact = 0
        self.FsizeCount = 0
        self.FsizeCount2 = 0

        #self.dir_ = [self.dir_[0]]

        for i in self.dir_:
            self.data[i], self.size = self.read_data(i+"/block_stats.txt", self.n)

        print(self.data)
        print(self.action)

    def read_data(self, name, num_val):
        f = open(name, "r")
        data = f.read()
        f.close()
        data = data.split("\n")
        t = []
        size = len(data)
        for i in range(self.index*num_val, (self.index + 1)*num_val, 1):
            t_ = data[i].split(" ")
            temp = []
            for j in t_:
                if j != "":
                    temp.append(j)
            if len(temp) > 2:
                if temp[2] not in "[BLOCK_REQUEST]" and temp[2] not in "[ADD_FILE]" and temp[2] not in "[BLOCK_WRITE]":# and temp[2] not in "[BLOCK_READ_HIT]":
                    if temp[1] not in self.cacheID.keys():
                        self.cacheID[temp[1]] = self.countcache
                        self.countcache += 1
                    if temp[2] not in self.action.keys():
                        self.action[temp[2]] = self.countact
                        self.countact += 1
                    temp[1] = self.cacheID[temp[1]]
                    temp[2] = self.action[temp[2]]
                    t.append(temp)
        data = np.array(t)
        return data,size

    def formatdata(self):
        keys = list(self.data.keys())
        t = {}
        for i in keys:
            for j in self.data[i]:
                if j[0] not in t.keys():
                    t[j[0]] = []
                t[j[0]].append(j)#j[1:])

        keys2 = list(t.keys())
        keys2 = sorted(keys2)
        temp = []
        for i in keys2:
            for j in t[i]:
                tt = []
                for x in j:
                    tt.append(float(x))
                temp.append(tt)
        temp = np.array(temp)

        if (self.index + 1)*self.n < self.size:
            self.index = self.index + 1
        #for i in self.dir_:
        #    self.data[i], self.size = self.read_data(i+"/block_stats.txt", self.n)
        return temp

time = block(1000000)
data = time.formatdata()
scaler_y = MinMaxScaler()
data = scaler_y.fit_transform(data)
print(data)

labels = ["timestamps", "Cache ID", "Action", "Block ID", "File ID"]#, "File size"]

for i in range(len(labels)):
    fig = plt.figure() 
    ax = fig.add_subplot(1,1,1)
    ax.spines["right"].set_color("none")
    ax.spines["top"].set_color("none")
    ax.xaxis.set_ticks_position("bottom")
    ax.yaxis.set_ticks_position("left")
    ax.set_ylabel("Partial autocorrelation")
    ax.set_xlabel("Timesteps")

    plot_pacf(data[:,i], title="Partial autocorrelation of all\nfeatures describing block access" , ax=ax, lags=20)
    plt.tight_layout()
    fig.savefig("partial_auto_"+labels[i]+".png")
#    print(labels[i])
#    fig = plt.figure() 
#    # Adding plot title.
#    plt.title("Autocorrelation Plot for "+labels[i])
# 
#    # Providing x-axis name.
#    plt.xlabel("Lags")
# 
#    # Plotting the Autocorrelation plot.
#    #plot_pacf(data[:,i], lags=20)
#    plt.acorr(data[:,i], maxlags = 20)
# 
#    # Displaying the plot.
#    #print("The Autocorrelation plot for the data is:")
#    plt.grid(True)
#
#    plt.tight_layout()
#    fig.savefig(labels[i]+".png")
