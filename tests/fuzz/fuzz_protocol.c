#include "boreline/boreline.h"

/* Fuzz target for the protocol parser.
 *
 * Build with clang and libFuzzer:
 *   clang -fsanitize=fuzzer,address,undefined -I include \
 *         src/common/protocol.c tests/fuzz/fuzz_protocol.c -o fuzz_protocol
 *
 * The target feeds arbitrary bytes into the incremental parser,
 * exercising malformed frames, fragmented/coalesced input, integer
 * overflow checks and header validation without crashing.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size == 0)
        return 0;

    boreline_parser_t *p = boreline_parser_create();
    if (p == NULL)
        return 0;

    /* Fragment input randomly to exercise incremental parsing */
    size_t off = 0;
    while (off < size) {
        /* Deterministic pseudo-random chunk size based on input */
        size_t chunk = 1 + (size_t)(data[off % size] % 17);
        if (chunk > size - off)
            chunk = size - off;

        boreline_parser_feed(p, data + off, chunk);

        boreline_frame_header_t hdr;
        const uint8_t *payload = NULL;
        int rc;
        while ((rc = boreline_parser_next(p, &hdr, &payload)) != BORELINE_ERR_AGAIN) {
            if (rc != BORELINE_OK)
                break;
        }
        off += chunk;
    }

    /* Drain any remaining buffered frames */
    boreline_frame_header_t hdr;
    const uint8_t *payload = NULL;
    int rc;
    while ((rc = boreline_parser_next(p, &hdr, &payload)) != BORELINE_ERR_AGAIN) {
        if (rc != BORELINE_OK)
            break;
    }

    boreline_parser_destroy(p);
    return 0;
}