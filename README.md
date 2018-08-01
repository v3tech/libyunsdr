## How to Build libyunsdr
### PCIE driver for Y5x0/IQX
1.  ```
    $git clone https://github.com/v3best/riffa
    $cd driver/linux
    $make && sudo make install && sudo ldconfig
    $sudo modprobe riffa
    ```

### libyunsdr for Y2x0/Y3x0/Y4x0/Y5x0/IQX
1.	Compile and install libyunsdr:
    ```
    $ git clone https://github.com/v3best/libyunsdr
    $ cd libyunsdr
    $ mkdir build
    $ cd build
    $ cmake ../
    $ make && sudo make install && sudo ldconfig
    ```

