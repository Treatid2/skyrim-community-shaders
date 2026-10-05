# Native upscaling status correlation

DevBench `communityshaders.renderscale` extension revision 11 attaches
`upscalingSnapshot` to status only after successful native `csx.upscaling`
`GetSnapshot` calls before and after the physical status read report the same
nonzero state revision and capability revision. Serialization uses the public
API adapter's existing snapshot serializer. The physical controller revision
remains a separate authority.

`upscalingCorrelation` records the authority, bracket method, success/failure
statuses and nullable observed revisions. Failed or changed brackets produce
a null snapshot and no retry. A separately read API snapshot can be compared
using its own state revision; consumers must retain profile and transition
guards. Service inspection refreshes its derived journal/snapshot through the
existing bounded main-thread dispatcher; it submits no renderer mutation.

This stable API bracket is not an atomic engine/GPU snapshot. Independent
render counters can advance, and an unavailable/busy service cannot establish
correlation. Render-scale qualification and runtime validation remain separate.
