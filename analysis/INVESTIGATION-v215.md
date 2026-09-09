# v2.15 investigation and containment

Native decompilation (analysis/inference-decompile.txt around 731-741) sets global structure separately from the semantic character channel. SkinStructure=0 does not zero the global structure channel. UseAutoMask at RVA 0x76e40 gates the separate character channel. The bridge now explicitly enables it and the UI calls the control AMD character structure. This is not pixel-perfect skin isolation. Achieving that requires an accessible validated mask and separate compositing, which are not implemented.

NR scale is latched on the first game bridge evaluation and cannot rebuild the model via slider edits during that session. Save settings and restart to apply. This contains the reported live-scale trigger; game resolution changes and the underlying native capture stall remain possible. The log associates a ~2 second capture wait and watchdog event with a subsequent 0x80000003 exception in Cyberpunk's executable, not a diagnosed pointer fault in the slider. No root-cause crash fix is claimed.

The v2.13 strength loss is caused by contrast-dependent confidence attenuation and residual clamping. Retained unchanged: simply lifting these limits risks the confirmed prior light flicker. 100% avoids this reduced-scale attenuation. Restoring equal strength and temporal stability at 25% remains unimplemented, not silently declared fixed.

Validation: native backend smoke test at 50% and forced timeout passed. No semantic skin reference images or in-game crash reproduction were available for validation. This release is a containment test, not completion of all three issues.
