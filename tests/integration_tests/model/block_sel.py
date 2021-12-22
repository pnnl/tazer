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
from tensorflow.keras.layers import Conv1D, LeakyReLU, Input, Dense, LSTM, GRU, SimpleRNN, Dropout
from tensorflow.keras.callbacks import EarlyStopping, CSVLogger, ModelCheckpoint
from tensorflow.keras.optimizers import Adam
from tensorflow.keras.models import Model, load_model, Sequential
from sklearn.preprocessing import MinMaxScaler, StandardScaler
from scipy.optimize import curve_fit
import sys
import glob

class fit_curve_:
    def Gauss(self,x, A, B, C, D, E, F, G):
        y = A*x**6 + B*x**5 + C*x**4 + D*x**3 + E*x**2 + F*x + G#A*np.exp(-1*B*x**2)
        return y

    def Get_par(self, xdata, ydata):
        parameters, covariance = curve_fit(self.Gauss, xdata, ydata)
        return parameters


    def predict(self, data, params):
        temp = []
        for i in range(len(data[0])):
            te = []
            for x in range(len(data)):
                s = 0
                ex = len(params)-1
                for j in range(len(params)-1):
                    s += params[j]*data[x,i]**ex
                    ex = ex - 1
                s += params[-1]
                te.append(s)
            temp.append(te)
        temp = np.array(temp)
        return temp

class block:
    def __init__(self, n, d):
        self.loc = d
        xx = [x[0] for x in os.walk(d)]#"../runner-test/integration/client/")]
        self.dir_ = []
        self.n = n
        self.model = None
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
        self.num_changes = 10000000000 #10
        self.num_changes_ind = 0
        self.last_added = []
        for i in xx:
            t = i.split("/")
            if "local" in t[-1]:
                #ff = glob.glob(i+"/*")
                #xx2 = []
                #for z in ff:
                #    if "test" in z:
                #        xx2.append(z)
                #for y in xx2:
                #    #print(y)
                self.dir_.append(i) #i)
        
        for i in self.dir_:
            self.data[i], self.size = self.read_data(i+"/block_stats.txt", self.n)
  

    def isfloat(self,value):
        try:
            float(value)
            return True
        except ValueError:
            return False

    def read_data(self, name, num_val):
        size = 0
        while not os.path.exists(name):
            time.sleep(1)
        while size < num_val:
            time.sleep(1)
            #f = open(name, "r")
            with open(name, encoding = "ISO-8859-1") as f:
                data = f.read()
            f.close()
            data = data.split("\n")
            t = []
            size = len(data)
            #time.sleep(1)
        while (self.index + 1)*num_val > size: #1)*num_val > size:
            self.index = self.index - 1#1
        temp_last_add = 0
        self.last_added = []
        for i in range(self.index*num_val, (self.index + 1)*num_val, 1):
            t_ = data[i].split(" ")
            temp = []
            for j in t_:
                if j != "":
                    temp.append(j)
            #if len(temp) < 3:
            #    print(temp)
            
            if temp[1] in "scalable" and temp[2] not in "BLOCKSTREAMING" and temp[2] not in "[ADD_FILE]": #and temp[2] not in "[BLOCK_REQUEST]" and temp[2] not in "BLOCK_EVICTED" and temp[2] not in "[BLOCK_WRITE]":# and temp[2] not in "[BLOCK_READ_HIT]":
                if temp[1] not in self.cacheID.keys():
                    self.cacheID[temp[1]] = self.countcache
                    self.countcache += 1
                if temp[2] not in self.action.keys():
                    self.action[temp[2]] = self.countact
                    self.countact += 1
                temp[1] = self.cacheID[temp[1]]
                temp[2] = self.action[temp[2]]   
                #If it doesn't work remove the next two lines
                #del temp[1]
                #del temp[2]
                t.append(temp)
            elif temp[2] in "[ADD_FILE]":
                temp_last_add = temp[3]
        self.last_added.append(temp_last_add)
        #t_ = []
        #for i in range(len(t)):
        #    if t[i][3] not in self.last_added:
        #        t_.append(t[i])

        data = np.array(t)
        return data,size

    def formatdata(self):
        keys = list(self.data.keys())
        """ 
        temp = {}
        for i in keys:
            print(len(self.data[i]))
            if len(self.data[i]) >= 20000:
                t = []
                for j in self.data[i]:
                    tt = []
                    ADD = True
                    for x in j:
                        if self.isfloat(x):
                            tt.append(float(x))
                        else:
                            ADD = False
                    if ADD:
                        t.append(tt)
                temp[i] = np.array(t)
        """
        t = {}
        for i in keys:
            for j in self.data[i]:
                if j[0] not in t.keys():
                    t[j[0]] = []
                t[j[0]].append(j)#j[1:])

        keys2 = list(t.keys())
        keys2 = sorted(keys2)
        temp_ = []
        for i in keys2:
            for j in t[i]:
                tt = []
                for x in j:
                    tt.append(float(x))
                temp_.append(tt)     
        temp_ = np.array(temp_) 
        temp = {}
        temp[keys[0]] = temp_
        
        if (self.index + 1)*self.n < self.size:
            self.index = self.index + 1 #1

        xx = [x[0] for x in os.walk(self.loc)]#"../runner-test/integration/client/")]
        self.dir_ = []
        for i in xx:
           t = i.split("/")
           if "local" in t[-1]:
               #ff = glob.glob(i+"/test_*")
               #xx2 = []
               #for z in ff:
               #    if "test" in z:
               #        xx2.append(z)
               #xx2 = [x[0] for x in os.walk(i+"/test_*")]
               #for y in xx2:
               self.dir_.append(i)
        for i in self.dir_:
            self.data[i], self.size = self.read_data(i+"/block_stats.txt", self.n) 
        return temp           

    def create_model(self, step_size, nb_features, out):
        inputs = Input(shape=(step_size, nb_features))
        preds = Conv1D(step_size,kernel_size=(int(round(4)),),activation='relu',padding='same')(inputs)
        preds = Conv1D(step_size,kernel_size=(int(round(1)),),activation='relu',padding='same')(preds) #was times 2
        #preds = LeakyReLU(alpha=0.2)(preds) #with 0.2, speedracer matches base case (no speedracer)
        #preds = Conv1D(int(round(step_size)),kernel_size=(int(round(step_size)),),activation='linear',padding='same')(preds)
        #preds = Conv1D(step_size,kernel_size=(int(round(4)),),activation='relu',padding='same')(preds)
        #preds = LeakyReLU(alpha=0.1)(preds)
        #preds = Conv1D(step_size,kernel_size=(int(round(4)),),activation='relu',padding='same')(preds)
        #preds = Conv1D(step_size,kernel_size=(int(round(nb_features/4)),),activation='linear',padding='same')(preds)
        ##preds = LeakyReLU(alpha=0.2)(preds)
        #preds = Conv1D(step_size,kernel_size=(int(round(nb_features/4)),),activation='linear',padding='same')(preds)
        #preds = Flatten()(preds)
        #preds = GRU(step_size, return_sequences=True)(inputs)
        ##preds = Dropout(0.1)(preds)
        #preds = GRU(step_size, return_sequences=True)(preds)

        #worked when 6 was 4
        #preds = Dense(step_size*4, activation='linear')(inputs) # With this one it did work
        #preds = Dropout(0.1)(preds)#remove this if it breaks :)
        #preds = Dense(step_size*4, activation='linear')(preds) #was 200
        #preds = Dropout(0.1)(preds)
        
        #preds = Conv1D(step_size,kernel_size=(int(round(nb_features)),),activation='linear',padding='same')(preds)
        #preds = Dense(step_size*4, activation='linear')(preds)
        #preds = Dense(100, activation='relu')(preds)
        #preds = Dense(100, activation='relu')(preds)
        #preds = Dense(out, activation='linear')(preds)
        preds = Dense(out, activation='sigmoid')(preds) #Conv1D(out,kernel_size=(out,),activation='linear',padding='same')(preds)#Dense(out, activation='relu')(preds)
        #preds = GRU(step_size, return_sequences=True)(preds)
        #r = tf.keras.regularizers.l1_l2(l1=0.001, l2=0.001)
        model = Model(inputs=inputs,outputs=preds)
        return model

    def train(self, X, Y, train_test_split):
        step_size = X.shape[1]
        nb_features = X.shape[2]
        out = Y.shape[2]
        tf.keras.backend.clear_session();
        if self.model == None or self.num_changes == self.num_changes_ind: # It worked when it was always True
            self.model = self.create_model(step_size, nb_features, out)
            rms = Adam(learning_rate=0.001) # was 0.001
            self.model.compile(loss="mse", optimizer=rms, metrics=["mse"])
            self.num_changes_ind = 0
        es = EarlyStopping(monitor='val_loss', mode='min', verbose=1, patience=20)
        hist = self.model.fit(X[:train_test_split], Y[:train_test_split], batch_size=10, epochs=150, verbose=1, validation_split=0.25, callbacks=[es])
        self.num_changes_ind += 1
        return self.model

    def predict(self, X_test):
        predicted = self.model.predict(X_test)
        return predicted

    def write_file(self,data):
        f = open("to_graph.csv", "a")
        for i in data:
            line = ""
            for j in i:
                line += str(j) + ","
            line = line[:-1]+ "\n"
            f.write(line)
        f.close()
"""
time_ = block(1000000, "../backup/no_speed/")
data = time_.formatdata() 

#time.write_file(data)
data = data[:,1:-1]
scaler_y = MinMaxScaler()
data = scaler_y.fit_transform(data)

fd = fit_curve_()
#opre = []
for i in range(len(data[0])):
    fd = fit_curve_()
    par = fd.Get_par(data[:int(round(len(data[:,i])*0.3))-1,i], data[int(round(len(data[:,i])*0.3)):int(round(len(data[:,i])*0.6)),i])
    t = data[int(round(len(data)*0.8)):,i].reshape((len(data[int(round(len(data)*0.8)):,i]), ))
    pp = data[int(round(len(data)*0.6)):int(round(len(data)*0.8)),i].reshape((len(data[int(round(len(data)*0.6)):int(round(len(data)*0.8)),i]), 1))
    pre = fd.predict(pp,par)
    print(pre)
    print("-----------------------------------")
    print(t)
    print("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX")


training = []
target = []
M=int(sys.argv[1])#17#14#8#4#1
N=int(sys.argv[2])#14#15#10
O=1

#scal = []
#temp = []
#for i in range(data.shape[1]):
#  scaler_y = MinMaxScaler()
#  t = data[:,i].reshape((len(data), 1))
#  t = scaler_y.fit_transform(t)
#  temp.append(t.reshape((len(data),)))
#  scal.append(scaler_y)

#temp = np.array(temp)
#data = temp.transpose()
#print(data.shape)
scaler_y = MinMaxScaler()
data = scaler_y.fit_transform(data)
#training = data[:int(round(len(data)/2))]
#target = data[int(round(len(data)/2)):]

#pr = []
#for i in range(len(training)):
#    pr.append(training[i].reshape(len(training[i]), 1))
#train = pr

#ta = []
#for i in range(len(target)):
#    ta.append(target[i].reshape(len(target[i]), 1))
#targ = ta

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

#for i in range(train.shape[1]):
q = train#[:,i]
#scaler_y = MinMaxScaler()
#q = scaler_y.fit_transform(q)
#q = q.reshape((train[:,i].shape[0],1,train[:,i].shape[1]))
p = targ#[:,i]
#p = scaler_y.fit_transform(p)
#p = p.reshape((targ[:,i].shape[0],1,targ[:,i].shape[1]))
print("wwwwwwwwwwwwwwwwwwwwwwww")
model = time.train(q, p, int(round(len(train)*0.8)))
prediction = np.array(time.predict(q[int(round(len(train)*0.8)):]))

prediction = prediction.reshape((prediction.shape[0],prediction.shape[1]))
targ1 = p[int(round(len(train)*0.8)):]
targ1 = targ1.reshape((targ1.shape[0], targ1.shape[1]))

#prediction = scaler_y.inverse_transform(prediction)
#targ1 = scaler_y.inverse_transform(targ1)

#print(prediction)#[:-(M+N+O)])
#print("---------------------")
#print(targ1)
f = open("accuracy.txt", "a")
line = str(M)+","+str(N)+","+str(O)+","
for i in range(prediction.shape[1]):
    tt = abs(prediction[:,i] - targ1[:,i])
    line += str(sum(tt)/len(tt)) + " +- "+ str(statistics.stdev(tt)) + ","

f.write(line[:-1]+"\n")
f.close()



prediction = scaler_y.inverse_transform(prediction)
targ1 = scaler_y.inverse_transform(targ1)

print(prediction)#[:-(M+N+O)])
print("---------------------")
print(targ1)

cacheID = time.cacheID
action = time.action

#print(cacheID)
#for i in range(len(prediction)):
#    for j in cacheID.keys():
#        if cacheID[j] == int(round(prediction[i,1])):
#            prediction[i, 1] = j#cacheID[int(round(prediction[i,1]))]
#        if cacheID[j] == int(round(targ1[i,1])):
#            targ1[i, 1] = j#cacheID[int(round(targ1[i,1]))]

#print("SSSSSSSSSSSSSSSSSSSSSSSSS")
#print(prediction)#[:-(M+N+O)])
#print("---------------------")
#print(targ1)
#print(cacheID)
#print(action)

"""
