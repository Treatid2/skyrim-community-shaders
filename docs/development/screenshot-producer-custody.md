# Screenshot producer custody

Frame and final-manifest receipts retain the byte size and SHA-256 returned
by `ScreenshotStorage::CommittedFile::WriteAtomically`. The producer computes
both values from the committed file handle after checking publication identity.
The encoder and manifest worker carry that result through receipt publication.
They do not reopen the destination pathname to derive authoritative metadata.

The receipt describes the producer's committed bytes even if another writer
subsequently replaces the pathname. Its path identifies the original publication
destination; it does not certify that the pathname still contains those bytes.
Directory confinement and retained sequence-directory ownership still apply.

A successful frame publication requires producer metadata with a complete
SHA-256 digest. Missing or invalid metadata leaves publication unresolved and
records that the worker claimed a commit; it cannot publish an authoritative
successful artifact. Encoder and manifest commit failures remain failures.

The worker regression links the real storage primitive and extracts the
production frame terminal publication and manifest worker. Controlled barriers
replace the destination after the producer returns and before receipt
application. Both frame and manifest receipts retain the original known size
and digest; unchanged-path controls and missing/invalid-metadata cases also run.
Directory preparation and journal failure injection remain fixture boundaries.
These host tests do not exercise GPU readback or WIC image encoding.
