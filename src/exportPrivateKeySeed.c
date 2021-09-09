#include <os.h>
#include "cx.h"
#include <stdint.h>
#include "util.h"
#include "globals.h"
#include <string.h>
#include "responseCodes.h"

// This class allows for the export of a number of very specific private keys. These private keys are made
// exportable as they are used in computations that are not feasible to carry out on the Ledger device.
// The key derivation paths that are allowed are restricted so that it is not possible to export
// keys that are used for signing.
static const uint32_t HARDENED_OFFSET = 0x80000000;
static exportPrivateKeySeedContext_t *ctx = &global.exportPrivateKeySeedContext;

void exportPrivateKey();

UX_STEP_CB(
    ux_export_private_key_0_step,
    bn,
    exportPrivateKey(),
    {
        (char *) global.exportPrivateKeySeedContext.displayHeader,
        (char *) global.exportPrivateKeySeedContext.display
    });
UX_FLOW(ux_export_private_key,
    &ux_export_private_key_0_step
);

#define ID_CRED_SEC 0
#define PRF_KEY 1

#define pathLength 6

void exportPrivateKey() {
    cx_ecfp_private_key_t privateKey;
    BEGIN_TRY {
        TRY {
            ctx->path[5] = PRF_KEY | HARDENED_OFFSET;
            getPrivateKey(ctx->path, pathLength, &privateKey);
            uint8_t tx = 0;
            for (int i = 0; i < 32; i++) {
                G_io_apdu_buffer[tx++] = privateKey.d[i];
            }

            if (ctx->exportBoth) {
                ctx->path[5] = ID_CRED_SEC | HARDENED_OFFSET;
                getPrivateKey(ctx->path, pathLength, &privateKey);
                for (int i = 0; i < 32; i++) {
                    G_io_apdu_buffer[tx++] = privateKey.d[i];
                }
            }

            sendSuccess(tx);
        }
        FINALLY {
            explicit_bzero(&privateKey, sizeof(privateKey));
        }
    }
    END_TRY;
}

#define ACCOUNT_SUBTREE 0
#define NORMAL_ACCOUNTS 0

// Export the PRF key seed
#define P1_PRF_KEY           0x00
#define P1_PRF_KEY_RECOVERY  0x01
// Export the PRF key seed and the IdCredSec seed
#define P1_BOTH              0x02

void handleExportPrivateKeySeed(uint8_t *dataBuffer, uint8_t p1, uint8_t p2, volatile unsigned int *flags) {
    if ((p1 != P1_BOTH && p1 != P1_PRF_KEY && p1 != P1_PRF_KEY_RECOVERY) || p2 != 0x01) {
        THROW(ERROR_INVALID_PARAM);
    }

    uint32_t identity = U4BE(dataBuffer, 0);
    uint32_t keyDerivationPath[5] = {
        CONCORDIUM_PURPOSE | HARDENED_OFFSET,
        CONCORDIUM_COIN_TYPE | HARDENED_OFFSET,
        ACCOUNT_SUBTREE | HARDENED_OFFSET,
        NORMAL_ACCOUNTS | HARDENED_OFFSET,
        identity | HARDENED_OFFSET
    };
    memmove(ctx->path, keyDerivationPath, sizeof(keyDerivationPath));

    memmove(ctx->display, "ID #", 4);
    bin2dec(ctx->display + 4, identity);

    if (p1 == P1_BOTH) {
        memmove(ctx->displayHeader, "Create credential", 17);
        ctx->displayHeader[17] = '\0';
        ctx->exportBoth = true;
    } else if (p1 == P1_PRF_KEY_RECOVERY) {
        memmove(ctx->displayHeader, "Recover credentials", 19);
        ctx->displayHeader[19] = '\0';
        ctx->exportBoth = false;
    } else if (p1 == P1_PRF_KEY) {
        memmove(ctx->displayHeader, "Decrypt", 7);
        ctx->displayHeader[7] = '\0';
        ctx->exportBoth = false;
    }
    ux_flow_init(0, ux_export_private_key, NULL);
    *flags |= IO_ASYNCH_REPLY;
}
