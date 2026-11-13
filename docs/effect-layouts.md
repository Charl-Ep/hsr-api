# Effect layouts

`PresentationContext::CreateEffectLayouts` returns a prepared, engine-thread
service for existing owned Mahjong-layout effect receipts.

`EffectLayouts::Execute(reference, command)` supports:

| Action | Inputs | Result |
|---|---|---|
| Hide | Source receipt | Writes the layout's hidden input. |
| Show | Source receipt, fade | Shows the layout through its normal Tick. |
| Perform | Source receipt, type 0–3 | Calls AddImmediately, RemoveImmediately, SwitchPerform or MaxHighlightPerform. |
| Transfer | Distinct source/target receipts, fade | Copies the source layout into the target, clears the source, and preserves official inheritance links. |

Transfer requires the same explicit actor receiver. An inactive source returns
`NoMatch`. Loading requests return `NotLoaded`; absent plugins return
`Unsupported`. Writes require current owned receipts and successful native calls.
A failed transfer may be partial.

Keep both effect requests alive through transfer. Their owners handle creation,
updates and removal. Cancellation uses normal owned-request removal; reversing
a transfer requires an explicit command. `Close` releases the service's managed
references.
