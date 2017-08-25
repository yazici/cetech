#ifndef CETECH_ASSET_PROPERTY_H
#define CETECH_ASSET_PROPERTY_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

//==============================================================================
// Includes
//==============================================================================

#include <stddef.h>

//==============================================================================
// Typedefs
//==============================================================================
typedef void (*ct_ap_on_asset)(uint64_t type, uint64_t name);

//==============================================================================
// Api
//==============================================================================

struct ct_asset_property_a0 {
    void (*register_asset)(uint64_t type, ct_ap_on_asset on_asset);
};

#ifdef __cplusplus
}
#endif

#endif //CETECH_ASSET_PROPERTY_H
