# Test fixtures

Messages that another implementation wrote, so a libe3 change cannot break that peer without a test failing.

## aerial/e3_message_examples.json

NVIDIA's published example messages for every E3AP message type their agent speaks, copied unchanged from
`dapps/docs/e3_message_examples.json` in [NVIDIA/aerial-sample-apps](https://github.com/NVIDIA/aerial-sample-apps)
at commit `99b10eb5c2a9df54b71df6b21d2a578899cfbede` (dapps v1.1.0). It is Apache-2.0, as is libe3, and keeps
its own copyright notice in the `_copyright` key. `tests/test_interop_aerial.cpp` decodes every example.

To move to a newer Aerial, replace the file with the one from the new commit, update the commit above and in
`docs/interop.md`, and let the test say what changed.
