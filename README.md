# WISE-1570-PSM


## Introduction

This example is demonstrated for power saving on WISE-1570.


## Deployment

Please follow the below steps to setup for development:

1. `git clone https://github.com/ADVANTECH-Corp/WISE-1570-PSM.git`

1. `cd WISE-1570-PSM`

1. `mbed config root .`

1. `mbed deploy`

1. `cd mbed-os; git apply ../patch/*`


## Set the APN

Set marco "MBED_CONF_APP_APN" to the APN provided by telecom in the main.cpp.

```cpp

    # define MBED_CONF_APP_APN          "your_apn_string"

```

## Select TCP or UDP

Modify the file `mbed_app.json` to choose which socket type the application should use.

```json

     "sock-type": "TCP",

```

## Compilation

Goes into WISE-1570-PSM directory and runs the below script to compile the example.

`./compile.sh`


