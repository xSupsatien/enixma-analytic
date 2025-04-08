*Copyright (C) 2025, Enixma Global, Hamburg, Germany. All Rights Reserved.*

# Enixma Analytic

## Overview

The traffic counting and event detection application is based on Axis cameras equipped with Edge TPU but can also be easily configured to run on CPU or ARTPEC-8 cameras (DLPU), using a pre-trained model [MobileNet SSD v2 (COCO)] to detect the position of 7 different types of vehicles, cones and people.

## Prerequisites

- Axis camera equipped with CPU, an [Edge TPU](https://coral.ai/docs/edgetpu/faq/) or DLPU (for ARTPEC-8)
- [Docker](https://docs.docker.com/get-docker/)

## Quickstart

The following instructions can be executed to simply run the example.

1. Compile the ACAP application:

    ```sh
    docker build --build-arg ARCH=<ARCH> --build-arg CHIP=<CHIP>--tag enixma_analytic:1.0 .
    docker cp $(docker create enixma_analytic:1.0):/opt/app ./build
    ```

    where the values are found:
    - \<CHIP\> is the chip type. Supported values are *artpec8*, *cpu* and *edgetpu*.
    - \<ARCH\> is the architecture. Supported values are armv7hf (default) and aarch64

2. Find the ACAP application `.eap` file

    ```sh
    build/Enixma_Analytic_1_0_0_<ARCH>.eap
    ```

3. Install and start the ACAP application on your camera through the camera web GUI

4. SSH to the camera

5. View its log to see the ACAP application output:

    ```sh
    tail -f /var/volatile/log/info.log | grep object_detection
    ```

## License

**[Apache License 2.0](../LICENSE)**
