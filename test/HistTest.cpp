#include <iostream>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include "Histogram.h"

int main(int argc, char **argv) {
    Histogram test(10, true);
    for(int i=0; i<100; i++) {
        test.addData((double)(i%27), 1);
    }
    test.printBins();
    std::cout << -1 << " " << test.getValue((double)-1) << std::endl;
    for(int i=0; i<27; i++) {
        std::cout << i << " " << test.getValue((double)i) << std::endl;
    }
    std::cout << 27 << " " << test.getValue((double)27) << std::endl;
    return 0;
}