# OptiScaler AMD PreSR Multipass v2.18

UI: Neural lighting no longer has the experimental suffix. Removed the NR slider-release explanatory line.

Neural lighting strength, global structure, character structure and pass count now stage edits until slider release or text-edit completion, like NR resolution. Rendering continues with the committed values during a drag, avoiding repeated neural history resets/rebuilds for every intermediate mouse position. Changes apply in-session without restart. Checkbox changes remain immediate. Optional spatial controls remain unchanged.

This addresses repeated configuration updates, not all possible capture stalls or flicker. Release compilation completed. Mouse/keyboard interaction and visual results still require in-game testing. Prior unresolved crash/quality limitations remain; renaming a control does not establish NVIDIA parity.

NR resolution controls model dimensions relative to the current SR input. Pre-SR specifies processing placement. The AMD backend uses both together; it does not expose an implemented post-SR or post-RR alternative.

Install: extract all files, close game and run INSTALAR_AMD.bat. Keep matching runtimes and all dependencies. v2.16 remains the earlier reference, v2.17 adds neural lighting. This release retains the latter's controls/defaults and reduced-scale highlight protection.
