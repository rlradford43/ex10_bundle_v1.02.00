# Overview

The Ex10 C Development Kit is a host library and examples which communicate with
an Ex10 device to run RFID reader operations. It is a modular design which can
support multiple hardware designs by implementing board-specific components
against a defined board interface. It also provides multiple levels of
abstaction so that users of the library can decide at what level of abstraction
they would prefer to operate at.

# Make Targets

In the following 

 * `list`: List all available board targets. These can be used in the `<board>`
   and `<board>-clean` targets.
 * `<board>`: Compile static libraries and example binaries. Example,`make e710_ref_design`
 * `<board>-clean`: Remove build output for a specific board. Example, `make r807_driver-clean`
 * `all`: Compile static libraries and example binaries for the default board.
 * `clean`: Remove build output for all boards.

Compiled output will be in `build/<board>/`. Each example is compiled into a
separate output file can be found in `build/<board>/examples/`.

# E710 Reference Design Example Usage

The host library includes an example implementation for use on an Impinj E710
Reference Design board.

## Dependencies

The following dependencies are required for the e710_ref_design implementation:

 * GNU Make
 * GCC 8.3.0
 * libgpiod (shared library)
 * pthreads (shared library)

## Build and Run

To compile the examples for the Impinj E710 Reference Design and run the
inventory example:

    make list
    make e710_ref_design
    sudo ./build/e710_ref_design/inventory_example.bin

## Limitations

All examples must be run as root due to the pigpio library dependency.

# Directory structure

 * `board`: Contains header files which describe an interface that must be
            implemented for any specific board design. Also contains
            subdirectories for board-specific implementations that match the
            header file interfaces.
 * `board/e710_ref_design`: Contains source files which implement the board
                            customimzations needed for the Impinj E710
                            Reference Design.
 * `examples`: A number of examples which demonstrate how to use the C Host
               library.
 * `include`: Header files which define the interfaces in the C host library.
 * `src`: Implementation of the C host library.

# General Guidance for Porting to a New Board

Suppose you have a new board design which is called "The Best Reader". To use
this host library with your design, the high-level steps are:

1. Create a new board-specific directory.
    mkdir board/best_reader
2. Implement a Makefile which will compile the sources in `board/best_reader`
   into a static library named `libboard.a`. The top-level Makefile will link
   against this library when compiling the examples.
   * It may be easiest to start with the Makefile in the e710_ref_design:
       ```
       cp board/e710_ref_design/Makefile board/best_reader/
       ```
3. Inside the `board/best_reader` directory, implement all the functions defined
   in the `board/*.h` files.
   * It may be easiest to start with the implementation for the e710_ref_design
     and modify it to your needs. Copy the example implementation into your
     board's directory:
       ```
       cp board/e710_ref_design/*.c board/best_reader/
       ```
4. Run `make best_board` to compile.
5. Run `./build/best_board/examples/inventory_example.bin` to run inventory.

Presently, compiled output is in the form of an ELF file. Changing from an ELF
file to a file compatible with a particular system is left to the user.

# Tracing

LTTng tracepoints are embedded in the SDK and may be optionally compiled in by
setting LTTNG=1 in the Make environment. See the LTTng docs for more
information: https://lttng.org/docs/.

## Dependencies

On a Linux system, the LTTng library can be installed with:
    ```
    sudo apt install liblttng-ust-dev
    ```

In addition, the LTTng command line tools are needed for capture:
    ```
    sudo apt install lttng-tools
    ```

## General Usage

The general flow for using LTTng tracepoints to trace an example is as follows:
    ```
    LTTNG=1 make
    lttng create $session_name
    lttng enable-event --userspace pi_ex10sdk* --loglevel=TRACE_DEBUG
    lttng start
    ./build/e710_ref_design/examples/inventory_example.bin
    lttng stop
    ```

At this point the LTTng tools have captured any tracepoints generated by running
the example and have stored the data in ~/lttng-traces. The tracepoints can be
viewed using any of the following:
 * Command line tool `lttng view`
 * TraceCompass GUI (an Eclipse-based graphical tool)
 * babeltrace library for post-processing in scripts

Finally, to clean up the session and avoid building up too many trace files:
    ```
    lttng destroy
    ```

For more detailed information, refer to the LTTng documentation.

## Microcontroller Support

LTTng tools rely on a Linux platform so they cannot be used as-is on a
microcontroller. There may be other LTTng or CTF (common trace format) tools
which can be utilized on a microcontroller, but are not currently supported.
