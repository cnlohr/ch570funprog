# ch570funprog

CH570-based programmer for CH32 microcontrollers.

<img width="1883" height="935" alt="Image" src="https://github.com/user-attachments/assets/37eef7a6-dd0e-41a3-9c07-9184f502477f" />

### General Game Plan

- [x] Be a cheap, minichlink-compatible programmer programming over USB
- [ ] Test on more boards (And tune SWD)
- [ ] Be a iSLER gateway.
- [ ] Enable flashing of slave processors over iSLER.
- [ ] Enable self-program over iSLER


### Some Developer Notes

 * To force-enter SWD mode, hold D+ high.
 * You will need to `minichlink -P` to enable writing to the 570.

