# EarPods inline remote: 2007 Classic investigation

Status: local, read-only reverse engineering. No iPod probes, register writes,
new driver installation, or NOR changes were performed for this investigation.

## Hardware gate

The user's original identity is MB029, a 2007 Classic. Changing its back case,
storage and OS does not add a remote-detection controller to its motherboard.
The current [Rockbox Mikey driver](https://github.com/Rockbox/rockbox/blob/master/firmware/target/arm/s5l8702/ipod6g/mikey-6g.c)
(reviewed 2026-10-11) explicitly treats the 2007 model as lacking that chip.
It distinguishes this from tested 120GB units, also sometimes called “6G.”
This is stronger evidence than assuming every Classic shares remote hardware.
The board itself has not been opened or electrically measured here.

Mikey detects inline-button electrical signaling and exposes it over I2C.
Software click counting cannot recover presses which the board never detects.
Consequently a software-only double/triple-click driver for the original 2007
board is not established as feasible, and no pretend support toggle is added.
Missing recording support alone would not prove this; the model-specific
driver evidence is the relevant distinction.

## Pinned Apple 2.0.5 evidence

Inspected only the already-preserved local plaintext OSOS, SHA-256
`660eb6ff5f498bb1a75e85f1f9921bf0c753525a8bca9b4fa47ed260e5ad0f1e`.
Addresses are observations for that image, not a portable public ABI.

| Address | Observed role |
| --- | --- |
| `0x08389f50` | Mikey task/state-event dispatch. |
| `0x08079718` | Accessory detection through register reads, including 4 and 6. |
| `0x08079d48` | Button-event processing involving registers 5 and 6. |
| `0x08364da0` | Serialized register read: lock, register-address transaction, read, unlock; only stores the result on success. |
| `0x083625b8`, `0x08362384` | Write/read transactions used with device address `0x39`. |
| `0x081c3a1c` | Existing action dispatch includes center single/double/triple and volume handlers. |

Apple's 7-bit I2C address `0x39` and Rockbox's 8-bit `0x72` refer to the same
address convention, not two different chips. Existing Apple gesture handlers
prove that code exists for compatible hardware, not that the 2007 jack supplies
events. Porting a polling driver would also require careful accessory, recording,
debounce and phantom-volume handling; it must not compete with Apple's driver.

No electrical workaround or random I2C scan is warranted on the user's only
iPod. A future compatible-board or external-accessory project would be a
separate hardware decision. This does not prevent fixing the iPod's physical
clickwheel controls on the lyrics screen.
