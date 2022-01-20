#ifndef _CONCORDIUM_APP_CONFIGURE_BAKER_H_
#define _CONCORDIUM_APP_CONFIGURE_BAKER_H_

/**
 * Handles the signing flow for an 'Configure Baker' transaction. It validates
 * that the correct UpdateType is supplied and will fail otherwise.
 * @param cdata please see /doc/ins_configure_delegation.md for details
 */
void handleSignConfigureBaker(uint8_t *cdata, uint8_t p1, volatile unsigned int *flags, bool isInitialCall);

typedef struct {
    uint8_t displayCapital[26];
    uint8_t displayRestake[4];
    uint8_t displayOpenForDelegation[15];
    bool hasCapital;
    bool hasRestakeEarnings;
    bool hasOpenForDelegation;
    bool hasSignatureVerifyKey;
    bool hasElectionVerifyKey;
    bool hasAggregationVerifyKey;
    bool hasMetadataUrl;
    bool hasTransactionFeeCommission;
    bool hasBakingRewardCommission;
    bool hasFinalizationRewardCommission;
    bool firstUrl;
    uint16_t urlLength;
    uint8_t url[255];
} signConfigureBaker_t;

#endif
