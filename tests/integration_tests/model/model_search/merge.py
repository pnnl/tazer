import sys, os
parent_dir = os.getcwd() # find the path to module a
# Then go up one level to the common parent directory
path = os.path.dirname(parent_dir)
# Add the parent to sys.pah
sys.path.append(path)


import numpy as np
np.random.seed(3900) #5400
os.environ["CUDA_DEVICE_ORDER"] = "PCI_BUS_ID"
os.environ["CUDA_VISIBLE_DEVICES"]="0"
import tensorflow.keras
import time
import math
from tensorflow.keras.models import Model, load_model
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Input, GaussianNoise, Embedding, BatchNormalization
from tensorflow.keras.layers import Dense, Dropout, Activation, Flatten, TimeDistributed
from tensorflow.keras.layers import Conv1D, MaxPooling1D, LeakyReLU, PReLU
#from keras.utils import np_utils
from tensorflow.keras.callbacks import CSVLogger, ModelCheckpoint
from tensorflow.keras.regularizers import l2
import h5py
from tensorflow.keras.layers import LSTM, SimpleRNN, GRU, Reshape, Bidirectional
import tensorflow as tf
from tensorflow.python.client import device_lib
from sklearn.preprocessing import MinMaxScaler, StandardScaler
#from keras.backend.tensorflow_backend import set_session
from tensorflow.python.keras.backend import set_session
from tensorflow.keras import layers
from tensorflow.keras import backend as K
from math import sqrt
from tensorflow.keras.optimizers import RMSprop, Adam, SGD
from tensorflow.keras.callbacks import EarlyStopping
from kerastuner import HyperModel
from kerastuner.tuners import RandomSearch, Hyperband
import kerastuner
from block_sel import block
import statistics
import glob

class MyHyperModel(HyperModel):

    def __init__(self, num_classes,feat, output):
        self.num_classes = 1000
        self.M = num_classes
        #self.nb_feat = num_classes
        self.out = output
        self.nb_feat = feat
        #self.M = feat
        print("initializing the model size")

    def build(self, hp):
        print("initializing the model")
        model = Sequential()
        model.add(layers.Input(shape=(self.M, self.nb_feat)) )
        #model.add(layers.Input(shape=(self.nb_feat, self.M)) )
        for i in range(hp.Int('num_layers', 0, 10)):
            layer = []
            layer.append([layers.Dense(units=hp.Int('units_'+str(i),min_value=self.nb_feat,max_value=self.nb_feat*100,step=self.nb_feat),activation=hp.Choice('activation_'+str(i),values=['relu', 'elu','linear' ]))])
            layer.append([layers.LSTM(units=hp.Int('units_'+str(i),min_value=self.nb_feat,max_value=self.nb_feat*100,step=self.nb_feat), return_sequences=True)])
            layer.append([layers.GRU(units=hp.Int('units_'+str(i),min_value=self.nb_feat,max_value=self.nb_feat*100,step=self.nb_feat), return_sequences=True)])
            layer.append([layers.SimpleRNN(units=hp.Int('units_'+str(i),min_value=self.nb_feat,max_value=self.nb_feat*100,step=self.nb_feat), return_sequences=True,)] )
            #layer.append([layers.Conv1D(filters=hp.Int('filters_'+str(i),min_value=self.M,max_value=self.M*10,step=1), kernel_size=1 )])
            #layer.append([layers.Dropout(hp.Float('dropout_val'+str(i),min_value=0.01,max_value=0.99, step=0.01))])
            #vals = []
            #for w in range(len(layer)):
            #    vals.append(int(w))
            for w in layer[hp.Int('layer_'+str(i), 1, len(layer))-1]:
                model.add(w)
            #layers.Dense(units=hp.Int('units_'+str(i),min_value=self.M,max_value=self.M*10,step=1),activation='relu'))
        model.add(layers.Dense(self.out,activation=hp.Choice('activation_last_layer',values=['relu',  'linear' ])))
        model.compile(
            optimizer=Adam(
                hp.Choice('learning_rate',
                          values=[1e-2, 1e-3, 1e-4])),
            loss='mse',
            metrics=['mse'])
        print("done")
        return model

class MyTuner(kerastuner.tuners.BayesianOptimization):#Hyperband ):#RandomSearch ):
  def run_trial(self, trial, *args, **kwargs):
    # You can add additional HyperParameters for preprocessing and custom training loops
    # via overriding `run_trial`
    kwargs['batch_size'] = trial.hyperparameters.Int('batch_size', 50, 500, step=10)
    kwargs['epochs'] = trial.hyperparameters.Int('epochs', 50, 500, step=10)
    super(MyTuner, self).run_trial(trial, *args, **kwargs)


def is_number(s):
    try:
        float(s)
        return True
    except ValueError:
        pass

    try:
        import unicodedata
        unicodedata.numeric(s)
        return True
    except (TypeError, ValueError):
        pass

    return False

time = block(1000000, "../../backup/no_speed/")
data = time.formatdata()
data = data[:,:-1]

training = []
target = []

M=12#int(sys.argv[1])#17#14#8#4#1
N=6#int(sys.argv[2])#14#15#10
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
#gggggggggggggg

#train = data[:len(data)*0.7]

#targ = data[len(data)*0.7:]

#print(train)
#print("======================")
#print(targ)


main_project_dir = "my_dir/*"
dirs_ = glob.glob(main_project_dir)
for i in range(len(dirs_)):
    tt = dirs_[i].split("/") #+ "/trial_*/"
    dirs_[i] = tt[-1]#glob.glob(tt)
print(dirs_)

#copy_tree(fromDirectory, toDirectory)
#merged_dir = main_project_dir[:-1] + "merged"
#for i in range(len(dirs_)):
#    for y in range(len(dirs_[i])):
#        tt = dirs_[i][y].split("/")
#        print(merged_dir+"/"+tt[-2])
#        copy_tree(dirs_[i][y], merged_dir+"/trial_"+str(i*(y+1)))

model = []
loss_arr = []


for i in dirs_:
    hypermodel = MyHyperModel(num_classes=train.shape[1], feat=train.shape[2],output=O)#targ.shape[1])

    tuner = MyTuner(
        hypermodel,
        objective='val_loss',
        max_trials=1,
        directory='my_dir',
        project_name=i)#'merged')


    tuner.search( train[:int(round(0.8*len(train)))], targ[:int(round(0.8*len(train)))], batch_size=1, epochs=100, verbose=1, validation_split=0.25)

    #Show the best model and their corresponding hyper-parameters:
    tuner.results_summary()


    # Retrieve the best model.
    best_model = tuner.get_best_models(num_models=1)[0]
    model.append(best_model)

    # Evaluate the best model.
    loss, accuracy = best_model.evaluate( train[int(round(0.8*len(train))):], targ[int(round(0.8*len(train))):])
    print('loss:', loss)
    loss_arr.append(loss)
    #print('accuracy:', accuracy)
    #print(best_model.summary())

print(loss_arr)
index = loss_arr.index(min(loss_arr))
print("The best model is:")
print(model[index].summary())
"""
hypermodel = MyHyperModel(num_classes=train.shape[1], feat=train.shape[2],output=targ.shape[1])

#tuner = RandomSearch(
#tuner = Hyperband(
tuner = MyTuner(
    hypermodel,
    objective='val_loss',
    #factor=12,
    max_trials=10,
    #max_epochs=100,
    directory='my_dir',
    project_name='helloworld'+sys.argv[1])

print(len(train))
print(0.8*len(train))
print(int(round(0.8*len(train))))
print(train.shape)

#tuner.search(train[:int(round(0.6*len(train)))], targ[:int(round(0.6*len(train)))],
#tuner.search(train, targ,
tuner.search( train[:int(round(0.8*len(train)))], targ[:int(round(0.8*len(train)))], batch_size=1, epochs=100, verbose=1, validation_split=0.25)
#             epochs=100,
#             validation_data=(train[int(round(0.6*len(train))): int(round(0.8*len(train)))], targ[int(round(0.6*len(train))): int(round(0.8*len(train)))]))

#Show the best model and their corresponding hyper-parameters:
tuner.results_summary()


# Retrieve the best model.
best_model = tuner.get_best_models(num_models=1)[0]

# Evaluate the best model.
loss, accuracy = best_model.evaluate( train[int(round(0.8*len(train))):], targ[int(round(0.8*len(train))):])
print('loss:', loss)
print('accuracy:', accuracy)
print(best_model.summary())

file_ = open("thread_model.txt", "r")
data = int(file_.read())
file_.close()
file_ = open("thread_model.txt", "w")
file_.write(str(data+1))
file_.close()


pre = train_model(train, targ, int(round(0.8*len(train))), O)
pre = pre.reshape((pre.shape[0], pre.shape[1]))
pre = scaler_y.inverse_transform(pre)
for i in range(len(pre)):
    for j in range(len(pre[i])):
        pre[i,j] = round(pre[i,j])
target_inv = targ[int(round(0.8*len(train))):]
target_inv = target_inv.reshape((target_inv.shape[0], target_inv.shape[1]))
target_inv = scaler_y.inverse_transform(target_inv)
#print(pre)
#print("========================")
#print(target_inv)

move_time = []
for i in range(pre.shape[1]):#-1, 0, -1):
    zfound = False
    for j in range(pre.shape[0]-1, 0, -1):
        if float(pre[j,i]) == 0.0 and zfound==False:
            zfound = True
        if float(pre[j,i]) != 0.0 and zfound==True:
            move_time.append((j-(6))*5)#float(date_new[-1])+(j*5))
            break
            
if len(move_time) == 0:
    for i in range(pre.shape[1]):
        move_time.append(0)

print(move_time)

move_time_r = []
for i in range(target_inv.shape[1]):#-1, 0, -1):
    zfound = False
    for j in range(target_inv.shape[0]-1, 0, -1):
        if float(target_inv[j,i]) == 0.0 and zfound==False:
            zfound = True
        if float(target_inv[j,i]) != 0.0 and zfound==True:
            move_time_r.append((j)*5)#float(date_new[-1])+(j*5))
            break

if len(move_time_r) == 0:
    for i in range(pre.shape[1]):
        move_time_r.append(0)

print(move_time_r)
"""
'''
file_ind_nm = []
if len(move_time) == len(move_time_r):
    error_ = False
    for i in range(len(move_time)):
        if move_time[i] == 0 and move_time_r[i] == 0:
            print("we will not be moving file "+str(i))
            file_ind_nm.append(i)
            error_ = True
        elif move_time[i] == 0 and move_time_r[i] != 0:
            print("Something went wrong and the model was unable to correctly predict when to move the data")
            error_ = True
            break
    min_val = min(move_time)
    max_val = max(move_time)
    if min_val == max_val and error_ == False:
        time.sleep(min_val)
        print("We will be moving all the data in "+str(min_val)+"s")
    elif error_ == False:
        move_dict = {}
        for i in range(len(move_time)):
            if move_time[i] not in move_dict.keys():
                move_dict[move_time[i]] = [i]
            else:
                move_dict[move_time[i]].append(i)
        keys = list(move_dict.keys())
        keys = sorted(keys)
        for i in range(1,len(keys)):
            keys[i] = keys[i] - keys[0]
        for i in keys:
            time.sleep(i)
            print("We will be moving all the data in "+str(i)+"s")

else:
    print("Something went wrong and the model was unable to correctly predict when to move the data")
   '''        
