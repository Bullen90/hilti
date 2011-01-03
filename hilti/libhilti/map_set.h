// $Id$
///
/// \addtogroup map
/// @{
/// Functions for maps and sets. They share sufficient code to be implemented in the same module.
/// @}

#ifndef HILTI_MAP_SET_H
#define HILTI_MAP_SET_H

#include "hilti.h"

typedef struct __hlt_map hlt_map;            /// Type for representing a HILTI map.
typedef struct __hlt_map_iter hlt_map_iter;  /// Type for representing an iterator to a HILTI map.

typedef struct __hlt_set hlt_set;            /// Type for representing a HILTI set.
typedef struct __hlt_set_iter hlt_set_iter;  /// Type for representing an iterator to a HILTI set.

typedef void* __khkey_t;

/// Cookie for map entry expiration timers.
typedef struct {
    hlt_map* map;
    __khkey_t key;
} __hlt_map_timer_cookie;

/// Cookie for set entry expiration timers.
typedef struct {
    hlt_set* set;
    __khkey_t key;
} __hlt_set_timer_cookie;

////////// Maps.

/// Instantiates a new map.
///
/// key: The type for the map's keys.
///
/// value: The type for the map's values.
///
/// tmgr: A timer manager to be associated with the map. Can be null if
/// expiration is not going to be used.
///
/// excpt: &
struct __hlt_timer_mgr;
extern hlt_map* hlt_map_new(const hlt_type_info* key, const hlt_type_info* value, struct __hlt_timer_mgr* tmgr, hlt_exception** excpt, hlt_execution_context* ctx);

/// Gets the value associated with a key in map.
///
/// m: The map.
///
/// key: The key to look up.
///
/// excpt: &
///
/// Returns: The value.
///
/// Raises: IndexError - If the key does not exist.
extern void* hlt_map_get(hlt_map* m, const hlt_type_info* type, void* key, hlt_exception** excpt, hlt_execution_context* ctx);

/// Gets the value associated with a key in map if it exists, and returns a
/// default value otherwise.
///
/// m: The map.
///
/// key: The key to look up.
///
/// def: The default value.
///
/// excpt: &
///
/// Returns: The value, or the default if key does not exist.
///
/// Raises: KeyError - If the key does not exist and not default has been set via ~~hlt_map_default.
extern void* hlt_map_get_default(hlt_map* m, const hlt_type_info* tkey, void* key, const hlt_type_info* tdef, void* def, hlt_exception** excpt, hlt_execution_context* ctx);

/// Inserts a key with a value into a map. If the key already exisits, the
/// old value is overwritten.
///
/// m: The map.
///
/// key: The key.
///
/// value: The value to store for the key.
///
/// excpt: &
extern void  hlt_map_insert(hlt_map* m, const hlt_type_info* tkey, void* key, const hlt_type_info* tval, void* value, hlt_exception** excpt, hlt_execution_context* ctx);

/// Checks whether a key exists in a map.
///
/// m: The map.
///
/// key: The key.
///
/// excpt: &
///
/// Returns: True if the key exists.
extern int8_t hlt_map_exists(hlt_map* m, const hlt_type_info* type, void* key, hlt_exception** excpt, hlt_execution_context* ctx);

/// Remove a key from a map. If the key does not exists, the function call has not effect.
///
/// m: The map.
///
/// key: The key.
///
/// excpt: &
extern void hlt_map_remove(hlt_map* m, const hlt_type_info* type, void* key, hlt_exception** excpt, hlt_execution_context* ctx);

/// Called by an expiring timer to remove an element from the map.
///
/// cookie: The cookie identifying the element to be removed.
extern void hlt_list_map_expire(__hlt_map_timer_cookie cookie);

/// Returns the number of keys in a map.
///
/// m: The map.
///
/// excpt: &
///
/// Returns: The size of the map.
extern int64_t hlt_map_size(hlt_map* m, hlt_exception** excpt, hlt_execution_context* ctx);

/// Remove all keys from a map.
///
/// m: The map.
///
/// excpt: &
extern void hlt_map_clear(hlt_map* m, hlt_exception** excpt, hlt_execution_context* ctx);

/// Sets a default to return when a key does not exist.
///
/// m: The map.
///
/// def: The default value.
///
/// excpt: &
extern void  hlt_map_default(hlt_map* m, const hlt_type_info* tdef, void* def, hlt_exception** excpt, hlt_execution_context* ctx);

/// Actives automatic expiration of map entries. Subsequently added entries
/// will be expired after a timeout expires.
///
/// m: The map.
///
/// strategy: The expiration strategy to use
///
/// timeout: Timeout in seconds after which to expire entries. If zero, expiration is disabled.
///
/// excpt: &
///
/// Raises: NoTimerManager if not timer manager has been associated with the map.
extern void hlt_map_timeout(hlt_map* m, hlt_enum strategy, double timeout, hlt_exception** excpt, hlt_execution_context* ctx);

/// Returns an iterator pointing the first map element.
///
/// m: The map.
///
/// excpt: &
///
/// Returns: The start of the map.
extern hlt_map_iter hlt_map_begin(hlt_map* v, hlt_exception** excpt, hlt_execution_context* ctx);

/// Returns an iterator pointing the last element of any map.
///
/// m: The map.
///
/// excpt: &
///
/// Returns: The end of a map.
extern hlt_map_iter hlt_map_end(hlt_exception** excpt, hlt_execution_context* ctx);

/// Advances a map iterator to the next position. Note that the order in
/// which elements are visited is undefined.
///
/// m: The map.
///
/// i: The iterator.
///
/// excpt: &
///
/// Returns: An iterator advanced by one element.
extern hlt_map_iter hlt_map_iter_incr(hlt_map_iter i, hlt_exception** excpt, hlt_execution_context* ctx);

/// Dereferences a map iterator.
///
/// m: The map.
///
/// tuple: The type of the result tuple.
///
/// i: The iterator.
///
/// excpt: &
///
/// Returns: Tuple ```(key, value)`` at the position the iterator points to.
///
/// Raises: InvalidIterator if the iterator does not refer to a deferencable
/// position.
///
/// Note: Dereferencing an iterator does not count as an access to the entry
/// for restarting its expiration timer.
extern void* hlt_map_iter_deref(const hlt_type_info* tuple, hlt_map_iter i, hlt_exception** excpt, hlt_execution_context* ctx);

/// Compares two map iterators whether they are refering to the same element.
///
/// i1: The first iterator.
///
/// i2: The second iterator.
///
/// excpt: &
///
/// Returns: True if both iterators are equal.
extern int8_t hlt_map_iter_eq(hlt_map_iter i1, hlt_map_iter i2, hlt_exception** excpt, hlt_execution_context* ctx);

////////// Sets.

/// Instantiates a new set.
///
/// key: The type for the set's keys.
///
/// tmgr: A timer manager to be associated with the set. Can be null if
/// expiration is not going to be used.
///
/// excpt: &
extern hlt_set* hlt_set_new(const hlt_type_info* key, struct __hlt_timer_mgr* tmgr, hlt_exception** excpt, hlt_execution_context* ctx);

// Inserts a key with a value into a set. If the key already exisits, the
/// old value is overwritten.
///
/// m: The set.
///
/// key: The key.
///
/// excpt: &
extern void  hlt_set_insert(hlt_set* m, const hlt_type_info* tkey, void* key, hlt_exception** excpt, hlt_execution_context* ctx);

/// Checks whether a key exists in a set.
///
/// m: The set.
///
/// key: The key.
///
/// excpt: &
///
/// Returns: True if the key exists.
extern int8_t hlt_set_exists(hlt_set* m, const hlt_type_info* type, void* key, hlt_exception** excpt, hlt_execution_context* ctx);

/// Remove a key from a set. If the key does not exists, the function call has not effect.
///
/// m: The set.
///
/// key: The key.
///
/// excpt: &
extern void hlt_set_remove(hlt_set* m, const hlt_type_info* type, void* key, hlt_exception** excpt, hlt_execution_context* ctx);

/// Called by an expiring timer to remove an element from the set.
///
/// cookie: The cookie identifying the element to be removed.
extern void hlt_list_set_expire(__hlt_set_timer_cookie cookie);

/// Returns the number of keys in a set.
///
/// m: The set.
///
/// excpt: &
///
/// Returns: The size of the set.
extern int64_t hlt_set_size(hlt_set* m, hlt_exception** excpt, hlt_execution_context* ctx);

/// Remove all keys from a set.
///
/// m: The set.
///
/// excpt: &
extern void hlt_set_clear(hlt_set* m, hlt_exception** excpt, hlt_execution_context* ctx);

/// Actives automatic expiration of set entries. Subsequently added entries
/// will be expired after a timeout expires.
///
/// m: The set.
///
/// strategy: The expiration strategy to use
///
/// timeout: Timeout in seconds after which to expire entries. If zero, expiration is disabled.
///
/// excpt: &
///
/// Raises: NoTimerManager if not timer manager has been associated with the set.
extern void hlt_set_timeout(hlt_set* m, hlt_enum strategy, double timeout, hlt_exception** excpt, hlt_execution_context* ctx);

/// Returns an iterator pointing the first set element.
///
/// m: The set.
///
/// excpt: &
///
/// Returns: The start of the set.
extern hlt_set_iter hlt_set_begin(hlt_set* v, hlt_exception** excpt, hlt_execution_context* ctx);

/// Returns an iterator pointing the last element of any set.
///
/// m: The set.
///
/// excpt: &
///
/// Returns: The end of a set.
extern hlt_set_iter hlt_set_end(hlt_exception** excpt, hlt_execution_context* ctx);

/// Advances a set iterator to the next position. Note that the order in
/// which elements are visited is undefined.
///
/// m: The set.
///
/// i: The iterator.
///
/// excpt: &
///
/// Returns: An iterator advanced by one element.
extern hlt_set_iter hlt_set_iter_incr(hlt_set_iter i, hlt_exception** excpt, hlt_execution_context* ctx);

/// Dereferences a set iterator.
///
/// m: The set.
///
/// tuple: The type of the result tuple.
///
/// i: The iterator.
///
/// excpt: &
///
/// Returns: Tuple ```(key, value)`` at the position the iterator points to.
///
/// Raises: InvalidIterator if the iterator does not refer to a deferencable
/// position.
///
/// Note: Dereferencing an iterator does not count as an access to the entry
/// for restarting its expiration timer.
extern void* hlt_set_iter_deref(hlt_set_iter i, hlt_exception** excpt, hlt_execution_context* ctx);

/// Compares two set iterators whether they are refering to the same element.
///
/// i1: The first iterator.
///
/// i2: The second iterator.
///
/// excpt: &
///
/// Returns: True if both iterators are equal.
extern int8_t hlt_set_iter_eq(hlt_set_iter i1, hlt_set_iter i2, hlt_exception** excpt, hlt_execution_context* ctx);

////////// Other helpers.

/// Calculates a hash value for a sequence of bytes.
///
/// s: The bytes.
///
/// len: The number of bytes to include, starting at *s*.
///
/// Returns: The hash value.
static inline hlt_hash hlt_hash_bytes(const int8_t *s, int16_t len)
{
    // This is copied and adapted from hhash.h
    if ( ! len )
        return 0;

	hlt_hash h = 0;
    while ( len-- )
        h = (h << 5) - h + *s++;

	return h;
}

#endif
