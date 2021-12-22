from mpl_toolkits.axes_grid1.inset_locator import zoomed_inset_axes, mark_inset
import numpy as np
import matplotlib.pyplot as plt


def plotAccesses(ax, accesses, zoomed, position, name):
    # fig = plt.figure(figsize=(9, 6))

    # ax1 = fig.add_axes([0.1, 0.1, 0.85, 0.85])
    ax.plot(np.floor_divide(accesses.T[0],
                            (2**20)), "s", label="block offset")

    ax.plot(accesses.T[0]/(2**20), ".", color="r",
            markersize=4.0, label="file offset")

    # ax.set_ylabel("Tazer block")
    # ax1 = ax.twinx()
    # ax1.set_ylabel("file offset (MB)")
    # ax1.plot(accesses.T[0]/(2**20), "o", color="r", label="file offset")

    # lines, labels = ax1.get_legend_handles_labels()
    # lines2, labels2 = ax1.get_legend_handles_labels()
    # ax1.legend(lines + lines2, labels + labels2, loc=0)

    # ax2 = fig.add_axes(position)
    # ax2.plot(np.floor_divide(accesses.T[0], (2**20))[0:zoomed], "s")
    # ax2.set_ylabel("Tazer block")
    # ax2 = ax2.twinx()
    # # ax2.set_ylabel("file offset (MB)")
    # ax2.plot(accesses.T[0][0:zoomed]/(2**20), "o", color="r")


L1_size = 64*1024*1024
# for k in [32, 16*1024, 300 * 1024]:
for k in [16*1024, 300 * 1024]:

    # fig, axs = plt.subplots(3, 7, figsize=(18, 9))
    fig, axs = plt.subplots(1, 3, figsize=(18, 9))
    x = 0
    y = 0

    for p in [".25", ".5", 1]:
        # fig, axs = plt.subplots(1, 7, figsize=(18, 9))
        # x = 0
        # y = 0
        # axs[x, 0].set_ylabel("p = "+str(p))
        axs[y].set_title("p = "+str(p))
        for r in [1, 4, 8]:
            for s in [0, int(L1_size/8), int(L1_size/2)]:

                print("5_var2_k_"+str(k)+"_p"+str(p) +
                      "_s"+str(s)+"_r"+str(r)+".txt")
                print(x, y)
                # if s == 0 or r != 1:
                if s == 0 and r == 1:
                    data = np.genfromtxt(
                        "5_var2_k_"+str(k)+"_p"+str(p)+"_s"+str(s)+"_r"+str(r)+".txt", usecols=(1, 2))
                    # if k == 32:
                    # data = data[np.where(data.T[0] < 10*1024*1024)]
                    data = data[:100]

                    # ax = axs[x, y]
                    ax = axs[y]

                    plotAccesses(ax, data, 200, [0.6, 0.2, 0.3, 0.3], "name")
                    # if x == 0:
                    #     if s != 0:
                    #         ax.set_title("r = "+str(r)+", s = " +
                    #                      str(int(s/(1024*1024)))+"MB")
                    #     else:
                    #         ax.set_title("r = "+str(r)+", s = N/A")
                    y += 1
        x += 1
    plt.tight_layout()
    # plt.savefig(name)
    plt.show()


# data = np.genfromtxt("linear.txt", usecols=(1, 2))
# plotAccesses(data, 200, [0.6, 0.2, 0.3, 0.3], "linear.pdf")
# data = np.genfromtxt("sparse_linear.txt", usecols=(1, 2))
# plotAccesses(data, 100, [0.6, 0.2, 0.3, 0.3], "sparse_linear.pdf")
# data = np.genfromtxt("linear_batch_rep.txt", usecols=(1, 2))
# plotAccesses(data, 400, [0.6, 0.2, 0.3, 0.3], "batch_linear.pdf")
# data = np.genfromtxt("1_batch_4_reps.txt", usecols=(1, 2))
# plotAccesses(data, 400, [0.7, 0.2, 0.2, 0.2], "linear_repeat.pdf")
# data = np.genfromtxt("4_batch_4_reps.txt", usecols=(1, 2))
# plotAccesses(data, 400, [0.6, 0.2, 0.3, 0.3], "segmented_linear_repeat.pdf")
# data = np.genfromtxt("strided_batch_global_rep.txt", usecols=(1, 2))
# plotAccesses(data, 200, [0.6, 0.2, 0.3, 0.3], "6.pdf")
# data = np.genfromtxt("strided_batch_batch_rep.txt", usecols=(1, 2))
# plotAccesses(data, 200, [0.6, 0.2, 0.3, 0.3], "7.pdf")
# data = np.genfromtxt("strided_batch_rep.txt", usecols=(1, 2))
# plotAccesses(data, 250, [0.6, 0.2, 0.3, 0.3], "segmented_batched_repeat.pdf")
