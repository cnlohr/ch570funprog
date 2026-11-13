# ch570funprog

Project Status: It works, in a pinch, but hardware and software is still evolving.

CH570-based programmer for CH32 microcontrollers.

<img width="1883" height="935" alt="Image" src="https://github.com/user-attachments/assets/37eef7a6-dd0e-41a3-9c07-9184f502477f" />

## Firmware

Firmware uses common funprog logic that now lives in [ch32fun/minichlink/funprog](ch32fun/minichlink/funprog). Any updates to functionality and protocol should be updated via ch32fun submodule. Consult [README](ch32fun/minichlink/funprog/README.md) about available configurations.

Firmware creates CDC ACM device on the connected host that can be used either for debugging purposes or as CDC-UART adapter. In the latter case it will use ``SWD`` pin as ``UART RX`` and ``SWC`` pin as ``UART TX``, for the first version of the hardware. On the second revision, there are dedicated UART pins on the header, and they will be used instead.

To switch UART on and off (and toggle functionality of SWD pins on the first revision boards) you need to send a Vendor Command using minichlink like this:

``./minichlink -X u1`` (1/0 - turn UART on or off)

or

``./minichlink -X U1`` (turn UART on and save this setting to the flash)

To enable debug prints in this firmware set ``DEBUG_PRINT`` in [funprog_config.h](firmware/funprog_config.h) to 1. Also define hardware version there, this will affect UART related functions.

### Hardware Note

* You must connect an external 1.5k to 10k pull-up resistor between SWIO and VDD

### General Game Plan

- [x] Be a cheap, minichlink-compatible programmer programming over USB
- [x] Test on ch32v00x for SWIO mode (and tune)
- [ ] Test on more boards (And tune SWD)
- [ ] Be a iSLER gateway.
- [ ] Enable flashing of slave processors over iSLER.
- [ ] Enable self-program over iSLER

### Some Developer Notes

* To force-enter ISP (bootloader) mode, hold D+ high.
* You will need to `minichlink -P` to enable writing to the 570.
