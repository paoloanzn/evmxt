#include "abi.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// Encoding happens in two passes:
//
// 1. Validate the descriptor tree and calculate the exact encoded size.
// 2. Write heads and tails directly into the final contiguous buffer.
//
// The encoder therefore needs no per-value heap allocations or temporary
// layout tables. Scalar descriptors already contain their normalized ABI word.

// This flag preserves the element type of an empty fixed array such as string[0].
#define ABI_F_DYNAMIC_ELEMENT 1u

typedef struct {
    size_t encoded_size;
    bool is_dynamic;
} abi_layout;

static abi_value make_invalid_value(void)
{
    // Kind zero is invalid, so constructor errors are reported during encoding.
    abi_value value = {0};
    return value;
}

static abi_value make_zero_word(void)
{
    // Zero initialization supplies scalar left or right padding for free.
    abi_value value = {0};
    value.kind = ABI_VALUE_WORD;
    return value;
}

static abi_value make_list_value(
    abi_value_kind kind,
    const abi_value *values,
    size_t count,
    uint8_t flags)
{
    abi_value value = {0};

    // Containers borrow the descriptor array instead of copying it.
    if (count != 0 && values == NULL) {
        return value;
    }

    value.kind = (uint8_t)kind;
    value.flags = flags;
    value.as.list.values = values;
    value.as.list.count = count;
    return value;
}

abi_value abi_word(const uint8_t word[32])
{
    abi_value value = make_zero_word();

    if (word == NULL) {
        return make_invalid_value();
    }

    memcpy(value.as.word, word, 32);
    return value;
}

abi_value abi_uint_be(const void *data, size_t len)
{
    abi_value value = make_zero_word();

    if (len > 32 || (len != 0 && data == NULL)) {
        return make_invalid_value();
    }

    // ABI integers are right-aligned and zero-extended to one 32-byte word.
    if (len != 0) {
        memcpy(value.as.word + 32 - len, data, len);
    }

    return value;
}

abi_value abi_int_be(const void *data, size_t len)
{
    const uint8_t *bytes = data;
    abi_value value;

    if (len == 0 || len > 32 || bytes == NULL) {
        return make_invalid_value();
    }

    value = make_zero_word();

    // The sign bit becomes either 0x00 or 0xff without a data-dependent branch.
    memset(value.as.word, (uint8_t)-(bytes[0] >> 7), 32 - len);
    memcpy(value.as.word + 32 - len, bytes, len);
    return value;
}

abi_value abi_ufixed_be(const void *scaled_value, size_t len)
{
    return abi_uint_be(scaled_value, len);
}

abi_value abi_fixed_be(const void *scaled_value, size_t len)
{
    return abi_int_be(scaled_value, len);
}

abi_value abi_uint64(uint64_t number)
{
    abi_value value = make_zero_word();

    // Shift into ABI big-endian order independently of the host byte order.
    for (unsigned i = 0; i < 8; ++i) {
        value.as.word[31 - i] = (uint8_t)(number >> (i << 3));
    }

    return value;
}

abi_value abi_int64(int64_t number)
{
    abi_value value = make_zero_word();
    uint64_t bits = (uint64_t)number;

    // Initialize the upper 24 bytes with the sign extension.
    memset(value.as.word, (uint8_t)-(number < 0), 24);

    for (unsigned i = 0; i < 8; ++i) {
        value.as.word[31 - i] = (uint8_t)(bits >> (i << 3));
    }

    return value;
}

abi_value abi_bool(int boolean)
{
    return abi_uint64(boolean != 0);
}

abi_value abi_address(const uint8_t address[20])
{
    if (address == NULL) {
        return make_invalid_value();
    }

    return abi_uint_be(address, 20);
}

abi_value abi_fixed_bytes(const void *data, size_t len)
{
    abi_value value = make_zero_word();

    if (len == 0 || len > 32 || data == NULL) {
        return make_invalid_value();
    }

    // bytes<M> is left-aligned. make_zero_word() supplied its trailing padding.
    memcpy(value.as.word, data, len);
    return value;
}

abi_value abi_function(const uint8_t address_and_selector[24])
{
    if (address_and_selector == NULL) {
        return make_invalid_value();
    }

    // The ABI specification encodes function values exactly like bytes24.
    return abi_fixed_bytes(address_and_selector, 24);
}

abi_value abi_bytes(const void *data, size_t len)
{
    abi_value value = {0};

    if (len != 0 && data == NULL) {
        return value;
    }

    value.kind = ABI_VALUE_BYTES;
    value.as.bytes.data = data;
    value.as.bytes.len = len;
    return value;
}

abi_value abi_string_n(const char *utf8, size_t len)
{
    return abi_bytes(utf8, len);
}

abi_value abi_string(const char *utf8)
{
    if (utf8 == NULL) {
        return make_invalid_value();
    }

    return abi_bytes(utf8, strlen(utf8));
}

abi_value abi_array(const abi_value *values, size_t count)
{
    return make_list_value(ABI_VALUE_ARRAY, values, count, 0);
}

abi_value abi_fixed_array(const abi_value *values, size_t count)
{
    return make_list_value(ABI_VALUE_FIXED_ARRAY, values, count, 0);
}

abi_value abi_fixed_array_dynamic(const abi_value *values, size_t count)
{
    return make_list_value(
        ABI_VALUE_FIXED_ARRAY,
        values,
        count,
        ABI_F_DYNAMIC_ELEMENT);
}

abi_value abi_tuple(const abi_value *values, size_t count)
{
    return make_list_value(ABI_VALUE_TUPLE, values, count, 0);
}

static bool size_add_overflows(size_t left, size_t right, size_t *sum)
{
    // size_t is unsigned, so addition wraps in a defined way on overflow.
    *sum = left + right;
    return *sum < left;
}

static abi_status inspect_value(
    const abi_value *value,
    unsigned depth,
    abi_layout *layout);

static abi_status inspect_sequence(
    const abi_value *values,
    size_t count,
    unsigned depth,
    bool is_tuple,
    abi_layout *layout)
{
    size_t head_size = 0;
    size_t tail_size = 0;
    bool any_dynamic = false;
    bool have_first_element = false;
    bool first_element_is_dynamic = false;

    for (size_t i = 0; i < count; ++i) {
        abi_layout child;
        abi_status status = inspect_value(values + i, depth + 1, &child);

        if (status != ABI_OK) {
            return status;
        }

        // Tuples may mix types. Array elements must agree on whether their type
        // is static or dynamic, which catches the most dangerous schema mismatch.
        if (!is_tuple &&
            have_first_element &&
            first_element_is_dynamic != child.is_dynamic) {
            return ABI_INVALID_ARGUMENT;
        }

        have_first_element = true;
        first_element_is_dynamic = child.is_dynamic;
        any_dynamic |= child.is_dynamic;

        // Static values are written directly into the head. Dynamic values use
        // one offset word in the head and place their full encoding in the tail.
        size_t child_head_size = child.is_dynamic ? 32u : child.encoded_size;

        if (size_add_overflows(head_size, child_head_size, &head_size)) {
            return ABI_OVERFLOW;
        }

        if (child.is_dynamic &&
            size_add_overflows(tail_size, child.encoded_size, &tail_size)) {
            return ABI_OVERFLOW;
        }
    }

    if (size_add_overflows(head_size, tail_size, &layout->encoded_size)) {
        return ABI_OVERFLOW;
    }

    layout->is_dynamic = any_dynamic;
    return ABI_OK;
}

static abi_status inspect_value(
    const abi_value *value,
    unsigned depth,
    abi_layout *layout)
{
    if (value == NULL) {
        return ABI_INVALID_ARGUMENT;
    }

    if (depth > ABI_MAX_DEPTH) {
        return ABI_DEPTH_EXCEEDED;
    }

    switch (value->kind) {
    case ABI_VALUE_WORD:
        layout->encoded_size = 32;
        layout->is_dynamic = false;
        return ABI_OK;

    case ABI_VALUE_BYTES:
        if (value->as.bytes.len != 0 && value->as.bytes.data == NULL) {
            return ABI_INVALID_ARGUMENT;
        }

        if (value->as.bytes.len > SIZE_MAX - 31u) {
            return ABI_OVERFLOW;
        }

        // Dynamic bytes contain a length word followed by data rounded up to a
        // complete word. The mask is equivalent to division and multiplication.
        layout->encoded_size =
            (value->as.bytes.len + 31u) & ~(size_t)31u;

        if (size_add_overflows(
                layout->encoded_size,
                32u,
                &layout->encoded_size)) {
            return ABI_OVERFLOW;
        }

        layout->is_dynamic = true;
        return ABI_OK;

    case ABI_VALUE_ARRAY: {
        if (value->as.list.count != 0 && value->as.list.values == NULL) {
            return ABI_INVALID_ARGUMENT;
        }

        abi_status status = inspect_sequence(
            value->as.list.values,
            value->as.list.count,
            depth,
            false,
            layout);

        if (status != ABI_OK) {
            return status;
        }

        // A dynamic array adds its element count before the tuple-like contents.
        if (size_add_overflows(
                layout->encoded_size,
                32u,
                &layout->encoded_size)) {
            return ABI_OVERFLOW;
        }

        layout->is_dynamic = true;
        return ABI_OK;
    }

    case ABI_VALUE_FIXED_ARRAY: {
        if (value->as.list.count != 0 && value->as.list.values == NULL) {
            return ABI_INVALID_ARGUMENT;
        }

        abi_status status = inspect_sequence(
            value->as.list.values,
            value->as.list.count,
            depth,
            false,
            layout);

        if (status != ABI_OK) {
            return status;
        }

        // T[0] has no child descriptor from which to infer whether T is dynamic.
        if (value->as.list.count == 0) {
            layout->is_dynamic =
                (value->flags & ABI_F_DYNAMIC_ELEMENT) != 0;
            return ABI_OK;
        }

        // The explicit dynamic-element constructor is invalid for static values.
        if ((value->flags & ABI_F_DYNAMIC_ELEMENT) != 0 &&
            !layout->is_dynamic) {
            return ABI_INVALID_ARGUMENT;
        }

        return ABI_OK;
    }

    case ABI_VALUE_TUPLE:
        if (value->as.list.count != 0 && value->as.list.values == NULL) {
            return ABI_INVALID_ARGUMENT;
        }

        return inspect_sequence(
            value->as.list.values,
            value->as.list.count,
            depth,
            true,
            layout);

    default:
        return ABI_INVALID_ARGUMENT;
    }
}

static void write_size_word(uint8_t out[32], size_t number)
{
    // The output is already zeroed, so only native size_t bytes can be nonzero.
    for (unsigned i = 0; i < sizeof number; ++i) {
        out[31 - i] = (uint8_t)(number >> (i << 3));
    }
}

static void write_value(
    const abi_value *value,
    uint8_t *out,
    unsigned depth);

static void write_sequence(
    const abi_value *values,
    size_t count,
    uint8_t *out,
    unsigned depth)
{
    size_t head_size = 0;

    // Reinspect each child to find the tail boundary. The tree was already fully
    // validated, so these calls cannot fail and avoid allocating a layout table.
    for (size_t i = 0; i < count; ++i) {
        abi_layout child;
        (void)inspect_value(values + i, depth + 1, &child);
        head_size += child.is_dynamic ? 32u : child.encoded_size;
    }

    uint8_t *head = out;
    uint8_t *tail = out + head_size;

    for (size_t i = 0; i < count; ++i) {
        abi_layout child;
        (void)inspect_value(values + i, depth + 1, &child);

        if (child.is_dynamic) {
            // Offsets are relative to the start of this sequence. For a dynamic
            // array, the caller deliberately passes the byte after its count word.
            size_t tail_offset = (size_t)(tail - out);
            write_size_word(head, tail_offset);
            write_value(values + i, tail, depth + 1);

            head += 32;
            tail += child.encoded_size;
        } else {
            write_value(values + i, head, depth + 1);
            head += child.encoded_size;
        }
    }
}

static void write_value(
    const abi_value *value,
    uint8_t *out,
    unsigned depth)
{
    switch (value->kind) {
    case ABI_VALUE_WORD:
        memcpy(out, value->as.word, 32);
        break;

    case ABI_VALUE_BYTES:
        // The initial output clear already supplied the right-side zero padding.
        write_size_word(out, value->as.bytes.len);
        if (value->as.bytes.len != 0) {
            memcpy(out + 32, value->as.bytes.data, value->as.bytes.len);
        }
        break;

    case ABI_VALUE_ARRAY:
        write_size_word(out, value->as.list.count);

        // Array element offsets are relative to the contents after this count.
        write_sequence(
            value->as.list.values,
            value->as.list.count,
            out + 32,
            depth);
        break;

    case ABI_VALUE_FIXED_ARRAY:
    case ABI_VALUE_TUPLE:
        write_sequence(
            value->as.list.values,
            value->as.list.count,
            out,
            depth);
        break;
    }
}

abi_status abi_encoded_size(
    const abi_value *values,
    size_t count,
    size_t *size)
{
    abi_layout root;

    if (size == NULL || (count != 0 && values == NULL)) {
        return ABI_INVALID_ARGUMENT;
    }

    // Function arguments and return values are encoded as one implicit tuple.
    abi_status status = inspect_sequence(values, count, 0, true, &root);
    if (status == ABI_OK) {
        *size = root.encoded_size;
    }

    return status;
}

static abi_status encode_to_buffer(
    bool prepend_selector,
    const uint8_t selector[4],
    const abi_value *values,
    size_t count,
    uint8_t *out,
    size_t capacity,
    size_t *written)
{
    if (prepend_selector && selector == NULL) {
        return ABI_INVALID_ARGUMENT;
    }

    size_t arguments_size;
    abi_status status = abi_encoded_size(values, count, &arguments_size);
    if (status != ABI_OK) {
        return status;
    }

    size_t selector_size = prepend_selector ? 4u : 0u;
    size_t total_size;
    if (size_add_overflows(arguments_size, selector_size, &total_size)) {
        return ABI_OVERFLOW;
    }

    if (written != NULL) {
        *written = total_size;
    }

    if (capacity < total_size) {
        return ABI_NO_SPACE;
    }

    if (total_size != 0 && out == NULL) {
        return ABI_INVALID_ARGUMENT;
    }

    if (total_size == 0) {
        return ABI_OK;
    }

    // One sequential clear supplies integer high bytes and bytes/string padding.
    memset(out, 0, total_size);

    if (prepend_selector) {
        memcpy(out, selector, 4);
    }

    // Selector bytes are not part of the base used by ABI argument offsets.
    write_sequence(values, count, out + selector_size, 0);
    return ABI_OK;
}

abi_status abi_encode(
    const abi_value *values,
    size_t count,
    uint8_t *out,
    size_t capacity,
    size_t *written)
{
    return encode_to_buffer(
        false,
        NULL,
        values,
        count,
        out,
        capacity,
        written);
}

abi_status abi_encode_call(
    const uint8_t selector[4],
    const abi_value *values,
    size_t count,
    uint8_t *out,
    size_t capacity,
    size_t *written)
{
    return encode_to_buffer(
        true,
        selector,
        values,
        count,
        out,
        capacity,
        written);
}

static abi_status allocate_and_encode(
    bool prepend_selector,
    const uint8_t selector[4],
    const abi_value *values,
    size_t count,
    abi_buffer *out)
{
    if (out == NULL || (prepend_selector && selector == NULL)) {
        return ABI_INVALID_ARGUMENT;
    }

    out->data = NULL;
    out->len = 0;

    size_t arguments_size;
    abi_status status = abi_encoded_size(values, count, &arguments_size);
    if (status != ABI_OK) {
        return status;
    }

    size_t selector_size = prepend_selector ? 4u : 0u;
    size_t total_size;
    if (size_add_overflows(arguments_size, selector_size, &total_size)) {
        return ABI_OVERFLOW;
    }

    if (total_size == 0) {
        return ABI_OK;
    }

    // This is the encoder's only allocation. calloc also supplies all padding.
    out->data = calloc(1, total_size);
    if (out->data == NULL) {
        return ABI_NO_MEMORY;
    }
    out->len = total_size;

    if (prepend_selector) {
        memcpy(out->data, selector, 4);
    }

    write_sequence(values, count, out->data + selector_size, 0);
    return ABI_OK;
}

abi_status abi_encode_alloc(
    const abi_value *values,
    size_t count,
    abi_buffer *out)
{
    return allocate_and_encode(false, NULL, values, count, out);
}

abi_status abi_encode_call_alloc(
    const uint8_t selector[4],
    const abi_value *values,
    size_t count,
    abi_buffer *out)
{
    return allocate_and_encode(true, selector, values, count, out);
}

void abi_buffer_free(abi_buffer *buffer)
{
    if (buffer == NULL) {
        return;
    }

    free(buffer->data);
    buffer->data = NULL;
    buffer->len = 0;
}

const char *abi_status_string(abi_status status)
{
    static const char *const messages[] = {
        "ok",
        "invalid argument",
        "size overflow",
        "output buffer too small",
        "out of memory",
        "maximum nesting depth exceeded"
    };

    size_t message_count = sizeof messages / sizeof messages[0];
    if ((unsigned)status >= message_count) {
        return "unknown ABI error";
    }

    return messages[status];
}
