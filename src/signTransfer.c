#include <os.h>
#include "util.h"
#include "sign.h"
#include "memo.h"
#include "accountSenderView.h"
#include "base58check.h"
#include "responseCodes.h"

static signTransferContext_t *ctx = &global.withMemo.signTransferContext;
static memoContext_t *memo_ctx = &global.withMemo.memoContext;
static tx_state_t *tx_state = &global_tx_state;

UX_STEP_NOCB(
    ux_sign_flow_1_step,
    bnnn_paging,
    {
        "Amount (GTU)",
        (char *) global.withMemo.signTransferContext.displayAmount
    });

UX_STEP_NOCB(
    ux_sign_flow_2_step,
    bnnn_paging,
    {
        .title = "Recipient",
        .text = (char *) global.withMemo.signTransferContext.displayStr
    });
UX_FLOW(ux_sign_flow,
    &ux_sign_flow_shared_review,
    &ux_sign_flow_account_sender_view,
    &ux_sign_flow_1_step,
    &ux_sign_flow_2_step,
    &ux_sign_flow_shared_sign,
    &ux_sign_flow_shared_decline
);

UX_STEP_CB(
    ux_sign_flow_2_step_cb,
    bnnn_paging,
    sendSuccessNoIdle(),
    {
        .title = "Recipient",
        .text = (char *) global.withMemo.signTransferContext.displayStr
    });

UX_FLOW(ux_transfer_initial_flow_memo,
    &ux_sign_flow_shared_review,
    &ux_sign_flow_account_sender_view,
    &ux_sign_flow_2_step_cb
);

UX_FLOW(ux_memo_sign_flow,
    &ux_sign_flow_1_step,
    &ux_sign_flow_shared_sign,
    &ux_sign_flow_shared_decline
);

#define P1_INITIAL                      0x00
#define P1_INITIAL_WITH_MEMO            0x01
#define P1_MEMO                         0x02
#define P1_AMOUNT                       0x03

int handleHeaderAndToAddressSimpleTransfer(uint8_t *cdata, uint8_t kind) {
    // Parse the key derivation path, which should always be the first thing received
    // in a command to the Ledger application.
    int keyPathLength = parseKeyDerivationPath(cdata);
    cdata += keyPathLength;

    // Initialize the hash that will be the hash of the whole transaction, which is what will be signed
    // if the user approves.
    cx_sha256_init(&tx_state->hash);
    int headerLength = hashAccountTransactionHeaderAndKind(cdata, kind);
    cdata += headerLength;

    // Extract the recipient address and add to the hash.
    uint8_t toAddress[32];
    memmove(toAddress, cdata, 32);
    cdata += 32;
    cx_hash((cx_hash_t *) &tx_state->hash, 0, toAddress, 32, NULL, 0);

    // The recipient address is in a base58 format, so we need to encode it to be
    // able to display in a humand-readable way. This is written to ctx->displayStr as a string
    // so that it can be displayed.
    size_t outputSize = sizeof(ctx->displayStr);
    if (base58check_encode(toAddress, sizeof(toAddress), ctx->displayStr, &outputSize) != 0) {
        // The received address bytes are not a valid base58 encoding.
        THROW(ERROR_INVALID_TRANSACTION);
    }
    ctx->displayStr[55] = '\0';
    return keyPathLength + headerLength + 32;
}

void handleSignTransfer(uint8_t *cdata, volatile unsigned int *flags) {
    cdata += handleHeaderAndToAddressSimpleTransfer(cdata, TRANSFER);

    // Build display value of the amount to transfer, and also add the bytes to the hash.
    uint64_t amount = U8BE(cdata, 0);
    amountToGtuDisplay(ctx->displayAmount, amount);
    cx_hash((cx_hash_t *) &tx_state->hash, 0, cdata, 8, NULL, 0);

    // Display the transaction information to the user (recipient address and amount to be sent).
    ux_flow_init(0, ux_sign_flow, NULL);

    // Tell the main process to wait for a button press.
    *flags |= IO_ASYNCH_REPLY;
}

void finishMemo(volatile unsigned int *flags) {
    ctx->state = TX_TRANSFER_AMOUNT;
    ux_flow_init(0, ux_sign_transfer_memo, NULL);
    *flags |= IO_ASYNCH_REPLY;
}

void handleSignTransferWithMemo(uint8_t *cdata, uint8_t p1, uint8_t dataLength, volatile unsigned int *flags, bool isInitialCall) {
    if (isInitialCall) {
        ctx->state = TX_TRANSFER_INITIAL;
    }

    if (p1 == P1_INITIAL_WITH_MEMO && ctx->state == TX_TRANSFER_INITIAL) {
        cdata += handleHeaderAndToAddressSimpleTransfer(cdata, TRANSFER_WITH_MEMO);

        // hash the memo length
        memo_ctx->memoLength = U2BE(cdata, 0);
        cx_hash((cx_hash_t *) &tx_state->hash, 0, cdata, 2, NULL, 0);

        ctx->state = TX_TRANSFER_MEMO_INITIAL;
        ux_flow_init(0, ux_transfer_initial_flow_memo, NULL);
        // Tell the main process to wait for a button press.
        *flags |= IO_ASYNCH_REPLY;
    } else if (p1 == P1_MEMO && ctx->state == TX_TRANSFER_MEMO_INITIAL) {
        cx_hash((cx_hash_t *) &tx_state->hash, 0, cdata, dataLength, NULL, 0);

        // Read initial part of memo and then display it:
        readMemoInitial(cdata, dataLength);

        if (memo_ctx->memoLength == 0) {
            finishMemo(flags);
        } else {
            ctx->state = TX_TRANSFER_MEMO;
            sendSuccessNoIdle();
        }
    } else if (p1 == P1_MEMO && ctx->state == TX_TRANSFER_MEMO) {
        cx_hash((cx_hash_t *) &tx_state->hash, 0, cdata, dataLength, NULL, 0);

        // Read current part of memo and then display it:
        readMemoContent(cdata, dataLength);

        if (memo_ctx->memoLength != 0) {
            // The memo size is <=256 bytes, so we should always have received the complete memo by this point;
            THROW(ERROR_INVALID_STATE);
        }

        finishMemo(flags);
    } else if (p1 == P1_AMOUNT && ctx->state == TX_TRANSFER_AMOUNT) {
        // Build display value of the amount to transfer, and also add the bytes to the hash.
        uint64_t amount = U8BE(cdata, 0);
        amountToGtuDisplay(ctx->displayAmount, amount);
        cx_hash((cx_hash_t *) &tx_state->hash, 0, cdata, 8, NULL, 0);

        ux_flow_init(0, ux_memo_sign_flow, NULL);
        *flags |= IO_ASYNCH_REPLY;
    } else {
        THROW(ERROR_INVALID_STATE);
    }
}

