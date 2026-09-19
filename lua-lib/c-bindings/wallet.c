/* Native keychain access only: no files, subprocesses, or storage fallback. */
#include <lauxlib.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <secp256k1.h>
#include <string.h>

#include "wallet.h"

// Encode derived public data in the Lua API's lowercase, 0x-prefixed format.
static void push_hex(lua_State *L, const unsigned char *bytes, size_t length)
{
    static const char digits[] = "0123456789abcdef";
    char encoded[131] = "0x";

    for (size_t i = 0; i < length; ++i) {
        encoded[2 + i * 2] = digits[bytes[i] >> 4];
        encoded[3 + i * 2] = digits[bytes[i] & 15];
    }

    lua_pushlstring(L, encoded, 2 + length * 2);
}

// Return the public key and address as hex strings on the Lua stack.
static int derive(lua_State *L)
{
    const char *key = luaL_checkstring(L, 1);
    unsigned char bytes[32], public[65], hash[32];
    size_t length = sizeof(public);
    size_t key_length = 0;
    size_t hash_length = 0;
    secp256k1_pubkey pubkey;
    secp256k1_context *ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);

    // Skip the uncompressed public key's 0x04 prefix when hashing.
    int ok = strlen(key) == 66 && strncmp(key, "0x", 2) == 0
        && OPENSSL_hexstr2buf_ex(bytes, sizeof(bytes), &key_length, key + 2, '\0')
        && key_length == sizeof(bytes)
        && secp256k1_ec_pubkey_create(ctx, &pubkey, bytes)
        && secp256k1_ec_pubkey_serialize(ctx, public, &length, &pubkey,
                                       SECP256K1_EC_UNCOMPRESSED)
        && EVP_Q_digest(NULL, "KECCAK-256", NULL, public + 1, 64, hash, &hash_length)
        && hash_length == sizeof(hash);

    OPENSSL_cleanse(bytes, sizeof(bytes));
    secp256k1_context_destroy(ctx);

    if (!ok) {
        return luaL_error(L, "Invalid private key or unavailable crypto provider");
    }

    push_hex(L, public + 1, 64);

    // The address is the last 20 bytes of the hashed public key.
    push_hex(L, hash + 12, 20);

    return 2;
}

enum {
    STORE,
    LOAD,
    DELETE,
    LIST
};

#ifdef __APPLE__
#include <Security/Security.h>

static void set_string(CFMutableDictionaryRef dict, CFStringRef key, const char *value)
{
    CFStringRef string = CFStringCreateWithCString(NULL, value, kCFStringEncodingUTF8);

    CFDictionarySetValue(dict, key, string);
    CFRelease(string);
}

static int keychain(lua_State *L, int action, const char *address, const char *payload)
{
    // Restrict all queries to this application's generic password entries.
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(query, kSecAttrService, CFSTR("org.evmxt.wallet"));

    if (address) {
        set_string(query, kSecAttrAccount, address);
    }

    CFTypeRef result = NULL;
    OSStatus status;

    if (action == STORE) {
        CFDataRef data = CFDataCreate(NULL, (const UInt8 *)payload, strlen(payload));
        CFMutableDictionaryRef update = CFDictionaryCreateMutable(NULL, 0,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(update, kSecValueData, data);

        // Replace the existing secret, or create an entry if none exists.
        status = SecItemUpdate(query, update);
        if (status == errSecItemNotFound) {
            CFDictionarySetValue(query, kSecValueData, data);
            status = SecItemAdd(query, NULL);
        }

        CFRelease(update);
        CFRelease(data);
        lua_pushboolean(L, status == errSecSuccess);
    } else if (action == DELETE) {
        status = SecItemDelete(query);
        lua_pushboolean(L, status == errSecSuccess);
    } else {
        // LIST returns account attributes; LOAD returns one secret payload.
        if (action == LIST) {
            CFDictionarySetValue(query, kSecReturnAttributes, kCFBooleanTrue);
            CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
        } else {
            CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
            CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);
        }

        status = SecItemCopyMatching(query, &result);

        if (action == LIST) {
            lua_newtable(L);

            if (status == errSecSuccess) {
                for (CFIndex i = 0; i < CFArrayGetCount(result); ++i) {
                    CFDictionaryRef item = CFArrayGetValueAtIndex(result, i);
                    CFStringRef account = CFDictionaryGetValue(item, kSecAttrAccount);
                    char buffer[43];

                    if (!account || !CFStringGetCString(account, buffer, sizeof(buffer),
                                                       kCFStringEncodingUTF8)) {
                        status = errSecDecode;
                        break;
                    }

                    lua_pushstring(L, buffer);
                    lua_rawseti(L, -2, i + 1);
                }
            }
        } else if (status == errSecSuccess) {
            lua_pushlstring(L, (const char *)CFDataGetBytePtr(result),
                CFDataGetLength(result));
        } else {
            lua_pushnil(L);
        }
    }

    // Release native objects before raising a Lua error, which does not return.
    if (result) {
        CFRelease(result);
    }
    CFRelease(query);

    if (status != errSecSuccess && status != errSecItemNotFound) {
        return luaL_error(L, "macOS keychain operation failed (%d)", (int)status);
    }

    return 1;
}

#elif defined(__linux__)
#include <libsecret/secret.h>

static const SecretSchema schema = {
    .name = "org.evmxt.wallet",
    .flags = SECRET_SCHEMA_NONE,
    .attributes = {
        { "address", SECRET_SCHEMA_ATTRIBUTE_STRING },
        { NULL, 0 }
    }
};

static int keychain(lua_State *L, int action, const char *address, const char *payload)
{
    GError *error = NULL;

    if (action == STORE) {
        // Matching addresses are replaced in the persistent default collection.
        lua_pushboolean(L, secret_password_store_sync(&schema, SECRET_COLLECTION_DEFAULT,
            "evmxt wallet", payload, NULL, &error, "address", address, NULL));
    } else {
        GHashTable *attrs = secret_attributes_build(&schema, NULL);

        if (address) {
            g_hash_table_insert(attrs, g_strdup("address"), g_strdup(address));
        }

        // Request access for reads and deletes, including locked entries.
        GList *items = secret_service_search_sync(NULL, &schema, attrs,
            SECRET_SEARCH_ALL | SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS,
            NULL, &error);
        g_hash_table_unref(attrs);

        // Start with the result to return when no entry matches.
        if (action == LIST) {
            lua_newtable(L);
        } else if (action == DELETE) {
            lua_pushboolean(L, 0);
        } else {
            lua_pushnil(L);
        }

        int index = 0;
        for (GList *it = items; it && !error; it = it->next) {
            SecretValue *secret = secret_item_get_secret(it->data);

            // A search can succeed even when the user declines to unlock an item.
            if (!secret) {
                g_set_error_literal(&error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
                                    "Keychain item is locked or inaccessible");
                break;
            }

            if (action == LIST) {
                GHashTable *attributes = secret_item_get_attributes(it->data);
                const char *account = g_hash_table_lookup(attributes, "address");

                if (account) {
                    lua_pushstring(L, account);
                    lua_rawseti(L, -2, ++index);
                }

                g_hash_table_unref(attributes);
            } else if (action == DELETE) {
                int removed = secret_item_delete_sync(it->data, NULL, &error);

                lua_pop(L, 1);
                lua_pushboolean(L, removed);
            } else {
                gsize size;
                const char *value = secret_value_get(secret, &size);

                lua_pop(L, 1);
                lua_pushlstring(L, value, size);
            }

            secret_value_unref(secret);

            if (action == LOAD) {
                break;
            }
        }

        g_list_free_full(items, g_object_unref);
    }

    if (error) {
        g_error_free(error);
        return luaL_error(L, "Linux keychain operation failed: "
            "Secret Service unavailable or access denied");
    }

    return 1;
}

#else
#error Wallet storage supports only macOS and Linux
#endif

// Dispatch the Lua module's request(action, address, payload) call.
static int request(lua_State *L)
{
    static const char *actions[] = { "store", "load", "delete", "list", NULL };
    int action = luaL_checkoption(L, 1, NULL, actions);
    const char *address = NULL;
    const char *payload = NULL;

    if (action != LIST) {
        address = luaL_checkstring(L, 2);
    }

    if (action == STORE) {
        payload = luaL_checkstring(L, 3);
    }

    return keychain(L, action, address, payload);
}

int luaopen_wallet_native(lua_State *L)
{
    static const luaL_Reg functions[] = {
        { "request", request },
        { "derive", derive },
        { NULL, NULL }
    };

    luaL_newlib(L, functions);
    return 1;
}
